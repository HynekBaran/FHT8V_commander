#!/bin/sh
# dude-it.sh simple script to avrdude directly no ttyUSB0 (DTR must be connected), 
# use Sketch : Export compiled Binary  in Arduino IDE to generate fhtavr.ino.eightanaloginputs.hex
avrdude -v -patmega328p -carduino -P/dev/ttyUSB0 -b57600 -D -Uflash:w:fhtavr.ino.eightanaloginputs.hex:i 