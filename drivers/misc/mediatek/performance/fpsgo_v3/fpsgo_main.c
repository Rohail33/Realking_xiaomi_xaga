// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
//#define DEBUG
#include <linux/kthread.h>
#include <sched/sched.h>
#include <linux/unistd.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/cpufreq.h>
#include <linux/topology.h>
#include <linux/arch_topology.h>
#include <linux/cpumask.h>
#include <trace/events/power.h>
#include <linux/tracepoint.h>
#include <linux/kallsyms.h>

#include "mt-plat/fpsgo_common.h"
#include "fpsgo_base.h"
#include "fpsgo_sysfs.h"
#include "fpsgo_usedext.h"
#include "fpsgo_cpu_policy.h"
#include "fbt_cpu.h"
#include "fstb.h"
#include "fps_composer.h"
#include "xgf.h"
#include "mtk_drm_arr.h"
#include "uboost.h"
#include "gbe_common.h"

//add for feas perfmgr +{
#if IS_ENABLED(CONFIG_FEAS)
#include <perfmgr.h>

#define ENABLE_DELAYED_USECS  15000000 //15s
void perfmgr_notify_qudeq(int pid,unsigned long long identifier);
void perfmgr_notify_connect(int pid,unsigned long long identifier,int connectedAPI);
static struct mutex notify_lock;
static struct mutex list_lock;

struct list_head connected_buffer_list;
struct workqueue_struct *qbuffer_notifyworkqueue;
extern int perfmgr_enable;

extern void (*perfmgr_notify_qudeq_fp)(int pid,unsigned long long identifier);
extern void (*perfmgr_notify_connect_fp)(int pid,unsigned long long identifier,int connectedAPI);
void perfmgr_notify_connect(int pid,unsigned long long identifier,int connectedAPI);

#define MAX_CONNECTED_BUFFER 25
void *perfmgr_alloc_atomic(int i32Size)
{
	void *pvBuf;

	if (i32Size <= PAGE_SIZE)
		pvBuf = kmalloc(i32Size, GFP_ATOMIC);
	else
		pvBuf = vmalloc(i32Size);
	return pvBuf;
}

void perfmgr_free(void *pvBuf, int i32Size)
{
	if (!pvBuf)
		return;
	if (i32Size <= PAGE_SIZE)
		kfree(pvBuf);
	else
		vfree(pvBuf);
}

int perfmgr_is_enable(void)
{
	ktime_t current_time;
	s64 delta_usecs64;
	int enable=0;
	static ktime_t last_time;
	static int qudeq_count=0;
	static int need_delayed_enable=0;
	if(perfmgr_enable==0){
		need_delayed_enable=1;
		qudeq_count=0;
		enable=0;
	}
	else if((perfmgr_enable==1)&&(need_delayed_enable==1)){
		current_time = ktime_get();
		qudeq_count++;
		if(qudeq_count==1)
			last_time=current_time;
		delta_usecs64 = ktime_to_us(ktime_sub(current_time, last_time));
		if((perfmgr_enable==1)&&(delta_usecs64>=ENABLE_DELAYED_USECS)){
			need_delayed_enable=0;
			qudeq_count=0;
			enable = 1;
		}
	}else{
		mutex_lock(&notify_lock);
		enable = perfmgr_enable;
		mutex_unlock(&notify_lock);
	}
	pr_debug("[perfmgr_CTRL] isenable %d\n", enable);
	return enable;
}

int perfmgr_notify_connect_cb(int pid,unsigned long long identifier,int connectedAPI)
{

	struct connected_buffer *node= NULL;
	int caculate_buffer=0;
	int perfmgr_status=perfmgr_enable;
	pr_debug("%s pid:%d identifier %llu connectedAPI=%d \n",
                 __func__,pid,identifier,connectedAPI);

	if(connectedAPI){
		list_for_each_entry(node, &connected_buffer_list, list){
			if(node->identifier==identifier){
				return 0;
			}
		}
		node = kzalloc(sizeof(*node), GFP_KERNEL);
		if (!node) {
			return -ENOMEM;
		}
		memset(node, 0, sizeof(struct connected_buffer));
		node->pid=pid;
		node->identifier=identifier;
		list_add_tail(&node->list, &connected_buffer_list);
	}else{
		mutex_lock(&list_lock);
		list_for_each_entry(node, &connected_buffer_list, list){
			if(node->identifier==identifier){
				list_del(&node->list);
				kfree(node);
				break;
			}
		}
		mutex_unlock(&list_lock);
		list_for_each_entry(node, &connected_buffer_list, list){
			caculate_buffer++;
			pr_debug("%s connected_buffer_list: pid:%d identifier：%llu caculate_buffer=%d \n",
                                 __func__,node->pid,node->identifier,caculate_buffer);
		}
		if(caculate_buffer>=MAX_CONNECTED_BUFFER){
			struct connected_buffer *tmp;
			if(perfmgr_status)
				perfmgr_enable=0;
			mutex_lock(&list_lock);
			list_for_each_entry_safe(node, tmp, &connected_buffer_list, list) {
				list_del(&node->list);
				kfree(node);
			}
			mutex_unlock(&list_lock);
			if(perfmgr_status)
				perfmgr_enable=1;
			pr_info("free connected buffer list\n");
		}
	}

	return 0;
}

