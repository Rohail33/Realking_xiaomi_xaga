// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/version.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/genalloc.h>
#include <linux/sched.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/fb.h>
#include <mt-plat/mtk_gpu_utility.h>

#include "ged_base.h"
#include "ged_hal.h"
#include "ged_sysfs.h"

#include "ged_dvfs.h"

#include "ged_notify_sw_vsync.h"
#include "ged_kpi.h"
#include "ged_global.h"
#include "ged_dcs.h"

static struct kobject *hal_kobj;

int tokenizer(char *pcSrc, int i32len, int *pi32IndexArray, int i32NumToken)
{
	int i = 0;
	int j = 0;
	int head = -1;

	for ( ; i < i32len; i++) {
		if (pcSrc[i] != ' ') {
			if (head == -1)
				head = i;
		} else {
			if (head != -1) {
				pi32IndexArray[j] = head;
				j++;
				if (j == i32NumToken)
					return j;
				head = -1;
			}
			pcSrc[i] = 0;
		}
	}

	if (head != -1) {
		pi32IndexArray[j] = head;
		j++;
		return j;
	}

	return -1;
}

//-----------------------------------------------------------------------------
static ssize_t total_gpu_freq_level_count_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	unsigned int ui32FreqLevelCount;

	if (false == mtk_custom_get_gpu_freq_level_count(&ui32FreqLevelCount))
		ui32FreqLevelCount = 0;

	return scnprintf(buf, PAGE_SIZE, "%u\n", ui32FreqLevelCount);
}

static KOBJ_ATTR_RO(total_gpu_freq_level_count);
//-----------------------------------------------------------------------------
static ssize_t custom_boost_gpu_freq_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	unsigned int ui32BoostGpuFreqLevel = 0;

	ui32BoostGpuFreqLevel = ged_dvfs_get_custom_boost_gpu_freq();

	return scnprintf(buf, PAGE_SIZE, "%u\n", ui32BoostGpuFreqLevel);
}

static ssize_t custom_boost_gpu_freq_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0) {
				if (i32Value < 0)
					i32Value = 0;
				mtk_custom_boost_gpu_freq(i32Value);
			}
		}
	}

	return count;
}

static KOBJ_ATTR_RW(custom_boost_gpu_freq);
//-----------------------------------------------------------------------------
static ssize_t custom_upbound_gpu_freq_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	unsigned int ui32UpboundGpuFreqLevel = 0;

	ui32UpboundGpuFreqLevel = ged_dvfs_get_custom_ceiling_gpu_freq();

	return scnprintf(buf, PAGE_SIZE, "%u\n", ui32UpboundGpuFreqLevel);
}

static ssize_t custom_upbound_gpu_freq_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0) {
				if (i32Value < 0)
					i32Value = 0;
				mtk_custom_upbound_gpu_freq(i32Value);
			}
		}
	}

	return count;
}

static KOBJ_ATTR_RW(custom_upbound_gpu_freq);
//-----------------------------------------------------------------------------
static ssize_t current_freqency_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	struct GED_DVFS_FREQ_DATA sFreqInfo;

	ged_dvfs_get_gpu_cur_freq(&sFreqInfo);

	return scnprintf(buf, PAGE_SIZE, "%u %lu\n",
		sFreqInfo.ui32Idx, sFreqInfo.ulFreq);
}

static KOBJ_ATTR_RO(current_freqency);
//-----------------------------------------------------------------------------
static ssize_t previous_freqency_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	struct GED_DVFS_FREQ_DATA sFreqInfo;

	ged_dvfs_get_gpu_pre_freq(&sFreqInfo);

	return scnprintf(buf, PAGE_SIZE, "%u %lu\n",
		sFreqInfo.ui32Idx, sFreqInfo.ulFreq);
}

static KOBJ_ATTR_RO(previous_freqency);
//-----------------------------------------------------------------------------
static ssize_t gpu_utilization_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	unsigned int loading = 0;
	unsigned int block = 0;
	unsigned int idle = 0;

	mtk_get_gpu_loading(&loading);
	mtk_get_gpu_block(&block);
	mtk_get_gpu_idle(&idle);

	return scnprintf(buf, PAGE_SIZE, "%u %u %u\n", loading, block, idle);
}

static KOBJ_ATTR_RO(gpu_utilization);
//-----------------------------------------------------------------------------
static int32_t _boost_level = -1;
#define MAX_BOOST_DIGITS 10
static ssize_t gpu_boost_level_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", _boost_level);
}

static ssize_t gpu_boost_level_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char str_num[MAX_BOOST_DIGITS];
	long val;

	if (count > 0 && count < MAX_BOOST_DIGITS) {
		if (scnprintf(str_num, MAX_BOOST_DIGITS, "%s", buf)) {
			if (kstrtol(str_num, 10, &val) == 0)
				_boost_level = (int32_t)val;
		}
	}

	return count;
}

