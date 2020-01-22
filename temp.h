#ifndef TEMP_H_
#define TEMP_H_



#define TEMP_NA -640






#ifdef __cplusplus /* If this is a C++ compiler, use C linkage */
extern "C" {
#endif

void temp_init(void);

/* non-blocking measurements */
void temp_request_start(void);
void temp_request_wait(void);
int16_t temp_request_print(void);

/* blocking simple measurement */
void temp_print(void);

/* print temp value */
void temp_print_value(int16_t t10);
void temp_print_value_long(long tn, long e);

/* last measurement data */
int16_t temp_get_last_known_t10(void);

#ifdef __cplusplus /* If this is a C++ compiler, use C linkage */
}
#endif



#endif /* TEMP_H_ */
