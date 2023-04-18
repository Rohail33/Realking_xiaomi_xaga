/*
* Copyright (C) 2021 Oplus Inc.
*
* WIFI.BASIC.HARDWARE @chanbolun@oplus.com
* 
* Use for get wifi channel configuration from storage instead of memory
*/

#include <linux/string.h>

#include "gl_os.h"
#include "gl_kal.h"
#include "gl_wext.h"
#include "precomp.h"

#include "oplus_rlm_domain.h"

static int const DEBUG = 1;

static OPLUS_DOMAIN_INFO_ENTRY *oplus_active_domain_info = NULL;
static OPLUS_DOMAIN_INFO_ENTRY *oplus_passive_domain_info = NULL;

/*
* cut doamin memory length
*
* @parameter entry - domain cfg
* @parameter expected_size - expected domain cfg length
* @parameter current_size - current domain cfg length
* Return the pointer to handled pointer of DOMAIN_INFO_ENTRY 
*/
static struct DOMAIN_INFO_ENTRY* cut_unused_memory(struct DOMAIN_INFO_ENTRY* entry, int expected_size, int current_size) {
    struct DOMAIN_INFO_ENTRY* info_entry = NULL;
    DBGLOG(RLM, INFO, " cut_unused_memory  expected_size %d current_size %d \n", expected_size, current_size);
    if (expected_size > 0 && current_size > 0) {
        info_entry = (struct DOMAIN_INFO_ENTRY *)kalMemAlloc(sizeof(struct DOMAIN_INFO_ENTRY)*expected_size, VIR_MEM_TYPE);
        if(info_entry == NULL) {
            DBGLOG(RLM, ERROR, "kalMemAlloc adapt memory fail: %d\n", sizeof(struct DOMAIN_INFO_ENTRY)*expected_size);
            return entry;
        }
        kalMemSet(info_entry, 0, sizeof(struct DOMAIN_INFO_ENTRY)*expected_size);
        kalMemCopy(info_entry, entry, sizeof(struct DOMAIN_INFO_ENTRY)*expected_size);
        kalMemFree(entry, VIR_MEM_TYPE, current_size);
    }
    return info_entry;
}

/*
* extend doamin memory length
*
* @parameter entry - domain cfg
* @parameter num - current domain cfg length
* @parameter extend_size - how long we extend memory for us
* Return the number of size of current struct
*/
static int extend_doamin_mem(struct DOMAIN_INFO_ENTRY** entry, int num, int extend_size) {
    struct DOMAIN_INFO_ENTRY* info = NULL;
    DBGLOG(RLM, ERROR, " extend_doamin_mem %d extend size %d \n", num, extend_size);
    info = (struct DOMAIN_INFO_ENTRY *)kalMemAlloc(sizeof(struct DOMAIN_INFO_ENTRY)*(num + extend_size),
            VIR_MEM_TYPE);
    if(info == NULL) {
        DBGLOG(RLM, ERROR, "kalMemAlloc extend memory fail: %d\n", sizeof(struct DOMAIN_INFO_ENTRY)*(num + extend_size));
        return num;
    }
    kalMemSet(info, 0, sizeof(struct DOMAIN_INFO_ENTRY)*(num + extend_size));
    kalMemCopy(info, *entry, sizeof(struct DOMAIN_INFO_ENTRY) * num);
    kalMemFree(*entry, VIR_MEM_TYPE, sizeof(struct DOMAIN_INFO_ENTRY) * num);
    *entry = info;
    return num + extend_size;
}

/*
* count number of country code
*
* @parameter buff - string of country cfg in one line
* @parameter length of buffer
* Return the size of current string
*/
static int get_country_count(uint8_t *buff, int len) {
    int offset = 0 ,count =1;
    while(offset <= len) {
        offset++;
        if(buff[offset] == FORMAT_SEPARATION)
            count++;
    }
    DBGLOG(RLM, ERROR, " get_country_count %d \n", count);
    return count;
}

