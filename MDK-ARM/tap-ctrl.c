#include "stm32f1xx_hal.h"
#include <string.h>
#include <stdio.h>
//#include <stdlib.h>

#include "fatfs.h"
#include "usbd_cdc_if.h"
#include "sd.h"

extern char USER_Path[4]; /* logical drive path */
FATFS SDFatFs1;
FATFS *fs;
FIL MyFile;

//possible JTAG TAP states
#define TAP_STATE_TLR 0
#define TAP_STATE_RTI 1
#define TAP_STATE_SELECT_DR_SCAN 2
#define TAP_STATE_SELECT_IR_SCAN 3
#define TAP_STATE_CAPTURE_DR 4
#define TAP_STATE_CAPTURE_IR 5
#define TAP_STATE_SHIFT_DR 6
#define TAP_STATE_SHIFT_IR 7
#define TAP_STATE_EXIT_1_DR 8
#define TAP_STATE_EXIT_1_IR 9
#define TAP_STATE_PAUSE_DR 10
#define TAP_STATE_PAUSE_IR 11
#define TAP_STATE_EXIT_2_DR 12
#define TAP_STATE_EXIT_2_IR 13
#define TAP_STATE_UPDATE_DR 14
#define TAP_STATE_UPDATE_IR 15

#define MAX_STRING_LENGTH (255*4)
#define BUF_SIZE 1024

#define MIN_TCK_DELAY 100
char wordBuf[MAX_STRING_LENGTH];
//unsigned char * wordBuf;
//#define MAX_FREQUENCY 
int currentState = TAP_STATE_RTI, 
	ENDIRstate= TAP_STATE_RTI,
	ENDDRstate= TAP_STATE_RTI;
int tckDelay = 1;

//---------------------------------------------------------
//TAP functions
GPIO_InitTypeDef GPIO_InitStruct;//для установки TRST в Z
 
void pulseTCK(){
	HAL_GPIO_WritePin(GPIOC, TCK_Pin, GPIO_PIN_SET);
	HAL_Delay(tckDelay);
  HAL_GPIO_WritePin(GPIOC, TCK_Pin, GPIO_PIN_RESET);
	HAL_Delay(tckDelay);

 };

void outputCharToTDI(unsigned char c) {
			//HAL_Delay(10);			CDC_Transmit_FS((uint8_t*)"\r\nsend ch\r\n",15);
	unsigned char mask = 128;
	for (int i = 0; i < 8; i++)
	{
		//Проверяем старший бит)
		if (c & mask)
			HAL_GPIO_WritePin(GPIOB, TDI_Pin, GPIO_PIN_SET);
		else
			HAL_GPIO_WritePin(GPIOB, TDI_Pin, GPIO_PIN_RESET);
		//Сдвигаем влево на 1 бит
		c = c << 1;
		pulseTCK();
	}
//HAL_Delay(10);			CDC_Transmit_FS((uint8_t*)"\r\nend of ch\r\n",15);
};
void sendArray(unsigned char* ptr, unsigned int numChars) {
	//	HAL_Delay(10);			CDC_Transmit_FS((uint8_t*)"\r\nsend array\r\n",14);
	unsigned char c; 

	for (unsigned int i = 0; i < numChars; i++) {
			//HAL_Delay(10);			CDC_Transmit_FS((uint8_t*)"\r\nfor begin\r\n",14);
		c = *ptr;
		outputCharToTDI(c);
		ptr++;
	}
	//		HAL_Delay(10);			CDC_Transmit_FS((uint8_t*)"\r\nend arr\r\n",19);
};

