/**
  ******************************************************************************
  * File Name          : main.c
  * Description        : Main program body
  ******************************************************************************
  * This notice applies to any and all portions of this file
  * that are not between comment pairs USER CODE BEGIN and
  * USER CODE END. Other portions of this file, whether 
  * inserted by the user or by software development tools
  * are owned by their respective copyright owners.
  *
  * Copyright (c) 2018 STMicroelectronics International N.V. 
  * All rights reserved.
  *
  * Redistribution and use in source and binary forms, with or without 
  * modification, are permitted, provided that the following conditions are met:
  *
  * 1. Redistribution of source code must retain the above copyright notice, 
  *    this list of conditions and the following disclaimer.
  * 2. Redistributions in binary form must reproduce the above copyright notice,
  *    this list of conditions and the following disclaimer in the documentation
  *    and/or other materials provided with the distribution.
  * 3. Neither the name of STMicroelectronics nor the names of other 
  *    contributors to this software may be used to endorse or promote products 
  *    derived from this software without specific written permission.
  * 4. This software, including modifications and/or derivative works of this 
  *    software, must execute solely and exclusively on microcontroller or
  *    microprocessor devices manufactured by or for STMicroelectronics.
  * 5. Redistribution and use of this software other than as permitted under 
  *    this license is void and will automatically terminate your rights under 
  *    this license. 
  *
  * THIS SOFTWARE IS PROVIDED BY STMICROELECTRONICS AND CONTRIBUTORS "AS IS" 
  * AND ANY EXPRESS, IMPLIED OR STATUTORY WARRANTIES, INCLUDING, BUT NOT 
  * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A 
  * PARTICULAR PURPOSE AND NON-INFRINGEMENT OF THIRD PARTY INTELLECTUAL PROPERTY
  * RIGHTS ARE DISCLAIMED TO THE FULLEST EXTENT PERMITTED BY LAW. IN NO EVENT 
  * SHALL STMICROELECTRONICS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, 
  * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
  * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
  * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
  * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32f1xx_hal.h"
#include "fatfs.h"
#include "usb_device.h"

/* USER CODE BEGIN Includes */
#include "usbd_cdc_if.h"
#include "sd.h"
/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim2;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/
uint8_t Byte1=0;
volatile uint16_t Timer1=0;
uint8_t sect[512];//для сохранения бйтов из блока
uint8_t Buf[] = "test";
char buffer1[] ="random data 1234567890"; //Буфер данных для записи/чтения
extern char str1[60];
uint32_t byteswritten,bytesread;
uint8_t result;
extern char USER_Path[4]; /* logical drive path */
FATFS SDFatFs;
FATFS *fs;
FIL MyFile;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_TIM2_Init(void);
static void MX_SPI1_Init(void);

/* USER CODE BEGIN PFP */
/* Private function prototypes -----------------------------------------------*/

/* USER CODE END PFP */

/* USER CODE BEGIN 0 */
FRESULT ReadLongFile(void)
{
  uint16_t i=0, i1=0;
  uint32_t ind=0;
  uint32_t f_size = MyFile.fsize;
 // sprintf(str1,"fsize: %lu\r\n",(unsigned long)f_size);
 //HAL_UART_Transmit(&huart1,(uint8_t*)str1,strlen(str1),0x1000);
	//CDC_Transmit_FS((uint8_t*)str1,strlen(str1));
  ind=0;
  do
  {
    if(f_size<512)
    {
      i1=f_size;
    }
    else
    {
      i1=512;
    }
    f_size-=i1;
    f_lseek(&MyFile,ind);
    f_read(&MyFile,sect,i1,(UINT *)&bytesread);
// sprintf(str1,"bytes read: %lu\r\n",(unsigned long)bytesread);
 // 	CDC_Transmit_FS((uint8_t*)str1,strlen(str1));
		CDC_Transmit_FS((uint8_t*)sect,bytesread-1);
		
   
    ind+=i1;
  }
  while(f_size>0);
  CDC_Transmit_FS((uint8_t*)"\r\n",2);
  return FR_OK;
}


void read1(TCHAR* path){
		if(f_mount(&SDFatFs,(TCHAR const*)USER_Path,0)!=FR_OK)
	{
		//CDC_Transmit_FS((uint8_t*)" err1", 5); 
		Error_Handler();
	}
	else
	{
		
		if(f_open(&MyFile,path,FA_READ)!=FR_OK)
		{
			//CDC_Transmit_FS((uint8_t*)" err2", 5); 
			Error_Handler();
		}
		else
		{
		//	CDC_Transmit_FS((uint8_t*)" rlf ", 5); 
			ReadLongFile();
			f_close(&MyFile);
		}
	}
}