/*
* parse string into country cfg
*
* @parameter entry - domain cfg
* @parameter buff - string of country cfg in one line
* @parameter length of buffer
* Return the size of current string
*/
static int generate_country_group(struct DOMAIN_INFO_ENTRY* entry, uint8_t *buff, int len) {
    int offset = 0, count = 0;
    entry->u4CountryNum = get_country_count(buff, len);
    //skip format header.
    buff = buff + FORMAT_COUNTRY_GROUP_SIZE;
    len = len - FORMAT_COUNTRY_GROUP_SIZE;
    //set as default country config
    if(len == 0 && entry->u4CountryNum == 1) {
        DBGLOG(RLM, INFO, "country code set empty as default");
        entry->pu2CountryGroup = NULL;
        entry->u4CountryNum = 0;
        return len;
    }
    entry->pu2CountryGroup = (uint16_t *)kalMemAlloc(sizeof(uint16_t) * entry->u4CountryNum, VIR_MEM_TYPE);
    if(entry->pu2CountryGroup == NULL) {
        DBGLOG(RLM, ERROR, "kalMemAlloc country memory fail: %d\n", sizeof(uint16_t) * entry->u4CountryNum);
        return 0;
    }
    while(offset < len) {
        if(buff != NULL && buff[offset] == FORMAT_SEPARATION) {
            count++;
            offset++;
        } else {
            if (buff != NULL && buff[0] != FORMAT_STRING_END) {
                entry->pu2CountryGroup[count] = COUNTRY_CODE_CVT(buff[offset], buff[offset+1]);
            }

            if (DEBUG) {
                DBGLOG(RLM, INFO, " entry->pu2CountryGroup[%d] CC: %c%c , hex %x\n", count, buff[offset], buff[offset+1], entry->pu2CountryGroup[count]);
            }
            //skip to next country code
            offset += 2;
        }
    }
    return offset + 2;
}

int kalReadToFile(const unsigned char* pucPath, unsigned char* pucData, unsigned int u4Size, unsigned int* pu4ReadSize)
{
    struct GLUE_INFO *prGlueInfo = NULL;
    int ret = -1;

    DBGLOG(INIT, TRACE, "kalReadToFile() path %s\n", pucPath);

    prGlueInfo = wlanGetGlueInfo();
    if (prGlueInfo == NULL) {
        DBGLOG(INIT, WARN, "prGlueInfo invalid!!\n");
        return ret;
    }

    if (kalRequestFirmware(pucPath, pucData, u4Size, pu4ReadSize, prGlueInfo->prDev) != 0) {
         DBGLOG(INIT, WARN, "kalReadToFile kalRequestFirmware failed!!\n");
        return ret;
    }

    if (*pu4ReadSize) {
        ret = 0;
    }

    return ret;
}

/*
* convert string into int because mtk give a wrong implement in gl_kal.c
*
* @parameter s - given config
* Return the int of string
*/
static int convertstring(const char *s)
{
    static const char digits[] = "0123456789";  /* legal digits in order */
    unsigned val=0;         /* value we're accumulating */
    int neg=0;              /* set to true if we see a minus sign */

    /* skip whitespace */
    while (*s==' ' || *s=='\t') {
        s++;
    }

    /* check for sign */
    if (*s=='-') {
        neg=1;
        s++;
    } else if (*s=='+') {
        s++;
    }

    /* process each digit */
    while (*s) {
        const char *where;
        unsigned digit;

        /* look for the digit in the list of digits */
        where = strchr(digits, *s);
        if (where == 0) {
            /* not found; not a digit, so stop */
            break;
        }

        /* get the index into the digit list, which is the value */
        digit = (where - digits);

        /* could (should?) check for overflow here */

        /* shift the number over and add in the new digit */
        val = val*10 + digit;

        /* look at the next character */
        s++;
    }

    /* handle negative numbers */
    if (neg) {
        return -val;
    }

    /* done */
    return val;
}


