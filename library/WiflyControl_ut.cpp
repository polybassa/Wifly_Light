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

#include "trace.h"
#include "unittest.h"
#include "WiflyControl.h"
#include <string>
#include <stdlib.h>
#include <time.h>

static const uint32_t g_DebugZones = ZONE_ERROR | ZONE_WARNING | ZONE_INFO | ZONE_VERBOSE;

// global variables
uint8_t g_FlashRndDataPool[FLASH_SIZE];
uint8_t g_EepromRndDataPool[EEPROM_SIZE];
// empty wrappers to satisfy the linker
ClientSocket::ClientSocket(uint32_t, uint16_t, int) : mSock(0) {}
ClientSocket::~ClientSocket(void) {}
TcpSocket::TcpSocket(uint32_t addr, uint16_t port) : ClientSocket(addr, port, 0) {}
size_t TcpSocket::Recv(uint8_t* pBuffer, size_t length, timeval* timeout) const { return 0;}
size_t TcpSocket::Send(const uint8_t* frame, size_t length) const {return 0; }
ComProxy::ComProxy(const TcpSocket& sock) : mSock (sock) {}

#define return_resp {\
	for(unsigned int i = 0; i < sizeof(resp); i++)\
	{ \
		if(responseSize >= i) *pResponse++ = resp[i]; \
	}\
	return sizeof(resp);}


int32_t ComProxy::Send(BlRequest& req, uint8_t* pResponse, size_t responseSize, bool doSync) const
{
	if(typeid(req) == typeid(BlInfoRequest))
	{
		uint8_t resp[] = {0x00, 0x03, 0x01, 0x05, 0xff, 0x84, 0x00, 0xfd, 0x00, 0x00};
		return_resp;
	}
	if(typeid(req) == typeid(BlFlashEraseRequest))
	{
		BlFlashEraseRequest* mReq = dynamic_cast<BlFlashEraseRequest*>(&req);
		uint32_t address = ((uint32_t)mReq->addressU) << 16;
		address += ((uint32_t)mReq->addressHigh) << 8;
		address += (uint32_t)mReq->addressLow;
		
		uint16_t pages = (uint16_t)mReq->numPages;
		
		int endaddress = address - (pages * FLASH_ERASE_BLOCKSIZE);
		if(endaddress < 0) endaddress = 0;
		
		for(int i = address; i >= endaddress;  i--)
		{
			g_FlashRndDataPool[i] = 0x00;
		}
		
		uint8_t resp[] = {0x03};
		return_resp;
	}
	if(typeid(req) == typeid(BlFlashWriteRequest))
	{
		BlFlashWriteRequest* mReq = dynamic_cast<BlFlashWriteRequest*>(&req);
		
		uint32_t address = ((uint32_t)mReq->addressU) << 16;
		address += ((uint32_t)mReq->addressHigh) << 8;
		address += (uint32_t)mReq->addressLow;
		
		uint16_t bytes = ((uint16_t)mReq->numBlocksLow) * FLASH_WRITE_BLOCKSIZE;
		
		for (int i = 0; i < bytes; i++, address++) {
			g_FlashRndDataPool[address] = mReq->payload[i];
		}
		
		uint8_t resp[] = {0x04};
		return_resp;
	}
	if(typeid(req) == typeid(BlFlashReadRequest))
	{
		BlFlashReadRequest* mReq = dynamic_cast<BlFlashReadRequest*>(&req);
		
		uint32_t address = ((uint32_t)mReq->addressU) << 16;
		address += ((uint32_t)mReq->addressHigh) << 8;
		address += (uint32_t)mReq->addressLow;
		
		uint16_t bytes = ((uint16_t)mReq->numBytesHigh) << 8;
		bytes += ((uint16_t)mReq->numBytesLow);
		
		unsigned int i;
		for(i = 0; i < bytes; i++)
		{
			*pResponse++ = g_FlashRndDataPool[address + i];
		}
		return i;
	}
	if(typeid(req) == typeid(BlEepromReadRequest))
	{
		BlEepromReadRequest* mReq = dynamic_cast<BlEepromReadRequest*>(&req);
		
		uint32_t address = ((uint32_t)mReq->addressU) << 16;
		address += ((uint32_t)mReq->addressHigh) << 8;
		address += (uint32_t)mReq->addressLow;
		
		uint16_t bytes = ((uint16_t)mReq->numBytesHigh) << 8;
		bytes += ((uint16_t)mReq->numBytesLow);
		
		unsigned int i;
		for(i = 0; i < bytes; i++)
		{
			*pResponse++ = g_EepromRndDataPool[address + i];
		}
		return i;
	}
	if(typeid(req) == typeid(BlEepromWriteRequest))
	{
		BlEepromWriteRequest* mReq = dynamic_cast<BlEepromWriteRequest*>(&req);
		
		uint32_t address = ((uint32_t)mReq->addressU) << 16;
		address += ((uint32_t)mReq->addressHigh) << 8;
		address += (uint32_t)mReq->addressLow;
		
		uint16_t bytes = ((uint16_t)mReq->numBytesHigh) << 8;
		bytes += ((uint16_t)mReq->numBytesLow);
		
		for (int i = 0; i < bytes; i++, address++) {
			g_EepromRndDataPool[address] = mReq->payload[i];
		}
		
		uint8_t resp[] = {0x06};
		return_resp;
		
	}
	return -1;
}

