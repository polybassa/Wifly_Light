// Include-Datei f�r Serielle Kommunikation �ber Hardwaremodul des Pic
 //
 //
 // Nils Wei�
 // 29.11.2010
 // Compiler CC5x

#ifndef X86
//*******  Initialisierungs-Funktion  *************************************************
void USARTinit()
{
	//USART TX Pin als Ausgang
	TRISC.6 = 0;

    BRGH=1;					// High Baudrate activated
	BRG16=0;
	SPBRGL=25;				// 19200 Bps @ 8 MHz Clock
	SPBRGH=0;
    SPEN = 1;               // Set_Serial_Pins;
    SYNC = 0;               // Set_Async_Mode;
    TX9 = 0;                // Set_8bit_Tx;
    RX9 = 0;                // Set_8bit_Rx;
    CREN = 1;               // Enable_Rx;
    TXEN = 1;               // Enable_Tx;
    RCIE=1;                 // Rx Interrupt aus
	ADDEN=0;				// Disable Adressdetection
}

//*******  Sende-char-Funktion  *************************************************
void USARTsend(unsigned char ch)
{
	while(!TXIF);
	TXREG=ch;
}
#endif /* #ifndef X86 */

//*******  Sende-String-Funktion  *************************************************
void USARTsend_str(const char *string)
{
 char ps;
 ps = *string;
 while(ps > 0)
   {
    string++;
   	USARTsend(ps);
    ps = *string;
   }
}

//*******  Sende-Array-Funktion  *************************************************
void USARTsend_arr(char *array, char length)
{
	if(array == 0) return;
	char i;
	for(i=0;i<length;i++)
	{
		USARTsend(*array);
		array++;
	}
}

#ifdef TEST
//*******  Sende-Zahl-als-String-Funktion  *************************************************
void USARTsend_num(unsigned char input,unsigned char sign)
{
   char temp;
   char h,z,e;
 
   h=0;
   z=0;
   e=0; 
   
   if (input>99)
   {
      h=input/100;
      temp=0;
      temp=100*h;
      input=input-temp; 
   }

   if (input>9)
   {
      z=input/10;
      temp=0;
      temp=z*10;
      input=input-temp;      
   }

   if (input<=9)
   {
      e=input;
   }
   if(h!=0)USARTsend(h+0x30);
   USARTsend(z+0x30);
   USARTsend(e+0x30);
   USARTsend(sign); 	//Zeichen senden
}
#endif
/*
//SENDE BCD-Zahl als String
void USARTsendTime(unsigned char input,unsigned char sign)
{
	char 	temp;
	char	z,e;
	
	e=input&0x0f;
	input=swap(input);
	z=input&0x0f;
	if(e>9)
	{
		e-=10;
		z++;
	}
	
	if(z>5)z=0;
	
	USARTsend(z+0x30);
	USARTsend(e+0x30);
	USARTsend(sign);
}*/
	

