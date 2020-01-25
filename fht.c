/*
   FHT heating valve comms example with RFM22/23 for AVR

   Copyright (C) 2013 Mike Stirling

   The OpenTRV project licenses this file to you
   under the Apache Licence, Version 2.0 (the "Licence");
   you may not use this file except in compliance
   with the Licence. You may obtain a copy of the Licence at

   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing,
   software distributed under the Licence is distributed on an
   "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
   KIND, either express or implied. See the Licence for the
   specific language governing permissions and limitations
   under the Licence.

   \file fht.c
   \brief FHT protocol implementation for RFM22/23

   This is an implementation of the FHT8V valve protocol for SiLabs
   EzRadioPro devices such as found on HopeRF RFM22 and RFM23 modules.

   It is not extensively tested and is primarily a proof of concept.

*/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdint.h>
#include <string.h>

#include "board.h"
#include "common.h"
#include "si443x_min.h"
#include "fht.h"
#include "fht_eeprom.h"
#include "DS18x20.h"
#include "temp.h"

/*! Number of ticks to remain in sync mode (must be even) */
#define SYNC_TICKS		240
/*! Period (in ticks) for timeslot 0 - the period increases by
   one tick for each higher slot */
#define PERIOD_BASE		230

static volatile fht_msg_t g_message  [FHT_GROUPS_DIM];
static volatile uint8_t g_slot_count [FHT_GROUPS_DIM];
static volatile uint16_t g_ticks = 0;
static volatile uint32_t g_last_command_enqueued_time = 0;
static volatile uint8_t g_nbits;

static volatile uint8_t g_freezingMode = 0;

static void print_uptime(unsigned long seconds)
{
  unsigned long secs = seconds;
  unsigned long days = 0;
  unsigned long hours = 0;
  unsigned long mins = 0;
  mins = secs / 60; //convert seconds to minutes
  hours = mins / 60; //convert minutes to hours
  days = hours / 24; //convert hours to days
  secs = secs - (mins * 60); //subtract the coverted seconds to minutes in order to display 59 secs max
  mins = mins - (hours * 60); //subtract the coverted minutes to hours in order to display 59 minutes max
  hours = hours - (days * 24); //subtract the coverted hours to days in order to display 23 hours max
  //Display results
  LOG_FHT("1 Uptime  %lu days %lu:%lu:%lu\n", days, hours, mins, secs);
}

static void cmddump(fht_msg_t *msg)
{
  switch (msg->command & 0xf) {
    case FHT_SYNC_SET:
      printf_P(PSTR("SYNC_SET %u"), msg->extension);
      break;
    case FHT_VALVE_OPEN:
      printf_P(PSTR("VALVE_OPEN"));
      break;
    case FHT_VALVE_CLOSE:
      printf_P(PSTR("VALVE_CLOSE"));
      break;
    case FHT_VALVE_SET:
      printf_P(PSTR("VALVE_SET %u"), msg->extension);
      break;
    case FHT_OFFSET:
      printf_P(PSTR("OFFSET %d"), msg->extension);
      break;
    case FHT_DESCALE:
      printf_P(PSTR("DESCALE"));
      break;
    case FHT_SYNC:
      printf_P(PSTR("SYNC %u"), msg->extension);
      break;
    case FHT_TEST:
      printf_P(PSTR("TEST"));
      break;
    default:
      printf_P(PSTR("cmd 0x%hu ext 0x%hu"), msg->command & 0xf, msg->extension);
  }
}


static void cmdflagsdump(fht_msg_t *msg)
{
  if (msg->command & FHT_REPEAT)
    printf_P(PSTR("RPT "));
  if (msg->command & FHT_EXT_PRESENT)
    printf_P(PSTR("EXT "));
  if (msg->command & FHT_BATT_WARN)
    printf_P(PSTR("ENABLE_LOW_BATT_WARNING "));
}

/*
static void msgdump(fht_msg_t *msg)
{
  int n;
  uint8_t *_msg = (uint8_t*)msg;
  uint8_t mychecksum;

  // Calculate expected checksum 
  mychecksum = 0x0c;
  for (n = 0; n < 5; n++) {
    mychecksum += _msg[n];
  }

  printf_P(PSTR("Received message:\n"));
  printf_P(PSTR("HC1 = %hu, HC2 = %hu, ADDR = %hu\n"), msg->hc1, msg->hc2, msg->address);
  printf_P(PSTR("\n"));

  printf_P(PSTR("COMMAND: "));
  cmddump(&msg);

  printf_P(PSTR(" FLAGS: "));
  cmdflagsdump(&msg);

  if (msg->checksum == mychecksum) {
    printf_P(PSTR(" Checksum OK\n"));
  }
  else {
    printf_P(PSTR(" Checksum bad\n"));
  }
}
*/

