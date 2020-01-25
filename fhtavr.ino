#include <OneWire.h>
#include <DallasTemperature.h>


#include "temp.h"

extern "C" {
  int fhtsetup();
  int fhtloop();
}

void setup() {
  temp_init();
  fhtsetup();
}

void loop() {

  fhtloop();
}
