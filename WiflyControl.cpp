/**
		Copyright (C) 2012, 2013 Nils Weiss, Patrick Bruenn.

    This file is part of Wifly_Light.

    Wifly_Light is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Wifly_Light is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Wifly_Light.  If not, see <http://www.gnu.org/licenses/>. */

#include "WiflyControl.h"
#include "crc.h"
#include "trace.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>
#include <stddef.h>
#include <fstream>
#include <iomanip>
#include <cstdlib>
#include <unistd.h>
#include "intelhexclass.h"

#include "WiflyControlColorClass.h"

using std::cout;
using std::ifstream;
using std::hex;
using std::stringstream;

static const int g_DebugZones = ZONE_ERROR | ZONE_WARNING | ZONE_INFO | ZONE_VERBOSE;

/**
 * Macro to reduce code redundancy, while converting two 32 bit values into
 * an address array and red, green, blue values. 
 */
#define SetAddrRgb(REF, ADDRESS, RGBA) { \
	REF.addr[3] = (ADDRESS & 0xff000000) >> 24; \
	REF.addr[2] = (ADDRESS & 0x00ff0000) >> 16; \
	REF.addr[1] = (ADDRESS & 0x0000ff00) >> 8; \
	REF.addr[0] = (ADDRESS & 0x000000ff); \
	REF.red = (RGBA & 0xff000000) >> 24; \
	REF.green = (RGBA & 0x00ff0000) >> 16; \
	REF.blue = (RGBA & 0x0000ff00) >> 8; \
}


WiflyControl::WiflyControl(uint32_t addr, uint16_t port)
: mSock(addr, port), mProxy(mSock), mTelnet(mSock)
{
	//TODO remove length
	mCmdFrame.length = (uns8)sizeof(struct cmd_set_color) + 2;
}

/** --------------------------------------- BOOTLOADER METHODES --------------------------------------- **/
size_t WiflyControl::BlFlashErase(unsigned char* pBuffer, unsigned int endAddress, const size_t numPages, bool doSync) const
{
	BlFlashEraseRequest request(endAddress, numPages);
	// we expect only one byte as response, the command code 0x03
	return BlRead(request, pBuffer, 1, doSync);
}

bool WiflyControl::BlFlashErase(void) const
{
	BlInfo info;
	if(sizeof(info) != BlReadInfo(info))
	{
		cout << __FILE__ << "::" << __FUNCTION__
		<< "(): Erase flash failed, couldn't determine bootloader location" << endl;
		return FALSE;
	}
	const unsigned int firstAddress = info.GetAddress() - 1;
	unsigned int address = firstAddress;
	
	unsigned char buffer[BL_MAX_MESSAGE_LENGTH];
	size_t bytesRead;
	while(address > FLASH_ERASE_BLOCKSIZE * FLASH_ERASE_BLOCKS)
	{
		// force SYNC only for erase command
		bytesRead = BlFlashErase(&buffer[0], address, FLASH_ERASE_BLOCKS, true);
#ifdef DEBUG
		cout << __FILE__ << "::" << __FUNCTION__
		 << "(): Erase at " << hex << address << endl;
#endif
		 if((bytesRead < 1) || (0x03 != buffer[0])) 
		 {
		      cout << __FILE__ << "::" << __FUNCTION__
		      << "():Erase flash failed at address: " << hex << address << endl;
		      return FALSE; 
		 }
		 address -= FLASH_ERASE_BLOCKSIZE * FLASH_ERASE_BLOCKS;
	}
	/* now we erased everything until a part of the flash smaller than FLASH_ERASE_BLOCKS * FLASH_ERASE_BLOCKSIZE
	 * so we set our startaddress at the beginning of this block and erase */
	bytesRead = BlFlashErase(&buffer[0], FLASH_ERASE_BLOCKS * FLASH_ERASE_BLOCKSIZE -1, FLASH_ERASE_BLOCKS, true);
	if((bytesRead < 1) || (0x03 != buffer[0])) 
	{		    
		  cout << __FILE__ << "::" << __FUNCTION__
		  << "():Erase flash failed at address: " << hex << 0 << endl;
		  return FALSE; 
	}
	return TRUE;
}