void write_f1(TCHAR* path,TCHAR* wtext, UINT buf_size){
	FRESULT res;
		if(f_mount(&SDFatFs,(TCHAR const*)USER_Path,0)!=FR_OK)
	{
		Error_Handler();
	}
	else
	{
	
		if(f_open(&MyFile,path,FA_CREATE_ALWAYS|FA_WRITE)!=FR_OK)
		{
			Error_Handler();
		}
		else
		{
			res=f_write(&MyFile,wtext,buf_size,(void*)&byteswritten);
			if((byteswritten==0)||(res!=FR_OK))
			{
				Error_Handler();
			}
			f_close(&MyFile);
		}
	}
}


		void read_dir(FILINFO fileInfo,DIR dir){
			char *fn;
			DWORD fre_clust, fre_sect, tot_sect;
	if(f_mount(&SDFatFs,(TCHAR const*)USER_Path,0)!=FR_OK)
	{
		Error_Handler();
	}
	else
	{
		fileInfo.lfname = (char*)sect;
		fileInfo.lfsize = sizeof(sect);
		result = f_opendir(&dir, "/");
		if (result == FR_OK)
		{
			while(1)
			{
				result = f_readdir(&dir, &fileInfo);
				if (result==FR_OK && fileInfo.fname[0])
				{
					fn = fileInfo.lfname;
					if(strlen(fn)) CDC_Transmit_FS((uint8_t*)fn,strlen(fn));
					else CDC_Transmit_FS((uint8_t*)fileInfo.fname,strlen((char*)fileInfo.fname));
					if(fileInfo.fattrib&AM_DIR)
					{
						CDC_Transmit_FS((uint8_t*)"  [DIR]",7);
					}					
				}
				else break;
			CDC_Transmit_FS((uint8_t*)"\r\n",2);
			}
			f_closedir(&dir);
		}
	}
	f_getfree("/", &fre_clust, &fs);
	sprintf(str1,"fre_clust: %lu\r\n",fre_clust);
	CDC_Transmit_FS((uint8_t*)str1,strlen(str1));
	sprintf(str1,"n_fatent: %lu\r\n",fs->n_fatent);
	CDC_Transmit_FS((uint8_t*)str1,strlen(str1));
	sprintf(str1,"fs_csize: %d\r\n",fs->csize);
	CDC_Transmit_FS((uint8_t*)str1,strlen(str1));
	tot_sect = (fs->n_fatent - 2) * fs->csize;
	sprintf(str1,"tot_sect: %lu\r\n",tot_sect);
	CDC_Transmit_FS((uint8_t*)str1,strlen(str1));
	fre_sect = fre_clust * fs->csize;
	sprintf(str1,"fre_sect: %lu\r\n",fre_sect);
	CDC_Transmit_FS((uint8_t*)str1,strlen(str1));
	sprintf(str1, "%lu KB total drive space.\r\n%lu KB available.\r\n",
	fre_sect/2, tot_sect/2);
	CDC_Transmit_FS((uint8_t*)str1,strlen(str1)); 
		}
/* USER CODE END 0 */