void setTMSpath(int path[10], int steps){
	//HAL_Delay(10);			CDC_Transmit_FS((uint8_t*)"\r\nset path\r\n",15);
	//HAL_Delay(10);			CDC_Transmit_FS((uint8_t*)steps,15);
	for (int i = 0; i <= steps; i++){
			if (path[i]==0)
				HAL_GPIO_WritePin(GPIOB, TMS_Pin, GPIO_PIN_RESET);
			else 
				HAL_GPIO_WritePin(GPIOB, TMS_Pin, GPIO_PIN_SET);
			pulseTCK();
		
			}
//HAL_Delay(10);			CDC_Transmit_FS((uint8_t*)"\r\nend path\r\n",15);
}
void stateNavigate(int nextState){
	//HAL_Delay(10);			CDC_Transmit_FS((uint8_t*)"\r\nstateNavigate\r\n",25);
	int path[10], steps = 0;
		switch (currentState) {
			case TAP_STATE_SHIFT_IR: 
				switch (nextState) {
		case TAP_STATE_TLR:
			path[0] = 1; path[1] = 1;
			path[2] = 1; path[3] = 1;
			path[4] = 1; path[5] =1;
			steps = 5;			
			break;
		case TAP_STATE_RTI:			
			path[0] = 1; path[1] = 1;
			path[2] = 0; path[3] = 0;	
			steps = 3;
			break;
		case TAP_STATE_PAUSE_IR:
			path[0] = 1; path[1] = 0;
			path[2] = 0; 
			steps = 2;
			break;
		case TAP_STATE_PAUSE_DR:
			path[0] = 1; path[1] = 1;
			path[2] = 1; path[3] = 0;
			path[4] = 1; path[5] = 0;
			path[6] = 0;
			steps = 6;
			break;
		}
				break;
			case TAP_STATE_SHIFT_DR: 
				switch (nextState) {
		case TAP_STATE_TLR:
			path[0] = 1; path[1] = 1;
			path[2] = 1; path[3] = 1;
			path[4] = 0; path[5] = 0;
			steps = 5;			
			break;
		case TAP_STATE_RTI:			
			path[0] = 1; path[1] = 1;
			path[2] = 0; path[3] = 0;	
			steps = 3;
			break;
		case TAP_STATE_PAUSE_DR:
			path[0] = 1; path[1] = 0;
			path[2] = 0; 
			steps = 2;
			break;
		case TAP_STATE_PAUSE_IR:
			path[0] = 1; path[1] = 1;
			path[2] = 1; path[3] = 1;
			path[4] = 0; path[5] = 1;
			path[6] = 0; path[7] = 0;
			steps = 7;
			break;
		}
				break;
		case TAP_STATE_TLR:
			switch (nextState) {
			case TAP_STATE_RTI: path[0] = 1;
				break;
			case TAP_STATE_TLR:
				path[0] = 0; path[1] = 0; steps = 1;
				break;
			case TAP_STATE_PAUSE_DR:
				path[0] = 0; path[1] = 1;
				path[2] = 0; path[3] = 1;
				path[4] = 0; path[5] = 0; 
				steps = 5;
				break;
			case TAP_STATE_PAUSE_IR:
				path[0] = 0; path[1] = 1;
				path[2] = 1; path[3] = 0;
				path[4] = 1; path[5] = 0;
				path[6] = 0;
				steps = 6;
				break;
			}
			break;
		case TAP_STATE_RTI:
			switch (nextState) {
			case TAP_STATE_RTI:
				path[0] = 0;  steps = 0;
				break;
			case TAP_STATE_TLR:
				path[0] = 1; path[1] = 1;
				path[2] = 1; path[3] = 1;
				steps = 3;
				break;
			case TAP_STATE_PAUSE_DR:
				path[0] = 1; path[1] = 0;
				path[2] = 1; path[3] = 0;
				path[4] = 0;  
				steps = 4;
				break;
			case TAP_STATE_PAUSE_IR:
				path[0] = 1; path[1] = 1;
				path[2] = 0; path[3] = 1;
				path[4] = 0; path[5] = 0;
				steps = 5;
				break;
			}
			break;
		case TAP_STATE_PAUSE_DR:
			switch (nextState) {
			case TAP_STATE_RTI:
				path[0] = 1; path[1] = 1;
				path[2] = 0; path[3] = 0;
				steps = 3;
				break;
			case TAP_STATE_TLR:

				path[0] = 1; path[1] = 1;
				path[2] = 1; path[3] = 1;
				path[4] = 1; path[5] = 1;
				steps = 5;
				break;
			case TAP_STATE_PAUSE_DR:
				//DRPAUSE-DREXIT2-DRUPDATE-DRSELECTDRCAPTURE-DREXIT1-DRPAUSE
				path[0] = 1; path[1] = 1;
				path[2] = 1; path[3] = 0;
				path[4] = 1; path[5] = 0;
				path[6] = 0;  
				steps = 6;
				break;
			case TAP_STATE_PAUSE_IR:
				path[0] = 1; path[1] = 1;
				path[2] = 1; path[3] = 1;
				path[4] = 0; path[5] = 1;
				path[6] = 0; path[7] = 0;
				steps = 7;
				break;
			}
			break;
		case TAP_STATE_PAUSE_IR:
			switch (nextState) {
			case TAP_STATE_RTI:
				path[0] = 1; path[1] = 1;
				path[2] = 1; path[3] = 0;
				steps = 3;
				break;
			case TAP_STATE_TLR:
				path[0] = 1; path[1] = 1;
				path[2] = 1; path[3] = 1;
				path[4] = 1; path[5] = 1;
				path[6] = 0;
				steps = 6;
				break;
			case TAP_STATE_PAUSE_DR:
				path[0] = 1; path[1] = 1;
				path[2] = 1; path[3] = 0;
				path[4] = 1; path[5] = 0;
				steps = 5;
				break;
			case TAP_STATE_PAUSE_IR:
				path[0] = 1; path[1] = 1;
				path[2] = 1; path[3] = 1;
				path[4] = 0; path[5] = 1;
				path[6] = 0; path[7] = 0;
				steps = 7;
				break;
			}break;
		}		
	//HAL_Delay(10);			CDC_Transmit_FS((uint8_t*)"\r\nset path\r\n",25);
		setTMSpath(path, steps);
		//	HAL_Delay(10);			CDC_Transmit_FS((uint8_t*)"\r\nset cur.state\r\n",25);
currentState = nextState; 
	//HAL_Delay(10);			CDC_Transmit_FS((uint8_t*)"\r\nend stateNavigate\r\n",25);		
};

void setTAPtoDRSHIFT(){
int path[10], steps = 0;
		switch (currentState) {
		case TAP_STATE_TLR:
			path[0] = 0; path[1] = 1;
			path[2] = 0; path[3] = 0;
			path[4] = 0; 
			steps = 4;			
			break;
		case TAP_STATE_RTI:
			
			path[0] = 1; path[1] = 0;
			path[2] = 0; path[3] = 0;			
			steps = 3;
			break;
		case TAP_STATE_PAUSE_DR:
			path[0] = 1; path[1] = 0;
			path[2] = 0; 
			steps = 2;
			break;
		case TAP_STATE_PAUSE_IR:
			path[0] = 1; path[1] = 1;
			path[2] = 1; path[3] = 0;
			path[4] = 0; path[5] = 0;
			steps = 5;
			break;
		}
	setTMSpath(path, steps);	
currentState = TAP_STATE_SHIFT_DR; 			
};

void setTAPtoIRSHIFT(){
int path[10], steps = 0;
		switch (currentState) {
		case TAP_STATE_TLR:
			path[0] = 0; path[1] = 1;
			path[2] = 1; path[3] = 0;
			path[4] = 0; path[5] = 0;
			steps = 5;			
			break;
		case TAP_STATE_RTI:			
			path[0] = 1; path[1] = 1;
			path[2] = 0; path[3] = 0;	
			path[4] = 0;
			steps = 4;
			break;
		case TAP_STATE_PAUSE_IR:
			path[0] = 1; path[1] = 0;
			path[2] = 0; 
			steps = 2;
			break;
		case TAP_STATE_PAUSE_DR:
			path[0] = 1; path[1] = 1;
			path[2] = 1; path[3] = 1;
			path[4] = 0; path[5] = 0;
			path[6] = 0;
			steps = 5;
			break;
		}
setTMSpath(path, steps);
		currentState = TAP_STATE_SHIFT_IR; 		
};