bool WiflyControl::BlEepromErase(void) const
{
	unsigned char buffer[EEPROM_SIZE];
	
	for(unsigned int i = 0; i < sizeof(buffer); i++)
	{
	    buffer[i] = 0xff;
	}
	return BlWriteEeprom((unsigned int)0, &buffer[0], sizeof(buffer));
	
}

size_t WiflyControl::BlRead(BlRequest& req, unsigned char* pResponse, const size_t responseSize, bool doSync) const
{
	unsigned char buffer[BL_MAX_MESSAGE_LENGTH];
	size_t bytesReceived = mProxy.Send(req, buffer, sizeof(buffer), doSync);

#ifdef DEBUG
	cout << __FILE__ << "::" << __FUNCTION__ << "(): " << bytesReceived << ":" << sizeof(BlInfo) << endl;
#endif
	if(responseSize == bytesReceived)
	{
		memcpy(pResponse, buffer, responseSize);
		return responseSize;
	}
	return 0;
}

size_t WiflyControl::BlReadCrcFlash(unsigned char* pBuffer, unsigned int address, size_t numBlocks) const
{
	if(numBlocks * FLASH_ERASE_BLOCKSIZE + address > FLASH_SIZE)
	{
		cout << __FILE__ << "::" << __FUNCTION__
		<< "(): can not performe crc outside the flash" << endl;
		return false;
	}
  
	size_t bytesRead;
	size_t sumBytesRead = 0;
	while(numBlocks > FLASH_CRC_BLOCKSIZE)
	{
		BlFlashCrc16Request readRequest(address, FLASH_CRC_BLOCKSIZE);
		bytesRead = BlRead(readRequest, pBuffer, FLASH_CRC_BLOCKSIZE * 2);
		sumBytesRead += bytesRead;
		if(FLASH_CRC_BLOCKSIZE * 2 != bytesRead)
		{
			cout << __FILE__ << "::" << __FUNCTION__
			<< "(): only " << bytesRead << " bytes read not " << FLASH_CRC_BLOCKSIZE * 2 << endl;
			return sumBytesRead;
		}
		address += (FLASH_CRC_BLOCKSIZE * FLASH_ERASE_BLOCKSIZE);
		numBlocks -= FLASH_CRC_BLOCKSIZE;
		pBuffer += FLASH_CRC_BLOCKSIZE * 2;
	}

	BlFlashCrc16Request readRequest(address, numBlocks);
	bytesRead = BlRead(readRequest, pBuffer, numBlocks * 2);
	sumBytesRead += bytesRead;
	if(numBlocks * 2 != bytesRead)
	{
		cout << __FILE__ << "::" << __FUNCTION__
		<< "(): only " << bytesRead << " bytes read not " << numBlocks * 2 << endl;
	}
	return sumBytesRead;
}

size_t WiflyControl::BlReadEeprom(unsigned char* pBuffer, unsigned int address, size_t numBytes) const
{
	if(numBytes + address > EEPROM_SIZE)
	{
		cout << __FILE__ << "::" << __FUNCTION__
		<< "(): can not performe read outside the eeprom" << endl;
		return 0;
	}
	
	size_t bytesRead;
	size_t sumBytesRead = 0;
	BlEepromReadRequest readRequest;
	while(numBytes > EEPROM_READ_BLOCKSIZE)
	{
		readRequest.SetAddressNumBytes(address, EEPROM_READ_BLOCKSIZE);
		bytesRead = BlRead(readRequest, pBuffer, EEPROM_READ_BLOCKSIZE);
		sumBytesRead += bytesRead;
		if(EEPROM_READ_BLOCKSIZE != bytesRead)
		{
			cout << __FILE__ << "::" << __FUNCTION__
			<< "(): only " << bytesRead << " bytes read not " << EEPROM_READ_BLOCKSIZE << endl;
			return sumBytesRead;
		}
		address += EEPROM_READ_BLOCKSIZE;
		numBytes -= EEPROM_READ_BLOCKSIZE;
		pBuffer += EEPROM_READ_BLOCKSIZE;
	}
	readRequest.SetAddressNumBytes(address, numBytes);
	bytesRead = BlRead(readRequest, pBuffer, numBytes);
	sumBytesRead += bytesRead;
	if(numBytes != bytesRead)
	{
		cout << __FILE__ << "::" << __FUNCTION__
		<< "(): only " << bytesRead << " bytes read not " << numBytes << endl;
	}
	return sumBytesRead;
}

