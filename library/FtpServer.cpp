/**
 Copyright (C) 2014 Nils Weiss, Patrick Bruenn.
 
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

	static const timeval RESPONSE_TIMEOUT = {5, 0};
	static const uint32_t LOCALHOST = 2130706433;
	
	const FtpCommand FtpCommand::USER("roving",
			"331 Username ok, send password.\r\n",
			"430 Invalid username or password. Good Bye.\r\n");

	const FtpCommand FtpCommand::PASS("Pass123",
			"230 Login successful.\r\n",
			"430 Invalid username or password. Good Bye.\r\n");

	FtpCommand::FtpCommand(const char* param, const char* successMsg, const char* errorMsg)
		: mParam(param), mSuccess(successMsg), mError(errorMsg)
	{
	}

	bool FtpCommand::Run(const TcpSocket& telnet, std::stringstream& dataInput) const {
		std::string param;
		std::getline(dataInput, param);
		Trace(ZONE_INFO, ">%s<\n", param.c_str());
		//TODO do stronger string compare!
		if (param.find(mParam) != std::string::npos) {
			telnet.Send(mSuccess);
			return true;
		} else {
			telnet.Send(mError);
			return false;
		}
	}

	FtpServer::FtpServer(void) throw (FatalError)
	{
		mFtpServerThread = std::thread([&]{
			TcpServerSocket telnetListener(INADDR_ANY, FTP_PORT);
			while (mFtpServerRunning) {
				Trace(ZONE_VERBOSE, "FtpServer running\n");
				TcpSocket telnet(telnetListener.GetSocket());
				if (!mFtpServerRunning) {
					return;
				}

				Trace(ZONE_VERBOSE, "Starting client Thread\n");
				try {
					handleFiletransfer(telnet);
				} catch (FatalError &e) {
					Trace(ZONE_ERROR, "%s\n", e.what());
				}
			}
		});
	}
	
	FtpServer::~FtpServer(void) {
		mFtpServerRunning = false;

		TcpSocket shutdownSocket(LOCALHOST, FTP_PORT);
		mFtpServerThread.join();
	}

	void FtpServer::handleFiletransfer(const TcpSocket& telnet) {
		size_t bytesRead = 0;
		uint8_t buffer[1024];
		std::stringstream dataInput;
		bool isClientLoggedIn = false;
		TcpServerSocket *dataSocket = NULL;
		
		telnet.Send("220 WyLight Ftp Server running.\r\n");
		
		while (true) {
			
			if ( (bytesRead = telnet.Recv(reinterpret_cast<uint8_t *>(buffer), sizeof(buffer))) == 0) {
				throw FatalError("FtpServer: read error: " + std::to_string(errno));
			}
			
			dataInput.write(reinterpret_cast<const char *>(buffer), bytesRead);
			
			std::string requestCMD;
			dataInput >> requestCMD;
			
			if (requestCMD == "USER")
			{
				if(!FtpCommand::USER.Run(telnet, dataInput)) {
					return;
				}
			} else if (requestCMD == "PASS")
			{
				if(!FtpCommand::PASS.Run(telnet, dataInput)) {
					return;
				}
				isClientLoggedIn = true;
			} else if (requestCMD == "CWD" && isClientLoggedIn)
			{
				//=============================
				std::string directory;
				std::getline(dataInput, directory);
				
				if (directory.find("public") != std::string::npos) {
					telnet.Send("250 \"/public\" ist the current directory.\r\n");
				} else {
					telnet.Send("550 Requested action not taken. File unavailable (e.g., file not found, no access). Good Bye.\r\n");
					return;
				}
			} else if (requestCMD == "TYPE" && isClientLoggedIn)
			{
				//=============================
				std::string dataType;
				std::getline(dataInput, dataType);
				
				if (dataType.find("I") != std::string::npos) {
					telnet.Send("200 Type set to: Binary.\r\n");
				} else {
					telnet.Send("550 Requested action not taken. File unavailable (e.g., file not found, no access). Good Bye.\r\n");
					return;
				}
			} else if (requestCMD == "PASV" && isClientLoggedIn)
			{
				//=============================
				//clear content of dataInput
				dataInput.str(std::string());
				
				try {
					dataSocket = openDataConnection(telnet);
				} catch (FatalError& e) {
					telnet.Send(std::string("451 Internal error - ") + std::string(e.what()));
				}

				uint16_t port = dataSocket->GetPort();
				uint32_t ip = dataSocket->GetIp();

				telnet.Send("227 Entering pasive mode ("
					+ std::to_string((ip >> 24) & 0xff) + ","
					+ std::to_string((ip >> 16) & 0xff) + ","
					+ std::to_string((ip >>  8) & 0xff) + ","
					+ std::to_string((ip      ) & 0xff) + ","
					+ std::to_string(port / 256) + ","
					+ std::to_string(port % 256) + ").\r\n");
			} else if (requestCMD == "RETR" && isClientLoggedIn)
			{
				std::string fileName;
				std::getline(dataInput, fileName);
				
				//TODO: get the right file !!!!
				char currentDirectory[FILENAME_MAX];
				
				getcwd(currentDirectory, FILENAME_MAX);
							
				//FIXME: static filename only for debugging
				//fileName = "/Users/nilsweiss/Dropbox/Wifly_Light/FtpUpdateServer/public/wifly7-441.mif";
				fileName = "./rn171_fw/wifly7-441.mif";
				
				std::ifstream file(fileName, std::ios::in | std::ios::binary);
				
				if (file.is_open()) {
					telnet.Send("125 Data connection already open. Transfer starting.\r\n");
					transferDataPassive(file, *dataSocket);
					file.close();
					telnet.Send("226 Transfer complete.\r\n");
				} else {
					telnet.Send("550 Requested action not taken. File unavailable (e.g., file not found, no access). Good Bye.\r\n");
					delete dataSocket;
					return;
				}
				
			} else if (requestCMD == "QUIT" && isClientLoggedIn) {
				telnet.Send("221 Thank you for updating.\r\n");
				delete dataSocket;
				return;
				
			} else {
#define HAVE_MERCY 1
#if HAVE_MERCY
				//clear content of dataInput
				dataInput.str(std::string());
				Trace(ZONE_INFO, "Unkown command: >%s<\n", requestCMD.c_str());
				telnet.Send("150 Command not supported.\r\n");
#else
				telnet.Send("150 Command not supported. Good Bye.\r\n");
				delete dataSocket;
				return;
#endif
			}
		}
	}
	
	void FtpServer::transferDataPassive(std::ifstream& file, const TcpServerSocket& dataSocket) const throw(FatalError) {
		// The remote has 1 second to connect until we terminate the listening data socket
		const struct timespec timeout {1, 0};
		TcpSocket transferSocket(dataSocket.GetSocket(), &timeout);
		
		// send our file to the new data socket
		while(file.good()) {
			uint8_t buffer[FILE_BUFFER_SIZE];
			file.read((char*)buffer, sizeof(buffer));
			transferSocket.Send(buffer, file.gcount());
		}
	}
	
	TcpServerSocket* FtpServer::openDataConnection(const TcpSocket& telnet) throw(FatalError)
	{
		struct sockaddr_in sin;
		socklen_t len = sizeof(sin);
		if (getsockname(telnet.GetSocket(), (struct sockaddr *)&sin, &len) == -1) {
			throw FatalError("Getsockname failed");
		}
		
		TcpServerSocket *result = new TcpServerSocket(ntohl(sin.sin_addr.s_addr), 0);
		if(!result) {
			throw FatalError("openDataConnection(): create TcpServerSocket failed");
		}
		return result;
	}
} /* namespace WyLight */