static KOBJ_ATTR_RW(gpu_boost_level);
//-----------------------------------------------------------------------------
int ged_dvfs_boost_value(void)
{
	return _boost_level;
}

//-----------------------------------------------------------------------------
#ifdef MTK_GED_KPI
static ssize_t ged_kpi_show(struct kobject *kobj, struct kobj_attribute *attr,
		char *buf)
{
	unsigned int fps;
	unsigned int cpu_time;
	unsigned int gpu_time;
	unsigned int response_time;
	unsigned int gpu_remained_time;
	unsigned int cpu_remained_time;
	unsigned int gpu_freq;

	fps = ged_kpi_get_cur_fps();
	cpu_time = ged_kpi_get_cur_avg_cpu_time();
	gpu_time = ged_kpi_get_cur_avg_gpu_time();
	response_time = ged_kpi_get_cur_avg_response_time();
	cpu_remained_time = ged_kpi_get_cur_avg_cpu_remained_time();
	gpu_remained_time = ged_kpi_get_cur_avg_gpu_remained_time();
	gpu_freq = ged_kpi_get_cur_avg_gpu_freq();

	return scnprintf(buf, PAGE_SIZE, "%u,%u,%u,%u,%u,%u,%u\n",
			fps, cpu_time, gpu_time, response_time,
			cpu_remained_time, gpu_remained_time, gpu_freq);
}

static KOBJ_ATTR_RO(ged_kpi);
#endif /* MTK_GED_KPI */
//-----------------------------------------------------------------------------
static ssize_t dvfs_margin_value_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int i32DvfsMarginValue;
	int pos = 0;
	int length;

	if (false == mtk_get_dvfs_margin_value(&i32DvfsMarginValue)) {
		i32DvfsMarginValue = 0;
		length = scnprintf(buf + pos, PAGE_SIZE - pos,
				"call mtk_get_dvfs_margin_value false\n");
		pos += length;
	}
	length = scnprintf(buf + pos, PAGE_SIZE - pos,
			"%d\n", i32DvfsMarginValue);
	pos += length;

	return pos;
}

static ssize_t dvfs_margin_value_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0)
				mtk_dvfs_margin_value(i32Value);
		}
	}

	return count;
}

static KOBJ_ATTR_RW(dvfs_margin_value);
//-----------------------------------------------------------------------------
static ssize_t loading_base_dvfs_step_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int i32StepValue;
	int pos = 0;
	int length;

	if (false == mtk_get_loading_base_dvfs_step(&i32StepValue)) {
		i32StepValue = 0;
		length = scnprintf(buf + pos, PAGE_SIZE - pos,
				"call mtk_get_loading_base_dvfs_step false\n");
		pos += length;
	}
	length = scnprintf(buf + pos, PAGE_SIZE - pos,
			"%x\n", i32StepValue);
	pos += length;

	return pos;
}

static ssize_t loading_base_dvfs_step_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0)
				mtk_loading_base_dvfs_step(i32Value);
		}
	}

	return count;
}

static KOBJ_ATTR_RW(loading_base_dvfs_step);
//-----------------------------------------------------------------------------
static ssize_t timer_base_dvfs_margin_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int i32DvfsMarginValue;
	int pos = 0;
	int length;

	if (false == mtk_get_timer_base_dvfs_margin(&i32DvfsMarginValue)) {
		i32DvfsMarginValue = 0;
		length = scnprintf(buf + pos, PAGE_SIZE - pos,
				"call mtk_get_timer_base_dvfs_margin false\n");
		pos += length;
	}
	length = scnprintf(buf + pos, PAGE_SIZE - pos,
			"%d\n", i32DvfsMarginValue);
	pos += length;

	return pos;
}

static ssize_t timer_base_dvfs_margin_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0)
				mtk_timer_base_dvfs_margin(i32Value);
		}
	}

	return count;
}

static KOBJ_ATTR_RW(timer_base_dvfs_margin);


static ssize_t dvfs_loading_mode_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	unsigned int ui32DvfsLoadingMode;
	int pos = 0;
	int length;

	if (false == mtk_get_dvfs_loading_mode(&ui32DvfsLoadingMode)) {
		ui32DvfsLoadingMode = 0;
		length = scnprintf(buf + pos, PAGE_SIZE - pos,
				"call mtk_get_dvfs_loading_mode false\n");
		pos += length;
	}
	length = scnprintf(buf + pos, PAGE_SIZE - pos,
			"%d\n", ui32DvfsLoadingMode);
	pos += length;

	return pos;
}

static ssize_t dvfs_loading_mode_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0)
				mtk_dvfs_loading_mode(i32Value);
		}
	}

	return count;
}

