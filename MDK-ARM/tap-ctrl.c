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

#define MIN_TCK_DELAY 100
char wordBuf[MAX_STRING_LENGTH];

//#define MAX_FREQUENCY 
int currentState = TAP_STATE_RTI, 
	ENDIRstate= TAP_STATE_RTI,
	ENDDRstate= TAP_STATE_RTI;
int tckDelay = 100;

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

};
void sendArray(unsigned char* ptr, unsigned int numBits) {
	unsigned char c; 
	for (unsigned int i = numBits; i > 0; i = i - 8) {
		c = *ptr;
		outputCharToTDI(c);
		ptr++;
	}
};

void setTMSpath(int path[10], int steps){
	for (int i = 0; i <= steps; i++){
			if (path[i]==0)
				HAL_GPIO_WritePin(GPIOB, TMS_Pin, GPIO_PIN_RESET);
			else 
				HAL_GPIO_WritePin(GPIOB, TMS_Pin, GPIO_PIN_SET);
			pulseTCK();
			
			}

}
void stateNavigate(int nextState){
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
setTMSpath(path, steps);
currentState = nextState; 			
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
	return v;
}
unsigned char* read_hex_array(unsigned char* ptr, unsigned char* pdst, unsigned int num_bits, FIL* f, unsigned int* pcount, unsigned int* presult){
	unsigned char c,chr;
	unsigned int len,chr_count;
	char* pstr;

	//assume we fail
	*presult = 0;
	
	chr_count = 0;
	while(num_bits>0)
	{
		//get char from string and convert to hex digit
		chr = *ptr;
		c=char2hex(chr);
		if(c!=0xFF)
		{
			//hexadecimal digit is correct, save it
			*pdst++=c;
			//remember that 4 bits we got
			num_bits -= 4;
			//go to next char
			ptr++;
			chr_count++;
		}
		else
		{
			//char from string is not hexadecimal, but what is this?
			if(chr==')')
			{
				//end of hexadecimal array!!!
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
				return NULL;
			}
		}
	}
	//get char from string and convert to hex digit
	chr = *ptr++;
	if(chr==')')
	{
		//yes, we see final bracket
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
unsigned char* psdr_tdi_data   = NULL;
unsigned char* psdr_tdo_data   = NULL;
unsigned char* psdr_mask_data  = NULL;
unsigned char* psdr_smask_data = NULL;

unsigned char* psir_tdi_data   = NULL;
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
	unsigned int tck = 0;
	float sec = 0;

	int n = sscanf(wordBuf,"RUNTEST %d TCK;",&tck);
	if(n==1)
	{
		stateNavigate(TAP_STATE_RTI);
		for (int i = 0; i < tck; i++){
				 pulseTCK();	
		};
	}
	else
	{
		n = sscanf(wordBuf,"RUNTEST %f SEC",&sec);
		if(n == 1){			
			stateNavigate(TAP_STATE_RTI);
			HAL_Delay(1000*sec);	
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
void SDRproc(FIL* f)
{
	
		int sz;
	int has_tdi, has_tdo, has_mask, has_smask;
	unsigned int r;
	//unsigned char b;
	unsigned char word[16];
	//unsigned char* pdst;
	unsigned int num_bits = 0;
	unsigned int num_bytes;
	unsigned char* pdest, *temp_tdo, *temp_tdi, *temp_mask;
	unsigned int* pdest_count;
	unsigned char* ptr=(unsigned char*)wordBuf;
//save pointers to 1st elements of arrays
	temp_tdo = psdr_tdo_data; 
	temp_tdi = psdr_tdi_data; 
	temp_mask = psdr_mask_data;
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
	alloc_buffer(sz, psdr_tdi_data);
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
		ptr = ptr + strlen((const char*)word) + 1;
		//analyze words
		if(strncmp((char*)word,"TDI",3)==0)
		{
			has_tdi = 1;
			pdest = psdr_tdi_data;
			pdest_count  = &sdr_tdi_sz;
		}
		else
		if(strncmp((char*)word,"TDO",3)==0)
		{
			has_tdo = 1;
			pdest = psdr_tdo_data;
			pdest_count  = &sdr_tdo_sz;
		}
		else
		if(strncmp((char*)word,"MASK",4)==0)
		{
			has_mask = 1;
			pdest = psdr_mask_data;
			pdest_count  = &sdr_mask_sz;
		}
		else
		if(strncmp((char*)word,"SMASK",5)==0)
		{
			has_smask = 1;
			pdest = psdr_smask_data;
			pdest_count  = &sdr_smask_sz;
		}
		else
		if(strcmp((char*)word,";")==0)
		{
			//end of string!
			//send bitstream to jtag
			if (has_tdi){
				setTAPtoDRSHIFT();
				sendArray(temp_tdi, num_bits);				
			}
			if (has_tdo){
				readTDO(num_bits, has_mask,temp_tdo, temp_mask);
				//read answer
			};
		//	sdr_nbits(num_bits,has_tdo,has_mask,has_smask);
			stateNavigate(ENDDRstate);
			free_data_buffer(psdr_tdo_data);
			free_data_buffer(psdr_tdi_data);
			free_data_buffer(psdr_mask_data);
			free_data_buffer(psdr_smask_data);
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
		ptr = read_hex_array(ptr,pdest,num_bits,f,pdest_count,&r);
	}
	return ;
}	
	

void SIRproc(FIL* f)
{
	int sz;
	int has_tdi, has_tdo, has_mask, has_smask;
	unsigned int r;
	unsigned char b;
	unsigned char word[16];
	unsigned char* pdst;
	unsigned int num_bits = 0;
	unsigned int num_bytes;
	unsigned char* pdest, *temp_tdo, *temp_tdi, *temp_mask;
	unsigned int* pdest_count;
	unsigned char* ptr=(unsigned char*)wordBuf;
//save pointers to 1st elements of arrays
	temp_tdo = psir_tdo_data; 
	temp_tdi = psir_tdi_data; 
	temp_mask = psir_mask_data;
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
	alloc_buffer(sz, psir_tdi_data);
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
	has_smask=0;
	
	while(1)
	{
		//read word and skip it	
		sscanf((const char*)ptr, "%s", word);
		ptr = ptr + strlen((const char*)word) + 1;
		//analyze words
		if(strncmp((char*)word,"TDI",3)==0)
		{
			has_tdi = 1;
			pdest = psdr_tdi_data;
			pdest_count  = &sdr_tdi_sz;
		}
		else
		if(strncmp((char*)word,"TDO",3)==0)
		{
			has_tdo = 1;
			pdest = psdr_tdo_data;
			pdest_count  = &sdr_tdo_sz;
		}
		else
		if(strncmp((char*)word,"MASK",4)==0)
		{
			has_mask = 1;
			pdest = psdr_mask_data;
			pdest_count  = &sdr_mask_sz;
		}
		else
		if(strncmp((char*)word,"SMASK",5)==0)
		{
			has_smask = 1;
			pdest = psdr_smask_data;
			pdest_count  = &sdr_smask_sz;
		}
		else
		if(strcmp((char*)word,";")==0)
		{
			//end of string!
			//send bitstream to jtag
			if (has_tdi){
				setTAPtoIRSHIFT();
				sendArray(temp_tdi, num_bits);				
			}
			if (has_tdo){
				readTDO(num_bits, has_mask, temp_tdo, temp_mask);
				//read answer
			};
		//	sdr_nbits(num_bits,has_tdo,has_mask,has_smask);
			stateNavigate(ENDIRstate);
				free_data_buffer(psir_tdo_data);
			free_data_buffer(psir_tdi_data);
			free_data_buffer(psir_mask_data);
			free_data_buffer(psir_smask_data);
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
		ptr = read_hex_array(ptr,pdest,num_bits,f,pdest_count,&r);
	}
	return ;
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


void processLine(char* str){	
	HAL_Delay(10);
	CDC_Transmit_FS((uint8_t*)"\r\n begin\r\n", 10); 
//	strcpy(wordBuf,str);
	char temp;
	
	//проверяем, не комментарий ли эта строка
	if (wordBuf[0] == '/' || wordBuf[0] == '!')
		return; //ничего не делаем со строкой, если она - комментарий
	//если не комментарий
	//определяем, какая команда записана в строке
	if (strncmp(wordBuf, "ENDDR", 5)){
			HAL_Delay(10);CDC_Transmit_FS((uint8_t*)"\r\n ENDDR \r\n", 10); 
		//вызываем функцию обработки
		ENDDRproc();
		//выходим
		return;
	}
	if (strncmp(wordBuf, "ENDIR", 5)){
			HAL_Delay(10);CDC_Transmit_FS((uint8_t*)"\r\n ENDIR \r\n", 10); 
		ENDIRproc();
		return;
	}
	if (strncmp(wordBuf, "FREQUENCY",9)){
			HAL_Delay(10);CDC_Transmit_FS((uint8_t*)"\r\n FREQ \r\n", 10); 
		
		FREQUENCYproc();
	
		return;
	}
	if (strncmp(wordBuf, "HDR", 3)){
	
		HDRproc();
		return;
	}
	if (strncmp(wordBuf, "HIR",3)){
			HAL_Delay(10);CDC_Transmit_FS((uint8_t*)"\r\n HIR \r\n", 10); 
		HIRproc();
		return;
	}
	
	if (strncmp(wordBuf, "PIO ", 4)){
		PIOproc();
		return;
	}
	if (strncmp(wordBuf, "PIOMAP", 6)){
		PIOMAPproc();
		return;
	}
	if (strncmp(wordBuf, "RUNTEST", 7)){
			HAL_Delay(10); CDC_Transmit_FS((uint8_t*)"\r\n RUNTEST \r\n", 10); 
		RUNTESTproc();
		return;
	}
	if (strncmp(wordBuf, "SDR", 3)){
			HAL_Delay(10); CDC_Transmit_FS((uint8_t*)"\r\n SDR \r\n", 10); 
		SDRproc(&MyFile);
		return;
	}
	if (strncmp(wordBuf, "SIR", 3)){
			HAL_Delay(10);CDC_Transmit_FS((uint8_t*)"\r\n SIR \r\n", 10); 
		SIRproc(&MyFile);
		return;
	}
	if (strncmp(wordBuf, "STATE", 5)){
			HAL_Delay(10);
		CDC_Transmit_FS((uint8_t*)"\r\n STATE \r\n", 10); 
		
		STATEproc();
		return;
	}
	if (strncmp(wordBuf, "TDR", 3)){
		TDRproc();
		return;
	}
	if (strncmp(wordBuf, "TIR", 3)){
		TIRproc();
		return;
	}
	if (strncmp(wordBuf, "TRST", 4)){
		
		TRSTproc();
		return;
	}
}

void readLine(TCHAR* path){
 char str[1];
	//CDC_Transmit_FS((uint8_t*)"begin\r\n",15);
	// Переменная, в которую поочередно будут помещаться считываемые строки
  
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
      estr = f_gets (wordBuf,sizeof(wordBuf),&MyFile);			
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
			CDC_Transmit_FS((uint8_t*)"\r\nprocess line pls\r\n",22);
				HAL_Delay(10);
					
	processLine(str);
   }	
			
			f_close(&MyFile);
	
		}
	}
}		




