///////////////////////////////////////////////////////////////////////////////////
//read array, char to hex
//////////////////////////////////////////////////////////////////////////////////
unsigned char char2hex(unsigned char c){
	unsigned char v;
	if(c>='0' && c<='9')
		v=c-'0';
	else
	if(c>='a' && c<='f')
		v=c-'a'+10;
	else
	if(c>='A' && c<='F')
		v=c-'A'+10;
	else
		v=0xff; //not a hex
		//	HAL_Delay(10);			CDC_Transmit_FS((uint8_t*)"\r\nret hex\r\n ",12);
	return v;
}
unsigned char* read_hex_array_send(unsigned char* ptr, unsigned int numBits,  int flag, FIL *f){
	//flag == 0 if operation is SDR, ==1 if SIR
	if (flag) 
		setTAPtoIRSHIFT();
	else 
		setTAPtoDRSHIFT();
	int c, len;
	unsigned char bufArr[BUF_SIZE], chr;
		char* pstr;
	int charCount = 0;
	while(numBits>0)	{		
				if (charCount == BUF_SIZE) {
			//we had read 128 chars
			//now send them to JTAG
			sendArray(bufArr, charCount);
			charCount = 0;
	//	HAL_Delay(10);			CDC_Transmit_FS((uint8_t*)"\r\n1024 ch\r\n",10);
		}
		//get char from string and convert to hex digit
		while (*ptr=='(' || *ptr == ' ') ptr++;
		chr = *ptr;			
		c=char2hex(chr);
		if(c!=0xFF)
		{
		//	HAL_Delay(10);			CDC_Transmit_FS((uint8_t*)"\r\nchar cor\r\n",12);
			//hexadecimal digit is correct, save it			
			bufArr[charCount] = c	;
			//remember that 4 bits we got
			numBits -= 4;			
			ptr++;
			charCount++;
			
		}
		else
		{			
			//char from string is not hexadecimal, but what is this?
			if(chr==')')
			{				
				//end of hexadecimal array!
				ptr++;
				//return SUCCESS
//				*presult = 1;
	//			*pcount=chr_count;
				return ptr;
			}
			else
			if(chr==0 || chr==0xd || chr==0xa)
			{
					
				//end of string, we need to continue by reading next string from file
				pstr = f_gets(wordBuf,MAX_STRING_LENGTH-1,f);
				if(pstr==NULL)
				{
					//file read fails
					return NULL;
				}
				len = strlen(pstr);
				if(pstr[len-1]==0xA || pstr[len-1]==0xD)
					pstr[len-1] = 0;
				ptr = (unsigned char*)wordBuf;
				ptr = ptr+1;
			}
			else
			{
				//unexpected char, error in syntax?
				//printf("unexpected char %02X\n",chr);
					//HAL_Delay(10);			CDC_Transmit_FS((uint8_t*)"\r\nunexpected char\r\n",25);
				return NULL;
			}
		}
		
	}
	//get char from string and convert to hex digit
	chr = *ptr++;
	if(chr==')')//yes, we see final bracket
	{
		//send that we have in buffer
		sendArray(bufArr, charCount);	
		
		HAL_Delay(10);			CDC_Transmit_FS((uint8_t*)"\r\nend read hex arr1\r\n",21);
	//	*presult = 1;
	//	*pcount=chr_count;
		return ptr;
	}
	return NULL;
}


unsigned char* read_hex_array_save(unsigned char* ptr, unsigned char* pdest, unsigned int num_bits, unsigned int* pcount, unsigned int* presult, FIL *f){
	unsigned char c,chr;
	unsigned int len,chr_count;
	char* pstr;
//HAL_Delay(10);			CDC_Transmit_FS((uint8_t*)"\r\nread_hex_array bgn  \r\n",26);
	//HAL_Delay(10);			CDC_Transmit_FS((uint8_t*)ptr,sizeof(ptr));
	//assume we fail
	*presult = 0;
		//pdest = (unsigned char*)malloc(num_bits*4+8);
	if (pdest == NULL) {	HAL_Delay(10);			CDC_Transmit_FS((uint8_t*)"\r\npdest= 0 in read hex arr\r\n",32); }
	chr_count = 0;
	while(num_bits>0)	{		
		//get char from string and convert to hex digit
		while (*ptr=='(' || *ptr == ' ') ptr++;
		chr = *ptr;		
	//	HAL_Delay(10);			CDC_Transmit_FS((uint8_t*)"\r\nchar2hex\r\n",12);
		c=char2hex(chr);
	//	HAL_Delay(10);			CDC_Transmit_FS((uint8_t*)"\r\ncheck ch\r\n",12);
		if(c!=0xFF)
		{
		//	HAL_Delay(10);			CDC_Transmit_FS((uint8_t*)"\r\nchar cor\r\n",12);
			//hexadecimal digit is correct, save it			
			*pdest = c	;
			pdest++;		
			//remember that 4 bits we got
			num_bits -= 4;			
			ptr++;
			chr_count++;
			
		}
		else
		{			
			//char from string is not hexadecimal, but what is this?
			if(chr==')')
			{				
				//end of hexadecimal array!
				ptr++;
				//return SUCCESS
				*presult = 1;
				*pcount=chr_count;
				return ptr;
			}
			else
			if(chr==0 || chr==0xd || chr==0xa)
			{
					
				//end of string, we need to continue by reading next string from file
				pstr = f_gets(wordBuf,MAX_STRING_LENGTH-1,f);
				if(pstr==NULL)
				{
					//file read fails
					return NULL;
				}
				len = strlen(pstr);
				if(pstr[len-1]==0xA || pstr[len-1]==0xD)
					pstr[len-1] = 0;
				ptr = (unsigned char*)wordBuf;
				ptr = ptr+1;
			}
			else
			{
				//unexpected char, error in syntax?
				//printf("unexpected char %02X\n",chr);
					//HAL_Delay(10);			CDC_Transmit_FS((uint8_t*)"\r\nunexpected char\r\n",25);
				return NULL;
			}
		}
		
	}
	//get char from string and convert to hex digit
	chr = *ptr++;
	if(chr==')')
	{
		//yes, we see final bracket
		HAL_Delay(10);			CDC_Transmit_FS((uint8_t*)"\r\nend read hex arr2\r\n",21);
		*presult = 1;
		*pcount=chr_count;
		return ptr;
	}
	return NULL;
}




//////////////////////////////////////////////////////////////////////////////
// memory alloc/free functions
//////////////////////////////////////////////////////////////////////////////

