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

#ifndef _CLIENTSOCKET_H_
#define _CLIENTSOCKET_H_
#include <cassert>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stddef.h>
#include <sys/time.h>

class Endpoint
{
	public:
		Endpoint(sockaddr_storage& addr, const size_t size) {
			assert(sizeof(sockaddr_in) == size);
			m_Addr = ntohl(((sockaddr_in&)addr).sin_addr.s_addr);
			m_Port = ntohs(((sockaddr_in&)addr).sin_port);
		};
		Endpoint(uint32_t addr, uint16_t port) : m_Addr(addr), m_Port(port) {};
		uint32_t m_Addr;
		uint16_t m_Port;
};

class ClientSocket
{
	protected:
		const int mSock;
		struct sockaddr_in mSockAddr;

	public:
		ClientSocket(unsigned long addr, unsigned short port, int style);
		virtual ~ClientSocket();
		virtual size_t Recv(unsigned char* pBuffer, size_t length, timeval* timeout = NULL) const = 0;
		virtual int Send(const unsigned char* frame, size_t length) const = 0;
};

class TcpSocket : public ClientSocket
{
	public:
		TcpSocket(unsigned long Addr, unsigned short port);
		virtual size_t Recv(unsigned char* pBuffer, size_t length, timeval* timeout = NULL) const;
		virtual int Send(const unsigned char* frame, size_t length) const;
};

class UdpSocket : public ClientSocket
{
	public:
		UdpSocket(unsigned long addr, unsigned short port, bool doBind = true);
		virtual size_t Recv(unsigned char* pBuffer, size_t length, timeval* timeout = NULL) const;
		virtual size_t RecvFrom(unsigned char* pBuffer, size_t length, timeval* timeout = NULL, struct sockaddr* remoteAddr = NULL, socklen_t* remoteAddrLength = NULL) const;
		virtual int Send(const unsigned char* frame, size_t length) const;
};

#ifdef UNIT_TEST
class TestSocket : public ClientSocket
{
	private:
		timespec m_Delay;
	public:
		TestSocket(unsigned long addr, unsigned short port);
		virtual size_t Recv(unsigned char* pBuffer, size_t length, timeval* timeout = NULL) const;
		virtual int Send(const unsigned char* frame, size_t length) const;
		void SetDelay(timeval& delay);
};
#endif /* #ifndef UNIT_TEST */
#endif /* #ifndef _CLIENTSOCKET_H_ */

