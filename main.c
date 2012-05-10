//Nils Wei� 
//05.09.2011
//Compiler CC5x/
//#define TEST

#define MPLAB_IDE
#include "platform.h"

#define NO_CRC

#pragma sharedAllocation

//*********************** ENUMERATIONS *********************************************
#define TRUE  1
#define FALSE 0

#define STX 0xFF
#define SET_COLOR 0xFD
#define SET_FADE 0xFC
#define SET_RUN 0xFB
#define WAIT 0xFE
#define SET_ON 0xFA
#define SET_OFF 0xF9
#define DELETE 0xF8

#define CmdPointerAddr 0xff		// *** Address at EERPOM. Commandpointer indicates the nummer of commands
#define CmdLoopPointerAddr 0xfd // *** Address at EEPROM. CommandLoopPointer indicates the next command. Used in Loop-Mode

//*********************** INCLUDEDATEIEN *********************************************
#pragma codepage 1
#include "RingBuf.h"		//clean
#include "usart.h"			//clean
#include "eeprom.h"       	//clean 
#include "crc.h"			//clean
#include "ledstrip.h"		//under construction
#include "spi.h"			//clean
#include "timer.h"
//*********************** GLOBAL VARIABLES *******************************************
#define FRAMELENGTH 16			// *** max length of one commandframe
struct CommandBuffer{
    char cmd_counter;
    char frame_counter;
    char cmd_buf[FRAMELENGTH];
    char crcH;
    char crcL;
};
static struct CommandBuffer gCmdBuf;


// forget this:
// #define crc_failure 0
// #define eeprom_failure 1
// static char gERROR;
// this is how bits are defined in Ansi-C
// *** ERRORBITS
static struct {
		char crc_failure:1;
		char eeprom_failure:1;
}gERROR;

#ifndef X86
//*********************** INTERRUPTSERVICEROUTINE ************************************
#pragma origin 4					//Adresse des Interrupts	
interrupt InterruptRoutine(void)
{
	if (RCIF)
	{
		if(!RingBufHasError) RingBufPut(RCREG);
		else 
		{
			//Register lesen um Schnittstellen Fehler zu vermeiden
			char temp = RCREG;
		}
	}
}
#endif /* #ifndef X86 */

//*********************** FUNKTIONSPROTOTYPEN ****************************************
void init_all();
void throw_errors();
void get_commands();
void execute_commands();

//*********************** HAUPTPROGRAMM **********************************************
void main(void)
{
	init_all();
    while(1)
    {	
        throw_errors();
		get_commands();
		execute_commands();
    }
}
//*********************** UNTERPROGRAMME **********************************************

void init_all()
{
#ifndef X86
	//OSZILLATOR initialisieren: 4xPLL deactivated;INTOSC 16MHz
	OSCCON = 0b01110010;
#endif /* #ifndef X86 */
	RingBufInit();
	//initialise UART interface
	USARTinit();
	spi_init();
	ledstrip_init();

/** EEPROM contains FF in every Cell after inital start,
*** so I have to delet the pointer address
*** otherwise the PIC thinks he has the EEPROM full with commands
**/
	if (EEPROM_RD(CmdPointerAddr) == 0xff)
	EEPROM_WR(CmdPointerAddr, 0);
	EEPROM_WR(CmdLoopPointerAddr, 0);
	
#ifndef X86
	//Ausgang f�r FET initalisieren
	TRISC.0 = 0;
	//Spannungsversorgung f�r LED's einschalten
	PORTC.0 = 0;
#endif /* #ifndef X86 */

	//To Factory Restore WLAN Modul
	//TRISA.0 = 0;
	//PORTA.0 = 1;
    
    // *** load globals variables
    gERROR.crc_failure = 0;
    gERROR.eeprom_failure = 0;
    gCmdBuf.cmd_counter = 0;
    gCmdBuf.frame_counter = 0;
	
	char i;
	for(i=0;i<FRAMELENGTH;i++)
	{
        gCmdBuf.cmd_buf[i] = 0;
	}

#ifndef X86
	// *** allow interrupts
	RCIE=1;
	PEIE=1;
	GIE=1;
#endif /* #ifndef X86 */
	// *** send ready after init
	USARTsend('R');
	USARTsend('D');
	USARTsend('Y');

	
}

void throw_errors()
{
	if(RingBufHasError) 
	{
		USARTsend_str(" ERROR: Receivebuffer full");
		RingBufClearError;
	}
	if(gERROR.crc_failure)
	{
		USARTsend_str(" ERROR: CRC-Check failed");
		gERROR.crc_failure = 0;
	}
	if(gERROR.eeprom_failure)
	{
		USARTsend_str(" ERROR: EEPROM is full");
		gERROR.eeprom_failure = 0;
	}
}