//global pointers to sdr tdi, tdo, mask and smask arrays
//we expect that dr data can be very long, but we do not know exact size
//size of arrays allocated will be defined during SVF strings parcing

//sdr
// reducing the number of dynamic arrays

//unsigned char* psdr_tdi_data   = NULL;
unsigned char* psdr_tdo_data   = NULL;
unsigned char* psdr_mask_data  = NULL;
unsigned char* psdr_smask_data = NULL;

//unsigned char* psir_tdi_data   = NULL;
unsigned char* psir_tdo_data   = NULL;
unsigned char* psir_mask_data  = NULL;
unsigned char* psir_smask_data = NULL;

/* so, as i understand, we're making one-device chain so we don't need header and trailer
unsigned char* dr_header_data = NULL;
unsigned char* di_header_data = NULL;

unsigned char* dr_trailer_data = NULL;
unsigned char* di_trailer_data = NULL;

unsigned int sir_header_sz = 0;
unsigned int sir_trailer_sz = 0;
unsigned int sdr_header_sz = 0;
unsigned int sdr_trailer_sz = 0;

*/

unsigned int sdr_data_size = 0; //current size of arrays
unsigned int sdr_tdi_sz = 0;
unsigned int sdr_tdo_sz = 0;
unsigned int sdr_mask_sz = 0;
unsigned int sdr_smask_sz = 0;

unsigned int sir_data_size = 0;
unsigned int sir_tdi_sz = 0;
unsigned int sir_tdo_sz = 0;
unsigned int sir_mask_sz = 0;
unsigned int sir_smask_sz = 0;

void alloc_buffer(unsigned int size, unsigned char * data_buffer){
	//compare new size with size of already allocated buffers
	if(sizeof(data_buffer) >= size)
		return ; //ok, because already allocated enough

	//we need to allocate memory for arrays
	//but first free previously allocated buffers
	if(data_buffer){
		free(data_buffer); 
	  data_buffer=NULL; 
	}
		data_buffer = (unsigned char*)malloc(size);	
}

//free buffers allocated for sdr tdi, tdo, mask
void free_data_buffer(unsigned char * data_buffer){
	if(data_buffer)
		free(data_buffer);	
}



int readTDO(unsigned int numBits, int has_mask, unsigned char *tdo_ptr, unsigned char *tdo_mask){
	unsigned char tdo_char, c_mask, c;
	tdo_ptr = psdr_tdo_data;
	int rez = 1; //return 1 if all is ok, 0 if smthng wrong
	GPIO_PinState readPin;
	for (int i = numBits; i > 0 ; i=i-8){
		tdo_char = *tdo_ptr; tdo_ptr++;//next char
		if(has_mask){
			c_mask = *tdo_mask ;
			tdo_mask++;
		}		
		else
			c_mask = 255;//1111 1111 
		for (int j = 8; j > 0 ; j--){
		readPin = HAL_GPIO_ReadPin(GPIOC ,TDO_Pin);
		if (readPin == GPIO_PIN_SET) //1
			c = 128;//1000 0000
		else
			c = 0;//00000000
		
		if ((c ^ tdo_char)& c_mask)  //1st if bits are different   amd mask == 1
			
			rez = 0;
		
		tdo_char = tdo_char<<1;
		c_mask = c_mask<<1;
		pulseTCK();
		}
	
		
	}
	return rez;
}

/////////////////////////////////////////////////////////////////
//enddr, endir, freq
/////////////////////////////
//ENDDR Specifies default end state for DR scan operations.*/
void ENDDRproc(){
	char temp[16];
	//Valid stable states are IRPAUSE, DRPAUSE, RESET, and IDLE.
	int n = sscanf(wordBuf,"ENDDR %s",temp);
	if (n==1){
		if (strncmp(temp, "RESET", 5) == 0)
			ENDDRstate = TAP_STATE_TLR;
		else 
			if (strncmp(temp, "IDLE", 4) == 0)
				ENDDRstate = TAP_STATE_RTI;
			else 
				if (strncmp(temp, "DRPAUSE", 7) == 0)
					ENDDRstate = TAP_STATE_PAUSE_DR;
				else 
					if (strncmp(temp, "IRPAUSE", 7) == 0)
					ENDDRstate = TAP_STATE_PAUSE_IR;
					else return;
	
		}
};
//ENDIR Specifies default end state for IR scan operations.
void ENDIRproc(){
	char temp[16];
	//Valid stable states are IRPAUSE, DRPAUSE, RESET, and IDLE.
	int n = sscanf(wordBuf,"ENDIR %s",temp);
	if (n==1){
		if (strncmp(temp, "RESET", 5) == 0)
			ENDIRstate = TAP_STATE_TLR;
		else 
			if (strncmp(temp, "IDLE", 4) == 0)
				ENDIRstate = TAP_STATE_RTI;
			else 
				if (strncmp(temp, "DRPAUSE", 7) == 0)
					ENDIRstate = TAP_STATE_PAUSE_DR;
				else 
					if (strncmp(temp, "IRPAUSE", 7) == 0)
					ENDIRstate = TAP_STATE_PAUSE_IR;
					else return;
	
		}
};
//FREQUENCY Specifies maximum test clock frequency for IEEE 1149.1 bus operations.
void FREQUENCYproc(){
	float freq;
	char temp[16];
int n = sscanf(wordBuf,"FREQUENCY %s ",temp);
	if (n == 1){
		freq = atof(temp);//преобразуем из экспоненциальной записи во float
		//set frequency
		float t = 1 / freq * 1000;
		tckDelay = (int)t;
			
	}
	else if (n == 0){
		tckDelay = MIN_TCK_DELAY;
		//return to full speed
	}
};
////////////////////////////////////////////////////////////////
//runtest, state, trst
////////////////////////////////////////////////////////////////
//RUNTEST .........Forces the IEEE 1149.1 bus to a run state for a specified number of clocks or a specified time period.
void RUNTESTproc(){
//	int nextState = -1;

	//get command parameters
//	unsigned int
float	tck = 0;
	float sec = 0;
	char wordBuf[4];
	int n = sscanf(wordBuf,"RUNTEST %f %s;",&tck, wordBuf);
	if(n==2)
	{
		if (strncmp(wordBuf, "TCK", 3) == 0){		
		stateNavigate(TAP_STATE_RTI);
		for (int i = 0; i < tck; i++){
				 pulseTCK();	
		};
	}

	else
	if (strncmp(wordBuf, "SEC", 3) == 0){	{			
			stateNavigate(TAP_STATE_RTI);
			HAL_Delay(1000*tck);	
		}		
	}
}
};
void STATEproc(){
	char temp[10];
	int nextState = TAP_STATE_RTI;
	int n = sscanf(wordBuf,"STATE %s",temp);
	if (n==1){
		if (strncmp(temp, "RESET", 5) == 0)
			nextState = TAP_STATE_TLR;
		else 
			if (strncmp(temp, "IDLE", 4) == 0)
				nextState = TAP_STATE_RTI;
			else 
				if (strncmp(temp, "DRPAUSE", 7) == 0)
					nextState = TAP_STATE_PAUSE_DR;
				else 
					if (strncmp(temp, "IRPAUSE", 7) == 0)
					nextState = TAP_STATE_PAUSE_IR;
					else return;
	
		}
	
stateNavigate(nextState);	
	
	}	;

