
#include <OneWire.h>
#include <DallasTemperature.h>

#include "common.h"
#include "temp.h"


// OneWire DS18S20, DS18B20, DS1822 Temperature Example
//
// http://www.pjrc.com/teensy/td_libs_OneWire.html
//
// The DallasTemperature library can do all this work for you!
// http://milesburton.com/Dallas_Temperature_Control_Library
// https://github.com/milesburton/Arduino-Temperature-Control-Library

#include "board.h" // ONE_WIRE_BUS pin defined here

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);

// function to print a device address
void printAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    PRINTF("%02X", deviceAddress[i]);
  }
}

uint8_t dallas_temp_init_cpp(void)
{
  uint8_t devcount;
  // Start up the library
  PRINTF("Dallas Temperature: Setting up sensors...\n");
  //sensors.setWaitForConversion(false);
  sensors.begin();
  sensors.setResolution(12);
  devcount = sensors.getDeviceCount();
  PRINTF("Sensors set up, %u devices found.\n", devcount);
  return(devcount);
}

void dallas_temp_request_cpp(void)
{
  cli();
  sensors.requestTemperatures();  // issue a global temperature request to all devices on the bus
  sei();
}

int16_t dallas_temp10_get_last_known_value = TEMP_NA;

int16_t dallas_temp_print_cpp(void)
{ 
  uint8_t devcount;
  int16_t result = TEMP_NA;
  devcount = sensors.getDeviceCount();
  // DPRINTF("Requesting temperatures...\n");
  //sensors.requestTemperatures();  // issue a global temperature request to all devices on the bus
  // DPRINTF("Reading temperatures...\n");  
  for (uint8_t dev_index=0; dev_index<devcount; dev_index++) {
    DeviceAddress dev_addr;
    if (sensors.getAddress (dev_addr, dev_index)) {
      // device adress //PRINTF("Reading Dallas device of index %u: Address=", dev_index); printAddress(dev_addr);   PRINTF(". ");   
      
      // temperature
      float tf; 
      int16_t t10;   
      tf = sensors.getTempCByIndex(dev_index);
      t10 = (int16_t)(tf*10); 
      result = t10;
      //PRINTF("Temp="); temp_print_value(t10); PRINTF(" C. ");

      // resolution 
      //uint8_t dev_res = sensors.getResolution(dev_addr); //PRINTF("Resolution is %u. ", dev_res);
      
      // print data message
      MSG_TMP("LOCAL value='"); temp_print_value(t10); PRINTF("' unit='C' raw='%x' dev_type='DS18x20' dev_index='%u' dev_address='", t10, dev_index);
      printAddress(dev_addr); 
      PRINTF("'\n");      
    } else {
      LOG_TMP("0 Dallas device index %u is not available.\n", dev_index);
    }

  }
  dallas_temp10_get_last_known_value = result;
  return result; // the LAST device temperature
}

int16_t dallas_temp10_get_last_known_cpp(){
  return dallas_temp10_get_last_known_value;
  }

extern "C" { 

  uint8_t dallas_temp_init(void) {
     return dallas_temp_init_cpp();
  }	

  int16_t dallas_temp_print(void) {
     return dallas_temp_print_cpp();
  }
  
  void dallas_temp_request(void)
  {
    dallas_temp_request_cpp();
  }

  int16_t dallas_temp10_get_last_known(void) {
    return dallas_temp10_get_last_known_cpp();
  }


}

