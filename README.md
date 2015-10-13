
FHT8V_commander
===============

A collection of low-level routines for driving Conrad/ELV FHT8V radiator valve heads 
from an arbitrary "master" device (PC, Raspberry Pi, Linux router) equipped by serial interface (or USB).

Master computer | --- serial ---> | FHT8V_commander | --- radio --->| FHT8V valve
----------------|-----------------|-----------------|---------------|------------
                |                 |                 |--- radio ---> | FHT8V valve
                |                 |                 |      ...	    |

    

The brief description
======================

Commands given on serial interface (USB may be used) are transmitted by radio to FHT8V valves. 

HW is ATmega328 (Arduino [Pro Mini] may be used) equipped by RFM22/23B radio chip. 

Implementation of FHT8V radio protocol is based on kind help of  Mike Stirling and his code available under Apache 2.0 licence at
http://www.mike-stirling.com/2013/02/implementing-the-elv-fht-protocol-with-an-rfm23/

Besides of commands to FHT8V valves, some little measurements (onboard m328 temperature and Vcc, attached Dallas DS18x20 chip) can be read by CLI. 

Consider using http://fhem.de as a server on master device. See FHEM subdirecory to see how to use it.

Interfaces
==========

FHT8V_commander has two main interfaces:

1. CLI (command line interface) on its serial port (connected to master device)
2. Radio interface which drives Conrad/ELV FHT8V radiator valve(s).

Commands given on CLI are radio-transmitted to the valves.


Hardware
========

1. The main platform is Atmel AVR Atmega328P microprocessor running at 8 Mz, voltage is 3.3 V max. 
You may use our own device, breadboarded m328 (8 MHz crystal needed) or buy compatible Arduino (Mini Pro 8 MHz 3.3 V is well tested).  

2. RFM22B / RFM23B radio module. Take care of voltage, since RFM22/23B radios cannot run on 5V!

3. Moreover, some additional hardware may be connected and the measurements read by CLI. 
By the current FHT8V_commander source are already supported Dallas DS18x20 temperature sensors, 
but any general sensors with public Arduino library (e.g. DHTxx temperature and humidity sensors) may be easily implemented.



Installation and setup
======================

0. Make your hardware. The default RFM radio to m328/Arduino pin connection is

TRX_MOSI <---> B3<p>
TRX_MISO <---> B4<p>
TRX_SCK	 <---> B5<p>
<p>
TRX_SDN  <---> GND* <p>
<p>
nTRX_IRQ <---> R 1k <---> B1* <p>
nTRX_SEL <---> B2* <---> R 4k7 (pullup) <---> Vcc<p>
<p>           
Vcc <---> Vcc<p> 
GND <---> GND<p>

Pins marked (*) may be changed and configured in <code>boards.h</code>


1. Setup pin connections in <code>boards.txt</code> (additional hardware sensors may be configured here, too)

2. Compile and upload the sketch using Arduino IDE application, version 1.5.2. Other versions were not tested.

4. Use and enjoy. 


Usage
=====
See http://www.mike-stirling.com/notes/fht8v-protocol/ for explanation of FHT protocol. 
To use serial line, <code>sudo screen /dev/ttyAMA0  9600</code> or <code>sudo screen /dev/USB0  9600</code> is a choice.
On Raspbery Pi, consult https://github.com/lurch/rpi-serial-console to see how to enable Raspi onboard serial console (no USB needed).
Remember to set newline as line terminator.

1. If you want just test our stuff, let <i>grp</i> bellow be simply 1. Issue <code>fht groups 1</code> command and continue to the step 2.<p>
   The same rule apply if you have single room or you want to drive all valves in the house in global manner.<p>
   Optional: If you have more rooms and you want to regulate them separatedly:<p>
	   1.1 issue <code>fht groups <i>groups_number</i></code> command and<p>
	   1.2 repeat the whole procedure with <i>grp</i> equal to 1, 2, 3, ... up the number of your rooms.

2. From serial console (9600 baud), send command
<code>fht hc <i>grp</i> <i>hc1</i> <i>hc2</i></code>
where <i>hc1</i> and <i>hc2</i> are appropriate (decimal) house codes - e.g. 10 11.
This command saves your valve fht home code to be used in further communication with valves to ATMEGA EEPROM.

3. If your radiator valve is not powered on or never been used before, power up the valve, and after initial whir, click button once - wait for valve to calibrate.

4. Press and hold button on the valve until you hear three beeps. Valve is waiting for PAIR command.

5. Send command
<code>fht pair <i>grp</i></code>
(you will hear another multi-tone beep from the valve). 
The valve knows its actual home code now.

6. Valve should now be synced. Try sending beep command
<code>fht beep <i>grp</i></code>
wait upto 2 minutes for time slot to come around until you hear a beep from the valve.

7. To set valve opening value, send command
<code>fht set <i>grp</i> <i>value</i></code>
and wait up to 2 minutes.

8. After each ATMEGA restart, all groups are synced automatically. 
If our valves get out of sync, use <code>fht sync</code> command to resync whole system.