int perfmgr_notify_qudeq_cb(int pid,unsigned long long identifier)
{
	struct connected_buffer *priv=NULL;
	static int find_buffer=0;
	pr_debug("%s pid %d id %llu\n",__func__,pid,identifier);
	mutex_lock(&list_lock);
	list_for_each_entry(priv, &connected_buffer_list, list){
		if(priv->identifier==identifier){
			find_buffer=1;
			perfmgr_do_policy(priv);
			break;
		}
	}
	mutex_unlock(&list_lock);
	if(find_buffer==0)
		perfmgr_notify_connect(pid,identifier,1);
	return 0;
}

static void perfmgr_notifer_wq_cb(struct work_struct *psWork)
{
	struct PERFMGR_NOTIFIER_PUSH_TAG *vpPush =
		 container_of(psWork,
				struct PERFMGR_NOTIFIER_PUSH_TAG, sWork);

	if (!vpPush) {
		pr_debug("[perfmgr_CTRL] ERROR\n");
		return;
	}
	pr_debug("[perfmgr_CTRL] perfmgr_notifer_wq_cb push type = %d\n",
			vpPush->ePushType);

	switch (vpPush->ePushType) {

	case PERFMGR_NOTIFIER_QUEUE_DEQUEUE:
		perfmgr_notify_qudeq_cb(vpPush->pid,vpPush->identifier);
		break;
	case PERFMGR_NOTIFIER_CONNECT:
		perfmgr_notify_connect_cb(vpPush->pid,vpPush->identifier,
				vpPush->connectedAPI);
		break;

	default:
		pr_debug("[perfmgr_CTRL] unhandled push type  = %d\n",
				vpPush->ePushType);
		break;
	}
	perfmgr_free(vpPush, sizeof(struct PERFMGR_NOTIFIER_PUSH_TAG));
}

void perfmgr_notify_connect(int pid,unsigned long long identifier,int connectedAPI)
{
	struct PERFMGR_NOTIFIER_PUSH_TAG *vpPush=NULL;
	pr_debug("%s pid:%d identifier:%llu connectedAPI %d\n",
                 __func__,pid,identifier,connectedAPI);
	if (!perfmgr_is_enable())
		return;
	vpPush =
		(struct PERFMGR_NOTIFIER_PUSH_TAG *)
		perfmgr_alloc_atomic(sizeof(struct PERFMGR_NOTIFIER_PUSH_TAG));
	if (!vpPush) {
		pr_debug("[perfmgr_CTRL] OOM\n");
		return;
	}
	if (!qbuffer_notifyworkqueue) {
		pr_debug("[perfmgr_CTRL] NULL WorkQueue\n");
		perfmgr_free(vpPush, sizeof(struct PERFMGR_NOTIFIER_PUSH_TAG));
		return;
	}
	vpPush->ePushType = PERFMGR_NOTIFIER_CONNECT;
	vpPush->pid = pid;
	vpPush->identifier = identifier;
	vpPush->connectedAPI = connectedAPI;

	INIT_WORK(&vpPush->sWork, perfmgr_notifer_wq_cb);
	queue_work(qbuffer_notifyworkqueue, &vpPush->sWork);
}

void perfmgr_notify_qudeq(int pid,unsigned long long identifier)
{
	struct PERFMGR_NOTIFIER_PUSH_TAG *vpPush=NULL;
	pr_debug("%s pid %d id %llu  \n",__func__,pid,identifier);
	if (!perfmgr_is_enable())
		return;
	vpPush =
	(struct PERFMGR_NOTIFIER_PUSH_TAG *)
		perfmgr_alloc_atomic(sizeof(struct connected_buffer));
	if (!vpPush) {
		pr_debug("[perfmgr_CTRL] OOM\n");
		return;
	}

	if (!qbuffer_notifyworkqueue) {
		pr_debug("[perfmgr_CTRL] NULL WorkQueue\n");
		perfmgr_free(vpPush, sizeof(struct PERFMGR_NOTIFIER_PUSH_TAG));
		return;
	}

	vpPush->ePushType = PERFMGR_NOTIFIER_QUEUE_DEQUEUE;
	vpPush->pid = pid;
	vpPush->identifier = identifier;
	INIT_WORK(&vpPush->sWork, perfmgr_notifer_wq_cb);
	queue_work(qbuffer_notifyworkqueue, &vpPush->sWork);
}
#endif
//add for feas perfmgr +}