static KOBJ_ATTR_RW(dvfs_loading_mode);

static ssize_t fastdvfs_mode_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	unsigned int ui32FastDVFSMode;
	int pos = 0;
	int length;

	if (false == mtk_get_fastdvfs_mode(&ui32FastDVFSMode)) {
		ui32FastDVFSMode = 0;
		length = scnprintf(buf + pos, PAGE_SIZE - pos,
				"fdvfs is off\n");
		pos += length;
	}

	if ((ui32FastDVFSMode & 0x00000001) > 0) {
		length = scnprintf(buf + pos, PAGE_SIZE - pos,
				"FastDVFS is enabled (%d)\n", ui32FastDVFSMode);
	} else {
		length = scnprintf(buf + pos, PAGE_SIZE - pos,
				"FastDVFS is disabled\n");
	}
	pos += length;

	return pos;
}

static ssize_t fastdvfs_mode_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	unsigned int u32Value;
	unsigned int ui32FastDVFSMode;

	if (true == mtk_get_fastdvfs_mode(&ui32FastDVFSMode)) {
		ui32FastDVFSMode &= 0xFFFFFFFE;
		if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
			if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
				if (kstrtouint(acBuffer, 0, &u32Value) == 0) {
					ui32FastDVFSMode |= (u32Value & 0x1);
					mtk_set_fastdvfs_mode(ui32FastDVFSMode);
				}
			}
		}
	}

	return count;
}

static KOBJ_ATTR_RW(fastdvfs_mode);

//-----------------------------------------------------------------------------
static struct notifier_block ged_fb_notifier;

static int ged_fb_notifier_callback(struct notifier_block *self,
	unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int blank;

	/* If we aren't interested in this event, skip it immediately ... */
	if (event != FB_EVENT_BLANK)
		return 0;

	blank = *(int *)evdata->data;

	switch (blank) {
	case FB_BLANK_UNBLANK:
		g_ui32EventStatus |= GED_EVENT_LCD;
		ged_dvfs_probe_signal(GED_GAS_SIGNAL_EVENT);
		break;
	case FB_BLANK_POWERDOWN:
		g_ui32EventStatus &= ~GED_EVENT_LCD;
		ged_dvfs_probe_signal(GED_GAS_SIGNAL_EVENT);
		break;
	default:
		break;
	}

	return 0;
}

struct ged_event_change_entry_t {
	ged_event_change_fp callback;
	void *private_data;
	char name[128];
	struct list_head sList;
};

static struct {
	struct mutex lock;
	struct list_head listen;
} g_ged_event_change = {
	.lock     = __MUTEX_INITIALIZER(g_ged_event_change.lock),
	.listen   = LIST_HEAD_INIT(g_ged_event_change.listen),
};

bool mtk_register_ged_event_change(const char *name,
	ged_event_change_fp callback, void *private_data)
{
	struct ged_event_change_entry_t *entry = NULL;

	entry = kmalloc(sizeof(struct ged_event_change_entry_t), GFP_KERNEL);
	if (entry == NULL)
		return false;

	entry->callback = callback;
	entry->private_data = private_data;
	strncpy(entry->name, name, sizeof(entry->name) - 1);
	entry->name[sizeof(entry->name) - 1] = 0;
	INIT_LIST_HEAD(&entry->sList);

	mutex_lock(&g_ged_event_change.lock);

	list_add(&entry->sList, &g_ged_event_change.listen);

	mutex_unlock(&g_ged_event_change.lock);

	return true;
}

bool mtk_unregister_ged_event_change(const char *name)
{
	struct list_head *pos, *head;
	struct ged_event_change_entry_t *entry = NULL;

	mutex_lock(&g_ged_event_change.lock);

	head = &g_ged_event_change.listen;
	list_for_each(pos, head) {
		entry = list_entry(pos, struct ged_event_change_entry_t, sList);
		if (strncmp(entry->name, name, sizeof(entry->name) - 1) == 0)
			break;
		entry = NULL;
	}

	if (entry) {
		list_del(&entry->sList);
		kfree(entry);
	}

	mutex_unlock(&g_ged_event_change.lock);

	return true;
}

void mtk_ged_event_notify(int events)
{
	struct list_head *pos, *head;
	struct ged_event_change_entry_t *entry = NULL;

	mutex_lock(&g_ged_event_change.lock);

	head = &g_ged_event_change.listen;
	list_for_each(pos, head) {
		entry = list_entry(pos, struct ged_event_change_entry_t, sList);
		entry->callback(entry->private_data, events);
	}

	mutex_unlock(&g_ged_event_change.lock);
}

