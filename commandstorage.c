#include "platform.h"
#include "commandstorage.h"

struct led_cmd* commandstorage_read(struct led_cmd *pDest)
{
	// if the commandstorage is waiting
	if(gCmdBuf.WaitValue == 0) 
	{
		//check parameter
		if(0 == pDest) return 0;

		//commands available in eeprom?
		char nextCmd = EEPROM_RD(CmdPointerAddr);
		if(0 == nextCmd) return 0;
	
		if(gCmdBuf.LoopMode)
		{
			nextCmd = EEPROM_RD(CmdLoopPointerAddr);
			if(0 == nextCmd)
				nextCmd = EEPROM_RD(CmdPointerAddr);
				EEPROM_WR(CmdLoopPointerAddr, nextCmd);
		}

		//read command from eeprom
		EEPROM_RD_BLK((char*)pDest, (nextCmd - CmdWidth), CmdWidth);

		//update the CmdPointer?
		if(gCmdBuf.LoopMode)
			EEPROM_WR(CmdLoopPointerAddr, nextCmd - CmdWidth);		
		else
			EEPROM_WR(CmdPointerAddr, nextCmd - CmdWidth);
			
#ifdef TEST
		USARTsend_str("Read_Done");
#endif 
		return pDest;
	}
	else return 0;
}

bit commandstorage_write(char *pSrc, char length)
{
	//check parameter
	if(0 == pSrc) return FALSE;
	
	//enought free space in eeprom?
	char nextCmd = EEPROM_RD(CmdPointerAddr);
	if(nextCmd >= (CmdPointerAddr - CmdWidth)) return FALSE;

	//increase the command pointer in eeprom
	EEPROM_WR(CmdPointerAddr,(nextCmd + CmdWidth));
		
	//write data to eeprom
	EEPROM_WR_BLK(pSrc, nextCmd, length);
		
	return TRUE;
}

void commandstorage_get_commands()
{	
	if(RingBufHasError)
	{
		// *** if a RingBufError occure, I have to throw away the current command,
		// *** because the last byte was not saved. Commandstring is inconsistent
		ClearCmdBuf();
	}

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
			if((uns8)new_byte == STX)
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
								USARTsend('D');
								return;
							}
						case SET_ON: 
							{
								PowerOnLEDs();
								return;
							}
						case SET_OFF: 
							{
								PowerOffLEDs();
								return;
							}
						case LOOP_ON:
							{	
								gCmdBuf.LoopMode = 1;
								USARTsend('L');
								USARTsend('1');
								return;
							}
						case LOOP_OFF:
							{	
								gCmdBuf.LoopMode = 0;
								gCmdBuf.WaitValue = 0;
								USARTsend('L');
								USARTsend('0');
								return;
							}
						default:
							{
								if( commandstorage_write(&gCmdBuf.cmd_buf[2], (gCmdBuf.cmd_counter - 4)))
								{
									USARTsend('G');
									USARTsend('C');
								}
								else 
									gERROR.eeprom_failure = 1;
							}
					}							
					
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

void commandstorage_execute_commands()
{
	// *** get the pointer to commands in the EEPROM
	struct led_cmd nextCmd;

	// read next command from eeprom and move command pointer in eeprom to the next command (TRUE)
	struct led_cmd *result = commandstorage_read(&nextCmd);
		
	if(0 != result)
	{
#ifdef TEST
USARTsend_str("executeCommand");
#endif
		// *** commands available, check what to do
		switch(nextCmd.cmd) 
		{	
			case SET_COLOR: 
			{
				ledstrip_set_color(&nextCmd.data.set_color);
				break;
			}
			case SET_FADE:
			{
				ledstrip_set_fade(&nextCmd.data.set_fade);
				break;
			}
			case WAIT:
			{
				struct cmd_wait *pCmd = &nextCmd.data.wait;
#ifdef TEST
				USARTsend_num(pCmd->valueH,'H');
				USARTsend_num(pCmd->valueL,'L');
#endif
				
				gCmdBuf.WaitValue = pCmd->valueH;
				gCmdBuf.WaitValue = gCmdBuf.WaitValue << 8;
				gCmdBuf.WaitValue |= pCmd->valueL;
				break;
			}
			case SET_RUN: {break;}
		}
	}
}

void commandstorage_wait_interrupt()
{
	if(gCmdBuf.WaitValue != 0) 
		gCmdBuf.WaitValue = --gCmdBuf.WaitValue;					
}	

void commandstorage_init()
{
	/** EEPROM contains FF in every cell after inital start,
	*** so I have to delete the pointer address
	*** otherwise the PIC thinks he has the EEPROM full with commands
	**/
	if (EEPROM_RD(CmdPointerAddr) == 0xff)
		EEPROM_WR(CmdPointerAddr, 0);
	gCmdBuf.LoopMode = 0;
	gCmdBuf.WaitValue = 0;

	// set loop pointer address to start
	EEPROM_WR(CmdLoopPointerAddr, 0);
}