#define CREATE_TRACE_POINTS

#define TARGET_UNLIMITED_FPS 240

enum FPSGO_NOTIFIER_PUSH_TYPE {
	FPSGO_NOTIFIER_SWITCH_FPSGO			= 0x00,
	FPSGO_NOTIFIER_QUEUE_DEQUEUE		= 0x01,
	FPSGO_NOTIFIER_CONNECT				= 0x02,
	FPSGO_NOTIFIER_DFRC_FPS				= 0x03,
	FPSGO_NOTIFIER_BQID				= 0x04,
	FPSGO_NOTIFIER_VSYNC				= 0x05,
	FPSGO_NOTIFIER_SWAP_BUFFER          = 0x06,
};

/* TODO: use union*/
struct FPSGO_NOTIFIER_PUSH_TAG {
	enum FPSGO_NOTIFIER_PUSH_TYPE ePushType;

	int pid;
	unsigned long long cur_ts;

	int enable;

	int qudeq_cmd;
	unsigned int queue_arg;

	unsigned long long bufID;
	int connectedAPI;
	int queue_SF;
	unsigned long long identifier;
	int create;

	int dfrc_fps;

	struct list_head queue_list;
};

static struct mutex notify_lock;
static struct task_struct *kfpsgo_tsk;
static int fpsgo_enable;
static int fpsgo_force_onoff;
static int gpu_boost_enable_perf;
static int gpu_boost_enable_camera;
static int perfserv_ta;

int powerhal_tid;

void (*rsu_cpufreq_notifier_fp)(int cluster_id, unsigned long freq);

/* TODO: event register & dispatch */
int fpsgo_is_enable(void)
{
	int enable;

	mutex_lock(&notify_lock);
	enable = fpsgo_enable;
	mutex_unlock(&notify_lock);

	FPSGO_LOGI("[FPSGO_CTRL] isenable %d\n", enable);
	return enable;
}

static void fpsgo_notifier_wq_cb_vsync(unsigned long long ts)
{
	FPSGO_LOGI("[FPSGO_CB] vsync: %llu\n", ts);

	if (!fpsgo_is_enable())
		return;

	fpsgo_ctrl2fbt_vsync(ts);
	fpsgo_uboost_traverse(ts);
}

static void fpsgo_notifier_wq_cb_swap_buffer(int pid)
{
	FPSGO_LOGI("[FPSGO_CB] swap_buffer: %d\n", pid);

	if (!fpsgo_is_enable())
		return;

	fpsgo_update_swap_buffer(pid);
}

static void fpsgo_notifier_wq_cb_dfrc_fps(int dfrc_fps)
{
	FPSGO_LOGI("[FPSGO_CB] dfrc_fps %d\n", dfrc_fps);

	fpsgo_ctrl2fstb_dfrc_fps(dfrc_fps);
	fpsgo_ctrl2xgf_set_display_rate(dfrc_fps);
	fpsgo_ctrl2fbt_dfrc_fps(dfrc_fps);
}

static void fpsgo_notifier_wq_cb_connect(int pid,
		int connectedAPI, unsigned long long id)
{
	FPSGO_LOGI(
		"[FPSGO_CB] connect: pid %d, API %d, id %llu\n",
		pid, connectedAPI, id);

	if (connectedAPI == WINDOW_DISCONNECT)
		fpsgo_ctrl2comp_disconnect_api(pid, connectedAPI, id);
	else
		fpsgo_ctrl2comp_connect_api(pid, connectedAPI, id);
}

static void fpsgo_notifier_wq_cb_bqid(int pid, unsigned long long bufID,
	int queue_SF, unsigned long long id, int create)
{
	FPSGO_LOGI(
		"[FPSGO_CB] bqid: pid %d, bufID %llu, queue_SF %d, id %llu, create %d\n",
		pid, bufID, queue_SF, id, create);

	fpsgo_ctrl2comp_bqid(pid, bufID, queue_SF, id, create);
}