void TRSTproc(){
char temp[6];
	
	int n = sscanf(wordBuf,"TRST %s",temp);
	if (n == 1){
		if (strncmp(temp, "ON",2) == 0) {
		//ia iaoea, eae ii?aaaeeou ii?iaeuii, iiyoiio ?a?ac aeiaaeuio? ia?aiaiio? 
			if (GPIO_InitStruct.Mode == GPIO_MODE_INPUT){
			//set output mod
				 GPIO_InitStruct.Pin = TRST_Pin;
				 GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
         GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
         HAL_GPIO_Init(TRST_GPIO_Port, &GPIO_InitStruct);
			}
			HAL_GPIO_WritePin(GPIOC, TRST_Pin, GPIO_PIN_SET);
				 pulseTCK();
		}
			else if (strncmp(temp, "OFF",3) == 0) {
				if (GPIO_InitStruct.Mode == GPIO_MODE_INPUT){
			//set output mod
				 GPIO_InitStruct.Pin = TRST_Pin;
				 GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
         GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
         HAL_GPIO_Init(TRST_GPIO_Port, &GPIO_InitStruct);
			}
			HAL_GPIO_WritePin(GPIOC, TRST_Pin, GPIO_PIN_RESET);
				 pulseTCK();
			}
				else if (strncmp(temp, "Z",2) == 0) {
					//ia?aaiaei a ?a?ei auaiaa, ?oiau iieo?eou ninoiyiea aunieiai eiiaaaina
				 
					GPIO_InitStruct.Pin = TRST_Pin;
					GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
					GPIO_InitStruct.Pull = GPIO_NOPULL;
					HAL_GPIO_Init(TRST_GPIO_Port, &GPIO_InitStruct);
				 pulseTCK();
				}
					else if (strncmp(temp, "ABSENT",6) == 0) {
					//not present
					} ;
	}
};
/////////////////////////////////////////////////////////////////
//sdr, sdi
////////////////////////////////////////////////////////////////

void SDRproc(FIL *f)
{
	int sz;
	int flag = 0; //0 - tdo, 1 = tdi, 2 = mask, 3 = smask
	int has_tdi, has_tdo, has_mask, has_smask;
	unsigned int r;
	unsigned char b;
	unsigned char word[16];
	unsigned char* pdst;
	unsigned int num_bits = 0;
	unsigned int num_bytes;
	unsigned char* pdest=NULL, *temp_tdo=NULL, *temp_tdi=NULL, *temp_mask=NULL;
	unsigned int* pdest_count;
	unsigned char* ptr = (unsigned char*)wordBuf;
	//HAL_Delay(10);	CDC_Transmit_FS((uint8_t*)ptr,sizeof(wordBuf));
//save pointers to 1st elements of arrays
	temp_tdo = psir_tdo_data; 
//	temp_tdi = psir_tdi_data; 
	temp_mask = psir_mask_data;
//HAL_Delay(10);	CDC_Transmit_FS((uint8_t*)wordBuf,sizeof(wordBuf));
	
	
	//at begin of string we expect to see word "SDR"
	//skip it	
	ptr = ptr+4;
	//now we expect to get decimal number of bits for shift
	sscanf((const char*)ptr, "%d", &num_bits);
	unsigned char c;	
	do{
		c= *ptr;
		ptr++;
	}while (c != ' ');
//we skiped int
	//how many bytes? calculate space required and allocate
	num_bytes = (num_bits+7)/8;
	//each byte takes 2 chars in string, plus we reserve 8 additional bytes for safity
	sz=num_bytes*2+8;
//	alloc_buffer(sz, psdr_tdi_data);
	alloc_buffer(sz, psdr_tdo_data);
	alloc_buffer(sz, psdr_mask_data);
	alloc_buffer(sz, psdr_smask_data);	

sdr_data_size = sz; //current size of arrays
sdr_tdi_sz = sz;
sdr_tdo_sz = sz;
sdr_mask_sz = sz;
sdr_smask_sz = sz;

	//we expect some words like TDI, TDO, MASK or SMASK here
	//order of words can be different
	has_tdi=0;
	has_tdo=0;
	has_mask=0;
	has_smask=0;
	
	while(1)
	{
		//read word and skip it	
					sscanf((const char*)ptr, "%s", word);
				//ptr = ptr + strlen((const char*)word) + 1;
					
		//analyze words
		if(strncmp((char*)word,"TDI",3)==0)
		{
			ptr = ptr + 4;
			has_tdi = 1; flag = 1;
		//	pdest = psdr_tdi_data;
			pdest_count  = &sdr_tdi_sz;		
		}
		else
		if(strncmp((char*)word,"TDO",3)==0)
		{
			ptr = ptr + 4;
			has_tdo = 1; flag = 0;
			pdest = psdr_tdo_data;
			pdest_count  = &sdr_tdo_sz;
		}
		else
		if(strncmp((char*)word,"MASK",4)==0)
			
		{
			ptr = ptr + 5;
			has_mask = 1; flag = 2;
			pdest = psdr_mask_data;
			pdest_count  = &sdr_mask_sz;
		}
		else
		if(strncmp((char*)word,"SMASK",5)==0)
		{
			ptr = ptr + 6;
			has_smask = 1; flag = 3;
			pdest = psdr_smask_data;
			pdest_count  = &sdr_smask_sz;
		}
		else
		if(strcmp((char*)word,";")==0)
		{
			//end of string!
			//tdi was already send
			HAL_Delay(10);			CDC_Transmit_FS((uint8_t*)"\r\nend of string!  \r\n",21);
			if (has_tdo){
				readTDO(num_bits, has_mask, temp_tdo, temp_mask);	
	HAL_Delay(10);			CDC_Transmit_FS((uint8_t*)"\r\nhas tdo     \r\n",21);				
			};	
			stateNavigate(ENDDRstate);
				HAL_Delay(10);			CDC_Transmit_FS((uint8_t*)"\r\nfree buffers\r\n",21);
			free_data_buffer(psdr_tdo_data);
		 // free_data_buffer(psdr_tdi_data);
			free_data_buffer(psdr_mask_data);
			free_data_buffer(pdest);
				HAL_Delay(10);			CDC_Transmit_FS((uint8_t*)"\r\nend of free buffers\r\n",21);
			return;
		}
		else
		{
			//printf("syntax error for SDR command, unknown parameter word\n");
			return ;
		}
//we finished word analize, now read the parameter		
//parameter should be in parentheses				
	ptr = ptr+1;//skip '('
		//now expect to read hexadecimal array of  data
	//pdest = (unsigned char*)malloc(sz);		
		if (flag == 1)//tdi
			ptr = read_hex_array_send(ptr,num_bits,0, f);
		else 
			ptr = read_hex_array_save(ptr, pdst, num_bits,  pdest_count,&r, f); 
		
	}
//	return ;
}	
	
