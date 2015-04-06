/**
                Copyright (C) 2012 - 2014 Nils Weiss, Patrick Bruenn.

    This file is part of WyLight.

    WyLight is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    WyLight is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with WyLight.  If not, see <http://www.gnu.org/licenses/>. */

#include "WiflyControl.h"
#include "crc.h"
#include "trace.h"
#include "MaskBuffer.h"
#include "Version.h"

#include <algorithm>
#include <sstream>
#include <cctype>
#include <stddef.h>
#include <fstream>
#include <iomanip>
#include <cstdlib>
#include <unistd.h>
#include <memory>
#include "intelhexclass.h"
#include "WiflyColor.h"
#include <thread>
#include <chrono>

using std::cout;
using std::hex;
using std::ifstream;

namespace WyLight
{
static const int __attribute__((unused)) g_DebugZones = ZONE_ERROR | ZONE_WARNING | ZONE_INFO | ZONE_VERBOSE;

const std::string Control::LEDS_ALL {"ffffffff"};

const size_t FwCmdScript::INDENTATION_MAX;
const char FwCmdScript::INDENTATION_CHARACTER;

const std::string FwCmdSetFade::TOKEN("fade");
const std::string FwCmdSetGradient::TOKEN("gradient");
const std::string FwCmdLoopOn::TOKEN("loop");
const std::string FwCmdLoopOff::TOKEN("loop_off");
const std::string FwCmdWait::TOKEN("wait");

Control::Control(uint32_t addr, uint16_t port) :
    mBootloader(mProxy),
    mConfig(mTelnet),