/*
* parse string into channel cfg
*
* @parameter entry - domain cfg
* @parameter buff - string of country cfg in one line
* @parameter length of buffer
* Return the size of current string
*/
static int generate_channel_cfg(struct DOMAIN_SUBBAND_INFO* info, uint8_t *buff, int len) {
    int offset = 0, data_offset = 0;
    uint8_t data[TEMP_BUFFER_SIZE] ={0};
    OPLUS_RLM_INDEX  index = INDEX_UNKNOWN;
    buff = buff + FORMAT_CHANNEL_CFG_SIZE;
    len = len - FORMAT_CHANNEL_CFG_SIZE;
    while(buff != NULL && offset < len) {
        if(buff[offset] != FORMAT_SEPARATION) {
            data[data_offset] = buff[offset];
            data_offset++;
        } else {
            index++;
            // add end format
            if (data_offset < TEMP_BUFFER_SIZE)
                data[data_offset] = FORMAT_STRING_END;
            switch(index) {
                case INDEX_CLASS:
                    info->ucRegClass = convertstring(&data[0]);
                    break;
                case INDEX_BAND:
                    info->ucBand = convertstring(&data[0]);
                    break;
                case INDEX_CHAN_SPAN:
                    info->ucChannelSpan = convertstring(&data[0]);
                    break;
                case INDEX_FIRST_CHAN:
                    info->ucFirstChannelNum = convertstring(&data[0]);
                    break;
                case INDEX_CHAN_NUM:
                    info->ucNumChannels = convertstring(&data[0]);
                    break;
                case INDEX_DFS:
                    //use more effort way to give value
                    info->fgDfs = data[0] == '1' ? (uint8_t)1 : (uint8_t)0;
                    break;
                default:
                    return offset;
            }
            kalMemSet(data, 0, sizeof(uint8_t) * TEMP_BUFFER_SIZE);
            // reset buffer offset
            data_offset = 0;
        }
        offset++;
    }
    if (DEBUG) {
        DBGLOG(RLM, ERROR, "ucRegClass %d, ucBand %d, ucChannelSpan %d,ucFirstChannelNum %d, ucNumChannels %d, fgDfs %x", 
                info->ucRegClass, info->ucBand, info->ucChannelSpan, info->ucFirstChannelNum, info->ucNumChannels, info->fgDfs);
    }
    return offset;
}

/*
* Dump all the domain info as debug function
*
* @parameter entry - domain info container
* @parameter expected_size - how long does domain info malloced
*
*/
static void dump_domain_info(struct DOMAIN_INFO_ENTRY* entry, int expected_size) {
    int i = 0, j = 0, k = 0;
    struct DOMAIN_INFO_ENTRY* domain_info;
    domain_info = entry;
    for (j = 0; j < expected_size && domain_info != NULL; j++) {
        domain_info = entry + j;
        DBGLOG(RLM, ERROR, "country size: %d current num %d", domain_info->u4CountryNum, j);
        for (k = 0; k < domain_info->u4CountryNum && domain_info->pu2CountryGroup != NULL; k++) {
            DBGLOG(RLM, ERROR, "country code: %x", domain_info->pu2CountryGroup[k]);
        }
        for (i = 0; i < FORMAT_CHANNEL_NUM; i++) {
            DBGLOG(RLM, ERROR, "ucRegClass %d, ucBand %d, ucChannelSpan %d,ucFirstChannelNum %d, ucNumChannels %d, fgDfs %x",
            domain_info->rSubBand[i].ucRegClass, domain_info->rSubBand[i].ucBand, domain_info->rSubBand[i].ucChannelSpan,
            domain_info->rSubBand[i].ucFirstChannelNum, domain_info->rSubBand[i].ucNumChannels, domain_info->rSubBand[i].fgDfs);
        }
    }
}

