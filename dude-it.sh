#!/bin/bash
# dude-it.sh simple script to avrdude directly no ttyUSB0 (DTR must be connected), 
# use Sketch : Export compiled Binary  in Arduino IDE to generate fhtavr.ino.eightanaloginputs.hex
fuser /dev/ttyUSB0
read -rsn1 -p"Press enter to stop fhem service";echo
service fhem stop
fuser /dev/ttyUSB0
read -rsn1 -p"Press enter to reflash /dev/ttyUSB0";echo
avrdude -v -patmega328p -carduino -P/dev/ttyUSB0 -b57600 -D -Uflash:w:fhtavr.ino.eightanaloginputs.hex:i 
