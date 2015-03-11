/*
   HB (c) Hynek Baran
*/

#ifndef DEBUG 
#define DEBUG 1
# warning tweaking a little
// missing defines in Arduino IDE

#if ARDUINO==152
#define strcmp_PF strcmp_P
#endif

#define __AVR_ATmega328__
#endif
