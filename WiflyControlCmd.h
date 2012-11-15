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

#ifndef _WIFLYCONTROLCMD_H_
#define _WIFLYCONTROLCMD_H_
#include <iostream>
#include <string>
#include <iomanip>

using namespace std;

class WiflyControlCmd
{
	public:
		WiflyControlCmd(string description) : m_CmdDescription(description) {};
		friend ostream& operator<< (ostream& stream, const WiflyControlCmd& ref) {
			stream << ref.m_CmdDescription;
			return stream;
		};
		virtual void Run(WiflyControl& control) const = 0;
	protected:
		const string m_CmdDescription;
		void Print(const unsigned char* const pBuffer, const size_t size, const unsigned int address) const {
			for(size_t i = 0; i < size; i++) {
				if(0 == (i % 16)) {
					cout << endl << "0x" << setw(4) << setfill('0') << hex << int(address+i) << ": ";
				}
				cout << setw(2) << setfill('0') << hex << int(pBuffer[i]) << ' ';
			}
			cout << endl;
		}
		void PrintCrc(const unsigned char* const pBuffer, const size_t size, const unsigned int address) const {
			for(size_t i = 0; i < size; i++) {
				if(0 == (i % 2)) {
					cout << endl 
					<< "0x" << setw(4) << setfill('0') << hex 
					<< int(address+i * FLASH_ERASE_BLOCKSIZE) 
					<< " - " 
					<< "0x" << setw(4) << setfill('0') << hex 
					<< int(address+ ((i+2) * FLASH_ERASE_BLOCKSIZE) -1) 
					<< ": ";
				}
				cout << setw(2) << setfill('0') << hex << int(pBuffer[i]) << ' ';
			}
			cout << endl;
		}
};

class ControlCmdAddColor : public WiflyControlCmd
{
	public:
		ControlCmdAddColor(void) : WiflyControlCmd(
				string("'addcolor <addr> <rgb> <hour> <minute> <second>'\n")
			+ string("    <addr> hex bitmask, which leds should be set to the new color\n")
			+ string("    <rgb> hex rgb value of the new color f.e. red: ff0000\n")
			+ string("    <hour> hour of date event\n")
			+ string("    <minute> minute of date event\n")
			+ string("    <second> second of date event")) {};

		virtual void Run(WiflyControl& control) const {
			string addr, color;
			unsigned long hour, minute, second;
			cin >> addr;
			cin >> color;
			cin >> hour;
			cin >> minute;
			cin >> second;
			cout << addr << " " << color << " " << hour << " " << minute << " " << second << endl; 
			control.AddColor(addr, color, hour, minute, second);
		};
};

class ControlCmdBlInfo : public WiflyControlCmd
{
	public:
		ControlCmdBlInfo(void) : WiflyControlCmd(
				string("'bl_info' - read bootloader information")) {};

		virtual void Run(WiflyControl& control) const {
			BlInfo info;
			if(sizeof(info) == control.BlReadInfo(info))
			{
				info.Print();
			}
			else
			{
				std::cout << "Read bootloader info failed" << endl;
			}
		};
};

class ControlCmdBlCrcFlash : public WiflyControlCmd
{
	public:
		ControlCmdBlCrcFlash(void) : WiflyControlCmd("'crc_flash'") {};
		virtual void Run(WiflyControl& control) const {
			unsigned int address;
			size_t numBlocks;
			cin >> address;
			cin >> numBlocks;
			unsigned char buffer[(FLASH_SIZE / FLASH_ERASE_BLOCKSIZE) * 2];
			if(sizeof(buffer) / 2 < numBlocks)
			{
				cout << "Read CRC failed. Too many CRCs requested" << endl;
				return;
			}

			size_t bytesRead = control.BlReadCrcFlash(buffer, address, numBlocks); 
			if(2 * numBlocks != bytesRead)
			{
				cout << "Read CRC failed" << endl;
				return;
			}
			PrintCrc(buffer, bytesRead, address);
		};
};

class ControlCmdBlEraseFlash : public WiflyControlCmd
{
	public:
		ControlCmdBlEraseFlash(void) : WiflyControlCmd(
				string("'erase_flash'")) {};

		virtual void Run(WiflyControl& control) const
		{
			if(control.BlFlashErase())
			{
			    cout << endl <<"Erase complete flash succesful"<<endl;
			}
			else
			{
			    cout << endl <<"Erase complete flash failed"<<endl;
			}
		}	
};

class ControlCmdBlEraseEeprom : public WiflyControlCmd
{
	public:
		ControlCmdBlEraseEeprom(void) : WiflyControlCmd(
				string("'erase_eeprom'")) {};

		virtual void Run(WiflyControl& control) const
		{
			if(control.BlEepromErase())
			{
			    cout << endl <<"Erase complete eeprom succesful"<<endl;
			}
			else
			{
			    cout << endl <<"Erase complete eeprom failed"<<endl;
			}
		}	
};

class ControlCmdBlAutostartEnable : public WiflyControlCmd
{
	public:
		ControlCmdBlAutostartEnable(void) : WiflyControlCmd(
				string("'enable_bl_autostart'")) {};

		virtual void Run(WiflyControl& control) const
		{
			if(control.BlEnableAutostart())
			{
			    cout << endl <<"Bootloader Autostart enabled!"<<endl;
			}
			else
			{
			    cout << endl <<"Bootloader Autostart not enabled!"<<endl;
			}
		}	
};


