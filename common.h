/*!
 * FHT heating valve comms example with RFM22/23 for AVR
 *
 * Copyright (C) 2007-2008, 2013 Mike Stirling
 *
 * The OpenTRV project licenses this file to you
 * under the Apache Licence, Version 2.0 (the "Licence");
 * you may not use this file except in compliance
 * with the Licence. You may obtain a copy of the Licence at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the Licence is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied. See the Licence for the
 * specific language governing permissions and limitations
 * under the Licence.
 *
 * \file common.h
 * \brief Common macros
 *
 */

#ifndef COMMON_H_
#define COMMON_H_

/*
 * Define standard types
 */

//! Defines a boolean type
typedef enum {
	False = 0,
	True
} bool_t;

#ifndef TRUE
//! Alternative to "True"
#define TRUE			(True)
#endif
#ifndef FALSE
//! Alternative to "False"
#define FALSE			(False)
#endif

#ifndef NULL
//! A NULL pointer value
#define NULL			(void*)(0)
#endif

// Define debugging macros.  These require extra include files
// which the following will pull in.
#include "defs.h"
#include <avr/pgmspace.h>
#ifdef DEBUG
#warning "DEBUG - pgmspace"
#include <stdio.h>
#include "debug.h"
#endif

#if defined DEBUG

#if DEBUG>1
#define DPRINTF(a,...)			{ fprintf_P(stderr,PSTR("# " __FILE__ "(%d): " a),__LINE__,##__VA_ARGS__); }
#else
#define DPRINTF(a,...)                   { printf_P(PSTR("# " a), ##__VA_ARGS__); }
#endif

#else //  not defined DEBUG
#define DPRINTF(a,...)                   {  }
#endif


#define PRINTF(a,...)                   { printf_P(PSTR(a), ##__VA_ARGS__); }


// classic old-style messages
#define MSG_TMP(a,...)                   { printf_P(PSTR("MSG TMP " a), ##__VA_ARGS__); }
#define LOG_TMP(a,...)                   { printf_P(PSTR("LOG TMP " a), ##__VA_ARGS__); }

//#define MSG_FHT(a,...)                   { printf_P(PSTR("MSG FHT " a), ##__VA_ARGS__); }
#define LOG_FHT(a,...)                   { printf_P(PSTR("LOG FHT " a), ##__VA_ARGS__); }

#define LOG_CLI(a,...)                   { printf_P(PSTR("CLI " a), ##__VA_ARGS__); }


#define MSG(a,...)                   { printf_P(PSTR("MSG " a), ##__VA_ARGS__); }



/*
 * Byte manipulation macros
 */

//! Select the low byte of a 16-bit word
#define UINT16_LOW(a)			(a & 0xff)

//! Select the high byte of a 16-bit word
#define UINT16_HIGH(a)			(a >> 8)

/*
 * Bit manipulation macros
 */

//! Set bits in a register
#define SET(reg, flags)			((reg) = (reg) | (flags))
//! Clear bits in a register
#define CLEAR(reg, flags)		((reg) &= ~(flags))

//! Determine if bits in a register are set
#define ISSET(reg, flags)		(((reg) & (flags)) == (flags))
//! Determine if bits in a register are clear
#define ISCLEAR(reg, flags)		(((reg) & (flags)) == 0)

//! Inserts a no-operation assembly instruction
#define nop()					asm volatile ("nop")

//! Provide access to the global tick counter.  Should be defined in main.
uint32_t get_tick_count(void);

#endif /*COMMON_H_*/