cmd_frame g_SendFrame;
int32_t ComProxy::Send(const cmd_frame* pFrame, response_frame* pResponse, size_t responseSize, bool doSync) const
{
	memcpy(&g_SendFrame, pFrame, sizeof(g_SendFrame));
	pResponse->length = sizeof(uns8) + sizeof(uns16) + sizeof(ErrorCode);
	pResponse->cmd = pFrame->led.cmd;
	pResponse->state = OK;
	return sizeof(g_SendFrame);
}

// wrapper to test WiflyControl
static std::list<std::string> g_TestBuffer;
static bool g_ProxySaved = false;
static bool g_ProxyConnected = false;


TelnetProxy::TelnetProxy(const TcpSocket& sock) : mSock (sock) {}
bool TelnetProxy::Close(bool doSave) const
{
	g_ProxySaved = doSave;
	g_ProxyConnected = false;
	return true;
}

bool TelnetProxy::Open(void) const
{
	return g_ProxyConnected = true;
}

bool TelnetProxy::Send(const std::string& message, const std::string& expectedResponse) const
{
	g_TestBuffer.push_back(message);
	return g_ProxyConnected;
}

bool TelnetProxy::SendString(const std::string& command, std::string value) const
{
	g_TestBuffer.push_back(command);
	g_TestBuffer.push_back(value);
	return g_ProxyConnected;
}


// Testcases
size_t ut_WiflyControl_BlEepromErase(void)
{
	TestCaseBegin();
	
	srand(time(NULL));
	
	/* fill gloabal random data pool */
	for(unsigned int i = 0; i < sizeof(g_EepromRndDataPool); i++)
	{
		g_EepromRndDataPool[i] = (uint8_t) rand() % 255;
	}
	
	WiflyControl testctrl(0,0);
	
	testctrl.BlEepromErase();
	
	for(unsigned int i = 0; i < EEPROM_SIZE; i++)
	{
		CHECK(0xff == g_EepromRndDataPool[i]);
	}
	
	TestCaseEnd();
}

size_t ut_WiflyControl_BlEepromRead(void)
{
	TestCaseBegin();
	srand(time(NULL));
	
	/* fill gloabal random data pool */
	for(unsigned int i = 0; i < sizeof(g_EepromRndDataPool); i++)
		g_EepromRndDataPool[i] = (uint8_t) rand() % 255;
	
	WiflyControl testctrl(0,0);
	
	uint8_t rcvEepromData[EEPROM_SIZE];
	
	size_t rcvBytes = testctrl.BlReadEeprom(rcvEepromData, 0, EEPROM_SIZE);
	
	CHECK(0 == memcmp(g_EepromRndDataPool, rcvEepromData, rcvBytes));
	TestCaseEnd();
}

size_t ut_WiflyControl_BlEepromWrite(void)
{
	TestCaseBegin();
	
	srand(time(NULL));
	
	uint8_t m_EepromRndDataPool[EEPROM_SIZE];
	
	/* fill gloabal random data pool */
	for(unsigned int i = 0; i < sizeof(g_EepromRndDataPool); i++)
	{
		g_EepromRndDataPool[i] = (uint8_t) rand() % 255;
		m_EepromRndDataPool[i] = (uint8_t) rand() % 255;
	}
	
	WiflyControl testctrl(0,0);
	
	testctrl.BlWriteEeprom(0, m_EepromRndDataPool, EEPROM_SIZE);
		
	for(unsigned int i = 0; i < EEPROM_SIZE; i++)
	{
		CHECK(m_EepromRndDataPool[i] == g_EepromRndDataPool[i]);
	}
	
	TestCaseEnd();

}