static void hexdump(uint8_t *buf, int size)
{
  int n;

  for (n = 0; n < size; n++) {
    if ((n & 15) == 0)
      printf_P(PSTR("# 0%X : "), n >> 4);
    if (buf[n] < 0x10)
      printf_P(PSTR("0")); /* deal with broken printf */
    printf_P(PSTR("%X "), buf[n]);
    if ((n & 15) == 15)
      printf_P(PSTR("\n"));
  }
  printf_P(PSTR("\n"));
}

/*
   Takes another FHT protocol bit and encodes it into either
   1100 (for a 0) or 111000 (for a 1) and pushes it to the
   transmitter output buffer.
*/
static void pushbit(int bit, uint8_t **outptr)
{
  static uint8_t byteval = 0;
  int size, shift;
  uint8_t mask;

  if (bit) {
    // 1 is 600us on/off - load 111000 to output
    size = 6;
    mask = 0x38;
  }
  else {
    // 0 is 400us on/off - load 1100 to output
    size = 4;
    mask = 0x0c;
  }

  while (size) {
    // shift to fit
    shift = (size > (8 - g_nbits)) ? (8 - g_nbits) : size;
    byteval = (byteval << shift) | (mask >> (size - shift));
    size -= shift;
    g_nbits += shift;
    if (g_nbits == 8) {
      *(*outptr) = byteval;
      (*outptr)++;
      g_nbits = 0;
    }
  }
}

/*!
   Encode a buffer into a format that will generate
   the FHT pulse-width encoding when transmitted.
   Parity bit is added for each byte.
*/
static int fht_rfm_encode(uint8_t *inbuf, uint8_t *outbuf, int insize)
{
  int n, nbits;
  uint8_t byte;
  uint8_t *outptr = outbuf;

  /* Reset pushbit */
  g_nbits = 0;

  /* Extra preamble for RFM receivers - not needed if only
   	  real FHT devices are listening */
  for (n = 0; n < 4; n++)
    *outptr++ = 0xaa;

  /* Fill FIFO bit-by-bit to simulate 400us/600us OOK protocol */
  for (n = 0; n < 12; n++)
    pushbit(0, &outptr);
  pushbit(1, &outptr);
  for (n = 0; n < 6; n++) {
    int parity = 0;
    byte = inbuf[n];
    for (nbits = 0; nbits < 8; nbits++) {
      pushbit(byte & 0x80, &outptr);
      parity ^= (byte / 0x80);
      byte <<= 1;
    }
    pushbit(parity, &outptr);
  }
  pushbit(0, &outptr);
  pushbit(0, &outptr);

  return outptr - outbuf + 1;
}

/*!
   Decode a buffer from FHT pulse width encoding into
   actual byte values.  TODO: Check parity
*/
static int fht_rfm_decode(uint8_t *inbuf, uint8_t *outbuf, int outsize)
{
  int inbits = 0, outbits = 0;
  int synced = 0;
  uint8_t symbol;
  uint16_t outbyte = 0;

  while (outsize) {
    /* Get next undecoded 8 bits from input buffer */
    symbol = (inbuf[0] << inbits) | (inbuf[1] >> (8 - inbits));

    /* Look for valid symbol (left-justified) */
    if ((symbol & 0xf0) == 0xc0) {
      /* zero bit */
      outbyte = outbyte << 1;
      inbits += 4;
    }
    else if ((symbol & 0xfc) == 0xe0) {
      /* one bit */
      outbyte = (outbyte << 1) | 1;
      inbits += 6;
    }
    else {
      /* invalid bit pattern */
      PRINTF("Invalid bit pattern - abort\n");
      return -1;
    }

    /* Step input */
    if (inbits >= 8) {
      inbits -= 8;
      inbuf++;
    }

    /* Don't start shifting bits out until after we have seen the first
     		  1 bit at the end of the preamble */
    if (!synced) {
      if (outbyte & 1)
        synced = 1;
      continue;
    }

    /* Step output */
    if (++outbits == 9) {
      *outbuf++ = (outbyte >> 1); /* remove parity bit */
      outsize--;
      outbits = 0;
    }
  }

  return 0;
}

