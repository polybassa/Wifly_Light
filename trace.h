/**
 Copyright (C) 2012 Nils Weiss, Patrick Bruenn.
 
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

#ifndef _TRACE_H_
#define _TRACE_H_
#ifdef TEST
	#include "usart.h"
	#include "RingBuf.h"
	
	extern struct RingBuffer g_TraceBuf;
	
	void Trace_String(const char *string);
	
	void Trace_Number(uns8 input);
	
	void Trace_Hex(uns8 input);
	
	void Trace_Hex16(uns16 input);
	
	void Trace_Char(uns8 input);
	
	void Trace_Print();
#elif DEBUG
	#include "stdio.h"
	#define Trace_String(str) do { printf("%s", str); } while (0)
	#define Trace_Number(input) do { printf("%04x", input); } while (0)
	#define Trace_Hex(hex) do { printf("%02x ", hex); } while(0)
	#define Trace_Hex16(hex) do { printf("%04x ", hex); } while(0)
	#define Trace_Print(x)
	#define Trace_Char(input) do { printf("%c", input); } while (0)
#else
	#define Trace_String(str)
	#define Trace_Number(input)
	#define Trace_Hex(hex)
	#define Trace_Hex16(hex)
	#define Trace_Print(x)
	#define Trace_Char(x)
#endif
#endif /* #ifndef _TRACE_H_ */

