/*
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
 * \file si443x_min.c
 * \brief Minimal polled driver for Si443x transceiver
 *
 * This is a cut down version of a larger driver.
 * It only supports raw FIFO mode and does not handle
 * packets larger than the radio's FIFO (64 bytes).
 *
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdint.h>

#include "si443x_min.h"
#include "board.h"
#include "common.h"
#include "temp.h"

static int radioStatus = 1 ; // 0=OK 1=not initialized 2=failed

int si443x_status(void)
{
  return radioStatus;
}

/***********************/
/* Radio configuration */
/***********************/

/* Configure radio to 868.35 MHz */

/*! Frequency of channel 0 (Hz) */
#define DEF_FREQUENCY_F0	(868 * MHZ)
/*! Channel spacing (kHz) */
#define DEF_CHANNEL_STEP	10
/*! Channel number */
#define DEF_CHANNEL			35

/* Bitrate is 5000 bps to give a bit period of 200 us, which is
 * the lowest common denominator of the OOK pulses used to
 * represent a 1 and 0 in the FHT protocol.
 */

#define DEF_BITRATE			5000

/* FHT is on/off keyed */

/*! Modulation mode */
#define DEF_MODULATION		MODTYP_OOK
/*! Modulation parameters */
#define DEF_MOD_FLAGS		MANCHESTER_OFF | WHITENING_OFF

/* Set transmitter power to maximum */

/*! Transmitter power */
#define DEF_TX_POWER		TXPOW_MAX


/*! Receiver parameters (from spreadsheet).  These are the values (in order)
 * for the following registers:
 * 0x1c - 0x25, 0x2a, 0x2c-0x2e
 */
#define DEF_RX_PARAMS		(uint8_t[]) { \
							0xc1, 0x40, 0x0a, 0x03, 0x96, 0x00, 0xda, 0x74, 0x00, 0xdc, \
							0x24, \
							0x28, 0xfa, 0x29 \
							}

/* Receiver is set to lock onto some additional preamble that we send, so
 * this won't receive packets from a real FHT transmitter, but it will receive
 * packets from another EzRadio-based transmitter.
 */

/*! Receiver minimum preamble length (nibbles) */
#define DEF_PREAMBLE_THRESH	2
/*! Sync word (left-justified if shorter than 32 bits) */
/* For FHT the preamble is 12 zero bits followed by a one, where
 * a zero is encoded as 1100 and a one as 111000.  This allows
 * us to derive a sync pattern as follows:
 * 11001100 11001100 11001100 11100011 (CCCCCCE3)
 * where the last two bits are part of the first data bit, but will
 * always be 11
 *
 * Unfortunately, this pattern occurs too late after the added AA
 * preamble, so it doesn't work.  Instead we will have to sync
 * on the first four CC bytes.
 */
#define DEF_SYNC_WORD		0xcccccccc
/*! Sync word length (bytes) */
#define DEF_SYNC_WORD_LEN	4


#define FIFO_SIZE			64

/********************************/
/* Board-specific configuration */
/********************************/

#define nSELECT				nTRX_SEL
#define nIRQ				nTRX_IRQ

/*! Macro to start device IO - must handle bus locking */
#define SELECT()			{ \
							CLEARP(nSELECT); \
							}
/*! Macro to end device IO - must handle bus locking */
#define DESELECT()			{ \
							SETP(nSELECT); \
							}

/* Define either LOW_BAND or HIGH_BAND before including si443x_regs.h */
#if (DEF_FREQUENCY_F0) < (480 * MHZ)
	#define LOW_BAND
#else
	#define HIGH_BAND
#endif

#include "si443x_regs.h"

/*************************/
/* Private helper macros */
/*************************/

/* These are deliberately kept private for code size reasons (most calculations collapse to a
 * constant in the preprocessor).  If they are to be exported then this should be done through
 * a wrapper function to avoid the need to make the register definitions public (which may lead
 * to namespace issues).
 */

/* Device identification */