#define FHT_BUFFER_SIZE	64

static void fht_transmit(uint8_t group)
{
  uint8_t *_msg = (uint8_t*) & (g_message[group]);
  int n, length;
  /* Output buffer for variable length bitstream
   	  The preamble is always 54 actual bits long (12 0's and a 1)
   	  The payload is 54 bits - maximum actual length = 54x6 = 324 bits
   	  Terminates with two 0's = 8 bits
   	  Maximum buffer size therefore = 49 bytes + 4 for extra preamble
  */
  uint8_t outbuf[FHT_BUFFER_SIZE];

  LED_TRX_ON();

  /* Clear output buffer */
  memset(outbuf, 0, FHT_BUFFER_SIZE);

  /* Calculate checksum */
  g_message[group].checksum = 0x0c;
  for (n = 0; n < 5; n++) {
    g_message[group].checksum += _msg[n];
  }

  /* Dump un-encoded message */
  // hexdump(_msg, sizeof(fht_msg_t));

  /* Dump encoded message */
  length = fht_rfm_encode(_msg, outbuf, sizeof(fht_msg_t));
  //if (DEBUG > 1) hexdump(outbuf, length);

  /* Transmit twice */
  si443x_transmit(outbuf, length);
  /* This delay is about right with debug enabled.  The actual gap
   	  should be about 8 ms */
  _delay_ms(5);
  si443x_transmit(outbuf, length);

  LED_TRX_OFF();

  // Log the trasmitted message
  LOG_FHT("0 RFM_TX ");
  msg_enq_print(group, 0);
  PRINTF("\n");
}

/* Init fht and read the configuration */
void fht_init(void)
{
  int r, g;
  cli();
  r = fht_eeprom_load(g_message);
  sei();
  if (r < 0)
    LOG_FHT("1 EEPROM incosistent configuration data from EEPROM ignored\n")
    else
    {
      PRINTF("Config loaded from eeprom, %d groups found.\n", r);
      g_groups_num = r;
      for (g = 0; g < r; g++)
        LOG_FHT("1 EEPROM Group %d HC is %d %d = %hu %hu.\n", grp_indx2name(g),  g_message[g].hc1, g_message[g].hc2,  g_message[g].hc1, g_message[g].hc2);
    }
}


grp_indx_t fht_get_groups_num(void)
{
  return g_groups_num;
}

void fht_set_groups_num(grp_name_t groupsnum)
{
  g_groups_num = groupsnum;
  fht_eeprom_save_header(g_groups_num);
}


void msg_enq_print(grp_indx_t group, int8_t verb)
{
  PRINTF("CMD='"); cmddump(&(g_message[group]));      PRINTF("' ");
  PRINTF("FLG='"); cmdflagsdump(&(g_message[group])); PRINTF("' ");
  PRINTF("grp='%d' adr='%u' ", grp_indx2name(group),  g_message[group].address);
  if (verb > 0) {
    PRINTF(" hc='%u %u'='%hu %hu' cmdL='0x%hu' cmdU='0x%hu' ext='0x%hu' ",
           g_message[group].hc1, g_message[group].hc2,
           g_message[group].hc1, g_message[group].hc2,
           g_message[group].command & 0xf, (g_message[group].command & 0xf0) >> 4, g_message[group].extension);
  }
}

