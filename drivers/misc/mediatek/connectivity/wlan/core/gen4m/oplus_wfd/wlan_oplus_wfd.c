/******************************************************************************
** Copyright (C), 2019-2029, Oplus Mobile Comm Corp., Ltd
** OPLUS_EDIT, All rights reserved.
** File: - wlan_oplus_wfd.c
** Description: wlan oplus wfd
**
** Version: 1.0
** Date : 2020/07/27
** TAG: OPLUS_FEATURE_WIFI_CAPCENTER
** ------------------------------- Revision History: ----------------------------
** <author>                                <data>        <version>       <desc>
** ------------------------------------------------------------------------------
 *******************************************************************************/
#ifdef OPLUS_FEATURE_WIFI_OPLUSWFD
#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/wireless.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <net/arp.h>
#include <net/cfg80211.h>
#include <net/mac80211.h>
#include <linux/nl80211.h>
#include <linux/spinlock.h>
#include "gl_os.h"
#include "debug.h"
#include "wlan_lib.h"
#include "gl_wext.h"
#include "gl_cfg80211.h"
#include "gl_kal.h"
#include "precomp.h"
#include "nic_cmd_event.h"
#include "wsys_cmd_handler_fw.h"
#include <net/oplus/oplus_wfd_wlan.h>
#include <linux/preempt.h>
#define LOG_TAG "[oplus_wfd_mtk] %s line:%d "
#define debug(fmt, args...) printk(LOG_TAG fmt, __FUNCTION__, __LINE__, ##args)

#define BIT_SUPPORTED_PHY_NO_HT     1
#define BIT_SUPPORTED_PHY_HT        2
#define BIT_SUPPORTED_PHY_VHT       4
#define BIT_SUPPORTED_PHY_HE        8

#define MTK_SUPPORT_AVOID_CHANNEL 1

static int is_remove_He_ie_from_prebe_request = 0;
static struct wireless_dev *s_hdd_ctx = NULL;
static DEFINE_MUTEX(ctx_mutex);

static DECLARE_COMPLETION(s_avoid_channel_query_comp);
static char s_avoid_channel_query_comp_inited = 0;
static struct EVENT_LTE_SAFE_CHN s_event_lte_safe_chn;

extern uint8_t wlanGetChannelIndex(enum ENUM_BAND eBand, uint8_t channel);
#if CFG_SUPPORT_IDC_CH_SWITCH
extern struct EVENT_LTE_SAFE_CHN g_rLteSafeChInfo;
#endif

int oplus_wfd_get_remove_He_ie_flag(void);
void oplus_wfd_set_hdd_ctx(struct wireless_dev *hdd_ctx);
void oplus_register_oplus_wfd_wlan_ops_qcom(void);

static void remove_he_ie_from_probe_request(int remove);
static int get_dbs_capacity(void);
static int get_phy_capacity(int band);
static void get_avoid_channels(int *len, int* freqs, int max_num);

static struct oplus_wfd_wlan_ops_t oplus_wfd_wlan_ops_mtk = {
    .remove_he_ie_from_probe_request = remove_he_ie_from_probe_request,
    .get_dbs_capacity = get_dbs_capacity,
    .get_phy_capacity = get_phy_capacity,
    .get_supported_channels = NULL,
#if MTK_SUPPORT_AVOID_CHANNEL
    .get_avoid_channels = get_avoid_channels,
#else
    .get_avoid_channels = NULL,
#endif
};

static void remove_he_ie_from_probe_request(int remove) {
    is_remove_He_ie_from_prebe_request = remove;
    debug("set remove he ie =%d", remove);
}

static struct wireless_dev * get_wdev_sta()
{
    return s_hdd_ctx;
}

static struct ADAPTER * get_adapter() {
    struct wireless_dev *wdev_sta = NULL;
    struct GLUE_INFO *prGlueInfo = NULL;
    struct ADAPTER *prAdapter = NULL;
    struct wiphy *wiphy;

    wdev_sta = get_wdev_sta();

    if (wdev_sta == NULL) {
        debug("wdev_sta is null, donothing");
        goto EXIT;
    }

    if (kalIsHalted()) {
        debug("kalIsHalted, donothing");
        goto EXIT;
    }

    wiphy = wdev_sta->wiphy;
    if (wiphy == NULL) {
        debug("net_dev is null, exit");
        goto EXIT;
    }

    WIPHY_PRIV(wiphy, prGlueInfo);
    if (prGlueInfo == NULL) {
        debug("prGlueInfo is null, exit");
        goto EXIT;
    }
    if (!prGlueInfo->fgIsRegistered) {
        debug("netdevice is not registed, exit");
        goto EXIT;
    }

