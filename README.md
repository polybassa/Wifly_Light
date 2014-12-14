Wy_Light
===========

Documentation: * [http://polybassa.github.com/Wy_Light/index.html

Toolchain: * [https://launchpad.net/gcc-arm-embedded/	
Version:  (arm-none-eabi-gcc -v)
gcc version 4.8.4 20140526 (release) [ARM/embedded-4_8-branch revision 211358] (GNU Tools for ARM Embedded Processors) 

Install:

./configure
make


Targets:

| test          | build and execute unit tests        |
| cc_fw         | build CC3200 firmware               |
| cc_bl         | build CC3200 bootloader             |
| sim           | build CC3200 bootloader simulator   |
| firmware_pic  | build PIC firmware                  |
| firmware_simu | build PIC simulator                 |
| cli           | build command line interface        |

Debug CC3200:
´´´
cd _debugging/
arm-none-eabi-gdb -tui -x gdbinit ../exe/WyLight_cc_fw.axf
´´´ 
