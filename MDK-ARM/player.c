

#include "stm32f1xx_hal.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "fatfs.h"
#include "usbd_cdc_if.h"
#include "sd.h"

extern char USER_Path[4]; /* logical drive path */
FATFS SDFatFs1;
FATFS *fs;
FIL MyFile;

//из "марсохода"
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

#define MAX_STRING_LENGTH (1024*4)
char wordBuf[MAX_STRING_LENGTH];

//#define MAX_FREQUENCY 


int currentState = TAP_STATE_RTI;
int tckDelay = 100;

int MASK_header=0, SMASK_header=0, TDI_HDR=0, TDI_HIR=0, TDO_header=0;//header
int MASK_scan=0, SMASK_scan=0, TDI_SDR=0, TDI_SIR=0, TDO_scan=0; //SIR, SDR
int MASK_TDR=0, MASK_TIR=0, SMASK_TDR=0, SMASK_TIR =0, TDI_TDR=0, TDI_TIR=0, TDO_trailer=0; //Trailer IR, DR
int ENDDRstate = TAP_STATE_RTI;
int ENDIRstate = TAP_STATE_RTI;
/*Some optional command parameters such as, MASK, SMASK, 
and TDI are “sticky” (they are remembered from the previous 
command until changed or invalidated) 
*/

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
		if (c & 0x80000000)
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

void finalizeSDR(){
	
	setTAPtoDRSHIFT();
}

