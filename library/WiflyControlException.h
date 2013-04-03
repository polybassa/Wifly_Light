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


#ifndef ____WiflyControlException__
#define ____WiflyControlException__

#include <stdio.h>
#include <exception>
#include <string>
#include <string.h>
#include <typeinfo>
#include "wifly_cmd.h"
#include "BlRequest.h"

class FatalError : public std::exception
{
public:
	FatalError(const std::string& description) throw () : mDescription(description) {};
	virtual ~FatalError(void) throw () {};

	friend std::ostream& operator<< (std::ostream& out, const FatalError& ref)
	{
		return out << ref.mDescription;
	};

protected:
	std::string mDescription;
};

class ConnectionLost : public FatalError
{
public:
	ConnectionLost(const std::string& description, uint32_t addr, uint16_t port)
		: FatalError(description), mAddress(addr), mPort(port)
	{
	};

	friend std::ostream& operator<< (std::ostream& out, const ConnectionLost& ref)
	{
		return out << "ConnectionLost to: " << ref.mAddress << ':' << ref.mPort
		           <<" due: '" << ref.mDescription << '\'';
	};

protected:
	uint32_t mAddress;
	uint16_t mPort;
};

class ConnectionTimeout : public FatalError
{
public:
	ConnectionTimeout(const std::string& description) : FatalError(description) {};
};

class InvalidParameter : public FatalError
{
public:
	InvalidParameter(const std::string& description) : FatalError(description) {};
};

class ScriptBufferFull : public FatalError
{
public:
	ScriptBufferFull(void) : FatalError("ScriptBuffer in PIC is full, clear it or wait") {};
};
#endif /* defined(____WiflyControlException__) */
