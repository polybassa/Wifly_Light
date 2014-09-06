/*
 Copyright (C) 2014 Nils Weiss, Patrick Bruenn.

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

#ifndef _BOOTLOADER_H_
#define _BOOTLOADER_H_
extern const unsigned long BOOTLOADER_VERSION;

#ifndef BUILD_AS_FW

#define FW_FILENAME			"/temp/firmware.bin"
#define APP_NAME			"WyLight Bootloader"
#define FIRMWARE_ORIGIN 	0x20012000

#else

#define FW_FILENAME			"/sys/mcuimg.bin"
#define APP_NAME			"WyLight Firmware"
#define FIRMWARE_ORIGIN 	0x20004000

#endif

#endif /* #ifndef _BOOTLOADER_H_ */
