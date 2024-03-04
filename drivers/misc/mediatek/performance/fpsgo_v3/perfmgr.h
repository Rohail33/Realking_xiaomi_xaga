/*
 * Copyright (C) 2021 XiaoMi Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#ifndef PERFMGR_H
#define PERFMGR_H
#include <linux/jiffies.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/utsname.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>
#include <linux/uaccess.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/notifier.h>
#include <linux/suspend.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/hrtimer.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/ioctl.h>

#define FRAME_UNIT 5
enum BUFFER_PUSH_TYPE {
	PERFMGR_NOTIFIER_QUEUE_DEQUEUE		= 0x01,
	PERFMGR_NOTIFIER_CONNECT		= 0x02,
	PERFMGR_NOTIFIER_VSYNC			= 0x03,
};

struct PERFMGR_NOTIFIER_PUSH_TAG {
	enum BUFFER_PUSH_TYPE ePushType;
	__u32 pid;
	__u32 connectedAPI;
	__u64 identifier;
	struct work_struct sWork;
};
struct connected_buffer {
	int pid;
	ktime_t last_time;
	ktime_t last_jank_time;
	s64 last_frame_unit[FRAME_UNIT];
	int target_fps;
	int rescue_keep_count;
	int rescue_keep_total_count;
	int frame_count;
	s64 count_time;
	__u64 identifier;
	struct list_head list;
};

static struct list_head connected_buffer;
extern int perfmgr_policy_init(void);
extern void perfmgr_do_policy(struct connected_buffer *cur);

#endif