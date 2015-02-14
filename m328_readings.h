#ifndef m328_readings_h
#define m328_readings_h

#include <stdio.h>
#include <inttypes.h>


#include "defs.h"

/* TEMP*10 */
// https://code.google.com/p/tinkerit/wiki/SecretThermometer
long m328_read_Temp();


/* VCC*10 */
// https://code.google.com/p/tinkerit/wiki/SecretVoltmeter
long m328_read_Vcc();


void m328_print_readings(void);

#endif
