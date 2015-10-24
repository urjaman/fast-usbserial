NOTE: This is a modified version of arduino-usbserial, and
was only tested on the ATmega16U2, and the below
readme is obsolete. On my 16U2 it is hit the 16U2 reset
with pliers and sudo make dfu :P

For better instructions (and an image) of how to flash your 16U2 / 8U2:
http://forum.arduino.cc/index.php?topic=52076.msg373873#msg373873
Also note that if your board is new enough (atleast the ones with 16U2 are) you dont
need to touch/ground the HWB (point near cap), just reset.
So you might as well see if it appears as DFU device (lsusb before and after) after only the reset.

To setup the project and upload the Arduino usbserial application firmware to an ATMEGA8U2 using the Arduino USB DFU bootloader:
2. set ARDUINO_MODEL_PID in the makefile as appropriate
3. do "make clean; make"
4. put the 8U2 into USB DFU mode:
4.a. assert and hold the 8U2's RESET line
4.b. assert and hold the 8U2's HWB line
4.c. release the 8U2's RESET line
4.d. release the 8U2's HWB line
5. confirm that the board enumerates as either "Arduino Uno DFU" or "Arduino Mega 2560 DFU"
6. do "make dfu" (OS X or Linux - dfu-programmer must be installed first) or "make flip" (Windows - Flip must be installed first)

Check that the board enumerates as either "Arduino Uno" or "Arduino Mega 2560".  Test by uploading a new Arduino sketch from the Arduino IDE.
