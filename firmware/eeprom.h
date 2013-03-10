/**
 Copyright (C) 2012 Nils Weiss, Patrick Brünn.
 
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

#ifndef _EEPROM_H_
#define _EEPROM_H_

#include "platform.h"

/* globals */
#define EEPROM_SCRIPTBUF_BASE 0
#define EEPROM_SCRIPTBUF_INLOOP 0x3fd
#define EEPROM_SCRIPTBUF_READ 0x3fb
#define EEPROM_SCRIPTBUF_WRITE 0x3f9

/* eeprom access functions */
void Eeprom_Write(const uns16 adress, const uns8 data);
uns8 Eeprom_Read(const uns16 adress);
void Eeprom_WriteBlock(const uns8 *array, uns16 adress, const uns8 length);
void Eeprom_ReadBlock(uns8 *array, uns16 adress, const uns8 length);
#endif
