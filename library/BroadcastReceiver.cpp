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

#include "BroadcastReceiver.h"
#include "timeval.h"
#include <iostream>
#include <stdio.h>

const int8_t BroadcastReceiver::BROADCAST_DEVICE_ID[] = "WiFly";
const size_t BroadcastReceiver::BROADCAST_DEVICE_ID_LENGTH = 5;
const int8_t BroadcastReceiver::STOP_MSG[] = "StopThread";
const size_t BroadcastReceiver::STOP_MSG_LENGTH = sizeof(STOP_MSG);

BroadcastReceiver::BroadcastReceiver(uint16_t port)
	: mPort(port), mIsRunning(true), mNumInstances(0)
{
}

BroadcastReceiver::~BroadcastReceiver(void)
{
	Stop();
	//TODO cleanup mIpTable
}

void BroadcastReceiver::operator() (std::ostream& out, timeval* pTimeout)
{
	// only one thread allowed per instance
	if(0 == std::atomic_fetch_add(&mNumInstances, 1))
	{
		size_t numRemotes = 0;
		timeval endTime, now;
		gettimeofday(&endTime, NULL);
		timeval_add(&endTime, pTimeout);
		do
		{
			Endpoint remote = GetNextRemote(pTimeout);
			if(remote.IsValid()) {
				out << numRemotes << ':' << remote << '\n';
				numRemotes++;
			}
			gettimeofday(&now, NULL);
		} while(mIsRunning && timeval_sub(&endTime, &now, pTimeout));
	}
	std::atomic_fetch_sub(&mNumInstances, 1);
}

uint32_t BroadcastReceiver::GetIp(size_t index) const
{
	return mIpTable[index].m_Addr;
}

Endpoint BroadcastReceiver::GetNextRemote(timeval* timeout)
{
	UdpSocket udpSock(INADDR_ANY, mPort, true, true);
	sockaddr_storage remoteAddr;
	socklen_t remoteAddrLength = sizeof(remoteAddr);

	BroadcastMessage msg;
	size_t bytesRead = udpSock.RecvFrom((uint8_t*)&msg, sizeof(msg), timeout, (sockaddr*)&remoteAddr, &remoteAddrLength);
	if(msg.IsWiflyBroadcast(bytesRead))
	{
		Endpoint newRemote(remoteAddr, remoteAddrLength, msg.port);
		mMutex.lock();
		mIpTable.push_back(newRemote);
		mMutex.unlock();
		return newRemote;
	}
	return Endpoint();
}

uint16_t BroadcastReceiver::GetPort(size_t index) const
{
	return mIpTable[index].m_Port;
}

size_t BroadcastReceiver::NumRemotes(void) const
{
	return mIpTable.size();
}

void BroadcastReceiver::Stop(void)
{
	mIsRunning = false;
	UdpSocket sock(INADDR_LOOPBACK, mPort, false);
	sock.Send((unsigned char const*)STOP_MSG, STOP_MSG_LENGTH);
}

