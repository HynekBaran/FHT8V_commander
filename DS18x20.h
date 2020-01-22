#ifndef DS18_H_
#define DS18_H_

#include "common.h"

#ifdef __cplusplus /* If this is a C++ compiler, use C linkage */
extern "C" {
#endif
  
  void dallas_temp_request(void);
  uint8_t dallas_temp_init(void);
  int16_t dallas_temp_print(void);
  int16_t dallas_temp10_get_last_known(void);

#ifdef __cplusplus /* If this is a C++ compiler, use C linkage */
}
#endif

#endif // DS18_H_