size_t WiflyControl::BlReadFlash(unsigned char* pBuffer, unsigned int address, size_t numBytes) const
{
	if(numBytes + address > FLASH_SIZE)
	{
		cout << __FILE__ << "::" << __FUNCTION__
		<< "(): can not performe read outside the flash" << endl;
		return 0;
	}
  
	size_t bytesRead;
	size_t sumBytesRead = 0;
	BlFlashReadRequest readRequest;
	while(numBytes > FLASH_READ_BLOCKSIZE)
	{
		readRequest.SetAddressNumBytes(address, FLASH_READ_BLOCKSIZE);
		bytesRead = BlRead(readRequest, pBuffer, FLASH_READ_BLOCKSIZE);
		sumBytesRead += bytesRead;
		if(FLASH_READ_BLOCKSIZE != bytesRead)
		{
			cout << __FILE__ << "::" << __FUNCTION__
			<< "(): only " << bytesRead << " bytes read not " << FLASH_READ_BLOCKSIZE << endl;
			return sumBytesRead;
		}
		address += FLASH_READ_BLOCKSIZE;
		numBytes -= FLASH_READ_BLOCKSIZE;
		pBuffer += FLASH_READ_BLOCKSIZE;
	}

	readRequest.SetAddressNumBytes(address, numBytes);
	bytesRead = BlRead(readRequest, pBuffer, numBytes);
	sumBytesRead += bytesRead;
	if(numBytes != bytesRead)
	{
		cout << __FILE__ << "::" << __FUNCTION__
		<< "(): only " << bytesRead << " bytes read not " << numBytes << endl;
	}
	return sumBytesRead;
}

size_t WiflyControl::BlReadInfo(BlInfo& blInfo) const
{
	BlInfoRequest request;
	return BlRead(request, reinterpret_cast<unsigned char*>(&blInfo), sizeof(BlInfo));
}

bool WiflyControl::BlWriteFlash(unsigned int address, unsigned char* pBuffer, size_t bufferLength) const
{
	if(bufferLength + address > FLASH_SIZE)
	{
		cout << __FILE__ << "::" << __FUNCTION__
		<< "(): can not performe write outside the flash" << endl;
		return false;
	}
  
	BlFlashWriteRequest request;
	unsigned char response;
	
	size_t bytesLeft = bufferLength;
	while(bytesLeft > FLASH_WRITE_BLOCKSIZE)
	{
		request.SetData(address, pBuffer, FLASH_WRITE_BLOCKSIZE);

		// we expect only the 0x04 command byte as response
		if(1 != BlRead(request, &response, sizeof(response)))
		{
			return false;
		}
		
		address += FLASH_WRITE_BLOCKSIZE;
		pBuffer += FLASH_WRITE_BLOCKSIZE;
		bytesLeft -= FLASH_WRITE_BLOCKSIZE;
	}

	request.SetData(address, pBuffer, bytesLeft);
	return (1 == BlRead(request, &response, sizeof(response)));
}

bool WiflyControl::BlWriteEeprom(unsigned int address, unsigned char* pBuffer, size_t bufferLength) const
{
	if(bufferLength + address > EEPROM_SIZE)
	{
		cout << __FILE__ << "::" << __FUNCTION__
		<< "(): can not performe write outside the eeprom" << endl;
		return false;
	}
      
	BlEepromWriteRequest request;
	unsigned char response;
	
	size_t bytesLeft = bufferLength;
	while(bytesLeft > EEPROM_WRITE_BLOCKSIZE)
	{
		request.SetData(address, pBuffer, EEPROM_WRITE_BLOCKSIZE);

		// we expect only the 0x06 command byte as response
		if(1 != BlRead(request, &response, sizeof(response)))
		{
			return false;
		}
		
		address += EEPROM_WRITE_BLOCKSIZE;
		pBuffer += EEPROM_WRITE_BLOCKSIZE;
		bytesLeft -= EEPROM_WRITE_BLOCKSIZE;
	}

	request.SetData(address, pBuffer, bytesLeft);
	return (1 == BlRead(request, &response, sizeof(response)));
}

