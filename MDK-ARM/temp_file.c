@@ -1,24 +1,17 @@
#include "stm32f1xx_hal.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
//#include <stdlib.h>

#include "fatfs.h"
#include "usbd_cdc_if.h"
#include "sd.h"

extern char USER_Path[4]; /* logical drive path */
FATFS SDFatFs1;
FATFS *fs;
FIL MyFile;

//?? "?????????"
/*
unsigned char byOutputBuffer[1024]; // Buffer to hold MPSSE commands and data to be sent to the FT2232H
unsigned char byInputBuffer[1024]; // Buffer to hold data read from the device
unsigned short dwNumBytesToSend = 0; // Index to the output buffer
unsigned short dwNumBytesSent = 0; // Count of actual bytes sent - used with FT_Write
unsigned short dwNumBytesToRead = 0; // Number of bytes available to read in the driver's input buffer
unsigned short dwNumBytesRead = 0; // Count of actual bytes read - used with FT_Read

//controller commands
#define CMD_DATA2TMS_NR 0x4b
#define CMD_DATABITS2TDI_NR 0x1b
#define CMD_DATABYTES2TDI_NR 0x19
*/
//possible JTAG TAP states
#define TAP_STATE_TLR 0
#define TAP_STATE_RTI 1
@ -37,46 +30,52 @@ unsigned short dwNumBytesRead = 0; // Count of actual bytes read - used with FT_
#define TAP_STATE_UPDATE_DR 14
#define TAP_STATE_UPDATE_IR 15

#define MAX_STRING_LENGTH (1024*4)
#define MAX_STRING_LENGTH (255*4)

#define MIN_TCK_DELAY 100
char wordBuf[MAX_STRING_LENGTH];

//#define MAX_FREQUENCY 


int currentState = TAP_STATE_RTI;
int currentState = TAP_STATE_RTI, 
	ENDIRstate= TAP_STATE_RTI,
	ENDDRstate= TAP_STATE_RTI;
int tckDelay = 100;

int MASK_header=0, SMASK_header=0, TDI_HDR=0, TDI_HIR=0, TDO_header=0;//header
int MASK_scan=0, SMASK_scan=0, TDI_SDR=0, TDI_SIR=0, TDO_scan=0; //SIR, SDR
int MASK_TDR=0, MASK_TIR=0, SMASK_TDR=0, SMASK_TIR =0, TDI_TDR=0, TDI_TIR=0, TDO_trailer=0; //Trailer IR, DR
int ENDDRstate = TAP_STATE_RTI;
int ENDIRstate = TAP_STATE_RTI;
/*Some optional command parameters such as, MASK, SMASK, 
and TDI are ?sticky? (they are remembered from the previous 
command until changed or invalidated) 
*/

 GPIO_InitTypeDef GPIO_InitStruct;//??? ????????? TRST ? Z
//---------------------------------------------------------
//TAP functions
GPIO_InitTypeDef GPIO_InitStruct;//??? ????????? TRST ? Z
 
void pulseTCK(){
	HAL_GPIO_WritePin(GPIOC, TCK_Pin, GPIO_PIN_SET);
	HAL_Delay(tckDelay);
  HAL_GPIO_WritePin(GPIOC, TCK_Pin, GPIO_PIN_RESET);
	HAL_Delay(tckDelay);

 };
