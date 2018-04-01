#include "stm32f1xx_hal.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>



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
void inputTDI(int data, int dataSize){
for (int i = 0; i < 32; i++)
	{
		//Проверяем старший бит)
		if (data & 0x80000000)
			HAL_GPIO_WritePin(GPIOB, TDI_Pin, GPIO_PIN_SET);
		else
			HAL_GPIO_WritePin(GPIOB, TDI_Pin, GPIO_PIN_RESET);
		//Сдвигаем влево на 1 бит
		data = data << 1; 	
		pulseTCK();
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
//так, вот это вообще надо или нет?
void PIOproc(){};
//PIOMAP ............(Parallel Input/Output Map) Maps PIO column positions to a logical pin.
void PIOMAPproc(){};
//-----------------------------------------------------
void processLine(char* str, int strSize, int strCount){
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