bool WiflyControl::BlProgramFlash(const std::string& pFilename)
{
	cout << endl << __FILE__ << "::" << __FUNCTION__ << "(" << pFilename << ")" << endl;
	cout << "opening '" << pFilename << "' ...";

	std::ifstream hexFile;
	hexFile.open(const_cast<char*>(pFilename.c_str()), ifstream::in);
	
	if(!hexFile.good())
	{
		cout << "failed!" << endl;
		return false;
	}
	cout << "done." << endl;

	
	intelhex hexConverter;
	hexFile >> hexConverter;
	
	BlInfo info;
	if(sizeof(info) != BlReadInfo(info))
	{
		cout << __FILE__ << "::" << __FUNCTION__ << "(" << pFilename << ")"
		<< ": can not read bootloader info from traget device" << endl;
		return false;
	}
	
	/*Check if last address of programmcode is not in the bootblock */
	
	unsigned long endAddress;
	
	if(hexConverter.endAddress(&endAddress))
	{
	    if(endAddress >= (unsigned long)(info.GetAddress()))
	    {
		  cout << __FILE__ << "::" << __FUNCTION__ << "(" << pFilename << ")"
		  << ": endaddress of program code is in bootloader area of the target device flash" << endl;
		  return false;
	    }
	}
	else
	{
	    cout << __FILE__ << "::" << __FUNCTION__ << "(" << pFilename << ")"
	    << ": can not read endAddress from hexConverter" << endl;
	    return false;
	}
	
	unsigned short word1, word2;
	unsigned int bootAddress;
	unsigned char resetVector[4];
	unsigned char appVector[4];
	
	/* Calculate the resetVector */
	bootAddress = (info.GetAddress() + 2) / 2;
	
	word1 = 0xEF00 | (bootAddress & 0xff);
	word2 = 0xF000 | ((bootAddress >> 8) & 0x0FFF);
	
	resetVector[0] = (unsigned char)word1;
	resetVector[1] = (unsigned char)(word1 >> 8);
	resetVector[2] = (unsigned char)word2;
	resetVector[3] = (unsigned char)(word2 >> 8);
	
	/*Put AppVektor from the beginning of hexfile at the startaddress of the bootloader */
	
	hexConverter.begin();
	if(hexConverter.currentAddress() != 0)
	{
		cout << __FILE__ << "::" << __FUNCTION__ << "(" << pFilename << ")"
		<< ": program code does not start at address 0x0000" << endl;
		return false;
	}
	
	for(unsigned int i = 0; i < sizeof(appVector); i++)
	{
		if(!hexConverter.getData(&appVector[i],(unsigned long)i))
		{
		    cout << __FILE__ << "::" << __FUNCTION__ << "(" << pFilename << ")"
		    << ": can not read data at address 0x" << hexConverter.currentAddress() << endl;
		    return false;
		}
	}
	
	if(!BlEnableAutostart())
	{
		cout << __FILE__ << "::" << __FUNCTION__ << "(" << pFilename << ")"
		<< ": can not enable bootloader autostart!" << endl;
		return false;
	}
	
	if(!BlFlashErase())
	{
		cout << __FILE__ << "::" << __FUNCTION__ << "(" << pFilename << ")"
		<< ": can not erase target device flash!" << endl;
		return false;
	}
	
	unsigned int writeAddress = 0;
	unsigned char flashBuffer[FLASH_SIZE];
	unsigned char nextByte;
	
	if(endAddress > FLASH_SIZE)
	{
		cout << __FILE__ << "::" << __FUNCTION__ << "(" << pFilename << ")"
		<< ": endaddress of program code is outside the target device flash" << endl;
		return false;
	}
	
	/* Write the resetVector to temporary flashBuffer*/
	memcpy(&flashBuffer[0], &resetVector[0], sizeof(resetVector));
	
	writeAddress += sizeof(resetVector);
	
	for(;writeAddress <= (unsigned int)endAddress; writeAddress++)
	{
		if(hexConverter.getData(&nextByte, writeAddress))
		{
		    flashBuffer[writeAddress] = nextByte;
		}
		else
		{
		    flashBuffer[writeAddress] = 0xff;	
		} 
	}
	if(!BlWriteFlash(0, &flashBuffer[0], (size_t)endAddress+1))
	{
		cout << __FILE__ << "::" << __FUNCTION__ << "(" << pFilename << ")"
		<< ": writing program code to target device flash failed!" << endl;
		return false;
	}
	
	/* we always have to write a FLASH_WRITE Block when we wanna write to device flash,
	 * so we have to pack the appVector at the end of a Block of data */
	
	unsigned char appVecBuf[FLASH_WRITE_BLOCKSIZE];
	
	for(int i = 0; i < FLASH_WRITE_BLOCKSIZE; i++)
	{
		appVecBuf[i] = 0xff;
	}
	
	memcpy(&appVecBuf[sizeof(appVecBuf) - sizeof(appVector)], &appVector[0], sizeof(appVector)); 
	
	if(!BlWriteFlash(info.GetAddress() - FLASH_WRITE_BLOCKSIZE, &appVecBuf[0], FLASH_WRITE_BLOCKSIZE))
	{
		cout << __FILE__ << "::" << __FUNCTION__ << "(" << pFilename << ")"
		<< ": writing application startvector to target device flash failed!" << endl;
		return false;
	}
	
	return true;
}

