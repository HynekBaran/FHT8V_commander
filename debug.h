/*!
 * FHT heating valve comms example with RFM22/23 for AVR
 *
 * Copyright (C) 2008, 2013 Mike Stirling
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
 * \file debug.h
 * \brief Un-buffered UART driver for debug IO
 *
 * Debug UART driver for ATMEL AVR ATMEGA.  Provides
 * integration with stdio and un-buffered input/output.
 *
 */

#ifndef DEBUG_H_
#define DEBUG_H_

#ifndef F_CPU
# warning "F_CPU not defined for <debug.h>"
# define F_CPU 1000000UL
#endif

/*
 * Module configuration
 */

/// Port number (where more than one available)
#define DEBUG_PORT			0

/// Desired baud rate
#define DEBUG_BAUD			9600

/// Number of data bits (5,6,7 or 8)
#define DEBUG_DATABITS		8

/// Number of stop bits (1 or 2)
#define DEBUG_STOPBITS		1

/// Parity (0 = none, 1 = odd, 2 = even)
#define DEBUG_PARITY		0

/// If the UART is to operate in high-speed mode
#define DEBUG_HIGHSPEED		1

/// If the UART is to operate in synchronous mode
#define DEBUG_SYNC			0

/// If the UART is to bind to stdio
#ifdef DEBUG
#define DEBUG_IS_STDIO		1
#else
#define DEBUG_IS_STDIO		0
#endif

/*
 * Internal macros
 */

/// Divider value for baud rate calculation
#if DEBUG_SYNC
#define DEBUG_DIV			2
#elif DEBUG_HIGHSPEED
#define DEBUG_DIV			8
#else
#define DEBUG_DIV			16
#endif

/// Calculated baud rate constant
#define DEBUG_BAUDCONST		(F_CPU/DEBUG_DIV/DEBUG_BAUD-1)

/*
 * Definitions
 */

/// Initialise the debug serial port and bind it to stdio if required
void debug_init(void);

/// Returns true if a character is available for reading
/// \return 0 if no character available, else 1
int debug_poll(void);

/// Returns a single character from the debug serial port, blocking until
/// one is available.
/// \return Received character
char debug_getc(void);

/// Sends a single character out of the debug serial port, blocking until
/// the transmission can commence.
/// \param c Character to send
void debug_putc(char c);

#endif /*DEBUG_H_*/
