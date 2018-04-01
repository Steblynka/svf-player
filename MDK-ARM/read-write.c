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
FATFS SDFatFs1;
FATFS *fs;
FIL MyFile, MyLogFile;

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


	/*	void read_dir(FILINFO fileInfo,DIR dir){
			char *fn;
			DWORD fre_clust, fre_sect, tot_sect;
	if(f_mount(&SDFatFs1,(TCHAR const*)USER_Path,0)!=FR_OK)
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


		*/
void read_line(TCHAR* path){
	int strCount = 0; //подсчет количества строк
	CDC_Transmit_FS((uint8_t*)"begin\r\n",15);
	// Переменная, в которую поочередно будут помещаться считываемые строки
   char str[MAX_STRING_LENGTH];
   //Указатель, в который будет помещен адрес массива, в который считана 
   // строка, или NULL если достигнут коней файла или произошла ошибка
   char *estr;

	
		if(f_mount(&SDFatFs1,(TCHAR const*)USER_Path,0)!=FR_OK)
	{
		 CDC_Transmit_FS((uint8_t*)"\r\nnot mounted\r\n",15);
		//CDC_Transmit_FS((uint8_t*)" err1", 5); 
		Error_Handler();
	}
	else
	{
	
		
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
             CDC_Transmit_FS((uint8_t*)"\r\nerrrrrror\r\n",12);
            break;
         }
      }
      //Если файл не закончился, и не было ошибки чтения 
      //выводим считанную строку  на экран
			//а на самом деле передаем в функцию обработки строки
			strCount++;
      CDC_Transmit_FS((uint8_t*)"\r\nnext line:\r\n",12);
			CDC_Transmit_FS((uint8_t*)str,sizeof(str));
			//processLine(str, sizeof(str), strCount);
   }
			
			
			f_close(&MyFile);
	//	f_close(&MyLogFile);
		}
	}
}				