    prAdapter = prGlueInfo->prAdapter;
    if (prAdapter == NULL) {
        debug("prAdapter is null, exit");
        goto EXIT;
    }

EXIT:
    return prAdapter;
}
static int get_dbs_capacity(void)
{
    #define DBS_UNKNOWN 0
    #define DBS_NULL 1
    #define DBS_SISO 2
    #define DBS_MIMO 3
    int cap = DBS_UNKNOWN;
    struct ADAPTER *prAdapter = NULL;
    mutex_lock(&ctx_mutex);

    prAdapter = get_adapter();
    if (prAdapter == NULL) {
        debug("prAdapter is null, exit");
        goto EXIT;
    }

    cap = DBS_NULL;

#if CFG_SUPPORT_DBDC
    if (prAdapter->rWifiVar.eDbdcMode != ENUM_DBDC_MODE_DISABLED) {
        cap = DBS_SISO;
    }
#endif

EXIT:
    mutex_unlock(&ctx_mutex);
    return cap;
}

static int get_phy_capacity(int band)
{
    int phy_bit = 0;
    struct ADAPTER *prAdapter = NULL;

    mutex_lock(&ctx_mutex);
    prAdapter = get_adapter();
    if (prAdapter == NULL) {
        debug("prAdapter is null, exit");
        goto EXIT;
    }

    if (prAdapter->rWifiVar.ucStaHt) {
        phy_bit |= BIT_SUPPORTED_PHY_HT;
    }
    if (prAdapter->rWifiVar.ucStaVht) {
        phy_bit |= BIT_SUPPORTED_PHY_VHT | BIT_SUPPORTED_PHY_HT;
    }
#if (CFG_SUPPORT_802_11AX == 1)
    if (prAdapter->rWifiVar.ucStaHe) {
        phy_bit |= BIT_SUPPORTED_PHY_HE | BIT_SUPPORTED_PHY_VHT | BIT_SUPPORTED_PHY_HT;
    }
#endif

EXIT:
    mutex_unlock(&ctx_mutex);
    return phy_bit;
}

static int convertChannelToFrequency(int channel) {
    if ((channel >= 0) && (channel < 14)) {
        return 2412 + 5 * (channel - 1);
    } else if (channel == 14) {
        return 2484;
    } else if ((channel >= 36) && (channel <= 181)) {
        return 5180 + (channel - 36) * 5;
    }
    return -1;
}

#if MTK_SUPPORT_AVOID_CHANNEL

void oplus_nicOidCmdTimeoutCommon(IN struct ADAPTER *prAdapter,
			    IN struct CMD_INFO *prCmdInfo) {
    debug("oplus_nicOidCmdTimeoutCommon timeout");
}

void oplus_WfdCmdEventQueryLteSafeChn(IN struct ADAPTER *prAdapter,
        IN struct CMD_INFO *prCmdInfo,
        IN uint8_t *pucEventBuf)
{
    struct EVENT_LTE_SAFE_CHN *prEvent;
    int index;
    do {
        if ((prAdapter == NULL) || (prCmdInfo == NULL) || (pucEventBuf == NULL)
            || (prCmdInfo->pvInformationBuffer == NULL)) {
            debug("oplus_WfdCmdEventQueryLteSafeChn error");
            break;
        }

        prEvent = (struct EVENT_LTE_SAFE_CHN *) pucEventBuf;

        /* Statistics from FW is valid */
        if (prEvent->u4Flags & BIT(0)) {
            memcpy(&s_event_lte_safe_chn, prEvent, sizeof(s_event_lte_safe_chn));
            debug("FW's event is valid");
            for (index = 0 ; index < ENUM_SAFE_CH_MASK_MAX_NUM; index++) {
                debug("[%d]=0x%x",index, prEvent->rLteSafeChn.au4SafeChannelBitmask[index]);
            }
        } else {
            debug("FW's event is NOT valid");
        }
    } while (false);

    complete(&s_avoid_channel_query_comp);
}

static uint32_t oplus_wlanQueryLteSafeChannel(IN struct ADAPTER *prAdapter,
        IN uint8_t ucRoleIndex)
{
    uint32_t rResult = WLAN_STATUS_FAILURE;
    struct CMD_GET_LTE_SAFE_CHN rQuery_LTE_SAFE_CHN;

    kalMemZero(&rQuery_LTE_SAFE_CHN, sizeof(rQuery_LTE_SAFE_CHN));

    do {
        if (!prAdapter)
        break;

        kalMemZero(&s_event_lte_safe_chn, sizeof(s_event_lte_safe_chn));

        /* Get LTE safe channel list */
        wlanSendSetQueryCmd(prAdapter,
            CMD_ID_GET_LTE_CHN,
            FALSE,
            TRUE,
            FALSE, /* Query ID */
            oplus_WfdCmdEventQueryLteSafeChn, /* The handler to receive*/
            oplus_nicOidCmdTimeoutCommon,
            sizeof(struct CMD_GET_LTE_SAFE_CHN),
            (uint8_t *)&rQuery_LTE_SAFE_CHN,
            &s_event_lte_safe_chn,
        0);
        rResult = WLAN_STATUS_SUCCESS;
    } while (0);

    return rResult;
}