static void fpsgo_notifier_wq_cb_qudeq(int qudeq,
		unsigned int startend, int cur_pid,
		unsigned long long curr_ts, unsigned long long id)
{
	FPSGO_LOGI("[FPSGO_CB] qudeq: %d-%d, pid %d, ts %llu, id %llu\n",
		qudeq, startend, cur_pid, curr_ts, id);

	if (!fpsgo_is_enable())
		return;

	switch (qudeq) {
	case 1:
		if (startend) {
			FPSGO_LOGI("[FPSGO_CB] QUEUE Start: pid %d\n",
					cur_pid);
			fpsgo_ctrl2comp_enqueue_start(cur_pid,
					curr_ts, id);
		} else {
			FPSGO_LOGI("[FPSGO_CB] QUEUE End: pid %d\n",
					cur_pid);
			fpsgo_ctrl2comp_enqueue_end(cur_pid, curr_ts,
					id);
		}
		break;
	case 0:
		if (startend) {
			FPSGO_LOGI("[FPSGO_CB] DEQUEUE Start: pid %d\n",
					cur_pid);
			fpsgo_ctrl2comp_dequeue_start(cur_pid,
					curr_ts, id);
		} else {
			FPSGO_LOGI("[FPSGO_CB] DEQUEUE End: pid %d\n",
					cur_pid);
			fpsgo_ctrl2comp_dequeue_end(cur_pid,
					curr_ts, id);
		}
		break;
	default:
		break;
	}
}

static void fpsgo_notifier_wq_cb_enable(int enable)
{
	FPSGO_LOGI(
	"[FPSGO_CB] enable %d, fpsgo_enable %d, force_onoff %d\n",
	enable, fpsgo_enable, fpsgo_force_onoff);

	mutex_lock(&notify_lock);
	if (enable == fpsgo_enable) {
		mutex_unlock(&notify_lock);
		return;
	}

	if (fpsgo_force_onoff != FPSGO_FREE &&
			enable != fpsgo_force_onoff) {
		mutex_unlock(&notify_lock);
		return;
	}

	fpsgo_ctrl2fbt_switch_fbt(enable);
	fpsgo_ctrl2fstb_switch_fstb(enable);
	fpsgo_ctrl2xgf_switch_xgf(enable);

	fpsgo_enable = enable;

	if (!fpsgo_enable)
		fpsgo_clear();

	FPSGO_LOGI("[FPSGO_CB] fpsgo_enable %d\n",
			fpsgo_enable);
	mutex_unlock(&notify_lock);
}

static LIST_HEAD(head);
static int condition_notifier_wq;
static DEFINE_MUTEX(notifier_wq_lock);
static DECLARE_WAIT_QUEUE_HEAD(notifier_wq_queue);
static void fpsgo_queue_work(struct FPSGO_NOTIFIER_PUSH_TAG *vpPush)
{
	mutex_lock(&notifier_wq_lock);
	list_add_tail(&vpPush->queue_list, &head);
	condition_notifier_wq = 1;
	mutex_unlock(&notifier_wq_lock);

	wake_up_interruptible(&notifier_wq_queue);
}

static void fpsgo_notifier_wq_cb(void)
{
	struct FPSGO_NOTIFIER_PUSH_TAG *vpPush;

	wait_event_interruptible(notifier_wq_queue, condition_notifier_wq);
	mutex_lock(&notifier_wq_lock);

	if (!list_empty(&head)) {
		vpPush = list_first_entry(&head,
			struct FPSGO_NOTIFIER_PUSH_TAG, queue_list);
		list_del(&vpPush->queue_list);
		if (list_empty(&head))
			condition_notifier_wq = 0;
		mutex_unlock(&notifier_wq_lock);
	} else {
		condition_notifier_wq = 0;
		mutex_unlock(&notifier_wq_lock);
		return;
	}

	switch (vpPush->ePushType) {
	case FPSGO_NOTIFIER_SWITCH_FPSGO:
		fpsgo_notifier_wq_cb_enable(vpPush->enable);
		break;
	case FPSGO_NOTIFIER_QUEUE_DEQUEUE:
		fpsgo_notifier_wq_cb_qudeq(vpPush->qudeq_cmd,
				vpPush->queue_arg, vpPush->pid,
				vpPush->cur_ts, vpPush->identifier);
		break;
	case FPSGO_NOTIFIER_CONNECT:
		fpsgo_notifier_wq_cb_connect(vpPush->pid,
				vpPush->connectedAPI, vpPush->identifier);
		break;
	case FPSGO_NOTIFIER_DFRC_FPS:
		fpsgo_notifier_wq_cb_dfrc_fps(vpPush->dfrc_fps);
		break;
	case FPSGO_NOTIFIER_BQID:
		fpsgo_notifier_wq_cb_bqid(vpPush->pid, vpPush->bufID,
			vpPush->queue_SF, vpPush->identifier, vpPush->create);
		break;
	case FPSGO_NOTIFIER_VSYNC:
		fpsgo_notifier_wq_cb_vsync(vpPush->cur_ts);
		break;
	case FPSGO_NOTIFIER_SWAP_BUFFER:
		fpsgo_notifier_wq_cb_swap_buffer(vpPush->pid);
		break;
	default:
		FPSGO_LOGE("[FPSGO_CTRL] unhandled push type = %d\n",
				vpPush->ePushType);
		break;
	}
	fpsgo_free(vpPush, sizeof(struct FPSGO_NOTIFIER_PUSH_TAG));

}