void SIRproc(FIL* f)
{
	//HAL_Delay(10);CDC_Transmit_FS((uint8_t*)"\r\nSIRproc begin\r\n", 17); 
	int sz, flag=0;
	int has_tdi, has_tdo, has_mask;
	unsigned int r;
	unsigned char word[16];
//	unsigned char* pdst;
	unsigned int num_bits = 0;
	unsigned int num_bytes;
	unsigned char* pdest=NULL, *temp_tdo=NULL, *temp_tdi=NULL, *temp_mask=NULL;
	unsigned int* pdest_count;
	unsigned char* ptr = (unsigned char*)wordBuf;
	

	//at begin of string we expect to see word "SDR"
	//skip it	
	ptr = ptr+4;
	//now we expect to get decimal number of bits for shift
	sscanf((const char*)ptr, "%d", &num_bits);
	unsigned char c;	
	do{
		c= *ptr;
		ptr++;
	}while (c != ' ');
//we skiped int
	//how many bytes? calculate space required and allocate
	num_bytes = (num_bits+7)/8;
	//each byte takes 2 chars in string, plus we reserve 8 additional bytes for safity
	sz=num_bytes*2+8;
//	alloc_buffer(sz, psir_tdi_data);
	alloc_buffer(sz, psir_tdo_data);
	alloc_buffer(sz, psir_mask_data);
	alloc_buffer(sz, psir_smask_data);

sir_data_size = sz; //current size of arrays
sir_tdi_sz = sz;
sir_tdo_sz = sz;
sir_mask_sz = sz;
sir_smask_sz = sz;

	//we expect some words like TDI, TDO, MASK or SMASK here
	//order of words can be different
	has_tdi=0;
	has_tdo=0;
	has_mask=0;
//	has_smask=0;
	
	while(1)
	{
		//read word and skip it			
		sscanf((const char*)ptr, "%s", word);
		//analyze words
		if(strncmp((char*)word,"TDI",3)==0)
		{
			ptr = ptr + 4;
			has_tdi = 1;
			//pdest = psir_tdi_data;
			flag = 1;
			pdest_count  = &sir_tdi_sz;		
		}
		else
		if(strncmp((char*)word,"TDO",3)==0)
		{
			ptr = ptr + 4;
			has_tdo = 1;
			pdest = psir_tdo_data;
			flag = 0;
			pdest_count  = &sir_tdo_sz;
			
			//save pointers to 1st elements of arrays
	temp_tdo = psir_tdo_data; 

		}
		else
		if(strncmp((char*)word,"MASK",4)==0)			
		{
			ptr = ptr + 5;
			has_mask = 1;
			flag = 2;
			pdest = psir_mask_data;
			pdest_count  = &sir_mask_sz;
			//save pointers to 1st elements of arrays 
	temp_mask = psir_mask_data;
		}
		else
		if(strncmp((char*)word,"SMASK",5)==0)
		{
			ptr = ptr + 6;
			//has_smask = 1;
			flag = 3;
			pdest = psir_smask_data;
			pdest_count  = &sir_smask_sz;
		}
		else
		if(strcmp((char*)word,";")==0)
		{
			//end of string!
			//send bitstream to jtag
			
			if (has_tdo){
				readTDO(num_bits, has_mask, temp_tdo, temp_mask);
				//read answer
			};		
			stateNavigate(ENDIRstate);
			free_data_buffer(psir_tdo_data);
			//free_data_buffer(psir_tdi_data);
			free_data_buffer(psir_mask_data);
	free_data_buffer(pdest);
			return;
		}
		else
		{
			//printf("syntax error for SDR command, unknown parameter word\n");
			return ;
		}
//we finished word analize, now read the parameter		
//parameter should be in parentheses		
	ptr = ptr+1;//skip '('
		//now expect to read hexadecimal array of tdi data
		pdest = (unsigned char*)malloc(sz);		
	  if (flag ==1)//tdi
			ptr = read_hex_array_send(ptr, num_bits, 1, f);
		else
			ptr = read_hex_array_save(ptr,pdest,num_bits,pdest_count,&r,f);
		//------------------------------------
	
	}
//	return ;
}	
	



