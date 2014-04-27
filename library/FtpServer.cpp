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

#include "FtpServer.h"
#include "trace.h"
#include <fstream>
#include <unistd.h>
#include <string>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sstream>


namespace WyLight {

#define FTP_PORT 60021
	
	static const int g_DebugZones = ZONE_ERROR | ZONE_WARNING | ZONE_INFO | ZONE_VERBOSE;

	static const timeval RESPONSE_TIMEOUT = {5, 0}; // three seconds timeout for fragmented responses from pic
	
	FtpServer::FtpServer(void) throw (FatalError) : mServerSock(socket(AF_INET, SOCK_STREAM, 0)), mServerSockAddr(INADDR_ANY, FTP_PORT)
	{
		if (-1 == mServerSock) {
			throw FatalError("Create socket failed");
		}
		mFtpServerRunning = 1;
		mClientDataSock = -1;
		mClientSock = -1;
		
		mFtpServerThread = std::thread([&]{
			int yes = 1;
			//optional, steal port if necessary
			if(0 != setsockopt(mServerSock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes))) {
				throw FatalError("setsockopt() failed");
			}
			
			if(0 != bind(mServerSock, reinterpret_cast<struct sockaddr *>(&mServerSockAddr), sizeof(sockaddr))) {
				throw FatalError("Bind FtpServer socket failed with error: " + std::to_string(errno));
			}
			
			if (0 != listen(mServerSock, 0)) {
				throw FatalError("FtpServer: listen failed with error: " + std::to_string(errno));
			}
			
			while (mFtpServerRunning) {
				Trace(ZONE_VERBOSE, "FtpServer running\n");
				fflush(stdout);
				sockaddr_in clientAddr;
				socklen_t clientAddrLen = sizeof(clientAddr);
				
				//blocking accept here !!
				mClientSock = accept(mServerSock, reinterpret_cast<struct sockaddr *>(&clientAddr), &clientAddrLen);
				
				uint16_t port = ntohs(clientAddr.sin_port);
				uint32_t ip = ntohl(clientAddr.sin_addr.s_addr);
				
				std::string message = "Client connected: "
						   + std::to_string((ip >> 24) & 0xff) + "."
						   + std::to_string((ip >> 16) & 0xff) + "."
						   + std::to_string((ip >>  8) & 0xff) + "."
						   + std::to_string((ip      ) & 0xff) + "::"
						   + std::to_string(port) + "\r\n";
				
				Trace(ZONE_INFO, "%s\n", message.c_str());
				fflush(stdout);
				
				{
					std::lock_guard<std::mutex> lock(mFtpServerRunningLock);
					if (!mFtpServerRunning) {
						close(mClientSock);
						mClientSock = -1;
						return;
					}
				}
				
				if( mClientSock == -1 ) {
					throw FatalError("FtpServer: accept error: " + std::to_string(errno));
				}
				
				Trace(ZONE_VERBOSE, "Starting client Thread\n");
				fflush(stdout);
				
				try {
					handleFiletransfer(mClientSock);
				} catch (FatalError &e) {
					Trace(ZONE_ERROR, "%s\n", e.what());
				}
				
				close(mClientSock);
				mClientSock = -1;
			}
		});
	}
	
	FtpServer::~FtpServer(void) {
		{
			std::lock_guard<std::mutex> lock(mFtpServerRunningLock);
			mFtpServerRunning = false;
		}

		TcpSocket shutdownSocket(2130706433, FTP_PORT);
		mFtpServerThread.join();
		
		if (mServerSock != -1) {
			close(mServerSock);
		}
		
		if (mClientSock != -1) {
			close(mClientSock);
		}
		
		if (mClientDataSock != -1) {
			close(mClientDataSock);
		}
	}
	
	bool FtpServer::Select(timeval *timeout, const int& socket) const throw (FatalError)
	{
		if (socket == -1) {
			throw FatalError("Invalid Client Socket");
			return false;
		}
		
		/* prepare socket set for select() */
		fd_set readSockets;
		FD_ZERO(&readSockets);
		FD_SET(socket, &readSockets);
		
		/* wait for receive data */
		const int selectState = select(socket + 1, &readSockets, NULL, NULL, timeout);
		if(0 == selectState) {
			Trace(ZONE_INFO, "Select timed out\n");
			return true;
		}
		
		/* and check if socket was correct */
		if((1 != selectState) || (!FD_ISSET(socket, &readSockets))) {
			throw FatalError("something strange happen in select() called by TcpSocket::Recv()");
		}
		return false;
	}
	
	void FtpServer::handleFiletransfer(int socket) {
		size_t bytesRead = 0;
		uint8_t buffer[1024];
		std::stringstream dataInput;
		bool isClientLoggedIn = false;
		
		this->Send("220 WyLight Ftp Server running.\r\n", mClientSock);
		
		while (true) {
			
			if ( (bytesRead = this->Recv(reinterpret_cast<uint8_t *>(buffer), sizeof(buffer), mClientSock)) == 0) {
				throw FatalError("FtpServer: read error: " + std::to_string(errno));
			}
			
			dataInput.write(reinterpret_cast<const char *>(buffer), bytesRead);
			
			std::string requestCMD;
			dataInput >> requestCMD;
			
			if (requestCMD == "USER") {
				//=============================
				std::string userName;
				std::getline(dataInput, userName);
				
				if (userName.find("roving") != std::string::npos) {
					this->Send("331 Username ok, send password.\r\n", mClientSock);
				} else {
					this->Send("430 Invalid username or password. Good Bye.\r\n", mClientSock);
					return;
				}
			} else if (requestCMD == "PASS") {
				//=============================
				std::string password;
				std::getline(dataInput, password);
				
				if (password.find("Pass123") != std::string::npos) {
					this->Send("230 Login successful.\r\n", mClientSock);
					isClientLoggedIn = true;
				} else {
					this->Send("430 Invalid username or password. Good Bye.\r\n", mClientSock);
					return;
				}
			} else if (requestCMD == "CWD" && isClientLoggedIn) {
				//=============================
				std::string directory;
				std::getline(dataInput, directory);
				
				if (directory.find("public") != std::string::npos) {
					this->Send("250 \"/public\" ist the current directory.\r\n", mClientSock);
				} else {
					this->Send("550 Requested action not taken. File unavailable (e.g., file not found, no access). Good Bye.\r\n", mClientSock);
					return;
				}
			} else if (requestCMD == "TYPE" && isClientLoggedIn) {
				//=============================
				std::string dataType;
				std::getline(dataInput, dataType);
				
				if (dataType.find("I") != std::string::npos) {
					this->Send("200 Type set to: Binary.\r\n", mClientSock);
				} else {
					this->Send("550 Requested action not taken. File unavailable (e.g., file not found, no access). Good Bye.\r\n", mClientSock);
					return;
				}
			} else if (requestCMD == "PASV" && isClientLoggedIn) {
				//=============================
				std::string dataType;
				std::getline(dataInput, dataType);
				
				openDataConnection();
				
				if (mClientDataSock != -1) {
					struct sockaddr_in sin;
					socklen_t len = sizeof(sin);
					if (getsockname(mClientDataSock, (struct sockaddr *)&sin, &len) != -1) {
						uint16_t port = ntohs(sin.sin_port);
						uint32_t ip = ntohl(sin.sin_addr.s_addr);

						this->Send("227 Entering pasive mode ("
								+ std::to_string((ip >> 24) & 0xff) + ","
								+ std::to_string((ip >> 16) & 0xff) + ","
								+ std::to_string((ip >>  8) & 0xff) + ","
								+ std::to_string((ip      ) & 0xff) + ","
								+ std::to_string(port / 256) + ","
								+ std::to_string(port % 256) + ").\r\n", mClientSock);
					}
					else {
						throw FatalError("Getsockname failed");
					}
				} else {
					throw FatalError("Invalid Client Data Socket");
				}
			} else if (requestCMD == "RETR" && isClientLoggedIn) {
				std::string fileName;
				std::getline(dataInput, fileName);
				
				
				//TODO: get the right file !!!!
				char currentDirectory[FILENAME_MAX];
				
				getcwd(currentDirectory, FILENAME_MAX);
							
				//FIXME: static filename only for debugging
				//fileName = "/Users/nilsweiss/Dropbox/Wifly_Light/FtpUpdateServer/public/wifly7-400.mif";
				fileName = "/home/gpb/workspace/Wifly_Light/wifly7-441.mif";
				
				std::ifstream file(fileName, std::ios::in | std::ios::binary);
				
				if (file.is_open()) {
					this->Send("125 Data connection already open. Transfer starting.\r\n", mClientSock);
					transferDataPassive(file);
					file.close();
					this->Send("226 Transfer complete.\r\n", mClientSock);
				} else {
					this->Send("550 Requested action not taken. File unavailable (e.g., file not found, no access). Good Bye.\r\n", mClientSock);
					if (mClientDataSock != -1) {
						close(mClientDataSock);
						mClientDataSock = -1;
					}
					return;
				}
				
			} else if (requestCMD == "QUIT" && isClientLoggedIn) {
				this->Send("221 Thank you for updating.\r\n", mClientSock);
				if (mClientDataSock != -1) {
					close(mClientDataSock);
					mClientDataSock = -1;
				}
				return;
				
			} else {
				this->Send("150 Command not supported. Good Bye.\r\n", mClientSock);
				if (mClientDataSock != -1) {
					close(mClientDataSock);
					mClientDataSock = -1;
				}
				return;
			}
		}
	}
	
	void FtpServer::transferDataPassive(std::ifstream& file) const throw(FatalError) {
		size_t bytesRead = 0;
		uint8_t buffer[2048];
		
		file.seekg(0, file.end);
		int length = (int)file.tellg();
        file.seekg(0, file.beg);
		
		sockaddr_in clientAddr;
		socklen_t clientAddrLen = sizeof(clientAddr);
		
		// start a timeout kill thread here!!
		int tempDataSock = -1;
		
		std::thread killThread = std::thread([&]{
			
			std::this_thread::sleep_for(std::chrono::seconds(1));
			
			struct sockaddr_in sin;
			socklen_t len = sizeof(sin);
			if (getsockname(mClientDataSock, (struct sockaddr *)&sin, &len) != -1) {
				uint16_t port = ntohs(sin.sin_port);

				//shutting down if no accept happend
				if (tempDataSock == -1) {
					TcpSocket shutdownSocket(2130706433, port);
				}
			}
		});
		
		//blocking accept here !!
		tempDataSock = accept(mClientDataSock, reinterpret_cast<struct sockaddr *>(&clientAddr), &clientAddrLen);
		
		if (tempDataSock == -1) {
			killThread.join();
			throw FatalError("Invalid Data Socket");
		}
		
		if (ntohl(clientAddr.sin_addr.s_addr) == 2130706433) {
			killThread.join();
			throw FatalError("Timeout occured, unblocked accept by killThread!! ");
		}
		
		for (bytesRead = 0; bytesRead < length - sizeof(buffer); bytesRead = bytesRead + sizeof(buffer)) {
			file.read((char*)buffer, sizeof(buffer));
			if (!file) {
				killThread.join();
				close(tempDataSock);
				throw FatalError("Error in Filestream occured");
			}
			Send(buffer, sizeof(buffer), tempDataSock);
		}
		
		file.read((char*)buffer, length - bytesRead);
		if (!file) {
			killThread.join();
			close(tempDataSock);
			throw FatalError("Error in Filestream occured");
		}
		
		Send(buffer, length - bytesRead, tempDataSock);

		killThread.join();
		close(tempDataSock);
	}
	
	void FtpServer::openDataConnection(void) throw(FatalError) {
		
		struct sockaddr_in sin;
		socklen_t len = sizeof(sin);
		if (getsockname(mClientSock, (struct sockaddr *)&sin, &len) == -1) {
			throw FatalError("Getsockname failed");
		}
		
		mClientDataSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		
		sin.sin_family = AF_INET;
		sin.sin_port = 0;
#ifdef __APPLE__
		sin.sin_len = sizeof(sin);
#endif
		
		int on = 1;
		
		if (mClientDataSock != -1 && setsockopt(mClientDataSock, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) != -1) {
			if (bind( mClientDataSock, (struct sockaddr *)&sin, sizeof(sin)) == -1){
				this->Send("451 Internal error - No data port available.\r\n", mClientSock);
				close(mClientDataSock);
				mClientDataSock = -1;
				throw FatalError("Unable to bind FTP_Data Socket");
			}
			
			if(listen(mClientDataSock, 1) == -1) {
				this->Send("451 Internal error.\r\n", mClientSock);
				close(mClientDataSock);
				mClientDataSock = -1;
				throw FatalError("Unable to listen FTP_Data Socket");
			}
		} else {
			this->Send("451 Internal error - No data socket available.\r\n", mClientSock);
			close(mClientDataSock);
			mClientDataSock = -1;
			throw FatalError("Unable to get FTP_Data Socket");
		}
	}
	
	size_t FtpServer::Recv(uint8_t *pBuffer, size_t length, const int& socket) const throw(FatalError)	{
		timeval mTimeout;
		memcpy(&mTimeout, &RESPONSE_TIMEOUT, sizeof(RESPONSE_TIMEOUT));
		
		if (socket != -1) {
			return FtpServer::Select(&mTimeout, socket) ? 0 : read(socket, pBuffer, length);
		}
		else {
			throw FatalError("Invalid Client Socket");
			return 0;
		}
	}
	
	size_t FtpServer::Send(const std::string &message, const int& socket) const throw (FatalError){
		return this->Send(reinterpret_cast<const uint8_t *>(message.data()), message.length(), socket);
	}
	
	size_t FtpServer::Send(const uint8_t *frame, const size_t length, const int& socket) const throw(FatalError) {
		if (socket != -1) {
			//TraceBuffer(ZONE_INFO, frame, length, "%02x ", "Sending on socket 0x%04x, %zu bytes: ", mClientSock, length);
			const ssize_t result = write(socket, frame, length);
			if(result == -1) {
				throw FatalError("send on socket " + std::to_string(socket) + " failed with returnvalue -1 and errno:" + std::to_string(errno));
			}
			return result;
		}
		else {
			throw FatalError("Invalid Socket");
		}
	}

} /* namespace WyLight */