class ControlCmdBlRead : public WiflyControlCmd
{
	public:
		ControlCmdBlRead(string name) : WiflyControlCmd(
				string("'read_") + name + string(" <addr> <numBytes>'\n")
			+ string("    <addr> address where to start reading\n")
			+ string("    <numBytes> number of bytes to read")), m_Name(name) {};

		const string m_Name;
		
		virtual size_t Read(WiflyControl& control, unsigned char* pBuffer, unsigned int address, const size_t numBytes) const = 0;

		virtual void Run(WiflyControl& control) const {
			unsigned int address;
			size_t numBytes;
			cin >> address;
			cin >> numBytes;
			unsigned char buffer[0x10000];
			if(sizeof(buffer) < numBytes)
			{
				cout << "Read " << m_Name << " failed. Too many bytes requested" << endl;
				return;
			}

			size_t bytesRead = Read(control, buffer, address, numBytes);

			if(bytesRead != numBytes) 
			{
				cout << "Read " << m_Name << " failed" << endl;
			} 
			else 
			{
				Print(buffer, bytesRead, address);
			}
		};
};

class ControlCmdBlReadEeprom : public ControlCmdBlRead
{
	public:
		ControlCmdBlReadEeprom(void) : ControlCmdBlRead("eeprom") {};
		size_t Read(WiflyControl& control, unsigned char* pBuffer, unsigned int address, const size_t numBytes) const {
			return control.BlReadEeprom(pBuffer, address, numBytes);
		};
};

class ControlCmdBlReadFlash : public ControlCmdBlRead
{
	public:
		ControlCmdBlReadFlash(void) : ControlCmdBlRead("flash") {};
		size_t Read(WiflyControl& control, unsigned char* pBuffer, unsigned int address, const size_t numBytes) const {
			return control.BlReadFlash(pBuffer, address, numBytes);
		};
};

class ControlCmdBlRunApp : public WiflyControlCmd
{
	public:
		ControlCmdBlRunApp(void) : WiflyControlCmd(
				string("'run_app' - start application and terminate bootloader")) {};

		virtual void Run(WiflyControl& control) const {
			cout << "Starting application... ";
			cout << (control.BlRunApp() ? "done." : "failed!") << endl;
		};
};

class ControlCmdStartBl : public WiflyControlCmd
{
	public:
		ControlCmdStartBl(void) : WiflyControlCmd(
				  string("'start_bl' - start bootloader and terminate application")) {};
				  
		virtual void Run(WiflyControl& control) const {
			cout << "Starting bootloader... ";
			control.StartBl();
		};
  
};

class ControlCmdClearScript : public WiflyControlCmd
{
	public:
		ControlCmdClearScript(void) : WiflyControlCmd(
				string("'clear' - clear script buffer")) {};

		virtual void Run(WiflyControl& control) const {
			control.ClearScript();
		};
};

class ControlCmdSetColor : public WiflyControlCmd
{
	public:
		ControlCmdSetColor(void) : WiflyControlCmd(
				string("'setcolor <addr> <rgb>'\n")
			+ string("    <addr> hex bitmask, which leds should be set to the new color\n")
			+ string("    <rgb> hex rgb value of the new color f.e. red: ff0000")) {};

		virtual void Run(WiflyControl& control) const {
			string addr, color;
			cin >> addr;
			cin >> color;
			control.SetColor(addr, color);
		};
};

class ControlCmdSetFade : public WiflyControlCmd
{
	public:
		ControlCmdSetFade(void) : WiflyControlCmd(
				string("'setfade <addr> <rgb> <time>'\n")
			+ string("    <addr> hex bitmask, which leds should be set to the new color\n")
			+ string("    <rgb> hex rgb value of the new color f.e. red: ff0000\n")
			+ string("    <time> the number of milliseconds the fade should take")) {};

		virtual void Run(WiflyControl& control) const {
			string addr, color;
			unsigned long timevalue;
			cin >> addr;
			cin >> color;
			cin >> timevalue;
			control.SetFade(addr, color, (unsigned short)timevalue * 1024);
		};
};

class WiflyControlCmdBuilder
{
	public:
		static const WiflyControlCmd* GetCmd(string name) {
			if("addcolor" == name) {
				return new ControlCmdAddColor();
			} else if("bl_info" == name) {
				return new ControlCmdBlInfo();
			} else if("clear" == name) {
				return new ControlCmdClearScript();
			} else if("crc_flash" == name) {
				return new ControlCmdBlCrcFlash();
			} else if("erase_flash" == name) {
				return new ControlCmdBlEraseFlash();
			} else if("read_eeprom" == name) {
				return new ControlCmdBlReadEeprom();
			} else if("read_flash" == name) {
				return new ControlCmdBlReadFlash();
			} else if("run_app" == name) {
				return new ControlCmdBlRunApp();
			} else if("setcolor" == name) {
				return new ControlCmdSetColor();
			} else if("setfade" == name) {
				return new ControlCmdSetFade();
			} else if("start_bl" == name) {
				return new ControlCmdStartBl();
			} else if("erase_eeprom" == name) {
				return new ControlCmdBlEraseEeprom();
			} else if("enable_bl_autostart" == name) {
				return new ControlCmdBlAutostartEnable();
			}
			return NULL;
		}
};
#endif /* #ifndef _WIFLYCONTROLCMD_H_ */

