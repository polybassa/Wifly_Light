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

#ifndef _BROADCAST_RECEIVER_H_
#define _BROADCAST_RECEIVER_H_

#include "ClientSocket.h"
#include "Endpoint.h"
#include <atomic>
#include <stdint.h>
#include <cstring>
#include <mutex>
#include <ostream>
#include <set>
#include <string>

class BroadcastReceiver
{
	public:
		static const uint16_t BROADCAST_PORT = 55555;
		static const int8_t BROADCAST_DEVICE_ID[];
		static const size_t BROADCAST_DEVICE_ID_LENGTH;
		static const int8_t STOP_MSG[];
		static const size_t STOP_MSG_LENGTH;
		static const Endpoint EMPTY_ENDPOINT;
		const uint16_t mPort;

		BroadcastReceiver(uint16_t port = BROADCAST_PORT);
		~BroadcastReceiver(void);

		/**
		 * Wait for broadcasts and print them to a stream
		 * @param out stream to print collected remotes on
		 * @param timeout in seconds, until execution is terminated
		 */
		void operator() (std::ostream& out, timeval* timeout = NULL);

		const Endpoint& GetEndpoint(size_t index) const;
		Endpoint GetNextRemote(timeval* timeout);

		/**
		 * @return number of known IP addresses
		 */
		size_t NumRemotes(void) const;

		/**
		 * Sends a stop event to terminate execution of operator()
		 */
		void Stop(void);

	private:
		std::set<Endpoint> mIpTable;
		volatile bool mIsRunning;
		std::atomic<int32_t> mNumInstances;
		std::mutex mMutex;
};

#pragma pack(push)
#pragma pack(1)
struct BroadcastMessage
{
	uint8_t mac[6];
	uint8_t channel;
	uint8_t rssi;
	uint16_t port;
	uint32_t rtc;
	uint16_t bat_mV;
	uint16_t gpioValue;
	int8_t asciiTime[13+1];
	int8_t version[26+1+1];// this seems a little strange. bug in wifly fw?
	int8_t deviceId[32];
	uint16_t bootTmms;
	uint16_t sensor[8];

	bool IsWiflyBroadcast(size_t length) {
		return ((sizeof(BroadcastMessage) == length) && (0 == memcmp(deviceId,	BroadcastReceiver::BROADCAST_DEVICE_ID, BroadcastReceiver::BROADCAST_DEVICE_ID_LENGTH)));
	};

	bool IsStop(size_t length) {
		return ((BroadcastReceiver::STOP_MSG_LENGTH == length)
						&& (0 == memcmp(&mac, BroadcastReceiver::STOP_MSG, BroadcastReceiver::STOP_MSG_LENGTH)));
	};
};
#pragma pack(pop)
#endif /* #ifndef _BROADCAST_RECEIVER_H_ */