bool WiflyControl::BlRunApp(void) const
{
	BlRunAppRequest request;
	unsigned char buffer[32];
	size_t bytesRead = BlRead(request, &buffer[0], 6);
	
	Trace(ZONE_VERBOSE, "We got %d bytes response.\n", bytesRead);
	TraceBuffer(ZONE_VERBOSE, (uint8_t*)&buffer[0], bytesRead, "%02x ", "Message: ");
	
	if(4 <= bytesRead)
	{
		struct response_frame *pResponse = (response_frame*)&buffer[0];
		if(pResponse->cmd == FW_STARTED) return true;
	}
	return false;
}

bool WiflyControl::BlEnableAutostart(void) const
{
	unsigned char value = 0xff;
	
	return BlWriteEeprom((unsigned int)BL_AUTOSTART_ADDRESS, &value, sizeof(value));
}

std::string WiflyControl::ConfGetSsid(void) const
{
	return "HUHU";
}

bool WiflyControl::ConfSetDefaults(void) const
{
	static const std::string commands[] = {
		"set broadcast interval 1\r\n",   // to support fast broadcast recognition
		"set uart baud 115200\r\n",       // PIC uart parameter
		"set uart flow 0\r\n",            // PIC uart parameter
		"set uart mode 0\r\n",            // PIC uart parameter
		"set wlan rate 0\r\n",            // slowest datarate but highest range
	};

	if(!mTelnet.Open())
	{
		Trace(ZONE_ERROR, "open telnet connection failed\n");
		return false;
	}

	for(size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++)
	{
		if(!mTelnet.Send(commands[i]))
		{
			Trace(ZONE_ERROR, "command: '%s' failed -> exit without saving\n", commands[i].data());
			return mTelnet.Close(false);
		} 
	}
	return mTelnet.Close(true);
}

bool WiflyControl::ConfSetWlan(const std::string& phrase, const std::string& ssid) const
{
	static const size_t PHRASE_MAX = 63;
	static const size_t SSID_MAX = 32;

	if((phrase.size() < 1) || (phrase.size() > PHRASE_MAX) || 0 != std::count_if(phrase.begin(), phrase.end(), iscntrl))
	{
		Trace(ZONE_WARNING, "Invalid wlan passphrase '%s'\n", phrase.data());
		return false;
	}
	
	if((ssid.size() < 1) || (ssid.size() > SSID_MAX))
	{
		Trace(ZONE_WARNING, "Invalid wlan ssid '%s'\n", phrase.data());
		return false;
	}

	if(!mTelnet.Open())
	{
		Trace(ZONE_ERROR, "open telnet connection failed\n");
		return false;
	}

	if(!mTelnet.SendString("set wlan phrase ", phrase))
	{
		Trace(ZONE_ERROR, "set wlan phrase to '%s' failed\n", phrase.data());
		mTelnet.Close(false);
		return false;
	}

	if(!mTelnet.SendString("set wlan ssid ", ssid))
	{
		Trace(ZONE_ERROR, "set wlan ssid to '%s' failed\n", ssid.data());
		mTelnet.Close(false);
		return false;
	}
	return mTelnet.Close(true);
}