size_t ut_WiflyControl_BlFlashErase(void)
{
	TestCaseBegin();
	
	srand(time(NULL));
	
	/* fill gloabal random data pool */
	for(unsigned int i = 0; i < sizeof(g_FlashRndDataPool); i++)
		g_FlashRndDataPool[i] = (uint8_t) rand() % 255;
		
	BlInfo blInfo;
	WiflyControl testctrl(0,0);
	testctrl.BlReadInfo(blInfo);
	WiflyControlException *pEx;
	pEx = NULL;
	
	try
	{
		testctrl.BlFlashErase();
	} catch (WiflyControlException &e) {
		pEx = &e;
	}
	CHECK(pEx == NULL);
	
	for(unsigned int i = 0; i < blInfo.GetAddress(); i++)
	{
		CHECK(0 == g_FlashRndDataPool[i]);
	}
	
	TestCaseEnd();
}

size_t ut_WiflyControl_BlFlashRead(void)
{
	TestCaseBegin();
	srand(time(NULL));
	
	/* fill gloabal random data pool */
	for(unsigned int i = 0; i < sizeof(g_FlashRndDataPool); i++)
		g_FlashRndDataPool[i] = (uint8_t) rand() % 255;
	
	WiflyControl testctrl(0,0);
	
	uint8_t rcvFlashData[FLASH_SIZE];
	
	size_t rcvBytes = testctrl.BlReadFlash(rcvFlashData, 0, FLASH_SIZE);
	
	CHECK(0 == memcmp(g_FlashRndDataPool, rcvFlashData, rcvBytes));	
	TestCaseEnd();
}

size_t ut_WiflyControl_BlFlashWrite(void)
{
	TestCaseBegin();
	
	srand(time(NULL));
	
	uint8_t m_FlashRndDataPool[FLASH_SIZE];
	
	/* fill gloabal random data pool */
	for(unsigned int i = 0; i < sizeof(g_FlashRndDataPool); i++)
	{
		g_FlashRndDataPool[i] = (uint8_t) rand() % 255;
		m_FlashRndDataPool[i] = (uint8_t) rand() % 255;
	}
	
	WiflyControl testctrl(0,0);
	
	testctrl.BlWriteFlash(0, m_FlashRndDataPool, sizeof(m_FlashRndDataPool));
	
	for(unsigned int i = 0; i < FLASH_SIZE; i++)
	{
		CHECK(m_FlashRndDataPool[i] == g_FlashRndDataPool[i]);
	}
	
	TestCaseEnd();
	
}

size_t ut_WiflyControl_BlReadInfo(void)
{
	TestCaseBegin();
	WiflyControl testctrl(0,0);
	BlInfo mInfo;
	testctrl.BlReadInfo(mInfo);
	CHECK(mInfo.familyId == 4);
	CHECK(mInfo.versionMajor == 1);
	CHECK(mInfo.versionMinor == 5);
	CHECK(mInfo.zero == 0);
	CHECK(mInfo.sizeHigh == 3);
	CHECK(mInfo.sizeLow == 0);
	CHECK(mInfo.startU == 0);
	CHECK(mInfo.startHigh == 253);
	CHECK(mInfo.startLow == 0);
	CHECK(mInfo.cmdmaskHigh == 255);
	//CHECK(mInfo.cmdmaskLow == 0x84);
	TestCaseEnd();
}

