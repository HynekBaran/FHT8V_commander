/*
* Copyright 2013 Hynek Baran
* 
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
* 
*     http://www.apache.org/licenses/LICENSE-2.0
* 
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

/*
* This code was strongly inspired by
* 
* http://playground.arduino.cc//Code/EEPROMWriteAnything
*
*/

#include <avr/eeprom.h>

#include "common.h"
#include "fht.h"


/*
*
* The configuration and its storage into EEPROM 
*
*/

/* global configuration */
typedef struct {
        const uint8_t version  ; // currently 1
        const uint8_t header_size ;  // size of this header data structure  
        const uint8_t group_size ;   // size of group data srtucture fht_eeprom_group_t
        grp_indx_t groups ; // number of groups in eeprom data
} fht_eeprom_header_t;

/* group configuration */
typedef struct {
        // home code of valves in current group
	uint8_t	hc1;
	uint8_t hc2;
        // consider to include some additional group data:
        // uint8_t value;  // last known valve opening state value (?)
        // uint8_t valves; // number of valves in the group (?)
        // in future, maybe some more data will be stored here
} fht_eeprom_group_t;

// The header to be burned to EEPROM may be prepared in global variable: 
fht_eeprom_header_t g_fht_eeprom_header = {1,  sizeof(fht_eeprom_header_t), sizeof(fht_eeprom_group_t), FHT_GROUPS_DIM}; // TODO: set correct number of groups (g_groups_num)

/* 
* Config data are stored in EEPROM sequentially as follows:
* <fht_eeprom_header_t> The header g_fht_eeprom_header (which contains number of groups which follows)
* <fht_eeprom_group_t>  (First) group data
*
* In the future, data following will be here (not implemented yet):
* <fht_eeprom_group_t>  Second group
* ...
* <fht_eeprom_group_t> Last group
* <?_t> CRC/footer
*/

/*
  TODO:
  Since number of EEPROM write cycles is limited, reimplement the code bellow in the way that 
  it will update only changed bytes in EEPROM only (or use  eeprom_update_block once available).
  See http://deans-avr-tutorials.googlecode.com/svn/trunk/EEPROM/Output/EEPROM.pdf
*/

/* save header to eeprom */
void fht_eeprom_save_header(grp_indx_t group_num)
{
  g_fht_eeprom_header.groups = group_num;
  // write a header to eeprom
  // TODO: use eeprom_update_block
  eeprom_write_block((const void*) &g_fht_eeprom_header, 
                     (void*) 0,          
                      g_fht_eeprom_header.header_size);  
}

/* save single group to eeprom */
void fht_eeprom_save_group(grp_indx_t group, fht_msg_t *msg) 
{
  fht_eeprom_group_t grp_cfg;
  
  grp_cfg.hc1 = msg->hc1;  
  grp_cfg.hc2 = msg->hc2;
                      
  // write our only group data to eeprom (a sequence of all groups will be here once multiple groups implemented) 
  // TODO: use eeprom_update_block
  eeprom_write_block((const void*) &grp_cfg,              
                     (void*) g_fht_eeprom_header.header_size + group*g_fht_eeprom_header.group_size, 
                     g_fht_eeprom_header.group_size);
}

/* load the whole config (header & all groups) from eeprom */
signed int fht_eeprom_load(fht_msg_t msgs[]) // returns number of groups or negative error code
{
  fht_eeprom_header_t header_cfg;
  fht_eeprom_group_t  grp_cfg;
  grp_indx_t  g;
  
  eeprom_read_block((void*) &header_cfg, 
                    (void*) 0,          
                    g_fht_eeprom_header.header_size); 
                    
  if ((header_cfg.version != 1 ) || (header_cfg.header_size != g_fht_eeprom_header.header_size)  ||  (header_cfg.group_size != g_fht_eeprom_header.group_size)) 
    return -2; // error, version or header/group data size does not match current implementation

  for (g = 0; g < header_cfg.groups; g++)
  {
    eeprom_read_block((void*) &grp_cfg,              
                      (void*) header_cfg.header_size + g*header_cfg.group_size, 
                      header_cfg.group_size);
  
    fht_set_hc_msg(&(msgs[g]), grp_cfg.hc1, grp_cfg.hc2);
  }
  
  return header_cfg.groups;
}


/* read all data from eeprom and print it to the console */
void fht_eeprom_print(void) {
  fht_eeprom_header_t header_cfg;
  fht_eeprom_group_t  grp_cfg;
  grp_indx_t  g;
  
  PRINTF("\n*** EEPROM data report:\n");
  
  eeprom_read_block((void*) &header_cfg, 
                    (void*) 0,          
                    g_fht_eeprom_header.header_size); 
                    
  if ((header_cfg.version != 1 ) || (header_cfg.header_size != g_fht_eeprom_header.header_size)  ||  (header_cfg.group_size != g_fht_eeprom_header.group_size)) 
    LOG_FHT("1 EEPROM Header mismatch!\n");
    
  
  
  PRINTF("version     %u\n", header_cfg.version)  ; 
  PRINTF("header_size %u\n", header_cfg.header_size) ;    
  PRINTF("group_size  %u\n", header_cfg.group_size) ;    
  PRINTF("groups num  %u\n", header_cfg.groups) ;  


  for (g = 0; g < header_cfg.groups; g++)
  {
    eeprom_read_block((void*) &grp_cfg,              
                      (void*) header_cfg.header_size + g*header_cfg.group_size, 
                      header_cfg.group_size);
    
    LOG_FHT("1 EEPROM Group='%u' hc='%u %u'='0x%X 0x%X'\n", grp_indx2name(g), grp_cfg.hc1, grp_cfg.hc2,  grp_cfg.hc1, grp_cfg.hc2);
  }
  
  return header_cfg.groups;
}
