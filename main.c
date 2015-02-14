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
 * \file main.c
 * \brief Top-level
 *
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/delay.h>
#include <avr/sleep.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "DS18x20.h"

#include "si443x_min.h"
#include "fht.h"
#include "cli.h"

#include "board.h"
#include "common.h"

#include "temp.h"


/*************************************************/

/* 2 Hz tick */
#define SYSTEM_TICK		2

static volatile uint32_t g_tick_count;

/* Half second tick interrupt */
ISR(TIMER1_COMPA_vect)
{
	g_tick_count++;

	/* Run half-second FHT driver jobs */
	fht_tick();
}

uint32_t get_tick_count(void)
{
	uint32_t tick_count;

	/* Atomic copy */
	cli();
	tick_count = g_tick_count;
	sei();
	return tick_count;
}

/*************************************************/

static int8_t TestIfGrpIsAll(grp_name_t g)
{
  if (g==grp_name_all) {
  	LOG_CLI("Command not supported for ALL groups. Please specify group number\n");
  	return 1;
  } else
  return 0;
}


/* Sync with a valve or set a command for transmission in the next
 * transmit window (which may be up to 2 minutes later) */
static int fht_handler(cli_t *ctx, void *arg, int argc, char **argv)
{
    uint8_t value;
    grp_name_t groupname;
    grp_indx_t group;

   /*
    Every command has a form fht <cmd> <group> [<optional_params>]
    where group is name of the group in quastion.
    Group names are 1, 2, 3, ... but internally C array indices are 0, 1, 2,  ... so in g_message are indices shifted by 1.
    Group name 0 is moreover reserved for "all groups" (not implemented yet).
    */

    if (argc < 2){
          LOG_CLI("Missing parameter.\n");
          return 1;
    }

    if (argc == 2) {
       // no groupname means all (if no additional parameters present)
       groupname = 0;
    } else {
      groupname = atoi(argv[2]);
      if (groupname < 0 || groupname > FHT_GROUPS_DIM) {
        LOG_CLI("Invalid group name.\n");
        return 1;
      }
    }

    group = grp_name2indx(groupname);

    if (strcmp_PF(argv[1], PSTR("hc")) == 0) {
    	// *** HC ***
		uint8_t hc1, hc2;

		if (argc < 5) return 2;
		if (TestIfGrpIsAll(groupname)) return 1;

		/* 'hc' takes two arguments which are the two house codes for
		 * the valve with which we are pairing e.g.
		 * > fht hc 12 34
		 */
		hc1 = atoi(argv[3]);
		hc2 = atoi(argv[4]);
		fht_set_hc_grp(group, hc1, hc2);
                fht_config_save_group(group);
		LOG_CLI("Home code of group %u was set to %u %u.\n", groupname, hc1, hc2);

	} else if (strcmp_PF(argv[1], PSTR("pair")) == 0) {
		// *** PAIR ***
                uint8_t valve;
		if (TestIfGrpIsAll(groupname)) return 1;
                if (argc = 4) {
                  // valve number specified: fht pair <group> <valve>
                  valve = atoi(argv[3]);
                } else {
                  // valve index no specified: fht pair <group>
                  valve = 0;
                }                
		LOG_CLI("Requesting group %u valve %u pairing\n", groupname, valve);
		fht_enqueue(group, valve, FHT_PAIR, 0);
	} else if (strcmp_PF(argv[1], PSTR("sync")) == 0) {
		// *** SYNC ***
		LOG_CLI("Syncing group %u valves\n", groupname);
		fht_sync(group);
	} else if (strcmp_PF(argv[1], PSTR("set")) == 0) {
		// *** SET ***
		if (argc < 4) return 1;
		/* 'set' takes one argument which is the valve position in the
		 * range 0 to 255 */
		value = atoi(argv[3]);
		LOG_CLI("Setting group %u valve position to %u\n", groupname, value);
		fht_enqueue(group, 0, FHT_VALVE_SET, value);
        } else if (strcmp_PF(argv[1], PSTR("offset")) == 0) {
		// *** OFFSET ***
                // not properly tested yet
		if (argc < 5) return 1;
		uint8_t valve = atoi(argv[3]); // valve number
		int8_t  offset_sdec = atoi(argv[4]); // offset (signed decimal value of offset)
                // according FHEM/11_FHT.pm, to be changed as follows:  $val = "offset: " . ($val>128?(128-$val):$val) }
                value = abs(offset_sdec);
                if (offset_sdec < 0) value |= 0b10000000;
		LOG_CLI("Setting group %u valve %u offset to %d = 0x%x\n", groupname, valve, offset_sdec, value);
		fht_enqueue(group, valve, FHT_OFFSET, value);
        } else if (strcmp_PF(argv[1], PSTR("groups")) == 0) {
		// *** GROUPS ***
		/* set number of currently used groups 
                   this command has no groupname parameter
                   so the number of groups is in groupname parameter
                */
                if (groupname<1 || groupname > FHT_GROUPS_DIM) {
                  LOG_CLI("Error: %u is out of the range [1, FHT_GROUPS_DIM=%u].\n", groupname, FHT_GROUPS_DIM);
                  return 1;
                }
                fht_set_groups_num(groupname);
		LOG_CLI("Number of active groups is set to %u.\n", groupname);
                LOG_FHT("1 CLI Please setup home codes for new groups! Call: fht hc <group> <hc1> <hc2>\n");
	} else if (strcmp_PF(argv[1], PSTR("beep")) == 0) {
		// *** BEEP ***
		/* 'beep' will instruct the valve to make a beep */
		LOG_CLI("Requesting group %u valve test beep\n", groupname);
		fht_enqueue(group, 0, FHT_TEST, 0);
        } else if (strcmp_PF(argv[1], PSTR("info")) == 0) {
                // *** INFO ***
                fht_print();                    
        }  else  {
		LOG_CLI("Unknown fht command.\n");
                return 1;
	}

	return 0;
}