size_t ut_WiflyControl_ConfSetDefaults(void)
{
	TestCaseBegin();
	static const std::string commands[] = {
		"set broadcast interval 1\r\n",    // to support fast broadcast recognition
		"set comm close 0\r\n",            // Disable *CLOS* string
		"set comm open 0\r\n",             // Disable *OPEN* string
		"set comm remote 0\r\n",           // Disable *Hello* string
		"set ip dhcp 1\r\n",               // enable DHCP client
		"set ftp address 169.254.7.57\r\n",// configure localhost as ftp server in ad-hoc connection
		"set ftp pass Pass123\r\n",        // configure ftp password
		"set ftp user roving\r\n",         // configure ftp username
		"set opt deviceid WiFly_Light\r\n",// Set deviceid which appears in broadcastmsg to "Wifly_Light"
		"set uart baud 115200\r\n",        // PIC uart parameter
		"set uart flow 0\r\n",             // PIC uart parameter
		"set uart mode 0\r\n",             // PIC uart parameter
		"set wlan auth 4\r\n",             // use WPA2 protection
		"set wlan join 1\r\n",             // scan for ap and auto join
		"set wlan rate 0\r\n",             // slowest datarate but highest range
		"set wlan tx 12\r\n",              // Set the Wi-Fi transmit power to maximum
	};
	static const size_t numCommands = sizeof(commands) / sizeof(commands[0]);

	WiflyControl testee(0, 0);

	g_TestBuffer.clear();
	CHECK(testee.ConfSetDefaults());
	CHECK(!g_ProxyConnected);
	CHECK(g_ProxySaved);
	CHECK(numCommands == g_TestBuffer.size());
	for(size_t i = 0; i < numCommands; i++)
	{
		CHECK(0 == commands[i].compare(g_TestBuffer.front()));
		g_TestBuffer.pop_front();
		
	}
	TestCaseEnd();
}

size_t ut_WiflyControl_ConfSetWlan(void)
{
	TestCaseBegin();
	static const std::string phraseBase("12345678911234567892123456789312345678941234567895123456789612");
	static const std::string phraseContainsNonAlNum(phraseBase + "\x1f");
	static const std::string phrase(phraseBase + "3");
	static const std::string phraseToLong(phrase + " ");
	static const std::string ssid      ("12345678911234567892123456789312");
	static const std::string ssidToLong(ssid + " ");
	WiflyControl testee(0, 0);
	
	// passphrase to short
	CHECK(!testee.ConfSetWlan("", ssid));

	// passphrase to long
	CHECK(!testee.ConfSetWlan(phraseToLong, ssid));

	// passphrase contains not only alphanumeric characters
	CHECK(!testee.ConfSetWlan(phraseContainsNonAlNum, ssid));

	// ssid to short
	CHECK(!testee.ConfSetWlan(phrase, ""));

	// ssid to long
	CHECK(!testee.ConfSetWlan(phrase, ssidToLong));

	// valid passphrase and ssid
	CHECK(testee.ConfSetWlan(phrase, ssid));
	
	TestCaseEnd();
}

size_t ut_WiflyControl_FwSetColorDirectRedOnly(void)
{
	TestCaseBegin();
	WiflyControl testee(0, 0);
	SimpleResponse response(SET_COLOR_DIRECT);
	
	// three leds only, first red, second green last blue
	uint8_t shortBuffer[1]{0xff};
	cmd_frame expectedOutgoingFrame;
	expectedOutgoingFrame.led.cmd = SET_COLOR_DIRECT;
	expectedOutgoingFrame.length = 2 + sizeof(cmd_set_color_direct);	
	memcpy(expectedOutgoingFrame.led.data.set_color_direct.ptr_led_array, shortBuffer, sizeof(shortBuffer));
	memset(expectedOutgoingFrame.led.data.set_color_direct.ptr_led_array + sizeof(shortBuffer), 0x00, sizeof(cmd_set_color_direct) - sizeof(shortBuffer));

	testee.FwSetColorDirect(response, shortBuffer, sizeof(shortBuffer));

	CHECK(response.IsValid());
	CHECK(0 == memcmp(&g_SendFrame, &expectedOutgoingFrame, expectedOutgoingFrame.length));
	TestCaseEnd();
}

size_t ut_WiflyControl_FwSetColorDirectThreeLeds(void)
{
	TestCaseBegin();
	WiflyControl testee(0, 0);
	SimpleResponse response(SET_COLOR_DIRECT);
	
	// three leds only, first red, second green last blue
	uint8_t shortBuffer[3 * 3];
	memset(shortBuffer, 0, sizeof(shortBuffer));
	shortBuffer[0 * 3 + 0] = 0xff; //first led red
	shortBuffer[1 * 3 + 1] = 0xff; //second led green
	shortBuffer[2 * 3 + 2] = 0xff; //third led blue
	cmd_frame expectedOutgoingFrame;
	expectedOutgoingFrame.led.cmd = SET_COLOR_DIRECT;
	expectedOutgoingFrame.length = 2 + sizeof(cmd_set_color_direct);	
	memcpy(expectedOutgoingFrame.led.data.set_color_direct.ptr_led_array, shortBuffer, sizeof(shortBuffer));
	memset(expectedOutgoingFrame.led.data.set_color_direct.ptr_led_array + sizeof(shortBuffer), 0x00, sizeof(cmd_set_color_direct) - sizeof(shortBuffer));

	testee.FwSetColorDirect(response, shortBuffer, sizeof(shortBuffer));

	CHECK(response.IsValid());
	CHECK(0 == memcmp(&g_SendFrame, &expectedOutgoingFrame, expectedOutgoingFrame.length));
	TestCaseEnd();
}