/* current setting report */
void fht_print(void) {
  int g;

  PRINTF("\n*** Current settings report:\n");
  PRINTF("Number of groups currently used: %u\n", g_groups_num);
  PRINTF("Number of groups maximum:        %u\n", FHT_GROUPS_DIM);

  fht_eeprom_print();

  PRINTF("\n*** Messages enqueued:\n");
  for (g = 0; g < g_groups_num; g++) {
    LOG_FHT("1 RFM_TQ ");
    msg_enq_print(g, DEBUG);
    PRINTF("\n");
  }

  PRINTF("\n*** Technical report:\n");
  PRINTF("Free mem is %u\n", freeMemory());
  if (fht_is_panic()) PRINTF("Panic! ");
  PRINTF("Uptime [ticks]: %u; last enq command at: %u\n", g_ticks, g_last_command_enqueued_time);
  if (g_freezingMode>0) PRINTF("Freezing! ");
  PRINTF("Last known temp: %d/10, freezing mode: %u\n", temp_get_last_known_t10(), g_freezingMode);
  // unsigned log upt = millis()/1000;
  // print_uptime(upt);

  m328_print_readings();
  if (si443x_status() == 0) {
    LOG_FHT("1 RADIO ok\n");
    // si443x_dump();
  }
  else {
    LOG_FHT("1 RADIO FAILED status is %d.\n", si443x_status());
  }

  PRINTF("SETTINGS: FHT_FREEZING_TEMP=%d, FHT_FREEZING_SET_VALUE=0x%X, FREEZING_INIT_COUNT=%u; FHT_PANIC_TIMEOUT=%u[ticks], FHT_PANIC_SET_VALUE=0x%X\n",
         FHT_FREEZING_TEMP, FHT_FREEZING_SET_VALUE, FREEZING_INIT_COUNT, FHT_PANIC_TIMEOUT, FHT_PANIC_SET_VALUE);
}

/* Save the configuration to EEPROM*/
void fht_config_save_group(grp_indx_t group)
{
  cli();
  fht_eeprom_save_group(group, &(g_message[group]));
  sei();
}

/* clear panic counter */
void fht_clear_panic_count(void) {
  g_last_command_enqueued_time = g_ticks;
}

/* cancel panic (if any) and clear panic counter*/
void fht_cancel_panic(void) {
  if (fht_is_panic()) {
    LOG_FHT("0 PANIC OFF tick='%u' last_enq='%u' pos='%u'\n", g_ticks, g_last_command_enqueued_time, FHT_PANIC_SET_VALUE);
    LOG_CLI("PANIC OFF");
  }
  fht_clear_panic_count();
  LED_RED_OFF();
}

bool_t fht_is_panic(void) {
  return g_ticks  >=  g_last_command_enqueued_time + FHT_PANIC_TIMEOUT ;
}

/* Called once every 500 ms from ISR */
void fht_tick(void) // HB
{
  LED_GREEN_ON();
  if (fht_is_panic()) { // panic?
    LOG_FHT("0 PANIC ON tick='%u' last_enq='%u' pos='%u'\n", g_ticks, g_last_command_enqueued_time, FHT_PANIC_SET_VALUE);
    LOG_CLI("PANIC ON setting all groups valve positions to 0x%X : message enqueued.\n", FHT_PANIC_SET_VALUE);
    fht_enqueue(grp_indx_all, 0, FHT_VALVE_SET, FHT_PANIC_SET_VALUE);
    fht_clear_panic_count();
    LED_RED_ON();
  }

  if (si443x_status() == 0) {
    grp_indx_t group;
    for (group = 0; group < g_groups_num; group++) {
      fht_tick_grp(group);
    }
    g_ticks++;
  } else {
    LED_RED_ON();
    LOG_CLI("fht_tick ignored,  radio not intialized.\n");
  }
  LED_GREEN_OFF();
}