static int kfpsgo(void *arg)
{
	struct sched_attr attr = {};

	attr.sched_policy = -1;
	attr.sched_flags =
		SCHED_FLAG_KEEP_ALL |
		SCHED_FLAG_UTIL_CLAMP |
		SCHED_FLAG_RESET_ON_FORK;
	attr.sched_util_min = 1;
	attr.sched_util_max = 1024;
	if (sched_setattr_nocheck(current, &attr) != 0)
		FPSGO_LOGE("[FPSGO_CTRL] %s set uclamp fail\n", __func__);

	set_user_nice(current, -20);

	while (!kthread_should_stop())
		fpsgo_notifier_wq_cb();

	return 0;
}
void fpsgo_notify_qudeq(int qudeq,
		unsigned int startend,
		int pid, unsigned long long id)
{
	unsigned long long cur_ts;
	struct FPSGO_NOTIFIER_PUSH_TAG *vpPush;

	FPSGO_LOGI("[FPSGO_CTRL] qudeq %d-%d, id %llu pid %d\n",
		qudeq, startend, id, pid);
#if IS_ENABLED(CONFIG_FEAS)
	if((qudeq==1)&&(startend==0)){
		FPSGO_LOGI("[FPSGO_CTRL] perfmgr qudeq:%d startend:%d id %llu\n",
                           qudeq, startend, id);
		perfmgr_notify_qudeq(pid,id);
	}
#endif
	if (!fpsgo_is_enable())
		return;

	vpPush =
		(struct FPSGO_NOTIFIER_PUSH_TAG *)
		fpsgo_alloc_atomic(sizeof(struct FPSGO_NOTIFIER_PUSH_TAG));
	if (!vpPush) {
		FPSGO_LOGE("[FPSGO_CTRL] OOM\n");
		return;
	}

	if (!kfpsgo_tsk) {
		FPSGO_LOGE("[FPSGO_CTRL] NULL WorkQueue\n");
		fpsgo_free(vpPush, sizeof(struct FPSGO_NOTIFIER_PUSH_TAG));
		return;
	}

	cur_ts = fpsgo_get_time();

	vpPush->ePushType = FPSGO_NOTIFIER_QUEUE_DEQUEUE;
	vpPush->pid = pid;
	vpPush->cur_ts = cur_ts;
	vpPush->qudeq_cmd = qudeq;
	vpPush->queue_arg = startend;
	vpPush->identifier = id;

	fpsgo_queue_work(vpPush);
}
void fpsgo_notify_connect(int pid,
		int connectedAPI, unsigned long long id)
{
	struct FPSGO_NOTIFIER_PUSH_TAG *vpPush;

	FPSGO_LOGI(
		"[FPSGO_CTRL] connect pid %d, id %llu, API %d\n",
		pid, id, connectedAPI);
#if IS_ENABLED(CONFIG_FEAS)
	pr_debug(
		"[FPSGO_CTRL] perfmgr connect pid %d, id %llu, API %d\n",
		pid, id, connectedAPI);
	perfmgr_notify_connect(pid,id,connectedAPI);
#endif
	vpPush =
		(struct FPSGO_NOTIFIER_PUSH_TAG *)
		fpsgo_alloc_atomic(sizeof(struct FPSGO_NOTIFIER_PUSH_TAG));

	if (!vpPush) {
		FPSGO_LOGE("[FPSGO_CTRL] OOM\n");
		return;
	}

	if (!kfpsgo_tsk) {
		FPSGO_LOGE("[FPSGO_CTRL] NULL WorkQueue\n");
		fpsgo_free(vpPush, sizeof(struct FPSGO_NOTIFIER_PUSH_TAG));
		return;
	}

	vpPush->ePushType = FPSGO_NOTIFIER_CONNECT;
	vpPush->pid = pid;
	vpPush->connectedAPI = connectedAPI;
	vpPush->identifier = id;

	fpsgo_queue_work(vpPush);
}

void fpsgo_notify_bqid(int pid, unsigned long long bufID,
	int queue_SF, unsigned long long id, int create)
{
	struct FPSGO_NOTIFIER_PUSH_TAG *vpPush;

	FPSGO_LOGI("[FPSGO_CTRL] bqid pid %d, buf %llu, queue_SF %d, id %llu\n",
		pid, bufID, queue_SF, id);

	vpPush = (struct FPSGO_NOTIFIER_PUSH_TAG *)
		fpsgo_alloc_atomic(sizeof(struct FPSGO_NOTIFIER_PUSH_TAG));

	if (!vpPush) {
		FPSGO_LOGE("[FPSGO_CTRL] OOM\n");
		return;
	}

	if (!kfpsgo_tsk) {
		FPSGO_LOGE("[FPSGO_CTRL] NULL WorkQueue\n");
		fpsgo_free(vpPush, sizeof(struct FPSGO_NOTIFIER_PUSH_TAG));
		return;
	}

	vpPush->ePushType = FPSGO_NOTIFIER_BQID;
	vpPush->pid = pid;
	vpPush->bufID = bufID;
	vpPush->queue_SF = queue_SF;
	vpPush->identifier = id;
	vpPush->create = create;

	fpsgo_queue_work(vpPush);
}