#define SUPPORTED_DEVICE_TYPE			0x08
#define SI443X_DEVICE_TYPE()			(si443x_read8(R_DEVICE_TYPE) & DT_MASK)

#define SUPPORTED_DEVICE_VERSION		0x06
#define SI443X_DEVICE_VERSION()			(si443x_read8(R_DEVICE_VERSION) & VC_MASK)


/* Interrupt status read/clear */

#define SI443X_STATUS()					(si443x_read16(R_INT_STATUS))


/* Operating mode selection */

#define SI443X_SWRESET()				si443x_write8(R_OP_CTRL1, SWRES)
#define SI443X_MODE_STANDBY()			si443x_write8(R_OP_CTRL1, 0)
#define SI443X_MODE_SLEEP()				si443x_write8(R_OP_CTRL1, ENWT)
#define SI443X_MODE_SENSOR()			si443x_write8(R_OP_CTRL1, ENLBD)
#define SI443X_MODE_READY()				si443x_write8(R_OP_CTRL1, XTON)
#define SI443X_MODE_TUNE()				si443x_write8(R_OP_CTRL1, PLLON)
#define SI443X_MODE_TX()				si443x_write8(R_OP_CTRL1, TXON | XTON)
#define SI443X_MODE_RX()				si443x_write8(R_OP_CTRL1, RXON | XTON)


/* FIFOs */

#define SI443X_CLEAR_TX_FIFO()			{ \
										si443x_write8(R_OP_CTRL2, FFCLRTX); \
										si443x_write8(R_OP_CTRL2, 0); \
										}
#define SI443X_CLEAR_RX_FIFO()			{ \
										si443x_write8(R_OP_CTRL2, FFCLRRX); \
										si443x_write8(R_OP_CTRL2, 0); \
										}

#define SI443X_SET_RX_FIFO_FULL_THRESH(n)	si443x_write8(R_RX_FIFO_CTRL, (n) & RXAFTHR_MASK)

/* Radio parameters */

#define SI443X_SET_F0(hz)				{ \
										si443x_write8(R_BAND_SELECT, FB(hz)); \
										si443x_write16(R_CARRIER_FREQ, FC(hz)); \
										}
#define SI443X_SET_OFFSET(hz)			si443x_write16(R_FREQ_OFFSET, FO(hz))
#define SI443X_SET_CHANNEL_STEP(khz)	si443x_write8(R_STEP, (khz) / 10)
#define SI443X_SET_CHANNEL(n)			si443x_write8(R_CHANNEL, n)
#define SI443X_SET_MODULATION(mode)		si443x_write8(R_MOD_CTRL2, TRCLK_NONE | DTMOD_FIFO | ((mode) & MODTYP_MASK))

/*! For data rates below 30 kbps */
#define SI443X_SET_RATE_LOW(bps, flags)	{ \
										si443x_write16(R_TX_RATE, TXDR_LOW(bps)); \
										si443x_write8(R_MOD_CTRL1, TXDTRTSCALE | (flags)); \
										}
/*! For data rates above 30 kbps */
#define SI443X_SET_RATE_HIGH(bps, flags) { \
										si443x_write16(R_TX_RATE, TXDR_HIGH(bps)); \
										si443x_write8(R_MOD_CTRL1, (flags)); \
										}
/*! Modulation flags (pass to SET_RATE_x) */
#define MANCHESTER_OFF					(0)
#define MANCHESTER_ON_01				(ENMANCH)
#define MANCHESTER_ON_10				(ENMANCH | ENMANINV)
#define WHITENING_OFF					(0)
#define WHITENING_ON					(ENWHITE)

/* Receiver parameters */

#define SI443X_RSSI()					(RSSI_DBM(si443x_read8(R_RSSI)))
#define SI443X_SET_PREAMBLE_THRESH(th)	(si443x_write8(R_PREAMBLE_CTRL, PREATH(th)))


/* Transmitter parameters */

