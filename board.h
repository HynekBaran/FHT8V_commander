/*!
 * FHT heating valve comms example with RFM22/23 for AVR
 *
 * Copyright (C) 2013 Mike Stirling
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
 * \file board.h
 * \brief IO definitions for proprietary wireless sensor board
 *
 */

#ifndef BOARD_H_
#define BOARD_H_

#include <avr/io.h>

/*
 * This is for targeting the currently unreleased wireless sensor hardware,
 * which is why some of the pin definitions have been redacted.  The board
 * uses an ATMEGA328 so targeting an Arduino should be straightforward,
 * possibly without changing anything here.
 * The RFM22/23 is connected to the SPI bus as shown below.  Interrupt
 * is connected to PD2 (/INT0).  The I2C bus is unused in this application.
 */
 

/* Dallas DS18x20 temperature chip pin */
// In <OneWire.h> notation!
#define ONE_WIRE_BUS 2 // Data wire is plugged into port 3 on the Arduino (ATmega328 chip port D3 pin 5)

#define LED_TRX 	D5
#define LED_GREEN   D3 //HB
#define LED_RED 	D6 //HB

//#define TRX_SDN			C,5,1 // may be connected directly to the ground thus B0 is available for general use


/* Standard pin definitions */

#define B0					B,0,1
#define nTRX_IRQ			B,1,1 
#define nTRX_SEL			B,2,1
#define TRX_MOSI			B,3,1
#define TRX_MISO			B,4,1
#define TRX_SCK				B,5,1

#define C3					C,3,1
#define I2C_SDA				C,4,1
#define I2C_SCL				C,5,1

#define DEBUG_RXD			D,0,1
#define DEBUG_TXD			D,1,1
// D2 is used for OneWire
#define D3					D,3,1
#define D5					D,5,1
#define D6					D,6,1
#define D7					D,7,1

/*
 * Input definitions for each hardware port
 */

#define PORTB_INS			(MASK(B0) | MASK(TRX_MISO) | MASK(nTRX_IRQ))
#define PORTC_INS			(0)

#ifdef LED_TRX
//HB #define PORTD_INS			(MASK(DEBUG_RXD) | MASK(D3))
#define PORTD_INS			(MASK(DEBUG_RXD)) //HB
#else
//HB #define PORTD_INS			(MASK(DEBUG_RXD) | MASK(D3) | MASK(D5))
#define PORTD_INS			(MASK(DEBUG_RXD)| MASK(D5)) //HB
#endif

#define PORTB_PULLUP		(MASK(TRX_MISO) | MASK(nTRX_IRQ))
#define PORTC_PULLUP		(0)
//HB #define PORTD_PULLUP		(MASK(DEBUG_RXD) | MASK(D3))
#define PORTD_PULLUP		(MASK(DEBUG_RXD)) //HB

/*
 * Output definitions for each hardware port
 */
// Gate B
#define PORTB_OUTS			(MASK(nTRX_SEL) | MASK(TRX_MOSI) | MASK(TRX_SCK))
// Gate C
#ifdef TRX_SDN
#define PORTC_OUTS			(MASK(TRX_SDN)  | MASK(C3) | MASK(I2C_SDA) | MASK(I2C_SCL))
#else
#define PORTC_OUTS			(MASK(C3) | MASK(I2C_SDA) | MASK(I2C_SCL))
#endif
// Gate D
#ifdef LED_TRX
//HB #define PORTD_OUTS			(MASK(DEBUG_TXD) | MASK(D6) | MASK(D7)))
#define PORTD_OUTS			(MASK(DEBUG_TXD) | MASK(D3) | MASK(D6) | MASK(D7))) //HB
#else
//HB #define PORTD_OUTS			(MASK(DEBUG_TXD) | MASK(LED_TRX) | MASK(D6) | MASK(D7))) 
#define PORTD_OUTS			(MASK(DEBUG_TXD) | MASK(LED_TRX) |  MASK(D3) | MASK(D6) | MASK(D7))) //HB 

#endif

// Gate B
#define PORTB_INITIAL		(MASK(nTRX_SEL))
// Gate C
#ifdef TRX_SDN
#define PORTC_INITIAL		(MASK(TRX_SDN) | MASK(C3) | MASK(I2C_SDA) | MASK(I2C_SCL))
#else
#define PORTC_INITIAL		(MASK(C3) | MASK(I2C_SDA) | MASK(I2C_SCL))
#endif
// Gate D
#ifdef LED_TRX
#define PORTD_INITIAL		(MASK(LED_TRX) | MASK(DEBUG_TXD) ) //
#else
#define PORTD_INITIAL		(MASK(DEBUG_TXD))
#endif


/*
 * Initial DDR values - unused pins are set to outputs to save power
 */

#define DDRB_VAL			(0xff & ~PORTB_INS)
#define DDRC_VAL			(0xff & ~PORTC_INS)
#define DDRD_VAL			(0xff & ~PORTD_INS)

/*
 * Initial port values
 */

#define PORTB_VAL			(PORTB_PULLUP | PORTB_INITIAL)
#define PORTC_VAL			(PORTC_PULLUP | PORTC_INITIAL)
#define PORTD_VAL			(PORTD_PULLUP | PORTD_INITIAL)

/*
 * Helper macros
 */

#define MASK_(port,pin,sz)		(((1 << (sz)) - 1) << (pin))
#define INP_(port,pin,sz)		((PIN##port >> (pin)) & ((1 << (sz)) - 1))
#define OUTP_(port,pin,sz,val)	(PORT##port = (PORT##port & ~MASK_(port,pin,sz)) | (((val) << (pin)) & MASK_(port,pin,sz)))
#define SETP_(port,pin,sz)		(PORT##port |= MASK_(port,pin,sz))
#define CLEARP_(port,pin,sz)	(PORT##port &= ~MASK_(port,pin,sz))
#define TOGGLEP_(port,pin,sz)	(PORT##port ^= MASK_(port,pin,sz))

/*! Return bit mask for named IO port */
#define MASK(id)				MASK_(id)
/*! Return the current value of the named IO port */
#define INP(id)					INP_(id)
/*! Sets the named IO port to the specified value */
#define OUTP(id,val)			OUTP_(id,val)
/*! Turns on the named IO port */
#define SETP(id)				SETP_(id)
/*! Turns off the named IO port */
#define CLEARP(id)				CLEARP_(id)
/*! Toggled the named IO port */
#define TOGGLEP(id)				TOGGLEP_(id)

/*
 * IO macros
 */

#ifdef TRX_SDN
#define TRX_OFF()				SETP(TRX_SDN)
#define TRX_ON()				CLEARP(TRX_SDN)
#else
#define TRX_OFF()				{}
#define TRX_ON()				{}
#endif


#ifdef LED_TRX
#define LED_TRX_OFF()				CLEARP(LED_TRX)
#define LED_TRX_ON()				SETP(LED_TRX)
#else
#define LED_TRX_OFF()				{}
#define LED_TRX_ON()				{}
#endif

//HB:
#ifdef LED_GREEN
#define LED_GREEN_OFF()				CLEARP(LED_GREEN) 
#define LED_GREEN_ON()				SETP(LED_GREEN)   
#else
#define LED_GREEN_OFF()				{} 
#define LED_GREEN_ON()				{}   
#endif

#ifdef LED_RED
#define LED_RED_OFF()				CLEARP(LED_RED) 
#define LED_RED_ON()				SETP(LED_RED)   
#else
#define LED_RED_OFF()				{} 
#define LED_RED_ON()				{}   
#endif
//:HB


#endif /*BOARD_H_*/