int fpsgo_perfserv_ta_value(void)
{
	int value;

	mutex_lock(&notify_lock);
	value = perfserv_ta;
	mutex_unlock(&notify_lock);

	return value;
}

void fpsgo_set_perfserv_ta(int value)
{
	mutex_lock(&notify_lock);
	perfserv_ta = value;
	mutex_unlock(&notify_lock);
	fpsgo_ctrl2fbt_switch_uclamp(!value);
}


void fpsgo_notify_vsync(void)
{
	struct FPSGO_NOTIFIER_PUSH_TAG *vpPush;

	FPSGO_LOGI("[FPSGO_CTRL] vsync\n");

	if (!fpsgo_is_enable())
		return;

	vpPush = (struct FPSGO_NOTIFIER_PUSH_TAG *)
		fpsgo_alloc_atomic(sizeof(struct FPSGO_NOTIFIER_PUSH_TAG));

	if (!vpPush) {
		FPSGO_LOGE("[FPSGO_CTRL] OOM\n");
		return;
	}

	if (!kfpsgo_tsk) {
		FPSGO_LOGE("[FPSGO_CTRL] NULL WorkQueue\n");
		fpsgo_free(vpPush, sizeof(struct FPSGO_NOTIFIER_PUSH_TAG));
		return;
	}

	vpPush->ePushType = FPSGO_NOTIFIER_VSYNC;
	vpPush->cur_ts = fpsgo_get_time();

	fpsgo_queue_work(vpPush);
}

void fpsgo_notify_swap_buffer(int pid)
{
	struct FPSGO_NOTIFIER_PUSH_TAG *vpPush;

	FPSGO_LOGI("[FPSGO_CTRL] swap_buffer\n");

	if (!fpsgo_is_enable())
		return;

	vpPush = (struct FPSGO_NOTIFIER_PUSH_TAG *)
		fpsgo_alloc_atomic(sizeof(struct FPSGO_NOTIFIER_PUSH_TAG));

	if (!vpPush) {
		FPSGO_LOGE("[FPSGO_CTRL] OOM\n");
		return;
	}

	if (!kfpsgo_tsk) {
		FPSGO_LOGE("[FPSGO_CTRL] NULL WorkQueue\n");
		fpsgo_free(vpPush, sizeof(struct FPSGO_NOTIFIER_PUSH_TAG));
		return;
	}

	vpPush->ePushType = FPSGO_NOTIFIER_SWAP_BUFFER;
	vpPush->pid = pid;

	fpsgo_queue_work(vpPush);
}

void fpsgo_get_fps(int *pid, int *fps)
{
	//int pid = -1, fps = -1;
	if (unlikely(powerhal_tid == 0))
		powerhal_tid = current->pid;

	fpsgo_ctrl2fstb_get_fps(pid, fps);

	FPSGO_LOGI("[FPSGO_CTRL] get_fps %d %d\n", *pid, *fps);

	//return fps;
}

void fpsgo_get_cmd(int *cmd, int *value1, int *value2)
{
	int _cmd = -1, _value1 = -1, _value2 = -1;

	fpsgo_ctrl2base_get_pwr_cmd(&_cmd, &_value1, &_value2);


	FPSGO_LOGI("[FPSGO_CTRL] get_cmd %d %d %d\n", _cmd, _value1, _value2);
	*cmd = _cmd;
	*value1 = _value1;
	*value2 = _value2;

}

int fpsgo_get_fstb_active(long long time_diff)
{
	return is_fstb_active(time_diff);
}

int fpsgo_wait_fstb_active(void)
{
	return fpsgo_ctrl2fstb_wait_fstb_active();
}

void fpsgo_notify_cpufreq(int cid, unsigned long freq)
{
	FPSGO_LOGI("[FPSGO_CTRL] cid %d, cpufreq %lu\n", cid, freq);

	if (rsu_cpufreq_notifier_fp)
		rsu_cpufreq_notifier_fp(cid, freq);

	if (!fpsgo_enable)
		return;

	fpsgo_ctrl2fbt_cpufreq_cb(cid, freq);
}