#define SI443X_SET_POWER(dbm)			si443x_write8(R_TX_POWER, TXPOW(dbm) | LNA_SW)


/* GPIO */

#define SI443X_SET_GPIO_MODE(p, m)		si443x_write8(R_GPIO0_CFG + (p), (m))

/*********************/
/* Private functions */
/*********************/

/*! Perform a single byte SPI exchange */
static inline uint8_t si443x_io(uint8_t data)
{
	SPDR = data;
	while (!(SPSR & _BV(SPIF)));
	return SPDR;
}

/*! Write to 8-bit register */
static void si443x_write8(uint8_t addr, uint8_t val)
{
	SELECT();
	si443x_io((addr & 0x7f) | WRITE);
	si443x_io(val);
	DESELECT();
}

/*! Write to 16-bit big-endian register pair */
static void si443x_write16(uint8_t addr, uint16_t val)
{
	SELECT();
	si443x_io((addr & 0x7f) | WRITE);
	si443x_io(val >> 8);
	si443x_io(val);
	DESELECT();
}

/*! Write to 32-bit big-endian register group */
static void si443x_write32(uint8_t addr, uint32_t val)
{
	SELECT();
	si443x_io((addr & 0x7f) | WRITE);
	si443x_io(val >> 24);
	si443x_io(val >> 16);
	si443x_io(val >> 8);
	si443x_io(val);
	DESELECT();
}

/*! Read from 8-bit register */
static uint8_t si443x_read8(uint8_t addr)
{
	uint8_t val;

	SELECT();
	si443x_io((addr & 0x7f) | READ);
	val = si443x_io(0);
	DESELECT();

	return val;
}

/*! Read from 16-bit big-endian register pair */
static uint16_t si443x_read16(uint8_t addr)
{
	uint16_t val;

	SELECT();
	si443x_io((addr & 0x7f) | READ);
	val = (uint16_t)si443x_io(0) << 8;
	val |= (uint16_t)si443x_io(0);
	DESELECT();

	return val;
}

#if 0 // not used
/*! Read from 32-bit big-endian register group */
static uint32_t si443x_read32(uint8_t addr)
{
	uint32_t val;

	SELECT();
	si443x_io((addr & 0x7f) | READ);
	val = (uint32_t)si443x_io(0) << 24;
	val |= (uint32_t)si443x_io(0) << 16;
	val |= (uint32_t)si443x_io(0) << 8;
	val |= (uint32_t)si443x_io(0);
	DESELECT();

	return val;
}
#endif

static void si443x_standby(void)
{
	/* Return to standby */
	SI443X_MODE_STANDBY();

	/* Flush */
	SI443X_CLEAR_RX_FIFO();
	SI443X_CLEAR_TX_FIFO();

	/* Clear and disable any enabled flags */
	SI443X_STATUS();
	si443x_write16(R_INT_ENABLE, 0);
}

static void si443x_set_demod_params(uint8_t *params)
{
	int n;

	/* 0x1c - 0x25 */
	SELECT();
	si443x_io(0x1c | WRITE);
	for (n = 0; n < 10; n++)
		si443x_io(*params++);
	DESELECT();

	/* 0x2a */
	SELECT();
	si443x_io(0x2a | WRITE);
	si443x_io(*params++);
	DESELECT();

	/* 0x2c - 0x2e */
	SELECT();
	si443x_io(0x2c | WRITE);
	for (n = 0; n < 3; n++)
		si443x_io(*params++);
	DESELECT();
}

/********************/
/* Public functions */
/********************/

