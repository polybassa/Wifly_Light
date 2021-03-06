/*
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

#include "StartupManager.h"
#include <algorithm>
#include <functional>
#include <cctype>
#include <locale>

namespace WyLight
{
static const uint32_t __attribute__((unused)) g_DebugZones = ZONE_ERROR | ZONE_WARNING | ZONE_INFO | ZONE_VERBOSE;

const std::string StartupManager::StateDescription[StartupManager::NUM_STATES + 1] = {
    "Checking operation mode...",
    "Starting bootloader...",
    "Reading bootloader version...",
    "Reading firmware version...",
    "Updating firmware...\nDon't disconnect!",
    "Starting firmware...",
    "Startup failed!",
    "Startup successful!",
    "Something strange is going on..."
};

StartupManager::StartupManager(const std::function<void(StartupManager::State newState)>& onStateChange) :
    mOnStateChangeCallback(onStateChange) {}

const std::string& StartupManager::getStateDescription(StartupManager::State state)
{
    if ((0 <= state) && (state < StartupManager::NUM_STATES))
        return StateDescription[state];
    return StateDescription[StartupManager::NUM_STATES];
}

void StartupManager::setCurrentState(StartupManager::State newState)
{
    if (mState != newState) {
        if (mOnStateChangeCallback)
            mOnStateChangeCallback(newState);
        mState = newState;
    }
}
void StartupManager::startup(const WyLight::Control& control, const std::string& firmwareFilePath)
{
    if (control.getEndpoint().GetType() == Endpoint::RN171)
        this->startup(dynamic_cast<const WyLight::RN171Control&>(control), firmwareFilePath);
    else if (control.getEndpoint().GetType() == Endpoint::CC3200)
        this->startup(dynamic_cast<const WyLight::CC3200Control&>(control), firmwareFilePath);
}

void StartupManager::startup(const WyLight::CC3200Control& control, const std::string& firmwareFilePath){}

void StartupManager::startup(const WyLight::RN171Control& control, const std::string& firmwareFilePath)
{
    try {
        mHexFileVersion = control.mFirmware->ExtractFwVersion(firmwareFilePath);
    } catch (std::exception& e) {
        throw InvalidParameter("Can not read version string from hexFile");
    }

    //--------------------MODE-CHECK----------------
    try {
        setCurrentState(MODE_CHECK);
        size_t currentMode = control.GetTargetMode();
        if (currentMode == BL_IDENT) {
            setCurrentState(BL_VERSION_CHECK);
            bootloaderVersionCheckUpdate(control, firmwareFilePath);
            return;
        } else if (currentMode == FW_IDENT) {
            setCurrentState(FW_VERSION_CHECK);
            mTargetVersion = control.mFirmware->FwGetVersion();
            if ((mTargetVersion != 0) && (mTargetVersion >= mHexFileVersion)) {
                setCurrentState(STARTUP_SUCCESSFUL);
                return;
            }
        }
    } catch (std::exception& e) {
        // Exception in case of lost connection or old FW-Version without mode check support expected
        // Try to start bootloader, to get in BL-Mode. In BL-Mode, mode check is always supported.
        Trace(ZONE_ERROR, "StartupManager startup failure in state %d: %s\n", mState, e.what());
    }
    setCurrentState(START_BOOTLOADER);
    startBootloader(control, firmwareFilePath);
}

void StartupManager::startBootloader(const WyLight::RN171Control& control, const std::string& firmwareFilePath)
{
    try {
        *control.mFirmware << FwCmdStartBl();
        bootloaderVersionCheckUpdate(control, firmwareFilePath);
    } catch (std::exception& e) {
        // Exception in case of lost connection or wrong mode expected
        Trace(ZONE_ERROR, "StartupManager startup failure in state %d: %s\n", mState, e.what());
        setCurrentState(STARTUP_FAILURE);
    }
}

void StartupManager::bootloaderVersionCheckUpdate(const WyLight::RN171Control& control,
                                                  const std::string&           firmwareFilePath)
{
    try {
        mTargetVersion = control.mBootloader->BlReadFwVersion();
        if ((mTargetVersion == 0) || (mTargetVersion <= mHexFileVersion)) {
            //---- UPDATE STUFF ---------
            setCurrentState(UPDATING);
            control.mBootloader->BlEraseEeprom();
            control.mBootloader->BlProgramFlash(firmwareFilePath);
        }
        setCurrentState(RUN_APP);
        control.mBootloader->BlRunApp();
        control.mConfig->SetParameters(ConfigControl::RN171_BASIC_PARAMETERS) ? setCurrentState(STARTUP_SUCCESSFUL) :
        setCurrentState(STARTUP_FAILURE);
    } catch (std::exception& e) {
        // Expection in case of connection lost or error during update, or invalid update expected.
        // On invalid update, BlRunApp will fail, because firmware is not lanched on target, so no response will be send.
        Trace(ZONE_ERROR, "StartupManager startup failure in state %d: %s\n", mState, e.what());
        setCurrentState(STARTUP_FAILURE);
    }
}

const bool StartupManager::isAppOutdated()
{
    if (mState == STARTUP_SUCCESSFUL)
        return mHexFileVersion < mTargetVersion ? true : false;
    else return false;
}
}