/*
* CFG format:
* country group:**,**,**,**
* channel config: class,band type, bandwith, first channel, channel size, dfs
* channel config: class,band type, bandwith, first channel, channel size, dfs
* channel config: class,band type, bandwith, first channel, channel size, dfs
* channel config: class,band type, bandwith, first channel, channel size, dfs
* channel config: class,band type, bandwith, first channel, channel size, dfs
* channel config: class,band type, bandwith, first channel, channel size, dfs
* exmaple:
* G:JP,TW
* C:81,1,1,1,13,0,
* C:82,1,1,14,1,0,
* C:115,2,4,36,4,0,
* C:118,2,4,52,4,1,
* C:121,2,4,100,11,1,
* C:125,0,0,0,0,0,
*
* @parameter file - file stored domain cfg
* @parameter size - how much we read from file in time
*
* return analysised cfg in struct DOMAIN_INFO_ENTRY
*/
static OPLUS_DOMAIN_INFO_ENTRY* parse_cfg_file(const char *path, unsigned int size, int base_size, int extend_size) {
    uint8_t *pucConfigBuf = NULL;
    uint8_t *temp = NULL;
    struct DOMAIN_INFO_ENTRY* oplus_domain_info = NULL;
    char *oneLine = NULL;
    int cg_count = 0, cc_count = 0;
    int doamin_info_size = base_size;
    struct DOMAIN_INFO_ENTRY* domain_info = NULL;
    struct OPLUS_DOMAIN_INFO_ENTRY *oplus_entry = NULL;
    uint32_t u4ConfigReadLen;
    int len = 0;
    pucConfigBuf = (uint8_t *)kalMemAlloc(size, PHY_MEM_TYPE);
    if (pucConfigBuf == NULL) {
         DBGLOG(RLM, ERROR, "kalMemAlloc file buffer fail: %d\n", size);
         return NULL;
    }
    oplus_domain_info = (struct DOMAIN_INFO_ENTRY *)kalMemAlloc(sizeof(struct DOMAIN_INFO_ENTRY)*base_size, VIR_MEM_TYPE);
    if (oplus_domain_info == NULL) {
         DBGLOG(RLM, ERROR, "kalMemAlloc DOMAIN_INFO_ENTRY fail: %d\n", sizeof(struct DOMAIN_INFO_ENTRY)*base_size);
         kalMemFree(pucConfigBuf, VIR_MEM_TYPE, size);
         return NULL;
    }
    if (kalReadToFile(path, pucConfigBuf, size, &u4ConfigReadLen) != 0) {
        DBGLOG(RLM, ERROR, " failed to read file: %d", u4ConfigReadLen);
        kalMemFree(pucConfigBuf, VIR_MEM_TYPE, size);
        kalMemFree(oplus_domain_info, VIR_MEM_TYPE, sizeof(struct DOMAIN_INFO_ENTRY)*base_size);
        return NULL;
    }
    domain_info = oplus_domain_info;
    temp = pucConfigBuf;
    while ((oneLine = kalStrSep((char **)(&temp), "\r\n"))) {
        len = kalStrLen(oneLine);
        if (DEBUG) {
            DBGLOG(RLM, ERROR, "oneline %s len %d \n", oneLine, len);
        }

        if(oneLine[0] == FORMAT_COUNTRY_GROUP) {
            if(cg_count == doamin_info_size) {
                doamin_info_size = extend_doamin_mem(&oplus_domain_info, cg_count, extend_size);
            }
            domain_info = oplus_domain_info + cg_count;
            generate_country_group(domain_info, oneLine, len);
            cg_count ++;
        } else if(oneLine[0] == FORMAT_CHANNEL_CFG) {
            generate_channel_cfg(&domain_info->rSubBand[cc_count%FORMAT_CHANNEL_NUM], oneLine, len);
            cc_count++;
        } else if(oneLine[0] == FORMAT_FILE_END) {
            break;
        }
        //fix temp point visit address out of pucConfigBuf array index, Fail Kasan Test
        oneLine = (char *)temp;
        if (oneLine[0] == FORMAT_FILE_END) {
            break;
        }
    }
    kalMemFree(pucConfigBuf, PHY_MEM_TYPE, size);
    pucConfigBuf = NULL;
    oplus_entry = (struct OPLUS_DOMAIN_INFO_ENTRY *)kalMemAlloc(sizeof(struct OPLUS_DOMAIN_INFO_ENTRY), VIR_MEM_TYPE);
    if (oplus_entry == NULL) {
         DBGLOG(RLM, ERROR, "kalMemAlloc oplus entry fail \n");
         kalMemFree(oplus_domain_info, VIR_MEM_TYPE, sizeof(struct DOMAIN_INFO_ENTRY)*base_size);
         return NULL;
    }
    oplus_entry->entry = cg_count == doamin_info_size ?
            oplus_domain_info : cut_unused_memory(oplus_domain_info, cg_count, doamin_info_size);
    oplus_entry->size = cg_count;
    if (DEBUG) {
        dump_domain_info(oplus_entry->entry, cg_count);
    }
    return oplus_entry;
}

struct OPLUS_DOMAIN_INFO_ENTRY* get_active_channel_cfg() {
    DBGLOG(RLM, INFO, "get_active_channel_cfg/in \n");
    if(oplus_active_domain_info == NULL) {
        oplus_active_domain_info = parse_cfg_file(CFG_ACTIVE_CHANNEL_FILE, CFG_CHANNEL_ACTIVE_SIZE,
                MEM_BASE_ACTIVE_DOMAIN, MEM_EXTEND_ACTIVE_DOMAIN);
    }
    DBGLOG(RLM, INFO, "get_active_channel_cfg/out \n");
    return oplus_active_domain_info;
}

struct OPLUS_DOMAIN_INFO_ENTRY* get_passive_channel_cfg() {
    DBGLOG(RLM, INFO, "get_passive_channel_cfg/in \n");
    if(oplus_passive_domain_info == NULL) {
        oplus_passive_domain_info = parse_cfg_file(CFG_PASSIVE_CHANNEL_FILE, CFG_CHANNEL_PASSIVE_SIZE,
                MEM_BASE_PASSIVE_DOMAIN, MEM_EXTEND_PASSIVE_DOMAIN);
    }
    DBGLOG(RLM, INFO, "get_passive_channel_cfg/out \n");
    return oplus_passive_domain_info;
}