void fht_tick_grp(grp_indx_t group)
{
  /* Transmit slot depends on HC2 byte */
  int slot = (g_message[group]).hc2 & 7;

  if (((g_message[group]).command & 0xf) == FHT_SYNC) {
    /* Sync message is sent once per second for 2 minutes */
    if (g_slot_count[group] & 1) {
      /* transmit sync */
      //--//LOG_FHT("1 RFM_TX SYNC %u group %d sync %d\n", g_ticks, grp_indx2name(group), g_slot_count[group]);
      (g_message[group]).extension = g_slot_count[group];
      fht_transmit(group);
    }
    if (--g_slot_count[group] < 3) {
      /* We don't send the '0' sync count - we are now at 4 seconds before
       			  the first slot */
      g_slot_count[group] = PERIOD_BASE - 8;

      /* First actual message - will be sent when the correct timeslot is reached */
      (g_message[group]).command = FHT_EXT_PRESENT | FHT_SYNC_SET;
      (g_message[group]).extension = FHT_SYNC_SET_VALUE; // probably not working, my valves are set to 0 ignoring this parameter
    }
  }
  else if (((g_message[group]).command & 0xf) == FHT_PAIR) {
    //--//LOG_FHT("1 RFM_TX PAIR group %d hc %d %d tick %u slot %d tx\n",  grp_indx2name(group), (g_message[group]).hc1,  (g_message[group]).hc2, g_ticks, slot);
    g_slot_count[group] = 0;
    fht_transmit(group);
    /* First actual message - will be sent when the correct timeslot is reached */
    (g_message[group]).command = FHT_EXT_PRESENT | FHT_SYNC_SET;
    (g_message[group]).extension = 0;
  }
  else {
    g_slot_count[group]++;

    if (g_slot_count[group] == PERIOD_BASE + slot - 4) {
      //DPRINTF("Four ticks before the group %u timeslot (tick=%u) free mem is %u,  requesting temperatures measurement.\n",  grp_indx2name(group), g_ticks, freeMemory());
      if (group == 0) {
        temp_request_start();
      };
    }
    else if (g_slot_count[group] == PERIOD_BASE + slot - 2) {
      //DPRINTF("Two  ticks before the group %u timeslot (tick=%u) free mem is %u, temperatures are:\n",  grp_indx2name(group), g_ticks, freeMemory());
      //PRINTF("Two ticks before the group %u timeslot temperatures (tick=%u) are:\n",  grp_indx2name(group), g_ticks);
      ///// freezing protection
      // detection of freezing is done in group 0 ONLY
      int16_t lastT10 = TEMP_NA;
      if (group == 0) {
        // print and save measured group 0 temp
        lastT10 = temp_request_print();
        // update freezingMode
        if (lastT10 <= ((int16_t)(10 * FHT_FREEZING_TEMP))) { // is freezing
          if (g_freezingMode == 0) LOG_FHT("0 FREEZING ENTER lastT10='%d' tick='%u'\n", lastT10, g_ticks);
          g_freezingMode = FREEZING_INIT_COUNT;
          LED_RED_ON();
        }
        else { // not freezing
          if (g_freezingMode == 1) LOG_FHT("0 FREEZING LEAVE lastT10='%d tick='%u''\n", lastT10, g_ticks);
          if (g_freezingMode > 0)  g_freezingMode--;
        }
      }
      // if freezing mode is enabled, do the protecting work in CURRENT group
      if ((g_freezingMode > 0) && (((g_message[group]).command & 0xf) == FHT_VALVE_SET) && ((g_message[group]).extension < FHT_FREEZING_SET_VALUE)) {
        // Open  valves minimally to FHT_FREEZING_SET_VALUE
        LOG_FHT("0 FREEZING TX enforcing group='%u' valve opening to 0x%X\n", grp_indx2name(group), FHT_FREEZING_SET_VALUE);
        fht_enqueue(group, 0, FHT_VALVE_SET, FHT_FREEZING_SET_VALUE);  // modify FHT_VALVE_SET message to be transmitted
      }
    }
    else if (g_slot_count[group] == PERIOD_BASE + slot) {
      /* This is our timeslot - SEND the stored MESSAGE */

      // LOG_FHT("1 TIME tick='%u' slot='%u'\n", g_ticks, slot);

      // FHEM FHT_HB.classdef code to catch and parse last transmitted state:
      // reading pos_tx  match       "^LOG FHT \d RFM_TX CMD='VALVE_SET (\d+)' .* grp='%group_idx'.*$"
      // reading pos_tx  postproc { s/^LOG FHT \d RFM_TX CMD='VALVE_SET (\d+)' .* grp='%group_idx'.*$/$1/;; floor($_/255*100) }

      // reset timeslot counter
      g_slot_count[group] = 0;

      // transmit message
      fht_transmit(group);

      /* Set the repeat flag for next time */
      (g_message[group]).command |= FHT_REPEAT;
    }
  }
}

void fht_enqueue(grp_indx_t group, uint8_t address, uint8_t command, uint8_t value)
{
  LED_GREEN_ON();
  if (group == grp_indx_all) {
    //  all groups
    grp_indx_t g;
    for (g = 0; g < g_groups_num; g++)
      fht_enqueue(g, address, command, value);
  }
  else {
    // single group
    cli();
    (g_message[group]).address = address;
    (g_message[group]).command = FHT_EXT_PRESENT | (command & 0xf);
    (g_message[group]).extension = value;
    sei();
    LOG_FHT("0 RFM_TQ ");
    msg_enq_print(group, 0);
    PRINTF("\n");
  }
}

void fht_set_hc_grp(grp_indx_t group, uint8_t hc1, uint8_t hc2)
{
  fht_set_hc_msg(&(g_message[group]), hc1, hc2);
}