//функции обработки команд свф-файла

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
		//как это вообще делается, э		
	}
	else if (n == 0){
		//return to full speed
	}
};
//HDR ..................(Header Data Register) Specifies a header pattern that is
//prepended to the beginning of subsequent DR scan operations.
void HDRproc(){
	unsigned int length;//specifying the number of bits to be scanned. Setting the length to 0 removes the header.
	unsigned long param[4];
		char t[4];
	char temp[6][16];//параметры из строки сюда заносим, проще с массивом, наверн
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
			 
			for (int i = 0; i < n-2; i = i + 2){//пока есть прочитанные аргументы
				temp[i + 1][0] = ' ';
				param[i/2] = strtoul(temp[i+1],NULL, 16); //преобразуем считанное значение из hex в dec
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
	char temp[6][16];//параметры из строки сюда заносим, проще с массивом, наверн
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
			 
			for (int i = 0; i < n-2; i = i + 2){//пока есть прочитанные аргументы
				temp[i + 1][0] = ' ';
			param[i/2] = strtoul(temp[i+1],NULL, 16); //преобразуем считанное значение из hex в dec
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
//RUNTEST .........Forces the IEEE 1149.1 bus to a run state for a specified number of clocks or a specified time period.
void RUNTESTproc(){
	int nextState = -1;

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
//SDR...................(Scan Data Register) Performs an IEEE 1149.1 Data Register scan.
//SIR ....................(Scan Instruction Register) Performs an IEEE 1149.1 Instruction Register scan.
void SIRproc(){
	unsigned int length;//specifying the number of bits to be scanned. 
	unsigned long param[4];
		char t[4];
	char temp[6][16];//параметры из строки сюда заносим
	int n = sscanf(wordBuf,"SIR %d",&length);
	if (n == 1){
		if (length != 0){			
			n = sscanf(wordBuf, "%s%d%s%s%s%s%s%s",t,&length,
			temp[0], temp[1], temp[2], temp[3], temp[4],temp[5]);
			 
			for (int i = 0; i < n-2; i = i + 2){//пока есть прочитанные аргументы
				temp[i + 1][0] = ' ';
			param[i/2] = strtoul(temp[i+1],NULL, 16); //преобразуем считанное значение из hex в dec
			//выясняем, куда его запиcать
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
//TDR...................(Trailer Data Register) Specifies a trailer pattern that is appended to the end of subsequent DR scan operations.
void TDRproc(){
	unsigned int length;//specifying the number of bits to be scanned. Setting the length to 0 removes the header.
	unsigned long param[4];
		char t[4];
	char temp[6][16];//параметры из строки сюда заносим, проще с массивом, наверн
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
			 
			for (int i = 0; i < n; i = i + 2){//пока есть прочитанные аргументы
				temp[i + 1][0] = ' ';
			param[i/2] = strtoul(temp[i+1],NULL, 16); //преобразуем считанное значение из hex в dec
			//выясняем, куда его запиcать
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
	char temp[6][16];//параметры из строки сюда заносим, проще с массивом, наверн
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
			 
			for (int i = 0; i < n-2; i = i + 2){//пока есть прочитанные аргументы
				temp[i + 1][0] = ' ';
			param[i/2] = strtoul(temp[i+1],NULL, 16); //преобразуем считанное значение из hex в dec
			//выясняем, куда его запихать
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
		//не нашла, как определить нормально, поэтому через глобальную переменную 
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
					//переводим в режим вывода, чтобы получить состояние высокого импеданса
				 
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
//-----------------------------------------------------
//PIO....................(Parallel Input/Output) Specifies a parallel test pattern.
// это вообще надо или нет?
void PIOproc(){};
//PIOMAP ............(Parallel Input/Output Map) Maps PIO column positions to a logical pin.
void PIOMAPproc(){};
//-----------------------------------------------------
	
//global pointers to sdr tdi, tdo, mask and smask arrays
//we expect that dr data can be very long, but we do not know exact size
//size of arrays allocated will be defined during SVF strings parcing
unsigned char* psdr_tdi_data   = NULL;
unsigned char* psdr_tdo_data   = NULL;
unsigned char* psdr_mask_data  = NULL;
unsigned char* psdr_smask_data = NULL;
unsigned int sdr_data_size = 0; //current size of arrays
unsigned int sdr_tdi_sz;
unsigned int sdr_tdo_sz;
unsigned int sdr_mask_sz;
unsigned int sdr_smask_sz;
	//allocate arrays for sdr tdi, tdo and mask
//return 1 if ok or 0 if fail
	void free_sdr_data()
{
	if(psdr_tdi_data)
		free(psdr_tdi_data);
	if(psdr_tdo_data)
		free(psdr_tdo_data);
	if(psdr_mask_data)
		free(psdr_mask_data);
	if(psdr_smask_data)
		free(psdr_smask_data);
	sdr_data_size = 0;
}
unsigned int alloc_sdr_data(unsigned int size)
{
	//compare new size with size of already allocated buffers
	if(sdr_data_size>=size)
		return 1; //ok, because already allocated enough

	//we need to allocate memory for arrays
	//but first free previously allocated buffers

	//tdi
	if(psdr_tdi_data)
		{ free(psdr_tdi_data); psdr_tdi_data=NULL; }

	//tdo
	if(psdr_tdo_data)
		{ free(psdr_tdo_data); psdr_tdo_data=NULL; }

	//mask
	if(psdr_mask_data)
		{ free(psdr_mask_data); psdr_mask_data=NULL; }

	//smask
	if(psdr_smask_data)
		{ free(psdr_smask_data); psdr_smask_data=NULL; }

	psdr_tdi_data = (unsigned char*)malloc(size);
	if(psdr_tdi_data==NULL)
	{
		printf("error allocating sdr tdi buffer\n");
		return(0);
	}

	psdr_tdo_data = (unsigned char*)malloc(size);
	if(psdr_tdo_data==NULL)
	{
		free(psdr_tdi_data);
		psdr_tdi_data=NULL;
		printf("error allocating sdr tdo buffer\n");
		return(0);
	}

	psdr_mask_data = (unsigned char*)malloc(size);
	if(psdr_mask_data==NULL)
	{
		free(psdr_tdi_data);
		free(psdr_tdo_data);
		psdr_tdi_data=NULL;
		psdr_tdo_data=NULL;
		printf("error allocating sdr mask buffer\n");
		return(0);
	}

	psdr_smask_data = (unsigned char*)malloc(size);
	if(psdr_smask_data==NULL)
	{
		free(psdr_tdi_data);
		free(psdr_tdo_data);
		free(psdr_mask_data);
		psdr_tdi_data=NULL;
		psdr_tdo_data=NULL;
		psdr_mask_data=NULL;
		printf("error allocating sdr smask buffer\n");
		return(0);
	}

	//remember that we have allocated some size memory
	sdr_data_size = size;

	//we have successfully allocated buffers for sdr data!
	return 1;
}
unsigned char* get_and_skip_word(unsigned char* ptr, unsigned char* pword, unsigned int* presult)
{
	int i;
	unsigned char c;
	*presult=0;

	//assume error result
	*presult=0;
	ptr = ptr+1;
	for(i=0; i<8; i++)
	{
		c = *ptr;
		if(c==' ' || c==9 || c==0 || c==0xd || c==0xa)
		{
			//end of getting word
			*pword=0;
			*presult=1;
			return ptr;
		}
		*pword++ = c;
		ptr++;
	}
	*pword=0;
	return ptr;
}
unsigned char char2hex(unsigned char c)
{
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
unsigned char* read_hex_array(unsigned char* ptr, unsigned char* pdst, unsigned int num_bits, FILE* f, unsigned int* pcount, unsigned int* presult)
{
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
				pstr = fgets(wordBuf,MAX_STRING_LENGTH-1,f);
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
int do_SDR(FILE* f)
{
	int i,j,k;
	int has_tdi, has_tdo, has_mask, has_smask;
	unsigned int r;
	unsigned char b;
	unsigned char word[16];
	unsigned char* pdst;
	unsigned int num_bits = 0;
	unsigned int num_bytes;
	unsigned char* pdest;
	unsigned int* pdest_count;
	unsigned char* ptr=(unsigned char*)wordBuf;

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
	if(alloc_sdr_data(num_bytes*2+8)==0)
	{
		printf("error on SDR command\n");
		return 0;
	}

	//we expect some words like TDI, TDO, MASK or SMASK here
	//order of words can be different
	has_tdi=0;
	has_tdo=0;
	has_mask=0;
	has_smask=0;
	while(1)
	{
		ptr = ptr+1;
		ptr = get_and_skip_word(ptr,word,&r);
		if(r==0)
		{
			printf("syntax error for SDR command, cannot fetch parameter word\n");
			return 0;
		}
		
		//analyze words
		if(strcmp((char*)word,"TDI")==0)
		{
			has_tdi = 1;
			pdest = psdr_tdi_data;
			pdest_count  = &sdr_tdi_sz;
		}
		else
		if(strcmp((char*)word,"TDO")==0)
		{
			has_tdo = 1;
			pdest = psdr_tdo_data;
			pdest_count  = &sdr_tdo_sz;
		}
		else
		if(strcmp((char*)word,"MASK")==0)
		{
			has_mask = 1;
			pdest = psdr_mask_data;
			pdest_count  = &sdr_mask_sz;
		}
		else
		if(strcmp((char*)word,"SMASK")==0)
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
			sdr_nbits(num_bits,has_tdo,has_mask,has_smask);
			break;
		}
		else
		{
			printf("syntax error for SDR command, unknown parameter word\n");
			return 0;
		}

		//parameter should be in parentheses
ptr = ptr+1;//skip '('
		//now expect to read hexadecimal array of tdi data
		ptr = read_hex_array(ptr,pdest,num_bits,f,pdest_count,&r);
	}
	return 1;
}	
	
	
//-----------------------------------
	void SDRproc(){
	unsigned int length;//specifying the number of bits to be scanned. 
	unsigned long param[4];
	char t[4];
	char temp[6][50];//параметры из строки сюда заносим, проще с массивом, наверн
	int n = sscanf(wordBuf,"SDR %d",&length);
	if (n == 1){
		if (length != 0){		
			n = sscanf(wordBuf, "%s%d%s%s%s%s%s%s",t,&length,
			temp[0], temp[1], temp[2], temp[3], temp[4],temp[5]);
			 
			for (int i = 0; i < n-2; i = i + 2){//пока есть прочитанные аргументы
			temp[i+1][0] = ' ';//заменяем открывающую скобку на пробел
				//чтобы strtol могла обработать
				param[i/2] = strtoul(temp[i+1],NULL, 16); //преобразуем считанное значение из hex в dec
			//выясняем, куда его записать
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

void processLine(char* str, int strSize){	
	strcpy(wordBuf,str);
	char temp;
	int charCount = 0;
	//проверяем, не комментарий ли эта строка
	if (str[0] == '/' || str[0] == '!')
		return; //ничего не делаем со строкой, если она - комментарий
	//если не комментарий
	//определяем, какая команда записана в строке
	if (strncmp(wordBuf, "ENDDR", 5)){
		//вызываем функцию обработки
		ENDDRproc();
		//выходим
		return;
	}
	if (strncmp(wordBuf, "ENDIR", 5)){
		ENDIRproc();
		return;
	}
	if (strncmp(wordBuf, "FREQUENCY",9)){
		
		FREQUENCYproc();
	
		return;
	}
	if (strncmp(wordBuf, "HDR", 3)){
		HDRproc();
		return;
	}
	if (strncmp(wordBuf, "HIR",3)){
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
		RUNTESTproc();
		return;
	}
	if (strncmp(wordBuf, "SDR", 3)){//fix it!!!
		SDRproc();
		return;
	}
	if (strncmp(wordBuf, "SIR", 3)){
		SIRproc();
		return;
	}
	if (strncmp(wordBuf, "STATE", 5)){
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
 char str[MAX_STRING_LENGTH];
	CDC_Transmit_FS((uint8_t*)"begin\r\n",15);
	// Переменная, в которую поочередно будут помещаться считываемые строки
  
   //Указатель, в который будет помещен адрес массива, в который считана 
   // строка, или NULL если достигнут коней файла или произошла ошибка
   char *estr;

	
		if(f_mount(&SDFatFs1,(TCHAR const*)USER_Path,0)!=FR_OK){
		 CDC_Transmit_FS((uint8_t*)"\r\nnot mounted\r\n",15);		 
		Error_Handler();
	}
	else{		
		if(f_open(&MyFile,path,FA_READ)!=FR_OK)
		{
			
			 CDC_Transmit_FS((uint8_t*)"\r\nnot opened\r\n",15);
			Error_Handler();
		}
		else //если файл открыт
		{			
		//Чтение (построчно) данных из файла в бесконечном цикле
   while (1)
   {
      // Чтение одной строки  из файла
      estr = f_gets (str,sizeof(str),&MyFile);			
      //Проверка на конец файла или ошибку чтения
      if (estr == NULL)
      {
				CDC_Transmit_FS((uint8_t*)"\r\nerr check\r\n",13);
         // Проверяем, что именно произошло: кончился файл
         // или это ошибка чтения
         if ( f_eof (&MyFile) != 0)
         {  
            //Если файл закончился,
            //выходим из бесконечного цикла
           	 CDC_Transmit_FS((uint8_t*)"\r\nend of file\r\n",12);
            break;
         }
         else
         {
            //Если при чтении произошла ошибка, выводим сообщение 
            //об ошибке и выходим из бесконечного цикла
             CDC_Transmit_FS((uint8_t*)"\r\nerror\r\n",12);
            break;
         }
      }
      //Если файл не закончился, и не было ошибки чтения 
      //передаем в функцию обработки строки
	
			processLine(str, sizeof(str));
   }
			
			
			f_close(&MyFile);
	
		}
	}
}		