int si443x_init(void)
{
	uint8_t device, version;

	/* Configure SPI interface */
	/* CPOL = 0 (idle low), CPHA = 0 (sample on rising edge) */
	DESELECT();
	SPCR = 0;
	SPSR = 0;
	SPCR = _BV(SPE) | _BV(MSTR); // 2 MHz clock (/4)

	/* Check for supported device */
	device = SI443X_DEVICE_TYPE();
	version = SI443X_DEVICE_VERSION();
	LOG_FHT("1 RADIO Found device type %d version %d\n", device, version);
	if (device != SUPPORTED_DEVICE_TYPE || version != SUPPORTED_DEVICE_VERSION) {
		LOG_FHT("1 RADIO ERROR: Unsupported/missing radio\n");
		radioStatus = -1;
		return -1;
	}

	/* Software reset - poll for completion */
	LOG_FHT("2 RADIO Resetting radio...\n");
	SI443X_SWRESET();
	while (INP(nIRQ));
	SI443X_STATUS(); /* Clear interrupt flag */
	LOG_FHT("2 RADIO Done\n");

	/* Go to standby */
	si443x_standby();

	/* Function control options */
	/* ANTDIVxxx, RXMPK, AUTOTX, ENLDM */
	si443x_write8(R_OP_CTRL2, 0);

	/* Configure default radio parameters */
	SI443X_SET_F0(DEF_FREQUENCY_F0);				/* Base frequency in Hz (868 MHz) */
	SI443X_SET_CHANNEL_STEP(DEF_CHANNEL_STEP);		/* Channel step in kHz (10) */
	SI443X_SET_CHANNEL(DEF_CHANNEL);				/* Channel number (35 = 868.35 MHz) */
	SI443X_SET_OFFSET(0);							/* Offset = 0 */
	SI443X_SET_MODULATION(DEF_MODULATION);			/* Modulation = OOK */
#if (DEF_BITRATE) < TXDTRT_SCALE_MAX
	SI443X_SET_RATE_LOW(DEF_BITRATE, DEF_MOD_FLAGS);	/* Bitrate = 5000 bps, 200 us bit period */
#else
	SI443X_SET_RATE_HIGH(DEF_BITRATE, DEF_MOD_FLAGS);
#endif
	SI443X_SET_POWER(DEF_TX_POWER);					/* Maximum power */

	/* Demodulator parameters from spreadsheet */
	si443x_set_demod_params(DEF_RX_PARAMS);

	/* AGC enable */
	si443x_write8(R_AGC_OVERRIDE1, SGIN | AGCEN);

	/* Configure preamble and sync for receiver */
	SI443X_SET_PREAMBLE_THRESH(DEF_PREAMBLE_THRESH);
	si443x_write8(R_HEADER_CTRL2, SYNCLEN(DEF_SYNC_WORD_LEN));
	si443x_write32(R_SYNC_WORD, DEF_SYNC_WORD);

	/* Turn off packet handling.  The receiver still handles clock recovery and
	 * sync for us */
	si443x_write8(R_DATA_ACCESS_CTRL, 0);

	/* For RFM22B, set GPIO0/1 up for T/R switching */
	SI443X_SET_GPIO_MODE(0, GPIO_RX_STATE);
	SI443X_SET_GPIO_MODE(1, GPIO_TX_STATE);

	radioStatus = 0;
	return 0;
}

int si443x_receive(uint8_t *data, uint8_t data_length, int timeout, int *rssi)
{
	uint32_t give_up_time = get_tick_count() + timeout;

	if (data_length > FIFO_SIZE) {
		DPRINTF("Packet too large\n");
		return -1;
	}

	/* Get into known state, clear FIFOs */
	si443x_standby();

	/* Set FIFO threshold to desired length */
	SI443X_SET_RX_FIFO_FULL_THRESH(data_length - 1);

	/* Enable interrupt flag on sync detect */
	si443x_write16(R_INT_ENABLE, ENSWDET);

	/* Enter receive mode */
	DPRINTF("Waiting for rx\n");
	SI443X_STATUS(); /* clear status */
	SI443X_MODE_RX();

	/* Wait for received sync or timeout */
	while (INP(nIRQ)) {
		if (timeout && get_tick_count() >= give_up_time) {
			DPRINTF("Rx timed out\n");
			goto abort;
		}
	}
	SI443X_STATUS(); /* clear status */
	DPRINTF("Synced\n");

	/* Save rssi */
	*rssi = SI443X_RSSI();

	/* Enable interrupt on FIFO threshold */
	si443x_write16(R_INT_ENABLE, ENRXFFAFULL);

	/* Receive until required number of bytes received */
	while (INP(nIRQ)) {
		if (timeout && get_tick_count() >= give_up_time) {
			DPRINTF("Rx timed out\n");
			goto abort;
		}
	}
	SI443X_STATUS(); /* clear status */

	/* Turn off receiver but don't flush FIFOs */
	SI443X_MODE_STANDBY();

	/* Read the requested number of bytes from the FIFO */
	DPRINTF("receiving %hu bytes\n", data_length);

	/* Read from FIFO */
	SELECT();
	si443x_io(R_FIFO | READ);
	while (data_length--) {
		*data++ = si443x_io(0);
	}
	DESELECT();

	/* Clean up*/
	si443x_standby();
	return 0;

abort:
	si443x_standby();
	return -1;
}