void fht_set_hc_msg(fht_msg_t *msg, uint8_t hc1, uint8_t hc2)
{
  cli();
  msg->hc1 = hc1;
  msg->hc2 = hc2;
  sei();
}

void fht_sync_grp(grp_indx_t group)
{
  cli();
  (g_message[group]).address = 0;
  (g_message[group]).command = FHT_EXT_PRESENT | FHT_SYNC;
  (g_message[group]).extension = 0;
  g_slot_count[group] = SYNC_TICKS | 1;
  sei();
}

int fht_group_synced(grp_indx_t group)
{
  return ((g_message[group]).command & FHT_REPEAT);
}


int  fht_all_groups_synced(void)
{
  grp_indx_t g;
  for (g = 0; g < g_groups_num; g++)
    if (!fht_group_synced(g)) return 0 ;
  return 1;
}

void fht_sync(grp_indx_t group)
{
  /* Enqueue the sync message.  The interrupt handler will do the rest.  We can
   	  monitor for the command change to see when it has completed */
  if (group == grp_indx_all) {
    //  all groups
    grp_indx_t g;
    for (g = 0; g < g_groups_num; g++)
      fht_sync_grp(g);
    /* Wait for repeat bit to be set - this indicates that the first real command
     		  has been sent following the sync procedure */
    LOG_FHT("1 RFM_TX SYNC Waiting for ALL groups sync...\n");
    while (!fht_all_groups_synced());
    LOG_FHT("1 RFM_TX SYNC Sync of all groups complete\n");
  }
  else {
    // single group
    fht_sync_grp(group);
    /* Wait for repeat bit to be set - this indicates that the first real command
     		  has been sent following the sync procedure */
    LOG_FHT("1 RFM_TX SYNC Waiting for group %d sync...\n", grp_indx2name(group));
    while (!fht_group_synced(group));
    LOG_FHT("1 RFM_TX SYNC Sync group %d complete\n", grp_indx2name(group));
  }
}


void fht_receive(void)
{
  // disabled to save flash space
  //  fht_msg_t rxmsg;
  //  uint8_t buf[FHT_BUFFER_SIZE];
  //  int rssi;
  //
  //  /* Start receiver, no timeout.  Receive up to 46 bytes, which is
  //   	 * sufficient to capture the longest (all 1s) encoded packet
  //   	 * from the sync point onwards */
  //  si443x_receive(buf, FHT_BUFFER_SIZE, 0, &rssi);
  //
  //  DPRINTF("Rx rssi %d dBm\n", rssi);
  //
  //  /* Dump encoded message */
  //  hexdump(buf, FHT_BUFFER_SIZE);
  //
  //  if (fht_rfm_decode(buf, (uint8_t*)&rxmsg, sizeof(rxmsg)) < 0) {
  //    DPRINTF("Symbol error\n");
  //  }
  //
  //  /* Dump decoded message */
  //  hexdump((uint8_t*)&rxmsg, sizeof(rxmsg));
  //
  //  /* Decode contents */
  //  msgdump(&rxmsg);

}



void fht_test(void)
{
  //  fht_msg_t msg;
  //  uint8_t *_msg = (uint8_t*)&msg, *ptr;
  //  uint8_t buf[FHT_BUFFER_SIZE];
  //  int n, length;
  //
  //  msg.hc1 = 12;
  //  msg.hc2 = 34;
  //  msg.address = 0;
  //  msg.command = FHT_EXT_PRESENT | FHT_VALVE_SET;
  //  msg.extension = 20;
  //  msg.checksum = 0x0c;
  //  for (n = 0; n < 5; n++) {
  //    msg.checksum += _msg[n];
  //  }
  //
  //  DPRINTF("Encoding...\n");
  //  hexdump((uint8_t*)&msg, sizeof(msg));
  //  length = fht_rfm_encode((uint8_t*)&msg, buf, sizeof(msg));
  //  hexdump(buf, length);
  //
  //  DPRINTF("Decoding...\n");
  //  memset(&msg, 0, sizeof(msg));
  //  ptr = &buf[8]; /* Skip preamble and 'sync' */
  //  length -= 8;
  //  hexdump(ptr, length);
  //  fht_rfm_decode(ptr, (uint8_t*)&msg, sizeof(msg));
  //  hexdump((uint8_t*)&msg, sizeof(msg));
  //  msgdump(&msg);
}