int main(void)
{
  /* USER CODE BEGIN 1 */
	uint16_t i;
	FRESULT res; //результат выполнения
	uint8_t wtext[]="12345678900987654321aaaaaaaaaaaa.";
	FILINFO fileInfo;
	char *fn;
	DIR dir;
	DWORD fre_clust, fre_sect, tot_sect;
  /* USER CODE END 1 */

  /* MCU Configuration----------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_USB_DEVICE_Init();
  MX_FATFS_Init();
  MX_TIM2_Init();
  MX_SPI1_Init();

  /* USER CODE BEGIN 2 */

	HAL_Delay(5000);
	HAL_UART_Receive_IT(&huart1, &Byte1, 1);
	//запустмим таймер
	HAL_TIM_Base_Start_IT(&htim2);
	
	

disk_initialize(SDFatFs.drv);
//read_dir( fileInfo, dir);	
/*
if(f_mount(&SDFatFs,(TCHAR const*)USER_Path,0)!=FR_OK)
	{
		Error_Handler();
	}
	else
	{
		fileInfo.lfname = (char*)sect;
		fileInfo.lfsize = sizeof(sect);
		result = f_opendir(&dir, "/");
		if (result == FR_OK)
		{
			while(1)
			{
				result = f_readdir(&dir, &fileInfo);
				if (result==FR_OK && fileInfo.fname[0])
				{
					fn = fileInfo.lfname;
					if(strlen(fn)) CDC_Transmit_FS((uint8_t*)fn,strlen(fn));
					else CDC_Transmit_FS((uint8_t*)fileInfo.fname,strlen((char*)fileInfo.fname));
						CDC_Transmit_FS((uint8_t*)"\r\n",2);
					if(fileInfo.fattrib&AM_DIR)
					{
						CDC_Transmit_FS((uint8_t*)"  [DIR]",7);
						CDC_Transmit_FS((uint8_t*)"\r\n",2);
					}					
				}
				else break;
			CDC_Transmit_FS((uint8_t*)"\r\n",2);
			}
			f_closedir(&dir);
		}
	}
	f_getfree("/", &fre_clust, &fs);
	sprintf(str1,"fre_clust: %lu\r\n",fre_clust);
	CDC_Transmit_FS((uint8_t*)str1,strlen(str1));
	sprintf(str1,"n_fatent: %lu\r\n",fs->n_fatent);
	CDC_Transmit_FS((uint8_t*)str1,strlen(str1));
	sprintf(str1,"fs_csize: %d\r\n",fs->csize);
	CDC_Transmit_FS((uint8_t*)str1,strlen(str1));
	tot_sect = (fs->n_fatent - 2) * fs->csize;
	sprintf(str1,"tot_sect: %lu\r\n",tot_sect);
	CDC_Transmit_FS((uint8_t*)str1,strlen(str1));
	fre_sect = fre_clust * fs->csize;
	sprintf(str1,"fre_sect: %lu\r\n",fre_sect);
	CDC_Transmit_FS((uint8_t*)str1,strlen(str1));
	sprintf(str1, "%lu KB total drive space.\r\n%lu KB available.\r\n",
	fre_sect/2, tot_sect/2);
	CDC_Transmit_FS((uint8_t*)str1,strlen(str1)); 
	*/
read1("123.txt");
read1("/Download/mywrite2.txt");
write_f1("/Download/mywrite3.txt", wtext, sizeof(wtext));
	
read1("/Download/mywrite3.txt");
	
	
	
	
	
	
FATFS_UnLinkDriver(USER_Path);

	
	/* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
		CDC_Transmit_FS((uint8_t*)"LOOP\n", 5); 
	
		HAL_Delay(10000);
		
  /* USER CODE END WHILE */

  /* USER CODE BEGIN 3 */

  }
  /* USER CODE END 3 */

}

/** System Clock Configuration
*/
void SystemClock_Config(void)
{

  RCC_OscInitTypeDef RCC_OscInitStruct;
  RCC_ClkInitTypeDef RCC_ClkInitStruct;
  RCC_PeriphCLKInitTypeDef PeriphClkInit;

    /**Initializes the CPU, AHB and APB busses clocks 
    */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL6;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

    /**Initializes the CPU, AHB and APB busses clocks 
    */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_PLL;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

    /**Configure the Systick interrupt time 
    */
  HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/1000);

    /**Configure the Systick 
    */
  HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);

  /* SysTick_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
}

/* SPI1 init function */
static void MX_SPI1_Init(void)
{

  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

}

/* TIM2 init function */
static void MX_TIM2_Init(void)
{

  TIM_ClockConfigTypeDef sClockSourceConfig;
  TIM_MasterConfigTypeDef sMasterConfig;

  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 39999;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 10;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

}

/* USART1 init function */
static void MX_USART1_UART_Init(void)
{

  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

}

/** Configure pins as 
        * Analog 
        * Input 
        * Output
        * EVENT_OUT
        * EXTI
*/
static void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct;

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(SPI_CS_GPIO_Port, SPI_CS_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : SPI_CS_Pin */
  GPIO_InitStruct.Pin = SPI_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
  HAL_GPIO_Init(SPI_CS_GPIO_Port, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */

//----------------------------------------------------------
//инкрементируем глобальную переменную в обработчике прерываний таймера
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim){
if(htim==&htim2){
	Timer1++;
	}
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)//функция вызывается автоматически после приема данных в UART
{
	if (hUsbDeviceFS.dev_state == 0x03 && (Byte1 != NULL))//проверяем, подключен ли USB
		CDC_Transmit_FS(&Byte1,1);//отправляет байт в USB

	HAL_UART_Receive_IT(&huart1, &Byte1, 1);//заказываем прием следующего байта из UART
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  None
  * @retval None
  */
void _Error_Handler(char * file, int line)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  while(1) 
  {
  }
  /* USER CODE END Error_Handler_Debug */ 
}

#ifdef USE_FULL_ASSERT

/**
   * @brief Reports the name of the source file and the source line number
   * where the assert_param error has occurred.
   * @param file: pointer to the source file name
   * @param line: assert_param error line source number
   * @retval None
   */
void assert_failed(uint8_t* file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
    ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */

}

#endif

/**
  * @}
  */ 

/**
  * @}
*/ 

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
