#include <util/delay.h>
#include <stdint.h>

#include "common.h"

#include "DS18x20.h"
#include "si443x_min.h"
#include "m328_readings.h"

#include "temp.h"


/* 
Since troubles with printing of floats, all temperatures are converted to 
int16_t
and to get some accuracy the value is allways 10 times the real temperature.
*/

/*
Print the temperature given by 10*temp int value to the console
*/
void temp_print_value(int16_t t10) 
{
   if ( t10 == TEMP_NA ) {
      PRINTF("unknown");
   } else  {
     int16_t t, d;
     t = t10/10;
     d = t10 - 10*t;
     PRINTF("%d.%d", t, d);
   }
}

/*
Print the value given by tn*10^e long value to the console
*/
void temp_print_value_long(long tn, long e) // measured_value = tn / 10^e
{
   if ( tn == TEMP_NA ) {
      PRINTF("unknown");
   } else  {
     long t, d;
     t = tn/e;
     d = tn - e*t;
     PRINTF("%ld.%ld", t, d);
   }
}


/*
   Init all available temp devices
*/

void temp_init(void) 
{
	//  not needed for si443x, RFM chip is to be initialised in fht part of the code
	dallas_temp_init();
}



/*
   Request measurement of all available temp devices readings to the console
*/

void temp_request_start(void) 
{
	// si443x has no such feature
	dallas_temp_request();
}

/*
   Wait for all requested measurements
*/
void temp_request_wait(void) 
{
    _delay_ms(750);  
}

/*
  Print all measured values
*/
void temp_request_print(void) 
{
  m328_print_readings();	
  si443x_temp_print();
	dallas_temp_print();
}


/*
   Safely print all available temp devices readings to the console (blocking wait)
*/

void temp_print(void) 
{
  // TODO: Use m328 reading if Dallas not available?
  //DPRINTF("Temp request\n");
  temp_request_start();
  //DPRINTF("Temp delay\n");
  temp_request_wait();
  //DPRINTF("Temp ready to print\n");
  temp_request_print();
  return 0;
}

/*

For every temperature sensor interface, following functions must be defined:

uint8_t <dev>_temp_init() - initializes the sensing intrface & all devices, returns number of present sensors
int16_t <dev>_temp_print() - prints the measurements to the console, returns the last measured temperature*10

We have implemented the devices bellow.

RFM 22/23 onchip si443x sensor temperature:
  si443x_temp_init is not needed
  si443x_temp_print is implemented in si443x_min.c

Dallas sensor temperature:
  dallas_temp_* functions are implemented in DS18x20.cpp
*/