void dfrc_fps_limit_cb(unsigned int fps_limit)
{
	unsigned int vTmp = TARGET_UNLIMITED_FPS;
	struct FPSGO_NOTIFIER_PUSH_TAG *vpPush;

	if (fps_limit > 0 && fps_limit <= TARGET_UNLIMITED_FPS)
		vTmp = fps_limit;

	FPSGO_LOGI("[FPSGO_CTRL] dfrc_fps %d\n", vTmp);

	vpPush =
		(struct FPSGO_NOTIFIER_PUSH_TAG *)
		fpsgo_alloc_atomic(sizeof(struct FPSGO_NOTIFIER_PUSH_TAG));

	if (!vpPush) {
		FPSGO_LOGE("[FPSGO_CTRL] OOM\n");
		return;
	}

	if (!kfpsgo_tsk) {
		FPSGO_LOGE("[FPSGO_CTRL] NULL WorkQueue\n");
		fpsgo_free(vpPush, sizeof(struct FPSGO_NOTIFIER_PUSH_TAG));
		return;
	}

	vpPush->ePushType = FPSGO_NOTIFIER_DFRC_FPS;
	vpPush->dfrc_fps = vTmp;

	fpsgo_queue_work(vpPush);
}

/* FPSGO control */
void fpsgo_switch_enable(int enable)
{
	struct FPSGO_NOTIFIER_PUSH_TAG *vpPush = NULL;

	if (!kfpsgo_tsk) {
		FPSGO_LOGE("[FPSGO_CTRL] NULL WorkQueue\n");
		return;
	}

	FPSGO_LOGI("[FPSGO_CTRL] switch enable %d\n", enable);

	if (fpsgo_is_force_enable() !=
			FPSGO_FREE && enable !=
			fpsgo_is_force_enable())
		return;

	vpPush =
		(struct FPSGO_NOTIFIER_PUSH_TAG *)
		fpsgo_alloc_atomic(sizeof(struct FPSGO_NOTIFIER_PUSH_TAG));

	if (!vpPush) {
		FPSGO_LOGE("[FPSGO_CTRL] OOM\n");
		return;
	}

	vpPush->ePushType = FPSGO_NOTIFIER_SWITCH_FPSGO;
	vpPush->enable = enable;

	fpsgo_queue_work(vpPush);
}

int fpsgo_is_force_enable(void)
{
	int temp_onoff;
	mutex_lock(&notify_lock);
	temp_onoff = fpsgo_force_onoff;
	mutex_unlock(&notify_lock);

	return temp_onoff;
}

void fpsgo_force_switch_enable(int enable)
{
	mutex_lock(&notify_lock);
	fpsgo_force_onoff = enable;
	mutex_unlock(&notify_lock);

	fpsgo_switch_enable(enable?1:0);
}

/* FSTB control */
int fpsgo_is_fstb_enable(void)
{
	return is_fstb_enable();
}

int fpsgo_switch_fstb(int enable)
{
	return fpsgo_ctrl2fstb_switch_fstb(enable);
}

int fpsgo_fstb_sample_window(long long time_usec)
{
	return switch_sample_window(time_usec);
}

int fpsgo_fstb_fps_range(int nr_level,
		struct fps_level *level)
{
	return switch_fps_range(nr_level, level);
}

int fpsgo_fstb_process_fps_range(char *proc_name,
	int nr_level, struct fps_level *level)
{
	return switch_process_fps_range(proc_name, nr_level, level);
}

int fpsgo_fstb_thread_fps_range(pid_t pid,
	int nr_level, struct fps_level *level)
{
	return switch_thread_fps_range(pid, nr_level, level);
}

int fpsgo_fstb_fps_error_threhosld(int threshold)
{
	return switch_fps_error_threhosld(threshold);
}

int fpsgo_fstb_percentile_frametime(int ratio)
{
	return switch_percentile_frametime(ratio);
}

struct tracepoints_table {
	const char *name;
	void *func;
	struct tracepoint *tp;
	bool registered;
};

static void fpsgo_cpu_frequency_tracer(void *ignore, unsigned int frequency, unsigned int cpu_id)
{
	int cpu = 0, cluster = 0;
	struct cpufreq_policy *policy = NULL;

	policy = cpufreq_cpu_get(cpu_id);
	if (!policy)
		return;
	if (cpu_id != cpumask_first(policy->related_cpus)) {
		cpufreq_cpu_put(policy);
		return;
	}
	cpufreq_cpu_put(policy);

	for_each_possible_cpu(cpu) {
		policy = cpufreq_cpu_get(cpu);
		if (!policy)
			break;
		cpu = cpumask_first(policy->related_cpus);
		if (cpu == cpu_id)
			break;
		cpu = cpumask_last(policy->related_cpus);
		cluster++;
		cpufreq_cpu_put(policy);
	}

	if (policy) {
		fpsgo_notify_cpufreq(cluster, frequency);
		cpufreq_cpu_put(policy);
	}
}