//-----------------------------------------------------------------------------
#ifdef GED_DCS_POLICY
static ssize_t dcs_mode_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int dcs_enable = 0;
	int pos = 0;

	dcs_enable = is_dcs_enable();

	if (dcs_enable) {
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
					"DCS Policy is enable\n");
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
					"Current in use core num: %d\n", dcs_get_cur_core_num());
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
					"Available max core num: %d\n",	dcs_get_max_core_num());
	} else {
		pos += scnprintf(buf + pos, PAGE_SIZE - pos,
					"DCS Policy is disabled\n");
	}
	return pos;
}

static ssize_t dcs_mode_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0) {
				if (i32Value < 0)
					i32Value = 0;
				dcs_enable(i32Value);
			}
		}
	}
	return count;
}

static KOBJ_ATTR_RW(dcs_mode);
#endif /* GED_DCS_POLICY */

static ssize_t fw_idle_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	unsigned int ui32FwIdle;
	int pos = 0;
	int length;

	ui32FwIdle = ged_kpi_get_fw_idle();

	length = scnprintf(buf + pos, PAGE_SIZE - pos,
			"%d\n", ui32FwIdle);
	pos += length;

	return pos;
}

static ssize_t fw_idle_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];
	int i32Value;

	if ((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &i32Value) == 0)
				ged_kpi_set_fw_idle(i32Value);
		}
	}

	return count;
}
static KOBJ_ATTR_RW(fw_idle);
//-----------------------------------------------------------------------------
GED_ERROR ged_hal_init(void)
{
	GED_ERROR err = GED_OK;

	err = ged_sysfs_create_dir(NULL, "hal", &hal_kobj);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create hal dir!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj,
		&kobj_attr_total_gpu_freq_level_count);
	if (unlikely(err != GED_OK)) {
		GED_LOGE(
			"Failed to create total_gpu_freq_level_count entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_custom_boost_gpu_freq);
	if (unlikely(err != GED_OK)) {
		GED_LOGE(
			"Failed to create custom_boost_gpu_freq entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj,
		&kobj_attr_custom_upbound_gpu_freq);
	if (unlikely(err != GED_OK)) {
		GED_LOGE(
			"Failed to create custom_upbound_gpu_freq entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_current_freqency);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create current_freqency entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_previous_freqency);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create previous_freqency entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_gpu_utilization);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create gpu_utilization entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_gpu_boost_level);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create gpu_boost_level entry!\n");
		goto ERROR;
	}

#ifdef MTK_GED_KPI
	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_ged_kpi);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create ged_kpi entry!\n");
		goto ERROR;
	}
#endif

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_dvfs_margin_value);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create dvfs_margin_value entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj,
		&kobj_attr_loading_base_dvfs_step);
	if (unlikely(err != GED_OK)) {
		GED_LOGE(
			"Failed to create loading_base_dvfs_step entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj,
		&kobj_attr_timer_base_dvfs_margin);
	if (unlikely(err != GED_OK)) {
		GED_LOGE(
			"Failed to create timer_base_dvfs_margin entry!\n");
		goto ERROR;
	}

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_dvfs_loading_mode);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create dvfs_loading_mode entry!\n");
		goto ERROR;
	}

	ged_fb_notifier.notifier_call = ged_fb_notifier_callback;
	if (fb_register_client(&ged_fb_notifier))
		GED_LOGE("Register fb_notifier fail!\n");

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_fastdvfs_mode);
	if (unlikely(err != GED_OK))
		GED_LOGE("Failed to create fastdvfs_mode entry!\n");

#ifdef GED_DCS_POLICY
	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_dcs_mode);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create dcs_mode entry!\n");
		goto ERROR;
	}
#endif /* GED_DCS_POLICY */

	err = ged_sysfs_create_file(hal_kobj, &kobj_attr_fw_idle);
	if (unlikely(err != GED_OK)) {
		GED_LOGE("Failed to create fw_idle entry!\n");
		goto ERROR;
	}

	return err;

ERROR:

	ged_hal_exit();

	return err;
}
//-----------------------------------------------------------------------------
void ged_hal_exit(void)
{
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_fastdvfs_mode);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_dvfs_loading_mode);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_timer_base_dvfs_margin);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_loading_base_dvfs_step);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_dvfs_margin_value);

#ifdef MTK_GED_KPI
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_ged_kpi);
#endif
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_gpu_boost_level);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_gpu_utilization);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_previous_freqency);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_current_freqency);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_custom_upbound_gpu_freq);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_custom_boost_gpu_freq);
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_total_gpu_freq_level_count);
#ifdef GED_DCS_POLICY
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_dcs_mode);
#endif
	ged_sysfs_remove_file(hal_kobj, &kobj_attr_fw_idle);

	ged_sysfs_remove_dir(&hal_kobj);
}
//-----------------------------------------------------------------------------