    mTcpSock(addr, port),
    mUdpSock(addr, port, false, 0),
    mProxy(mTcpSock),
    mTelnet(mTcpSock)
{}

size_t Control::GetTargetMode(void) const throw(FatalError)
{
    return mProxy.SyncWithTarget();
}

std::string Control::FwGetCycletime(void) throw (ConnectionTimeout, FatalError, ScriptBufferFull)
{
    FwCmdGetCycletime cmd;
    *this << cmd;
    return cmd.mResponse.ToString();
}

void Control::FwGetRtc(tm& timeValue) throw (ConnectionTimeout, FatalError, ScriptBufferFull)
{
    FwCmdGetRtc cmd;
    *this << cmd;
    timeValue = cmd.mResponse.GetRealTime();
}

std::string Control::FwGetTracebuffer(void) throw (ConnectionTimeout, FatalError, ScriptBufferFull)
{
    FwCmdGetTracebuffer cmd;
    *this << cmd;
    return cmd.mResponse.ToString();
}

uint16_t Control::FwGetVersion(void) throw (ConnectionTimeout, FatalError, ScriptBufferFull)
{
    FwCmdGetVersion cmd;
    *this << cmd;
    return cmd.mResponse.getVersion();
}

uint8_t Control::FwGetLedTyp(void) throw (ConnectionTimeout, FatalError, ScriptBufferFull)
{
    FwCmdGetLedTyp cmd;
    *this << cmd;
    return cmd.mResponse.getLedTyp();
}

void Control::FwSend(FwCommand& cmd) const throw (ConnectionTimeout, FatalError, ScriptBufferFull)
{
    if (cmd.IsResponseRequired()) {
        response_frame buffer;
        size_t numCrcRetries = 8;
        do {
            const size_t bytesRead = mProxy.Send(cmd, &buffer, sizeof(buffer));
            TraceBuffer(ZONE_VERBOSE,
                        (uint8_t*)&buffer,
                        bytesRead,
                        "%02x ",
                        "We got %zd bytes response.\nMessage: ",
                        bytesRead);

            if (cmd.GetResponse().Init(buffer, bytesRead))
                return;
        } while (0 < --numCrcRetries);
        throw FatalError(std::string(__FILE__) + ':' + __FUNCTION__ + ": Too many retries");
    } else {
        MaskBuffer maskBuffer {BL_MAX_MESSAGE_LENGTH};
        maskBuffer.Mask(cmd.GetData(), cmd.GetData() + cmd.GetSize(), false);
        if (maskBuffer.Size() != mUdpSock.Send(maskBuffer.Data(), maskBuffer.Size()))
            throw FatalError("mUdpSock.Send() failed");
    }
}

void Control::FwStressTest(void)
{
    *this << FwCmdClearScript {};

    uns8 ledArr[NUM_OF_LED * 3];
    uns8 color = 0;
    while (true) {
        color++;
        std::fill_n(ledArr, sizeof(ledArr), color);
        *this << FwCmdSetColorDirect {ledArr, sizeof(ledArr)};
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

uint16_t Control::ExtractFwVersion(const std::string& pFilename) const
{
    std::ifstream hexFile;
    hexFile.open(const_cast<char*>(pFilename.c_str()), ifstream::in);

    if (!hexFile.good())
        throw FatalError("opening '" + pFilename + "' failed");

    intelhex hexConverter;
    hexFile >> hexConverter;
    union versionUnion {
        uint16_t version;
        uint8_t bytes[2];
    }
    versUnion;

    hexConverter.getData(&versUnion.bytes[1], VERSION_STRING_ORIGIN);
    hexConverter.getData(&versUnion.bytes[0], VERSION_STRING_ORIGIN + 1);

    return ntohs(versUnion.version) > 300 ? 0 : ntohs(versUnion.version);
}

Control& Control::operator<<(FwCommand&& cmd) throw (ConnectionTimeout, FatalError, ScriptBufferFull)
{
    this->FwSend(cmd);
    return *this;
}

Control& Control::operator<<(FwCommand& cmd) throw (ConnectionTimeout, FatalError, ScriptBufferFull)
{
    this->FwSend(cmd);
    return *this;
}

Control& Control::operator<<(const Script& script) throw (ConnectionTimeout, FatalError, ScriptBufferFull)
{
    for (auto it = script.begin(); it != script.end(); ++it) {
        *this << **it;
    }
    return *this;
}

void Control::FwTest(void)
{
#if 0
    static const timespec sleepTime {0, 50000000};
    uint32_t color = 0xff;
    for (size_t i = 0; i < 100; ++i) {
        color = ((color & 0xff) << 24) | (color >> 8);
        *this << FwCmdSetFade {color};
        nanosleep(&sleepTime, NULL);
    }
#else

    *this << FwCmdClearScript() << FwCmdSetFade(WiflyColor::BLACK, 2);

/*		WyLight::Script testScript("test.script");

                testScript.emplace_front(FwCmdLoopOn());
                testScript.emplace_back(FwCmdSetFade(WiflyColor::GREEN, 2000));
                testScript.emplace_back(FwCmdSetFade(WiflyColor::RED, 2000));
                testScript.emplace_back(FwCmdSetFade(WiflyColor::BLUE, 2000));
                testScript.emplace_back(FwCmdLoopOff(5));

 * this << testScript;
 */		/*
                uint32_t bitMask = 0x01;
                for(unsigned int i = 0; i < NUM_OF_LED; i++)
                {
                        ledColor.red((uint8_t)((0xff / NUM_OF_LED) * i));
                        ledColor.green((uint8_t)((0xff / NUM_OF_LED) * i));
                        ledColor.blue(0xff);
                        FwSetFade(setFadeResp, bitMask, ledColor.argb(), 20000 + i * 200, true);
                        bitMask = bitMask << 1;
                }

                testScript.push_back((setWaitResp, 30000);
                FwSetFade(setFadeResp, WiflyColor::GREEN,2000, 0xFFFFFFFF, false);
                FwSetFade(setFadeResp, WiflyColor::RED,  2000, 0x000000FF, true);
                FwSetFade(setFadeResp, WiflyColor::GREEN,2000, 0x0000FF00, true);
                FwSetFade(setFadeResp, WiflyColor::BLUE, 2000, 0x00FF0000, true);
                FwSetFade(setFadeResp, WiflyColor::WHITE,2000, 0xFF000000, false);
                FwSetWait(setWaitResp, 2000);
                FwSetFade(setFadeResp, WiflyColor::BLACK,2000, 0xFFFFFFFF, false);
                FwLoopOff(loopOffResp, 0);*/
#endif
}
} /* namespace WyLight */
