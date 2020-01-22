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
 * \file fht.h
 * \brief FHT protocol implementation for RFM22/23
 *
 * This is an implementation of the FHT8V valve protocol for SiLabs
 * EzRadioPro devices such as found on HopeRF RFM22 and RFM23 modules.
 *
 * It is not extensively tested and is primarily a proof of concept.
 *
 */

#ifndef FHT_H_
#define FHT_H_

/*
 * Global setup 
 */

// inital valve opening value
#define FHT_SYNC_SET_VALUE ((uint8_t) 255/2) 

// PANIC state setup
// (panic is a state when no FHT command is enqueued for a long time)
#define FHT_PANIC_SET_VALUE ((uint8_t) (255/3)) // panic state valve opening value (int8)
#define FHT_PANIC_TIMEOUT (2*60*15) // panic state timeout (in 0.5s ticks)

// FREEZE state setup
// (freeze state is used when local temp sensor is bellow the treshold to protect freezing)
#define FHT_FREEZING_SET_VALUE ((uint8_t) (255)) // freezing state valves minimum opening value
#define FHT_FREEZING_TEMP 12 // freezing temp treshold [Celsius]
#define FREEZING_INIT_COUNT 5 // how many tx cycles  keep in freezing mode before leaving

/*
 * FHT HW command codes
 */

#define FHT_REPEAT			(1 << 7)
#define FHT_EXT_PRESENT		(1 << 5)
#define FHT_BATT_WARN		(1 << 4)

#define FHT_SYNC_SET		0
#define FHT_VALVE_OPEN		1
#define FHT_VALVE_CLOSE		2
#define FHT_VALVE_SET		6
#define FHT_OFFSET			8
#define FHT_DESCALE			10
#define FHT_SYNC			12
#define FHT_TEST			14

#define FHT_PAIR			15

#define FHT_BROADCAST		0

typedef struct {
  uint8_t	hc1;
  uint8_t hc2;
  uint8_t address;
  uint8_t command;
  uint8_t extension;
  uint8_t checksum;
} 
__attribute__((packed)) fht_msg_t;

typedef  int8_t grp_indx_t; 
#define grp_indx_all -1

typedef  uint8_t grp_name_t;
#define grp_name_all 0

#define grp_indx2name(grp_indx) (grp_indx + 1)
#define grp_name2indx(grp_name) (grp_name - 1)


#define FHT_GROUPS_DIM                      8  // maximum number of groups = dimension of groups array (so the maximal group index is FHT_GROUPS_DIM-1)
static volatile grp_indx_t g_groups_num  = 1;  // currently used number of groups (initial 1 is overwiten by fht groups or value loaded from eeprom)

void fht_init(void);
grp_indx_t fht_get_groups_num(void);
void fht_set_groups_num(grp_name_t groupsnum);
void fht_print(void);
void msg_enq_print(grp_indx_t group, int8_t verb);
int16_t fht_print_temp(void);
void fht_config_save_group(grp_indx_t group);
void fht_do_not_panic(void);
void fht_tick(void);
void fht_tick_grp(grp_indx_t group);
void fht_enqueue(grp_indx_t group, uint8_t address, uint8_t command, uint8_t value);
void fht_sync(grp_indx_t group);
void fht_set_hc_grp(grp_indx_t group, uint8_t hc1, uint8_t hc2);
void fht_set_hc_msg(fht_msg_t *msg, uint8_t hc1, uint8_t hc2);
void fht_receive(void);

#endif /* FHT_H_ */