int si443x_transmit(uint8_t *data, uint8_t data_length)
{
	if (data_length > FIFO_SIZE) {
		DPRINTF("Packet too large\n");
		return -1;
	}

	/* Get into known state, clear FIFOs */
	si443x_standby();

	/* Push data to FIFO */
	//DPRINTF("Writing %hu bytes to tx FIFO\n", data_length);
	SELECT();
	si443x_io(R_FIFO | WRITE);
	while (data_length--) {
		si443x_io(*data++);
	}
	DESELECT();

	/* Enable interrupt flag on packet sent */
	si443x_write16(R_INT_ENABLE, ENPKSENT);

	/* Start transmitter - in raw FIFO mode the tx will
	 * run until the FIFO has been drained.  The length register
	 * does not need to be programmed. */
	//DPRINTF("Enabling tx\n");
	SI443X_STATUS();
	SI443X_MODE_TX();

	/* Wait for completion - poll interrupt pin */
	while (INP(nIRQ));
	si443x_standby();
	//if (DEBUG > 1) DPRINTF("Tx complete\n");

	return 0;
}

void si443x_dump(void)
{
	uint8_t val;
	int n;

	/* Dump all registers */
	printf_P(PSTR("     00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F\n"));
	for (n = 0; n < 0x7f; n++) {
		val = si443x_read8(n);
		if ((n & 15) == 0)
			printf_P(PSTR("0%X : "), n >> 4);
		if (val < 0x10)
			printf_P(PSTR("0")); /* deal with broken avrlibc printf */
		printf_P(PSTR("%X "), val);
		if ((n & 15) == 15)
			printf_P(PSTR("\n"));
	}
	printf_P(PSTR("\n"));
}


/* get temperature
   temp = return_value * 0.5 - 64
*/
int16_t si443x_get_temperature(void) {
  
        cli();
        // set adc input and reference
        si443x_write8(0x0f, 0x00 | (1 << 6) | (1 << 5) | (1 << 4));
        // set temperature range -64 to 64 C, ADC8 LSB: 0.5 C
        si443x_write8(0x12, 0x00 | (1 << 5));
        // adcstart
        si443x_write8(0x0f, 0x00 | (1 << 7));
        // wait for adc_done
        while (!si443x_read8(0x0f) & (1 << 7));
        // get adc value
        uint8_t raw_temp = si443x_read8(0x11); // temp in C = raw_temp * 0.5 - 64
        sei();
        // return value must be divided by 10 to obtain temp in Celsius
        if (raw_temp == 255) { return TEMP_NA ;} 
        else { return( (raw_temp * 5) - 640); }
}

/*
print the on-chip sensor temperature to the console
*/
int16_t si443x_temp_print(void)
{
   int16_t t10 = si443x_get_temperature();
   // print data message
   MSG_TMP("LOCAL value='"); temp_print_value(t10); PRINTF("' unit='C' raw='%x' dev_type='si443'\n", t10);   
   
   return (t10);
}
