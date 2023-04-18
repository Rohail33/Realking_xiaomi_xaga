/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/

#include <cpu_ctrl.h>
#include <topo_ctrl.h>
#include "precomp.h"
#include "wmt_exp.h"

#ifdef CONFIG_WLAN_MTK_EMI
#include <memory/mediatek/emi.h>
#define WIFI_EMI_MEM_OFFSET    0x1D0000
#define WIFI_EMI_MEM_SIZE      0x140000
#define REGION_PDMA	18
#define REGION_WIFI	26
#define DOMAIN_AP	0
#define DOMAIN_CONN	2
#endif

#define MAX_CPU_FREQ (3 * 1024 * 1024) /* in kHZ */
#define MAX_CLUSTER_NUM  3

uint32_t kalGetCpuBoostThreshold(void)
{
	DBGLOG(SW4, TRACE, "enter kalGetCpuBoostThreshold\n");
	/*  5, stands for 250Mbps */
	return 5;
}

int32_t kalBoostCpu(IN struct ADAPTER *prAdapter,
		    IN uint32_t u4TarPerfLevel,
		    IN uint32_t u4BoostCpuTh)
{
	struct cpu_ctrl_data freq_to_set[MAX_CLUSTER_NUM];
	int32_t i = 0, i4Freq = -1;

	uint32_t u4ClusterNum = topo_ctrl_get_nr_clusters();

	ASSERT(u4ClusterNum <= MAX_CLUSTER_NUM);
	/* ACAO, we dont have to set core number */
	i4Freq = (u4TarPerfLevel >= u4BoostCpuTh) ? MAX_CPU_FREQ : -1;
	for (i = 0; i < u4ClusterNum; i++) {
		freq_to_set[i].min = i4Freq;
		freq_to_set[i].max = i4Freq;
	}

	update_userlimit_cpu_freq(CPU_KIR_WIFI, u4ClusterNum, freq_to_set);

	return 0;
}

#ifdef CONFIG_WLAN_MTK_EMI
void kalSetEmiMpuProtection(phys_addr_t emiPhyBase, bool enable)
{
}

void kalSetDrvEmiMpuProtection(phys_addr_t emiPhyBase, uint32_t offset,
			       uint32_t size)
{
	struct emimpu_region_t region;
	unsigned long long start = emiPhyBase + offset;
	unsigned long long end = emiPhyBase + offset + size - 1;
	int ret;

	DBGLOG(INIT, INFO, "emiPhyBase: 0x%p, offset: %d, size: %d\n",
				emiPhyBase, offset, size);

	ret = mtk_emimpu_init_region(&region, REGION_PDMA);
	if (ret) {
		DBGLOG(INIT, ERROR, "mtk_emimpu_init_region(%u) failed %d\n",
				REGION_PDMA, ret);
		return;
	}
	mtk_emimpu_set_addr(&region, start, end);
	mtk_emimpu_set_apc(&region, DOMAIN_AP, MTK_EMIMPU_NO_PROTECTION);
	mtk_emimpu_set_apc(&region, DOMAIN_CONN, MTK_EMIMPU_NO_PROTECTION);
	mtk_emimpu_lock_region(&region, MTK_EMIMPU_LOCK);
	ret = mtk_emimpu_set_protection(&region);
	if (ret)
		DBGLOG(INIT, ERROR,
			"mtk_emimpu_set_protection(%u) failed %d\n",
			REGION_PDMA, ret);
	mtk_emimpu_free_region(&region);
}
#endif

int32_t kalGetFwFlavorByPlat(uint8_t *flavor)
{
	int32_t ret = 1;
	const uint32_t adie_chip_id = mtk_wcn_wmt_ic_info_get(WMTCHIN_ADIE);

	DBGLOG(INIT, INFO, "adie_chip_id: 0x%x\n", adie_chip_id);

	switch (adie_chip_id) {
	case 0x6631:
		*flavor = 'a';
		break;
	default:
		ret = 0;
		break;
	}

	return ret;
}
