#include "m328_readings.h"


#include "common.h"
#include "temp.h"


#include <Arduino.h> // ???




/* TEMP 10000*C */
// https://code.google.com/p/tinkerit/wiki/SecretThermometer
long m328_read_Temp() {
  long result;
  // Read temperature sensor against 1.1V reference
  ADMUX = _BV(REFS1) | _BV(REFS0) | _BV(MUX3);
  delay(5); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Convert
  while (bit_is_set(ADCSRA,ADSC));
  result = ADCL;
  result |= ADCH<<8;
  result = (result - 125) * 1075 ; 
  return result;

}

/* VCC*1000 */
// https://code.google.com/p/tinkerit/wiki/SecretVoltmeter
long m328_read_Vcc() {
  long result;
  // Read 1.1V reference against AVcc
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(5); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Convert
  while (bit_is_set(ADCSRA,ADSC));
  result = ADCL;
  result |= ADCH<<8;
  result = 1126400L / result; // Back-calculate AVcc in mV
  return result; 
}

/*
Print m328 readings
*/

void m328_print_readings(void) {
  long temp, vcc;
  temp = m328_read_Temp();
  vcc = m328_read_Vcc();
  
  MSG_TMP("LOCAL value='"); temp_print_value_long(temp,10000);  PRINTF("' unit='C' dev_type='m328'\n");
  MSG("VCC LOCAL value='%ld' unit='mV' dev_type='m328'\n", vcc);
}