bool WiflyControl::FwSend(struct cmd_frame* pFrame, size_t length, WiflyResponse& response) const
{
	pFrame->length = length + 2; //add cmd and length byte
	unsigned char buffer[512];
	int bytesRead = mProxy.Send(&mCmdFrame, &buffer[0], sizeof(buffer), false);
	
	Trace(ZONE_VERBOSE, "We got %d bytes response.\n", bytesRead);
	TraceBuffer(ZONE_VERBOSE, (uint8_t*)&buffer[0], bytesRead, "%02x ", "Message: ");
	
	response.Init((response_frame*)&buffer[0], bytesRead);
	return response.IsValid();
}

bool WiflyControl::FwClearScript(WiflyResponse& response)
{
	mCmdFrame.led.cmd = CLEAR_SCRIPT;
	return FwSend(&mCmdFrame, 0, response);
}

bool WiflyControl::FwLoopOn(WiflyResponse& response)
{
	mCmdFrame.led.cmd = LOOP_ON;
	return FwSend(&mCmdFrame, 0, response);
}

bool WiflyControl::FwLoopOff(WiflyResponse& response, unsigned char numLoops)
{
	mCmdFrame.led.cmd = LOOP_OFF;
	mCmdFrame.led.data.loopEnd.numLoops = numLoops;
	
   	return FwSend(&mCmdFrame, sizeof(cmd_set_fade), response);
}

bool WiflyControl::FwSetColorDirect(WiflyResponse& response, unsigned char* pBuffer, size_t bufferLength)
{
	if(pBuffer == NULL) return false; //TODO should be throw exceptiong wrong parameter
  
	mCmdFrame.led.cmd = SET_COLOR_DIRECT;
	for(unsigned int i = 0; i < NUM_OF_LED * 3; i++)
	{
	    if(i < bufferLength)
	    {
		mCmdFrame.led.data.set_color_direct.ptr_led_array[i] = *pBuffer++;
	    }
	    else
	    {
		mCmdFrame.led.data.set_color_direct.ptr_led_array[i] = 0;
	    }
	}
	return FwSend(&mCmdFrame, sizeof(struct cmd_set_color_direct),response);
}

bool WiflyControl::FwSetFade(WiflyResponse& response, unsigned long addr, unsigned long rgba, unsigned short fadeTmms, bool parallelFade)
{
	/*calibrate fadeTmms */
	fadeTmms = (unsigned short)(fadeTmms / 10);
	if(fadeTmms == 0) fadeTmms = 1;
	
	mCmdFrame.led.cmd = SET_FADE;
	SetAddrRgb(mCmdFrame.led.data.set_fade, addr, rgba);
	mCmdFrame.led.data.set_fade.fadeTmms = htons(fadeTmms);
	mCmdFrame.led.data.set_fade.parallelFade = parallelFade;

	return FwSend(&mCmdFrame, sizeof(cmd_set_fade), response);
}

bool WiflyControl::FwSetFade(WiflyResponse& response, string& addr, string& rgba, unsigned short fadeTmms, bool parallelFade)
{
	return FwSetFade(response, ToRGBA(addr), ToRGBA(rgba) << 8, fadeTmms, parallelFade);
}

bool WiflyControl::FwSetWait(WiflyResponse& response, unsigned short waitTmms)
{
	mCmdFrame.led.cmd = WAIT;
	mCmdFrame.led.data.wait.waitTmms = htons((unsigned short)(waitTmms / 10));
	
	return FwSend(&mCmdFrame, sizeof(cmd_set_fade), response);
}

