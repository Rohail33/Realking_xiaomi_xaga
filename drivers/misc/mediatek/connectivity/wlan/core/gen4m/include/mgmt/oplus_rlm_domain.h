/*
* Copyright (C) 2021 Oplus Inc.
*
* WIFI.BASIC.HARDWARE @chanbolun@oplus.com
* 
* Use for get wifi channel configuration from storage instead of memory
*/

#ifndef OPLUS_RLM_DOMAIN_H
#define OPLUS_RLM_DOMAIN_H

#include "rlm_domain.h"

#define COUNTRY_CODE_CVT(x,y) \
    (((uint16_t) x << 8) | (uint16_t) y)

#define CFG_ACTIVE_CHANNEL_FILE "wifi_active_channel.cfg"
#define CFG_PASSIVE_CHANNEL_FILE "wifi_passive_channel.cfg"

#define FORMAT_CHANNEL_CFG 'C'
#define FORMAT_CHANNEL_CFG_SIZE 2

#define FORMAT_COUNTRY_GROUP 'G'
#define FORMAT_COUNTRY_GROUP_SIZE 2

#define FORMAT_SEPARATION ','
#define FORMAT_STRING_END '\0'
#define FORMAT_FILE_END '#'

//one group cfg in 7 line
#define FORMAT_LINE_NUM 7
//one group cfg have 6 line of channel cfg
#define FORMAT_CHANNEL_NUM MAX_SUBBAND_NUM

//use 4K memory at most for active cfg
#define CFG_CHANNEL_ACTIVE_SIZE 4096
#define CFG_CHANNEL_PASSIVE_SIZE 1024

//use 20 number of DOMAIN_INFO_ENTRY as origin memory
#define MEM_BASE_ACTIVE_DOMAIN 20
#define MEM_EXTEND_ACTIVE_DOMAIN 5

#define MEM_BASE_PASSIVE_DOMAIN 1
#define MEM_EXTEND_PASSIVE_DOMAIN 1

#define TEMP_BUFFER_SIZE 4

typedef enum FORMAT_RLM_SUB_INDEX {
    INDEX_UNKNOWN = 0,
    INDEX_CLASS,
    INDEX_BAND,
    INDEX_CHAN_SPAN,
    INDEX_FIRST_CHAN,
    INDEX_CHAN_NUM,
    INDEX_DFS
} OPLUS_RLM_INDEX;

typedef struct OPLUS_DOMAIN_INFO_ENTRY {
    struct DOMAIN_INFO_ENTRY *entry;
    int size;
} OPLUS_DOMAIN_INFO_ENTRY;

/*
* @parameter DOMAIN_INFO_ENTRY link to rlm_domain.h 
* Return active channel config
*/
struct OPLUS_DOMAIN_INFO_ENTRY* get_active_channel_cfg(void);

/*
* @parameter DOMAIN_INFO_ENTRY link to rlm_domain.h 
* Return passive channel config
*/
struct OPLUS_DOMAIN_INFO_ENTRY* get_passive_channel_cfg(void);


#endif /* OPLUS_RLM_DOMAIN_H */
