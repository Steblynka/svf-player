#include "fatfs.h"
#include "usbd_cdc_if.h"
#include "sd.h"
//#include <stdio.h> 
//uint8_t Byte1=0;
#define MAX_STRING_LENGTH (1024*4)
uint8_t sect[512];//для сохранения бйтов из блока

extern char str1[60];
uint32_t byteswritten, bytesread;
uint8_t result;
extern char USER_Path[4]; /* logical drive path */
extern FATFS SDFatFs1;
 extern FATFS *fs;
extern FIL MyFile;

FRESULT ReadLongFile(void){
  uint16_t  i1=0;
  uint32_t ind=0;
  uint32_t f_size = MyFile.fsize;
 
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
		CDC_Transmit_FS((uint8_t*)sect,bytesread-1);
		
   
    ind+=i1;
  }
  while(f_size>0);
  CDC_Transmit_FS((uint8_t*)"\r\n",2);
  return FR_OK;
}


void read1(TCHAR* path){
		if(f_mount(&SDFatFs1,(TCHAR const*)USER_Path,0)!=FR_OK)
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
		if(f_mount(&SDFatFs1,(TCHAR const*)USER_Path,0)!=FR_OK)
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


	
//void read_line(TCHAR* path){
//	int strCount = 0; //подсчет количества строк
//	CDC_Transmit_FS((uint8_t*)"begin\r\n",15);
//	// Переменная, в которую поочередно будут помещаться считываемые строки
//   char str[MAX_STRING_LENGTH];
//   //Указатель, в который будет помещен адрес массива, в который считана 
//   // строка, или NULL если достигнут коней файла или произошла ошибка
//   char *estr;

//	
//		if(f_mount(&SDFatFs1,(TCHAR const*)USER_Path,0)!=FR_OK)
//	{
//		 CDC_Transmit_FS((uint8_t*)"\r\nnot mounted\r\n",15);		 
//		Error_Handler();
//	}
//	else
//	{
//	
//		
//		if(f_open(&MyFile,path,FA_READ)!=FR_OK)
//		{
//			
//			 CDC_Transmit_FS((uint8_t*)"\r\nnot opened\r\n",15);
//			Error_Handler();
//		}
//		else //если файл открыт
//		{			
//		//Чтение (построчно) данных из файла в бесконечном цикле
//   while (1)
//   {
//      // Чтение одной строки  из файла
//      estr = f_gets (str,sizeof(str),&MyFile);			
//      //Проверка на конец файла или ошибку чтения
//      if (estr == NULL)
//      {
//				CDC_Transmit_FS((uint8_t*)"\r\nerr check\r\n",13);
//         // Проверяем, что именно произошло: кончился файл
//         // или это ошибка чтения
//         if ( f_eof (&MyFile) != 0)
//         {  
//            //Если файл закончился,
//            //выходим из бесконечного цикла
//           	 CDC_Transmit_FS((uint8_t*)"\r\nend of file\r\n",12);
//            break;
//         }
//         else
//         {
//            //Если при чтении произошла ошибка, выводим сообщение 
//            //об ошибке и выходим из бесконечного цикла
//             CDC_Transmit_FS((uint8_t*)"\r\nerror\r\n",12);
//            break;
//         }
//      }
//      //Если файл не закончился, и не было ошибки чтения 
//      //передаем в функцию обработки строки
//			strCount++;
//      CDC_Transmit_FS((uint8_t*)"\r\nnext line:\r\n",12);
//			CDC_Transmit_FS((uint8_t*)str,sizeof(str));
//			//processLine(str, sizeof(str), strCount);
//   }
//			
//			
//			f_close(&MyFile);
//	//	f_close(&MyLogFile);
//		}
//	}
//}				