/** This function reads one byte from the ringbuffer and check
*** for framestart, framelength, or databyte 
*** if a frame is complete, the function save the frame as a new
*** command in the internal EEPROM and calculate the Pointer for the next Command
**/
void get_commands()
{	
	if(RingBufIsNotEmpty)
	{
		// *** preload variables and 
		// *** get new_byte from ringbuffer
		char new_byte, temp, j;
		temp = 0;
		j = 0;
		// *** get new byte
		new_byte = RingBufGet();	
		// *** do I wait for databytes?
		if(gCmdBuf.frame_counter == 0)
		{
			// *** I don't wait for databytes
			// *** Do I receive a Start_of_Text sign
			if(new_byte == STX)
			{
				// *** increse the cmd_counter
				gCmdBuf.cmd_counter = 1;
				// *** Write the startsign at the begin of the buffer
				gCmdBuf.cmd_buf[0] = new_byte;
                // *** Reset crc Variables
                newCRC(&gCmdBuf.crcH, &gCmdBuf.crcL);
                // *** add new_byte to crc checksum
                addCRC(new_byte, &gCmdBuf.crcH, &gCmdBuf.crcL);
			}
			else
			{	
				// *** to avoid arrayoverflow
				temp = FRAMELENGTH - 2;
				// *** check if I get the framelength byte
				if((new_byte < temp) && (gCmdBuf.cmd_counter == 1))
				{
					gCmdBuf.frame_counter = new_byte;
					gCmdBuf.cmd_buf[1] = new_byte;
					gCmdBuf.cmd_counter = 2;
                    // *** add new_byte to crc checksum
                    addCRC(new_byte, &gCmdBuf.crcH, &gCmdBuf.crcL);
				}
			}
		}
		else
		{
			// *** I wait for Databytes, so I save all bytes 
			// *** that I get until my framecounter is > 0
			gCmdBuf.cmd_buf[gCmdBuf.cmd_counter] = new_byte;
			gCmdBuf.cmd_counter++;
			
            // *** add new_byte to crc checksum
			if(gCmdBuf.frame_counter > 2)
            addCRC(new_byte, &gCmdBuf.crcH, &gCmdBuf.crcL);
			gCmdBuf.frame_counter--;
			// *** now I have to check if my framecounter is null.
			// *** If it's null my string is complete 
			// *** and I can give the string to the crc check function.
			if(gCmdBuf.frame_counter == 0)
			{
#ifdef NO_CRC
				if(1==1)
#else
                // *** verify crc checksum
                if( (gCmdBuf.crcL == gCmdBuf.cmd_buf[gCmdBuf.cmd_counter - 1]) &&
                    (gCmdBuf.crcH == gCmdBuf.cmd_buf[gCmdBuf.cmd_counter - 2]) )
#endif
                {
					// *** Execute the simple Commands
					switch(gCmdBuf.cmd_buf[2])
					{
						case DELETE: 
							{
								EEPROM_WR(CmdPointerAddr,0);
								return;
							}
#ifndef X86
						case SET_ON: 
							{
								BCF(PORTC.0); 
								return;
								}
						case SET_OFF: 
							{
								BSF(PORTC.0); 
								return;
							}
#endif /* #ifndef X86 */
					}
                    char CmdPointer = EEPROM_RD(CmdPointerAddr);
					// *** check if there is enough space in the EEPROM for the next command
                    if(CmdPointer < (CmdPointerAddr - CmdWidth))
                    {
                        // *** calculate the next address for EEPROM write
                        EEPROM_WR(CmdPointerAddr,(CmdPointer + CmdWidth));
                    }
                    else 
                    {
                        // *** EEPROM is full with commands
                        // *** Some errorhandling should be here
						gERROR.eeprom_failure = 1;
                        return;
                    } 
					// *** Write the new command without STX and CRC
					EEPROM_WR_BLK(&gCmdBuf.cmd_buf[2], CmdPointer, (gCmdBuf.cmd_counter -4));
					// *** Send a Message('G'et 'C'ommand) when a new Command is received successfull
					USARTsend('G');
					USARTsend('C');
#ifdef TEST
					USARTsend_arr(&gCmdBuf.cmd_buf[2], (gCmdBuf.cmd_counter - 4));
#endif
                }
                else
                {
                    // *** Do some error handling in case of an CRC failure here
					gERROR.crc_failure = 1;
                    return;
                }
			}
		}
	}
}

/** This function reads the pointer for commands in the EEPROM from a defined address 
*** in the EEPROM. After this one by one command is executed by this function. 
**/ 
void execute_commands()
{
	// *** get the pointer to commands in the EEPROM
	char pointer = EEPROM_RD(CmdPointerAddr);
	// *** are there commands to execute?
	if(pointer != 0)
	{
		// *** commands available, check what to do
		switch(EEPROM_RD(pointer - CmdWidth)) 
		{	
			case SET_COLOR: 
			{
				sub_func_set_color(&pointer);
				break;
			}
			case SET_FADE: {break;}
			case SET_RUN: {break;}
		}
		// *** set commandpointer to the next command
		EEPROM_WR(CmdPointerAddr, (pointer - CmdWidth));
	}
}

// cc5xfree is a bit stupid so we include the other implementation files here
#ifndef X86
#include "crc.c"
#include "eeprom.c"
#include "ledstrip.c"
#include "RingBuf.c"
#include "spi.c"
#include "timer.c"
#include "usart.c"
#endif /* #ifndef X86 */