size_t ut_WiflyControl_FwSetColorDirectToMany(void)
{
	TestCaseBegin();
	WiflyControl testee(0, 0);
	SimpleResponse response(SET_COLOR_DIRECT);
	
	// three leds only, first red, second green last blue
	uint8_t shortBuffer[2*NUM_OF_LED * 3];
	memset(shortBuffer, 0xff, sizeof(shortBuffer));
	cmd_frame expectedOutgoingFrame;
	expectedOutgoingFrame.led.cmd = SET_COLOR_DIRECT;
	expectedOutgoingFrame.length = 2 + sizeof(cmd_set_color_direct);	
	memcpy(expectedOutgoingFrame.led.data.set_color_direct.ptr_led_array, shortBuffer, sizeof(shortBuffer));

	testee.FwSetColorDirect(response, shortBuffer, sizeof(shortBuffer));

	CHECK(response.IsValid());
	CHECK(0 == memcmp(&g_SendFrame, &expectedOutgoingFrame, expectedOutgoingFrame.length));
	TestCaseEnd();
}

size_t ut_WiflyControl_FwSetFade(void)
{
	cmd_frame expectedOutgoingFrame;
	expectedOutgoingFrame.length = 2 + sizeof(cmd_set_fade);
	expectedOutgoingFrame.led.cmd = SET_FADE;
	expectedOutgoingFrame.led.data.set_fade.addr[0] = 0xff;
	expectedOutgoingFrame.led.data.set_fade.addr[1] = 0xff;
	expectedOutgoingFrame.led.data.set_fade.addr[2] = 0xff;
	expectedOutgoingFrame.led.data.set_fade.addr[3] = 0xff;
	expectedOutgoingFrame.led.data.set_fade.red = 0xff;
	expectedOutgoingFrame.led.data.set_fade.green = 0x00;
	expectedOutgoingFrame.led.data.set_fade.blue = 0xff;
	expectedOutgoingFrame.led.data.set_fade.parallelFade = 0x00;
	//TODO why do we use fadeTmms == 1 for SetColor?
	expectedOutgoingFrame.led.data.set_fade.fadeTmms = htons(0x0001);

	TestCaseBegin();
	WiflyControl testee(0, 0);
	SimpleResponse response(SET_FADE);
	
	// set color
	testee.FwSetFade(response, "ff00ff");
	CHECK(response.IsValid());
	TraceBuffer(ZONE_INFO, &g_SendFrame, expectedOutgoingFrame.length, "%02x ", "IS  :");
	TraceBuffer(ZONE_INFO, &expectedOutgoingFrame, expectedOutgoingFrame.length, "%02x ", "SOLL:");
	CHECK(0 == memcmp(&g_SendFrame, &expectedOutgoingFrame, expectedOutgoingFrame.length));
	
	
	TestCaseEnd();
}

int main (int argc, const char* argv[])
{
	UnitTestMainBegin();
	RunTest(true, ut_WiflyControl_ConfSetDefaults);
	RunTest(true, ut_WiflyControl_ConfSetWlan);
	RunTest(true, ut_WiflyControl_BlReadInfo);
	RunTest(true, ut_WiflyControl_BlFlashErase);
	RunTest(true, ut_WiflyControl_BlFlashRead);
	RunTest(true, ut_WiflyControl_BlFlashWrite);
	RunTest(true, ut_WiflyControl_BlEepromRead);
	RunTest(true, ut_WiflyControl_BlEepromWrite);
	RunTest(true, ut_WiflyControl_BlEepromErase);
	RunTest(true, ut_WiflyControl_FwSetColorDirectRedOnly);
	RunTest(true, ut_WiflyControl_FwSetColorDirectThreeLeds);
	RunTest(true, ut_WiflyControl_FwSetColorDirectToMany);
	RunTest(true, ut_WiflyControl_FwSetFade);
	UnitTestMainEnd();
}