void inputTDI(int data, int dataSize){
for (int i = 0; i < 32; i++)

void outputCharToTDI(unsigned char c) {
	unsigned char mask = 128;
	for (int i = 0; i < 8; i++)
	{
		//????????? ??????? ???)
		if (data & 0x80000000)
		if (c & mask)
			HAL_GPIO_WritePin(GPIOB, TDI_Pin, GPIO_PIN_SET);
		else
			HAL_GPIO_WritePin(GPIOB, TDI_Pin, GPIO_PIN_RESET);
		//???????? ????? ?? 1 ???
		data = data << 1; 	
		c = c << 1;
		pulseTCK();
	}
 };

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
@ -318,8 +317,201 @@ int path[10], steps = 0;
setTMSpath(path, steps);
		currentState = TAP_STATE_SHIFT_IR; 		
};
//??????? ????????? ?????? ???-?????


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
@ -370,98 +562,21 @@ int n = sscanf(wordBuf,"FREQUENCY %s ",temp);
	if (n == 1){
		freq = atof(temp);//??????????? ?? ???????????????? ?????? ?? float
		//set frequency
		//??? ??? ?????? ????????, ?		
		float t = 1 / freq * 1000;
		tckDelay = (int)t;
			
	}
	else if (n == 0){
		tckDelay = MIN_TCK_DELAY;
		//return to full speed
	}
};
//HDR ..................(Header Data Register) Specifies a header pattern that is
//prepended to the beginning of subsequent DR scan operations.
void HDRproc(){
	unsigned int length;//specifying the number of bits to be scanned. Setting the length to 0 removes the header.
	unsigned long param[4];
		char t[4];
	char temp[6][16];//????????? ?? ?????? ???? ???????, ????? ? ????????, ??????
	int n = sscanf(wordBuf,"HDR %d",&length);
	if (n == 1){
		if (length == 0){
			//remove the header
			MASK_header= 0; 
			SMASK_header= 0;
			TDI_HDR = 0;
			TDO_header= 0;
		}
		else{
			n = sscanf(wordBuf, "%s%d%s%s%s%s%s%s",t,&length,
			temp[0], temp[1], temp[2], temp[3], temp[4],temp[5]);
			 
			for (int i = 0; i < n-2; i = i + 2){//???? ???? ??????????? ?????????
				temp[i + 1][0] = ' ';
				param[i/2] = strtoul(temp[i+1],NULL, 16); //??????????? ????????? ???????? ?? hex ? dec
					if (strncmp(temp[i], "TDI",3) == 0)
				TDI_HDR = param[i/2];
			else
				if (strncmp(temp[i], "TDO",3) == 0)
					TDO_header= param[i/2];
				else 
					if (strncmp(temp[i], "MASK",4) == 0)
							MASK_header= param[i/2];
					else 
						if (strncmp(temp[i], "SMASK",5) == 0)
							SMASK_header= param[i/2];
							
			};
		}
	}
	else {
		//nothing
	}
};
//HIR....................(Header Instruction Register) Specifies a header pattern that is prepended to the beginning of subsequent IR scan operations.
void HIRproc(){
	unsigned int length;//specifying the number of bits to be scanned. Setting the length to 0 removes the header.
	unsigned long param[4];
		char t[4];
	char temp[6][16];//????????? ?? ?????? ???? ???????, ????? ? ????????, ??????
	int n = sscanf(wordBuf,"HIR %d",&length);
	if (n == 1){
		if (length == 0){
			//remove the header
			MASK_header= 0; 
			SMASK_header= 0;
			TDI_HIR = 0;
			TDO_header= 0;
		}
		else{
			n = sscanf(wordBuf, "%s%d%s%s%s%s%s%s",t,&length,
			temp[0], temp[1], temp[2], temp[3], temp[4],temp[5]);
			 
			for (int i = 0; i < n-2; i = i + 2){//???? ???? ??????????? ?????????
				temp[i + 1][0] = ' ';
			param[i/2] = strtoul(temp[i+1],NULL, 16); //??????????? ????????? ???????? ?? hex ? dec
				if (strncmp(temp[i], "TDI",3) == 0)
				TDI_HIR = param[i/2];
			else
				if (strncmp(temp[i], "TDO",3) == 0)
					TDO_header= param[i/2];
				else 
					if (strncmp(temp[i], "MASK",4) == 0)
							MASK_header= param[i/2];
					else 
						if (strncmp(temp[i], "SMASK",5) == 0)
							SMASK_header= param[i/2];
							
			};
		}
	}
	else {
		//nothing
	}
};;
////////////////////////////////////////////////////////////////
//runtest, state, trst
////////////////////////////////////////////////////////////////
//RUNTEST .........Forces the IEEE 1149.1 bus to a run state for a specified number of clocks or a specified time period.
void RUNTESTproc(){
	int nextState = -1;
//	int nextState = -1;

	//get command parameters
	unsigned int tck = 0;
@ -485,88 +600,6 @@ void RUNTESTproc(){
	}
	
};
//SDR...................(Scan Data Register) Performs an IEEE 1149.1 Data Register scan.
void SDRproc(){
	unsigned int length;//specifying the number of bits to be scanned. 
	unsigned long param[4];
	char t[4];
	char temp[6][50];//????????? ?? ?????? ???? ???????, ????? ? ????????, ??????
	int n = sscanf(wordBuf,"SDR %d",&length);
	if (n == 1){
		if (length != 0){		
			n = sscanf(wordBuf, "%s%d%s%s%s%s%s%s",t,&length,
			temp[0], temp[1], temp[2], temp[3], temp[4],temp[5]);
			 
			for (int i = 0; i < n-2; i = i + 2){//???? ???? ??????????? ?????????
			temp[i+1][0] = ' ';//???????? ??????????? ?????? ?? ??????
				//????? strtol ????? ??????????
				param[i/2] = strtoul(temp[i+1],NULL, 16); //??????????? ????????? ???????? ?? hex ? dec
			//????????, ???? ??? ????????
			if (strncmp(temp[i], "TDI",3) == 0){
				TDI_SDR = param[i/2];
					
			}
			else
				if (strncmp(temp[i], "TDO",3) == 0)
					TDO_scan= param[i/2];
				else 
					if (strncmp(temp[i], "MASK",4) == 0)
							MASK_scan= param[i/2];
					else 
						if (strncmp(temp[i], "SMASK",5) == 0)
							SMASK_scan= param[i/2];
							
			};
		}
	}
	else {
		//nothing
	}
	//state - > shift DR
	 setTAPtoDRSHIFT();
	//shift
	stateNavigate(ENDDRstate);
};
//SIR ....................(Scan Instruction Register) Performs an IEEE 1149.1 Instruction Register scan.
void SIRproc(){
	unsigned int length;//specifying the number of bits to be scanned. 
	unsigned long param[4];
		char t[4];
	char temp[6][16];//????????? ?? ?????? ???? ???????
	int n = sscanf(wordBuf,"SIR %d",&length);
	if (n == 1){
		if (length != 0){			
			n = sscanf(wordBuf, "%s%d%s%s%s%s%s%s",t,&length,
			temp[0], temp[1], temp[2], temp[3], temp[4],temp[5]);
			 
			for (int i = 0; i < n-2; i = i + 2){//???? ???? ??????????? ?????????
				temp[i + 1][0] = ' ';
			param[i/2] = strtoul(temp[i+1],NULL, 16); //??????????? ????????? ???????? ?? hex ? dec
			//????????, ???? ??? ????c???
			if (strncmp(temp[i], "TDI",3) == 0){
				TDI_SIR = param[i/2];				
			}
			else
				if (strncmp(temp[i], "TDO",3) == 0)
					TDO_scan= param[i/2];
				else 
					if (strncmp(temp[i], "MASK",4) == 0)
							MASK_scan= param[i/2];
					else 
						if (strncmp(temp[i], "SMASK",5) == 0)
							SMASK_scan= param[i/2];
							
			};
		}
	}
	else {
		//nothing
	}
	 setTAPtoIRSHIFT();
	//shift
	stateNavigate(ENDIRstate);
}
//STATE ..............Forces the IEEE 1149.1 bus to a specified stable state.
void STATEproc(){
	char temp[10];
	int nextState = TAP_STATE_RTI;
@ -590,98 +623,14 @@ void STATEproc(){
stateNavigate(nextState);	
	
	}	;
//TDR...................(Trailer Data Register) Specifies a trailer pattern that is appended to the end of subsequent DR scan operations.
void TDRproc(){
	unsigned int length;//specifying the number of bits to be scanned. Setting the length to 0 removes the header.
	unsigned long param[4];
		char t[4];
	char temp[6][16];//????????? ?? ?????? ???? ???????, ????? ? ????????, ??????
	int n = sscanf(wordBuf,"TDR %d",&length);
	if (n == 1){
		if (length == 0){
			//remove the header
			MASK_TDR= 0; 
			SMASK_TDR= 0;
			TDI_TDR = 0;
			TDO_trailer= 0;
		}
		else{
			n = sscanf(wordBuf, "%s%d%s%s%s%s%s%s",t,&length,
			temp[0], temp[1], temp[2], temp[3], temp[4],temp[5]);
			 
			for (int i = 0; i < n; i = i + 2){//???? ???? ??????????? ?????????
				temp[i + 1][0] = ' ';
			param[i/2] = strtoul(temp[i+1],NULL, 16); //??????????? ????????? ???????? ?? hex ? dec
			//????????, ???? ??? ????c???
			if (strncmp(temp[i], "TDI",3) == 0)
				TDI_TDR = param[i/2];
			else
				if (strncmp(temp[i], "TDO",3) == 0)
					TDO_trailer = param[i/2];
				else 
					if (strncmp(temp[i], "MASK",4) == 0)
							MASK_TDR = param[i/2];
					else 
						if (strncmp(temp[i], "SMASK",5) == 0)
							SMASK_TDR = param[i/2];
							
			};
		}
	}
	else {
		//nothing
	}
};
//TIR ....................(Trailer Instruction Register) Specifies a trailer pattern that is appended to the end of subsequent IR scan operations.
void TIRproc(){
	unsigned int length;//specifying the number of bits to be scanned. Setting the length to 0 removes the header.
	unsigned long param[4];
		char t[4];
	char temp[6][16];//????????? ?? ?????? ???? ???????, ????? ? ????????, ??????
	int n = sscanf(wordBuf,"TIR %d",&length);
	if (n == 1){
		if (length == 0){
			//remove the header
			MASK_TIR = 0; 
			SMASK_TIR= 0;
			TDI_TIR = 0;
			TDO_trailer= 0;
		}
		else{
			n = sscanf(wordBuf,"%s%d%s%s%s%s%s%s",t,&length,
			temp[0], temp[1], temp[2], temp[3], temp[4],temp[5]);
			 
			for (int i = 0; i < n-2; i = i + 2){//???? ???? ??????????? ?????????
				temp[i + 1][0] = ' ';
			param[i/2] = strtoul(temp[i+1],NULL, 16); //??????????? ????????? ???????? ?? hex ? dec
			//????????, ???? ??? ????????
			if (strncmp(temp[i], "TDI",3) == 0)
				TDI_TIR = param[i/2];
			else
				if (strncmp(temp[i], "TDO",3) == 0)
					TDO_trailer = param[i/2];
				else 
					if (strncmp(temp[i], "MASK",4) == 0)
							MASK_TIR = param[i/2];
					else 
						if (strncmp(temp[i], "SMASK",5) == 0)
							SMASK_TIR = param[i/2];
							
			};
		}
	}
	else {
		//nothing
	}
};
//TRST.................(Test ReSeT) Controls the optional Test Reset line.

void TRSTproc(){
char temp[6];
	
	int n = sscanf(wordBuf,"TRST %s",temp);
	if (n == 1){
		if (strncmp(temp, "ON",2) == 0) {
		//?? ?????, ??? ?????????? ?????????, ??????? ????? ?????????? ?????????? 
		//ia iaoea, eae ii?aaaeeou ii?iaeuii, iiyoiio ?a?ac aeiaaeuio? ia?aiaiio? 
			if (GPIO_InitStruct.Mode == GPIO_MODE_INPUT){
			//set output mod
				 GPIO_InitStruct.Pin = TRST_Pin;
@ -704,7 +653,7 @@ char temp[6];
				 pulseTCK();
			}
				else if (strncmp(temp, "Z",2) == 0) {
					//????????? ? ????? ??????, ????? ???????? ????????? ???????? ?????????
					//ia?aaiaei a ?a?ei auaiaa, ?oiau iieo?eou ninoiyiea aunieiai eiiaaaina
				 
					GPIO_InitStruct.Pin = TRST_Pin;
					GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
@ -717,43 +666,308 @@ char temp[6];
					} ;
	}
};
//-----------------------------------------------------
//PIO....................(Parallel Input/Output) Specifies a parallel test pattern.
//???, ??? ??? ?????? ???? ??? ????
void PIOproc(){};
//PIOMAP ............(Parallel Input/Output Map) Maps PIO column positions to a logical pin.
void PIOMAPproc(){};
//-----------------------------------------------------
void processLine(char* str, int strSize, int strCount){
	strcpy(wordBuf,str);
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
	int charCount = 0;
	
	//?????????, ?? ??????????? ?? ??? ??????
	if (str[0] == '/' || str[0] == '!')
	if (wordBuf[0] == '/' || wordBuf[0] == '!')
		return; //?????? ?? ?????? ?? ???????, ???? ??? - ???????????
	//???? ?? ???????????
	//??????????, ????? ??????? ???????? ? ??????
	if (strncmp(wordBuf, "ENDDR", 5)){
			HAL_Delay(10);CDC_Transmit_FS((uint8_t*)"\r\n ENDDR \r\n", 10); 
		//???????? ??????? ?????????
		ENDDRproc();
		//???????
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
@ -767,18 +981,24 @@ void processLine(char* str, int strSize, int strCount){
		return;
	}
	if (strncmp(wordBuf, "RUNTEST", 7)){
			HAL_Delay(10); CDC_Transmit_FS((uint8_t*)"\r\n RUNTEST \r\n", 10); 
		RUNTESTproc();
		return;
	}
	if (strncmp(wordBuf, "SDR", 3)){//fix it!!!
		SDRproc();
	if (strncmp(wordBuf, "SDR", 3)){
			HAL_Delay(10); CDC_Transmit_FS((uint8_t*)"\r\n SDR \r\n", 10); 
		SDRproc(&MyFile);
		return;
	}
	if (strncmp(wordBuf, "SIR", 3)){
		SIRproc();
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
@ -791,7 +1011,92 @@ void processLine(char* str, int strSize, int strCount){
		return;
	}
	if (strncmp(wordBuf, "TRST", 4)){
		
		TRSTproc();
		return;
	}
}
}

void readLine(TCHAR* path){
 char str[1];
	//CDC_Transmit_FS((uint8_t*)"begin\r\n",15);
	// ??????????, ? ??????? ?????????? ????? ?????????? ??????????? ??????
  
   //?????????, ? ??????? ????? ??????? ????? ???????, ? ??????? ??????? 
   // ??????, ??? NULL ???? ????????? ????? ????? ??? ????????? ??????
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
		else //???? ???? ??????
		{			
		//?????? (?????????) ?????? ?? ????? ? ??????????? ?????
   while (1)
   {
      // ?????? ????? ??????  ?? ?????
		  memset (wordBuf, '0', sizeof(wordBuf));
      estr = f_gets (wordBuf,sizeof(wordBuf),&MyFile);			
      //???????? ?? ????? ????? ??? ?????? ??????
      if (estr == NULL)
      {
				//CDC_Transmit_FS((uint8_t*)"\r\nerr check\r\n",13);
         // ?????????, ??? ?????? ?????????: ???????? ????
         // ??? ??? ?????? ??????
         if ( f_eof (&MyFile) != 0)
         {  
            //???? ???? ??????????,
            //??????? ?? ???????????? ?????
           		HAL_Delay(10); CDC_Transmit_FS((uint8_t*)"\r\nend of file\r\n",12);
            break;
         }
         else
         {
            //???? ??? ?????? ????????? ??????, ??????? ????????? 
            //?? ?????? ? ??????? ?? ???????????? ?????
           //  CDC_Transmit_FS((uint8_t*)"\r\nerror\r\n",12);
            break;
         }
      }
      //???? ???? ?? ??????????, ? ?? ???? ?????? ?????? 
      //???????? ? ??????? ????????? ??????	
			HAL_Delay(10);
			CDC_Transmit_FS((uint8_t*)"\r\nprocess line pls\r\n",22);
				HAL_Delay(10);
					
	processLine(str);
   }	
			
			f_close(&MyFile);
	
		}
	}
}		



