static void get_avoid_channels(int* out_len, int* out_freqs, int max_num)
{
    struct ADAPTER *prAdapter = NULL;
    uint32_t u4LteSafeChnBitMask_2G  = 0, u4LteSafeChnBitMask_5G_1 = 0,
            u4LteSafeChnBitMask_5G_2 = 0;
    struct RF_CHANNEL_INFO aucChannelList[MAX_CHN_NUM];
    uint8_t ucNumOfChannel;
    int freq;
    int len = 0;
    int index = 0;
    enum ENUM_BAND eBand;

    *out_len = 0;
    mutex_lock(&ctx_mutex);

    prAdapter = get_adapter();
    if (prAdapter == NULL) {
        debug("prAdapter is null, exit");
        goto EXIT;
    }

    debug("get_avoid_channels enter");
    reinit_completion(&s_avoid_channel_query_comp);
    if (WLAN_STATUS_SUCCESS == oplus_wlanQueryLteSafeChannel(prAdapter, 0)) {
        //wait for fw response
        debug("get_avoid_channels begine to wait for fw");
        if (!wait_for_completion_timeout(&s_avoid_channel_query_comp, msecs_to_jiffies(1000))) {
            //time out
            debug("get_avoid_channels wait for fm timeout ");
        } else {
            //fw return the information
            debug("get_avoid_channels fw return info, u4Flags=0x%x", s_event_lte_safe_chn.u4Flags);
            if (s_event_lte_safe_chn.u4Flags & BIT(0)) {
                u4LteSafeChnBitMask_2G = s_event_lte_safe_chn
                    .rLteSafeChn.au4SafeChannelBitmask[0];
                u4LteSafeChnBitMask_5G_1 = s_event_lte_safe_chn
                    .rLteSafeChn.au4SafeChannelBitmask[1];
                u4LteSafeChnBitMask_5G_2 = s_event_lte_safe_chn
                .rLteSafeChn.au4SafeChannelBitmask[2];
            } else {
                debug("no valid info of LTE safe channel");
                goto EXIT;
            }

            for (eBand = BAND_2G4; eBand <= BAND_5G; eBand++) {
                kalMemZero(aucChannelList, sizeof(struct RF_CHANNEL_INFO) * MAX_CHN_NUM);

                rlmDomainGetChnlList(prAdapter, eBand, TRUE, MAX_CHN_NUM,
                        &ucNumOfChannel, aucChannelList);

                //goto safe channel loop
                for (index = 0; index < ucNumOfChannel && len < max_num; index++) {
                    uint8_t ucIdx;
                    freq = -1;
                    ucIdx = wlanGetChannelIndex(eBand, aucChannelList[index].ucChannelNum);
                    if (ucIdx >= MAX_CHN_NUM) {
                        continue;
                    }

                    if (aucChannelList[index].ucChannelNum <= 14) {
                        if (!(u4LteSafeChnBitMask_2G & BIT(aucChannelList[index].ucChannelNum))) {
                           freq = convertChannelToFrequency(aucChannelList[index].ucChannelNum);
                        }
                    } else if ((aucChannelList[index].ucChannelNum >= 36) && (aucChannelList[index].ucChannelNum <= 144)) {
                        if (!(u4LteSafeChnBitMask_5G_1 & BIT((aucChannelList[index].ucChannelNum - 36) / 4))) {
                            freq = convertChannelToFrequency(aucChannelList[index].ucChannelNum);
                        }
                    } else if ((aucChannelList[index].ucChannelNum >= 149) && (aucChannelList[index].ucChannelNum <= 181)) {
                        if (!(u4LteSafeChnBitMask_5G_2 & BIT((aucChannelList[index].ucChannelNum - 149) / 4))) {
                            freq = convertChannelToFrequency(aucChannelList[index].ucChannelNum);
                        }
                    }
                    //debug("index=%d, channle=%d, freq=%d,max_num=%d", index, aucChannelList[index].ucChannelNum, freq, max_num);
                    if (freq > 0 && len < max_num) {
                         out_freqs[len++] = freq;
                    }
                }
            }
        }
    } else {
        debug("oplus_wlanQueryLteSafeChannel return false, get do nothing ");
    }

EXIT:
    *out_len = len;
    mutex_unlock(&ctx_mutex);
}
#if 0
//g_rLteSafeChInfo always not available
static void get_avoid_channels_tmp(int* out_len, int* out_freqs, int max_num)
{
    struct ADAPTER *prAdapter = NULL;
    uint32_t u4LteSafeChnBitMask_2G  = 0, u4LteSafeChnBitMask_5G_1 = 0,
            u4LteSafeChnBitMask_5G_2 = 0;
    struct RF_CHANNEL_INFO aucChannelList[MAX_CHN_NUM];
    uint8_t ucNumOfChannel;
    int freq;
    int len = 0;
    int index = 0;
    enum ENUM_BAND eBand;

    *out_len = 0;
    mutex_lock(&ctx_mutex);

    prAdapter = get_adapter();
    if (prAdapter == NULL) {
        debug("prAdapter is null, exit");
        goto EXIT;
    }

#if CFG_SUPPORT_IDC_CH_SWITCH
    debug("CFG_SUPPORT_IDC_CH_SWITCH support, u4Flags=0x%x", g_rLteSafeChInfo.u4Flags);
    if (g_rLteSafeChInfo.u4Flags & BIT(0)) {
        u4LteSafeChnBitMask_2G = g_rLteSafeChInfo
            .rLteSafeChn.au4SafeChannelBitmask[0];
        u4LteSafeChnBitMask_5G_1 = g_rLteSafeChInfo
            .rLteSafeChn.au4SafeChannelBitmask[1];
        u4LteSafeChnBitMask_5G_2 = g_rLteSafeChInfo
        .rLteSafeChn.au4SafeChannelBitmask[2];
    } else {
        debug("no valid info of LTE safe channel");
        goto EXIT;
    }

    for (eBand = BAND_2G4; eBand <= BAND_5G; eBand++) {
        kalMemZero(aucChannelList, sizeof(struct RF_CHANNEL_INFO) * MAX_CHN_NUM);

        rlmDomainGetChnlList(prAdapter, eBand, TRUE, MAX_CHN_NUM,
                &ucNumOfChannel, aucChannelList);

        //goto safe channel loop
        for (index = 0; index < ucNumOfChannel && len < max_num; index++) {
            uint8_t ucIdx;
            freq = -1;
            ucIdx = wlanGetChannelIndex(aucChannelList[index].ucChannelNum);
            if (ucIdx >= MAX_CHN_NUM) {
                continue;
            }

            if (aucChannelList[index].ucChannelNum <= 14) {
                if (!(u4LteSafeChnBitMask_2G & BIT(aucChannelList[index].ucChannelNum))) {
                   freq = convertChannelToFrequency(aucChannelList[index].ucChannelNum);
                }
            } else if ((aucChannelList[index].ucChannelNum >= 36) && (aucChannelList[index].ucChannelNum <= 144)) {
                if (!(u4LteSafeChnBitMask_5G_1 & BIT((aucChannelList[index].ucChannelNum - 36) / 4))) {
                    freq = convertChannelToFrequency(aucChannelList[index].ucChannelNum);
                }
            } else if ((aucChannelList[index].ucChannelNum >= 149) && (aucChannelList[index].ucChannelNum <= 181)) {
                if (!(u4LteSafeChnBitMask_5G_2 & BIT((aucChannelList[index].ucChannelNum - 149) / 4))) {
                    freq = convertChannelToFrequency(aucChannelList[index].ucChannelNum);
                }
            }
            if (freq > 0 && len < max_num) {
                 out_freqs[len++] = freq;
            }
        }
    }
#endif
EXIT:
    *out_len = len;
    mutex_unlock(&ctx_mutex);
}
#endif
#endif
/*************public begin********************/
int oplus_wfd_get_remove_He_ie_flag(void)
{
    return is_remove_He_ie_from_prebe_request;
}

void oplus_wfd_set_hdd_ctx(struct wireless_dev *hdd_ctx)
{
    debug("hdd_ctx =%p", hdd_ctx);
    mutex_lock(&ctx_mutex);
    s_hdd_ctx = hdd_ctx;
    mutex_unlock(&ctx_mutex);
}

void oplus_register_oplus_wfd_wlan_ops_mtk(void)
{
    if (s_avoid_channel_query_comp_inited == 0) {
        init_completion(&s_avoid_channel_query_comp);
        s_avoid_channel_query_comp_inited = 1;
    }
    register_oplus_wfd_wlan_ops(&oplus_wfd_wlan_ops_mtk);
}

/*************public end********************/
#endif