void SIRfunct(FIL* f){
		HAL_Delay(10);CDC_Transmit_FS((uint8_t*)"\r\nSIRfunct()    \r\n", 15); 
	
	int sz;
	int flag = 0; //0 - tdo, 1 = tdi, 2 = mask, 3 = smask
	int has_tdi, has_tdo, has_mask, has_smask;
	int temp;
	unsigned int r;	
	unsigned char word[16];
	unsigned char* pdst;
	unsigned int num_bits = 0;
	unsigned int num_bytes;
	unsigned char* pdest=NULL, *temp_tdo=NULL, *temp_tdi=NULL, *temp_mask=NULL;
	unsigned int* pdest_count;
	unsigned char* ptr = (unsigned char*)wordBuf;	
	HAL_Delay(10);CDC_Transmit_FS((uint8_t*)ptr, 100); 
		//at begin of string we expect to see word "SDR"
	//skip it	
	ptr = ptr+4;
	//now we expect to get decimal number of bits for shift
	sscanf((const char*)ptr, "%d", &num_bits);
	unsigned char c;	
	do{
		c= *ptr;
		ptr++;
	}while (c != ' ');
//we skiped int
	//how many bytes? calculate space required and allocate
	num_bytes = (num_bits+7)/8;
	//each byte takes 2 chars in string, plus we reserve 8 additional bytes for safity
	sz=num_bytes*2+8;
	
alloc_buffer(sz, pdest);
	alloc_buffer(sz, psir_tdo_data);
	alloc_buffer(sz, psir_mask_data);
	alloc_buffer(sz, psir_smask_data);
 
sir_data_size = sz; //current size of arrays
sir_tdi_sz = sz;
sir_tdo_sz = sz;
sir_mask_sz = sz;
sir_smask_sz = sz;

	//we expect some words like TDI, TDO, MASK or SMASK here
	//order of words can be different
	has_tdi=0;
	has_tdo=0;
	has_mask=0;
	
	while(1){
				HAL_Delay(10);CDC_Transmit_FS((uint8_t*)"\r\nwhile(1){    \r\n", 15); 
		//read word and skip it			
		sscanf((const char*)ptr, "%s", word);
		//analyze words
		if(strncmp((char*)word,"TDI",3)==0) {
			
				HAL_Delay(10);CDC_Transmit_FS((uint8_t*)"\r\ntdi word    \r\n", 15); 
			
			flag = 1;
			has_tdi = 1;
			ptr = ptr+4;//skip word
			ptr = read_hex_array_send(ptr,num_bits,1, f);
			
		}	
			else if (strncmp((char*)word,"TDO",3)==0) {
				ptr = ptr+4;//skip word
					HAL_Delay(10);CDC_Transmit_FS((uint8_t*)"\r\ntdo word    \r\n", 15); 
				flag = 0;
				has_tdo = 1;
				pdest = psdr_tdo_data;
				temp_tdo = psdr_tdo_data;
				ptr = read_hex_array_save(ptr,pdest,num_bits,pdest_count,&r, f);
			}	
				else if (strncmp((char*)word,"MASK",4)==0) {
					ptr = ptr+5;//skip word
						HAL_Delay(10);CDC_Transmit_FS((uint8_t*)"\r\nmask word    \r\n", 15); 
					flag = 2;
				has_mask = 1;
				pdest = psdr_mask_data;
				temp_tdo = psdr_mask_data;
				ptr = read_hex_array_save(ptr,pdest,num_bits,pdest_count,&r, f);
				}	
					else if (strncmp((char*)word,"SMASK",5)==0) {
						ptr = ptr+6;//skip word
							HAL_Delay(10);CDC_Transmit_FS((uint8_t*)"\r\nsmask word    \r\n", 15); 
						flag = 3;				
						pdest = ptr;
						ptr = read_hex_array_save(ptr,pdest,num_bits,pdest_count,&r, f);
					}	
						else	if(strcmp((char*)word,";")==0) {
							HAL_Delay(10);CDC_Transmit_FS((uint8_t*)"\r\nend of line  \r\n", 15); 
							if (has_tdi){
										HAL_Delay(10);CDC_Transmit_FS((uint8_t*)"\r\nhas tdi  \r\n", 15); 
								readTDO(num_bits, has_mask, temp_tdi, temp_mask);
							}
							stateNavigate(ENDIRstate);
									HAL_Delay(10);CDC_Transmit_FS((uint8_t*)"\r\nfree buf  \r\n", 15); 
							free_data_buffer(psir_tdo_data);
							free_data_buffer(psir_mask_data);
							free_data_buffer(pdest);
									HAL_Delay(10);CDC_Transmit_FS((uint8_t*)"\r\nend of func  \r\n", 15); 
							return;
						//end of string
						}
	}
	
	
	return;
}


///////////////////////////////////////////////////////
////tir, tdt, hdr, hir, pio, piomap -- empty
////////////////////////////////////////////////////////
	void TIRproc(){
		;}
	void TDRproc(){
			;}
		
	void HDRproc(){
	;}
	void HIRproc(){
		;}
	void PIOproc(){
		;}
	void PIOMAPproc(){
		;}

///////////////////////////////////////////////////////////
//svf file processing
//////////////////////////////////////////////////////////