void WiflyControl::FwTest(void)
{
	SimpleResponse clrResp(CLEAR_SCRIPT);
	SimpleResponse loopOffResp(LOOP_OFF);
	SimpleResponse loopOnResp(LOOP_ON);
	SimpleResponse setFadeResp(SET_FADE);
	SimpleResponse setWaitResp(WAIT);
	
	WiflyControlColorClass LedColor = WiflyControlColorClass(0xffffffff);
      
	static const unsigned long RED   = 0xFF000000;
	static const unsigned long GREEN = 0x00FF0000;
	static const unsigned long BLUE  = 0x0000FF00;
	static const unsigned long WHITE = 0xFFFFFF00;
	static const unsigned long BLACK = 0x00000000;
	
	FwClearScript(clrResp);
	FwLoopOn(loopOnResp);
	FwSetFade(setFadeResp, 0xFFFFFFFFLU, RED, 2000, false);
	

	/*uint32_t bitMask = 0x01;
	for(unsigned int i = 0; i < NUM_OF_LED; i++)
	{
		LedColor.red((uint8_t)((0xff / NUM_OF_LED) * i));
		LedColor.green((uint8_t)((0xff / NUM_OF_LED) * i));
		LedColor.blue(0xff);
		FwSetFade(setFadeResp, bitMask, LedColor.rgba(), 20000 + i * 200, true);
		bitMask = bitMask << 1;
	}*/
	FwSetWait(setWaitResp, 30000);
	FwSetFade(setFadeResp, 0xFFFFFFFFLU, GREEN,2000, false);
	FwSetFade(setFadeResp, 0x000000FFLU, RED,  2000, true);
	FwSetFade(setFadeResp, 0x0000FF00LU, GREEN,2000, true);
	FwSetFade(setFadeResp, 0x00FF0000LU, BLUE, 2000, true);
	FwSetFade(setFadeResp, 0xFF000000LU, WHITE,2000, false);
	FwSetWait(setWaitResp, 2000);
	FwSetFade(setFadeResp, 0xFFFFFFFFLU, BLACK,2000, false);
	FwLoopOff(loopOffResp, 0);
}

bool WiflyControl::FwPrintCycletime(WiflyResponse& response)
{
	mCmdFrame.led.cmd = GET_CYCLETIME;
	return FwSend(&mCmdFrame, 0, response);
}

bool WiflyControl::FwPrintTracebuffer(WiflyResponse& response)
{
	mCmdFrame.led.cmd = GET_TRACE;
	return FwSend(&mCmdFrame, 0, response);
}

bool WiflyControl::FwPrintFwVersion(WiflyResponse& response)
{
	mCmdFrame.led.cmd = GET_FW_VERSION;
	return FwSend(&mCmdFrame, 0, response);
}


bool WiflyControl::FwStartBl(WiflyResponse& response)
{
	mCmdFrame.led.cmd = START_BL;
	return FwSend(&mCmdFrame, 0, response);
}

bool WiflyControl::FwSetRtc(WiflyResponse& response, struct tm* timeValue)
{
	if(timeValue == NULL) return false; //TODO throw wrong parameter exception
  
  	mCmdFrame.led.cmd = SET_RTC;
	mCmdFrame.led.data.set_rtc.tm_sec  = (uns8) timeValue->tm_sec;
	mCmdFrame.led.data.set_rtc.tm_min  = (uns8) timeValue->tm_min;
	mCmdFrame.led.data.set_rtc.tm_hour = (uns8) timeValue->tm_hour;
	mCmdFrame.led.data.set_rtc.tm_mday = (uns8) timeValue->tm_mday;
	mCmdFrame.led.data.set_rtc.tm_mon  = (uns8) timeValue->tm_mon;
	mCmdFrame.led.data.set_rtc.tm_year = (uns8) timeValue->tm_year;
	mCmdFrame.led.data.set_rtc.tm_wday = (uns8) timeValue->tm_wday;
	
	return FwSend(&mCmdFrame, sizeof(struct rtc_time),response);
}

bool WiflyControl::FwGetRtc(WiflyResponse& response)
{    
	mCmdFrame.led.cmd = GET_RTC;
	return FwSend(&mCmdFrame, 0, response);
	
}

unsigned long WiflyControl::ToRGBA(string& s) const
{
	if(s.length() > 8) return 0;

	// use a stringstream to convert hex ascii string into machine bits
	unsigned long rgba;
	stringstream converter;
	converter << hex << s;
	converter >> rgba;
	return rgba;
}