//static int fhtrx_handler(cli_t *ctx, void *arg, int argc, char **argv)
//{
//	DPRINTF("FHT test receiver started - reset to exit\n");
//
//	/* Disable timer to inhibit transmitter */
//	TIMSK1 = 0;
//
//	while (1) {
//		fht_receive();
//	}
//	return 0;
//}


static int temp_handler(cli_t *ctx, void *arg, int argc, char **argv)
{
  temp_print();
  return 0;
}

int fhtsetup(void)
{
	uint8_t	mcustatus = MCUSR;

	MCUSR = 0;

	// Set up port directions and load initial values/enable pull-ups
	PORTB = PORTB_VAL;
	PORTC = PORTC_VAL;
	PORTD = PORTD_VAL;
	DDRB = DDRB_VAL;
	DDRC = DDRC_VAL;
	DDRD = DDRD_VAL;

#ifdef DEBUG
	debug_init();
	PRINTF("Debug level = %u\n", DEBUG);
#else
       	PRINTF("Debug level not defined.\n");
#endif

	PRINTF("\n\n"
			"FHT valve communication example for RFM22/23\n"
			"(C) 2013 Mike Stirling\n"
			"http://www.mikestirling.co.uk\n\n"
                        "(C) 2013 Hynek Baran\n"
			"https://github.com/HynekBaran/FHT8V\n\n");

	PRINTF("System clock = %lu Hz\n", F_CPU);
	PRINTF("Reset status = 0x%X\n", mcustatus);

        m328_print_readings();

	/* Configure tick interrupt for half second from internal
	 * 8 MHz clock using timer 1 */
	TCCR1A = 0; /* CTC mode */
	TCCR1B = _BV(WGM12) | _BV(CS12); /* divide by 256 */
	OCR1A = F_CPU / 256 / SYSTEM_TICK - 1;
	TIMSK1 = _BV(OCIE1A);

	sei();

	/* Turn on radio module */
	LOG_FHT("1 RADIO Enabling radio...\n");
	TRX_ON();
	_delay_ms(30);
        int radioStatus = si443x_init();
	if (radioStatus < 0) { // TODO: set global variable with radio status		
                #ifdef TRX_SDN
                LOG_FHT("0 RADIO FAILED,  RFM chip TRX_SDN pin defined, connect it correctly (or ground it)!\n");
                #else
                LOG_FHT("0 RADIO FAILED\n");
                #endif
		//while (1);
	} else {
		LOG_FHT("0 RADIO OK\n\n");
		si443x_dump(); 
        }

        /* Init fht */
        fht_init();
        fht_print();

	/* Set up CLI */
	cli_init(stdin, stdout, PSTR("FHT"));
	cli_register_command(PSTR("fht"), fht_handler, NULL, 
                             PSTR("fht groups <num_of_groups> | hc <grp> <hc1> <hc2> | pair <grp> [<valve>] | sync [<grp>] | offset  <grp> <valve> <value> | set <grp> <pos> | beep <grp> | info "));
	//cli_register_command(PSTR("fhtrx"), fhtrx_handler, NULL, PSTR("fhtrx - start receiver"));
        cli_register_command(PSTR("tmp"), temp_handler, NULL, PSTR("tmp - read the temperatures"));
        
        /* initial sync if radio available and at least one group configured*/
        if (radioStatus >= 0 &&  fht_get_groups_num()>0) { 
          LOG_CLI("Syncing all group valves...\n");
          fht_sync(0);
          LOG_CLI("Sync done.\n");     
          // set all valves default position, probably stored in EPROM to be configurable
        }
	return radioStatus;
}
int fhtloop() {
	cli_task(); /* never returns */
}


/*
int main(void)
{
	fhtsetup();
        temp_init();
	fhtloop();
}
*/