void processLine(FIL * f){	
//	strcpy(wordBuf,str);	
	//проверяем, не комментарий ли эта строка
	if (wordBuf[0] == '/' || wordBuf[0] == '!'){
		HAL_Delay(10);CDC_Transmit_FS((uint8_t*)"\r\n comment \r\n", 10); 
		return; 
	}
	//ничего не делаем со строкой, если она - комментарий
	//если не комментарий
	//определяем, какая команда записана в строке
	if (strncmp(wordBuf, "ENDDR", 5)==0){
			HAL_Delay(10);CDC_Transmit_FS((uint8_t*)"\r\n ENDDR \r\n", 10); 
		//вызываем функцию обработки
		ENDDRproc();
		//выходим
		return;
	}
	if (strncmp(wordBuf, "ENDIR", 5)==0){
			HAL_Delay(10);CDC_Transmit_FS((uint8_t*)"\r\n ENDIR \r\n", 10); 
		ENDIRproc();
		return;
	}
	if (strncmp(wordBuf, "FREQUENCY",9)==0){
		HAL_Delay(10);CDC_Transmit_FS((uint8_t*)"\r\n FREQ \r\n", 10); 
		
		FREQUENCYproc();
	
		return;
	}
	if (strncmp(wordBuf, "HDR", 3)==0){
	HAL_Delay(10);CDC_Transmit_FS((uint8_t*)"\r\n HDR \r\n", 10); 
		HDRproc();
		return;
	}
	if (strncmp(wordBuf, "HIR",3)==0){
		HAL_Delay(10);CDC_Transmit_FS((uint8_t*)"\r\n HIR \r\n", 10); 
		HIRproc();
		return;
	}
	
	if (strncmp(wordBuf, "PIO ", 4)==0){
		PIOproc();
		return;
	}
	if (strncmp(wordBuf, "PIOMAP", 6)==0){
		PIOMAPproc();
		return;
	}
	if (strncmp(wordBuf, "RUNTEST", 7)==0){
		HAL_Delay(10); CDC_Transmit_FS((uint8_t*)"\r\n RUNTEST \r\n", 10); 
		RUNTESTproc();
		return;
	}
	if (strncmp(wordBuf, "SDR", 3)==0){
		HAL_Delay(10); CDC_Transmit_FS((uint8_t*)"\r\n SDR \r\n", 10); 
		SDRproc(f);
		return;
	}
	if (strncmp(wordBuf, "SIR", 3)==0){
	HAL_Delay(10);CDC_Transmit_FS((uint8_t*)"\r\nSIR     \r\n", 15); 
		//SIRproc();
		SIRfunct(f);
		return;
	}
	if (strncmp(wordBuf, "STATE", 5)==0){
			HAL_Delay(10);
		CDC_Transmit_FS((uint8_t*)"\r\n STATE \r\n", 10); 
		
		STATEproc();
		return;
	}
	if (strncmp(wordBuf, "TDR", 3)==0){
		TDRproc();
		return;
	}
	if (strncmp(wordBuf, "TIR", 3)==0){
		TIRproc();
		return;
	}
	if (strncmp(wordBuf, "TRST", 4)==0){
		HAL_Delay(10);CDC_Transmit_FS((uint8_t*)"\r\n TRST \r\n", 10); 
		TRSTproc();
		return;
	}
	return; 
}

void readLine(TCHAR* path){
	
 char str[1];
	CDC_Transmit_FS((uint8_t*)"begin\r\n",15);
	  
   //Указатель, в который будет помещен адрес массива, в который считана 
   // строка, или NULL если достигнут коней файла или произошла ошибка
   char *estr;
	
		if(f_mount(&SDFatFs1,(TCHAR const*)USER_Path,0)!=FR_OK){
		// CDC_Transmit_FS((uint8_t*)"\r\nnot mounted\r\n",15);		 
		Error_Handler();
	}
	else{		
		if(f_open(&MyFile,path,FA_READ)!=FR_OK)
		{
			
			// CDC_Transmit_FS((uint8_t*)"\r\nnot opened\r\n",15);
			Error_Handler();
		}
		else //если файл открыт
		{			
		//Чтение (построчно) данных из файла в бесконечном цикле
   while (1)
   {
      // Чтение одной строки  из файла
		 memset (wordBuf, '0', sizeof(wordBuf));
		 //free_data_buffer(wordBuf);
		 //alloc_buffer(MAX_STRING_LENGTH, wordBuf);
      estr = f_gets (wordBuf,sizeof(wordBuf),&MyFile);	
	HAL_Delay(10); CDC_Transmit_FS((uint8_t*)"\r\nf_gets (w\r\n",12);		 
      //Проверка на конец файла или ошибку чтения
      if (estr == NULL)
      {
				//CDC_Transmit_FS((uint8_t*)"\r\nerr check\r\n",13);
         // Проверяем, что именно произошло: кончился файл
         // или это ошибка чтения
         if ( f_eof (&MyFile) != 0)
         {  
            //Если файл закончился,
            //выходим из бесконечного цикла
           		HAL_Delay(10); CDC_Transmit_FS((uint8_t*)"\r\nend of file\r\n",12);
            break;
         }
         else
         {
            //Если при чтении произошла ошибка, выводим сообщение 
            //об ошибке и выходим из бесконечного цикла
           //  CDC_Transmit_FS((uint8_t*)"\r\nerror\r\n",12);
            break;
         }
      }
      //Если файл не закончился, и не было ошибки чтения 
      //передаем в функцию обработки строки	
			HAL_Delay(10);
			CDC_Transmit_FS((uint8_t*)"\r\nprocess line:\r\n",17);
				HAL_Delay(10);
			CDC_Transmit_FS((uint8_t*)wordBuf,sizeof(wordBuf));	

			processLine(&MyFile);
   }	
			
			f_close(&MyFile);
	
		}
	}
}		


/*
void readLine(TCHAR* path){
 //char str[1];
	//CDC_Transmit_FS((uint8_t*)"begin\r\n",15);
	
   char *estr;
	
		if(f_mount(&SDFatFs1,(TCHAR const*)USER_Path,0)!=FR_OK){
		// CDC_Transmit_FS((uint8_t*)"\r\nnot mounted\r\n",15);		 
		Error_Handler();
	}
	else{		
		if(f_open(&MyFile,path,FA_READ)!=FR_OK)
		{
			
			// CDC_Transmit_FS((uint8_t*)"\r\nnot opened\r\n",15);
			Error_Handler();
		}
		else 
		{			
		
   while (1)
   { 		
			HAL_Delay(10); CDC_Transmit_FS((uint8_t*)"\r\nwhile (1) \r\n",12);
		  memset (wordBuf, '0', sizeof(wordBuf));
	
		//  estr = f_gets (wordBuf,sizeof(wordBuf),&MyFile);
		HAL_Delay(10); CDC_Transmit_FS((uint8_t*)"\r\nf_gets (w\r\n",12);
      if (f_gets (wordBuf,sizeof(wordBuf),&MyFile) == NULL)
      {				
         if ( f_eof (&MyFile) != 0)
         {  
           		HAL_Delay(10); CDC_Transmit_FS((uint8_t*)"\r\nend of file\r\n",12);
            break;
         }
         else
         {
           CDC_Transmit_FS((uint8_t*)"\r\nerror\r\n",12);
            break;
         }
      }      
			HAL_Delay(10);
			CDC_Transmit_FS((uint8_t*)"\r\nprocess line       \r\n",22);					
			processLine();
   }	
			
			f_close(&MyFile);
	
		}
	}
}

*/