struct tracepoints_table fpsgo_tracepoints[] = {
	{.name = "cpu_frequency", .func = fpsgo_cpu_frequency_tracer},
};

#define FOR_EACH_INTEREST(i) \
	for (i = 0; i < sizeof(fpsgo_tracepoints) / sizeof(struct tracepoints_table); i++)

static void lookup_tracepoints(struct tracepoint *tp, void *ignore)
{
	int i;

	FOR_EACH_INTEREST(i) {
		if (strcmp(fpsgo_tracepoints[i].name, tp->name) == 0)
			fpsgo_tracepoints[i].tp = tp;
	}
}

void tracepoint_cleanup(void)
{
	int i;

	FOR_EACH_INTEREST(i) {
		if (fpsgo_tracepoints[i].registered) {
			tracepoint_probe_unregister(
				fpsgo_tracepoints[i].tp,
				fpsgo_tracepoints[i].func, NULL);
			fpsgo_tracepoints[i].registered = false;
		}
	}
}


static void __exit fpsgo_exit(void)
{
	fpsgo_notifier_wq_cb_enable(0);

	if (kfpsgo_tsk)
		kthread_stop(kfpsgo_tsk);

#if IS_ENABLED(CONFIG_DRM_MEDIATEK)
	drm_unregister_fps_chg_callback(dfrc_fps_limit_cb);
#endif
	fpsgo_uboost_exit();
	fbt_cpu_exit();
	mtk_fstb_exit();
	fpsgo_composer_exit();
	fpsgo_sysfs_exit();

	/* game boost engine */
	exit_gbe_common();
}

static int __init fpsgo_init(void)
{
	int i;
	int ret;

	FPSGO_LOGI("[FPSGO_CTRL] init\n");

	fpsgo_cpu_policy_init();

	fpsgo_sysfs_init();


	kfpsgo_tsk = kthread_create(kfpsgo, NULL, "kfps");
	if (kfpsgo_tsk == NULL)
		return -EFAULT;
	wake_up_process(kfpsgo_tsk);

	for_each_kernel_tracepoint(lookup_tracepoints, NULL);

	FOR_EACH_INTEREST(i) {
		if (fpsgo_tracepoints[i].tp == NULL) {
			FPSGO_LOGE("FPSGO Error, %s not found\n", fpsgo_tracepoints[i].name);
			tracepoint_cleanup();
			return -1;
		}
	}
	ret = tracepoint_probe_register(fpsgo_tracepoints[0].tp, fpsgo_tracepoints[0].func,  NULL);
	if (ret) {
		FPSGO_LOGE("cpu_frequency: Couldn't activate tracepoint\n");
		goto fail_reg_cpu_frequency_entry;
	}
	fpsgo_tracepoints[0].registered = true;

fail_reg_cpu_frequency_entry:


	mutex_init(&notify_lock);

	fpsgo_force_onoff = FPSGO_FREE;
	gpu_boost_enable_perf = gpu_boost_enable_camera = -1;

	init_fpsgo_common();
	fbt_cpu_init();
	mtk_fstb_init();
	fpsgo_composer_init();
	fpsgo_uboost_init();

	/* game boost engine*/
	init_gbe_common();

	if (fpsgo_arch_nr_clusters() > 0)
		fpsgo_switch_enable(1);

	fpsgo_notify_vsync_fp = fpsgo_notify_vsync;

	fpsgo_notify_qudeq_fp = fpsgo_notify_qudeq;
	fpsgo_notify_connect_fp = fpsgo_notify_connect;
	fpsgo_notify_bqid_fp = fpsgo_notify_bqid;

	fpsgo_notify_swap_buffer_fp = fpsgo_notify_swap_buffer;
	fpsgo_get_fps_fp = fpsgo_get_fps;
	fpsgo_get_cmd_fp = fpsgo_get_cmd;
	fpsgo_get_fstb_active_fp = fpsgo_get_fstb_active;
	fpsgo_wait_fstb_active_fp = fpsgo_wait_fstb_active;

#if IS_ENABLED(CONFIG_DRM_MEDIATEK)
	drm_register_fps_chg_callback(dfrc_fps_limit_cb);
#endif

#if IS_ENABLED(CONFIG_FEAS)
	qbuffer_notifyworkqueue =
	alloc_ordered_workqueue("%s", WQ_MEM_RECLAIM | WQ_HIGHPRI, "perfmgr_wq");
	if (qbuffer_notifyworkqueue == NULL)
		return -EFAULT;
	mutex_init(&notify_lock);
	mutex_init(&list_lock);
	INIT_LIST_HEAD(&connected_buffer_list);
	perfmgr_policy_init();
#endif
	return 0;
}

module_init(fpsgo_init);
module_exit(fpsgo_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek FPSGO");
MODULE_AUTHOR("MediaTek Inc.");
