/*
 * Copyright (c) 2021, Xiaomi. All rights reserved.
 */
//#define DEBUG
#define pr_fmt(fmt) "perfmgr_policy: " fmt

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/cpu.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/timer.h>
#include <linux/pm_qos.h>
#include "perfmgr.h"

#define FREQ_MAX_DEFAULT_VALUE	S32_MAX
#define FPS_60  60
#define FPS_90  90
#define FPS_120 120
#define FPS_UNKNOW -1
#define FRAME_COUNT_MAX 30
#define M 1000
#define N 100


struct kobject *perfmgr_policy_kobj;
static struct delayed_work maxfreq_release_work;
static struct workqueue_struct *cpufreq_wq;

static int set_freq_level;
static int last_freq_level;
static int jank_happened;
int perfmgr_enable=0;
module_param(perfmgr_enable, int, 0644);
MODULE_PARM_DESC(perfmgr_enable, "perfmgr_enable");

static int rescue_freq_level = 0;
module_param(rescue_freq_level, int, 0644);
MODULE_PARM_DESC(rescue_freq_level, "rescue_freq_level");

static int normal_frame_keep_count = 8;
module_param(normal_frame_keep_count, int, 0644);
MODULE_PARM_DESC(normal_frame_keep_count, "normal_frame_keep_count");

static int jank_frame_keep_count = 30;
module_param(jank_frame_keep_count, int, 0644);
MODULE_PARM_DESC(jank_frame_keep_count, "jank_frame_keep_count");

static int jank_frame_delta = 60;
module_param(jank_frame_delta, int, 0644);
MODULE_PARM_DESC(jank_frame_delta, "jank_frame_delta");

static int max_freq_limit_level = 41;
module_param(max_freq_limit_level, int, 0644);
MODULE_PARM_DESC(max_freq_limit_level, "frame_keep_policy");

static int min_freq_limit_level = 2;
module_param(min_freq_limit_level, int, 0644);
MODULE_PARM_DESC(min_freq_limit_level, "min_freq_limit_level");

static int start_limit = 1;
module_param(start_limit, int, 0644);
MODULE_PARM_DESC(start_limit, "start_limit");

static int fixed_target_fps = FPS_UNKNOW;
module_param(fixed_target_fps, int, 0644);
MODULE_PARM_DESC(fixed_target_fps, "fixed_target_fps");

static int max_freq_delta = 300000;
module_param(max_freq_delta, int, 0644);
MODULE_PARM_DESC(max_freq_delta, "max_freq_delta");

static int boost_minfreq = 0;
module_param(boost_minfreq, int, 0644);
MODULE_PARM_DESC(boost_minfreq, "boost_minfreq");

static int scaling_a = 310;
module_param(scaling_a, int, 0644);
MODULE_PARM_DESC(scaling_a, "scaling_a");

static int scaling_b = -50;
module_param(scaling_b, int, 0644);
MODULE_PARM_DESC(scaling_b, "scaling_b");

static int scaling_c = 3;
module_param(scaling_c, int, 0644);
MODULE_PARM_DESC(scaling_c, "scaling_c");

static int scaling_d = 580;
module_param(scaling_d, int, 0644);
MODULE_PARM_DESC(scaling_d, "scaling_d");

static int load_reset = 1;
module_param(load_reset, int, 0644);
MODULE_PARM_DESC(load_reset, "load_reset");

static int load_scaling_x = 5;
module_param(load_scaling_x, int, 0644);
MODULE_PARM_DESC(load_scaling_x, "load_scaling_x");

static int load_scaling_y = 1;
module_param(load_scaling_y, int, 0644);
MODULE_PARM_DESC(load_scaling_y, "load_scaling_y");

//used_version_20
static int used_version_20 = 1;
module_param(used_version_20, int, 0644);
MODULE_PARM_DESC(used_version_20, "used_version_20");

static int cpu4_table_length;
static int cpu7_table_length;

int cpufreq_table4[42]={2850000,2850000,2750000,2750000,2650000,2650000,2550000,2550000,2450000,2450000,2400000,2400000,
						2350000,2350000,2300000,2300000,2250000,2250000,2200000,2200000,2100000,2100000,2050000,2050000,
						2000000,2000000,1900000,1900000,1800000,1800000,1700000,1700000,1600000,1600000,1500000,1500000,
						1400000,1400000,1300000,1200000,1100000,1000000};

int cpufreq_table7[42]={2800000,2700000,2700000,2600000,2600000,2550000,2550000,2500000,2500000,2450000,2450000,2400000,
						2400000,2300000,2300000,2250000,2250000,2200000,2200000,2150000,2150000,2100000,2100000,2050000,
						2050000,2000000,2000000,1900000,1900000,1800000,1800000,1700000,1700000,1600000,1600000,1500000,
						1500000,1400000,1400000,1300000,1300000,1300000};

//add for mtk cpufreq
static LIST_HEAD(cpufreq_dev_list);
static DEFINE_MUTEX(cpufreq_list_lock);

struct freq_table {
	u32 frequency;
};

struct cpufreq_device {
	int id;
	unsigned int cpufreq_state;
	unsigned int max_level;
	struct freq_table *freq_table;
	struct cpufreq_policy *policy;
	struct list_head node;
	struct freq_qos_request *qos_req;
};

static DEFINE_PER_CPU(struct freq_qos_request, qos_req);

static int cpufreq_set_level(struct cpufreq_device *cdev,
		unsigned long state)
{
	pr_debug("%s: state=%d \n",__func__, state);
	/* Request state should be less than max_level */
	if (WARN_ON(state > cdev->max_level))
		return -EINVAL;

	/* Check if the old cooling action is same as new cooling action */
	if (cdev->cpufreq_state == state)
		return 0;

	cdev->cpufreq_state = state;
	return freq_qos_update_request(cdev->qos_req,cdev->freq_table[state].frequency);
}

void cpu_limits_set_level(unsigned int cpu, unsigned int max_freq)
{
	struct cpufreq_device *cpufreq_dev;
	unsigned int level = 0;

	pr_debug("%s: cpufreq policy limit freq cpu%d  max_freq=%d \n",
					__func__, cpu,max_freq);
	list_for_each_entry(cpufreq_dev, &cpufreq_dev_list, node) {
		if (cpufreq_dev->id == cpu) {
			for (level = 0; level <= cpufreq_dev->max_level; level++) {
				int target_freq = cpufreq_dev->freq_table[level].frequency;
				if (max_freq >= target_freq) {
					cpufreq_set_level(cpufreq_dev, level);
					break;
				}
			}
			break;
		}
	}
}

static unsigned int find_next_max(struct cpufreq_frequency_table *table,
		unsigned int prev_max)
{
	struct cpufreq_frequency_table *pos;
	unsigned int max = 0;
	cpufreq_for_each_valid_entry(pos, table) {
		if (pos->frequency > max && pos->frequency < prev_max)
			max = pos->frequency;
	}
	return max;
}

static int cpu_policy_init(void)
{
	int cpu, ret;
	struct cpufreq_policy *policy;
	struct freq_qos_request *req;

	for_each_possible_cpu(cpu) {
		unsigned int i;
		unsigned int freq;
		struct cpufreq_device *cpufreq_dev;

		req = &per_cpu(qos_req, cpu);
		policy = cpufreq_cpu_get(cpu);
		if (!policy) {
			pr_err("%s: cpufreq policy not found for cpu%d\n",
					__func__, cpu);
			return -ESRCH;
		}
		printk(KERN_ERR "%s cpu=%d\n", __func__, cpu);

		i = cpufreq_table_count_valid_entries(policy);
		if (!i) {
			pr_err("%s: CPUFreq table not found or has no valid entries\n",
					__func__);
			return -ENODEV;
		}

		cpufreq_dev = kzalloc(sizeof(*cpufreq_dev), GFP_KERNEL);
		if (!cpufreq_dev)
			return -ENOMEM;

		cpufreq_dev->policy = policy;
		cpufreq_dev->qos_req = req;

		/* max_level is an index, not a counter */
		cpufreq_dev->max_level = i - 1;
		cpufreq_dev->id = policy->cpu;

		cpufreq_dev->freq_table = kmalloc_array(i,
				sizeof(*cpufreq_dev->freq_table),
				GFP_KERNEL);
		if (!cpufreq_dev->freq_table)
			return -ENOMEM;

		/* Fill freq-table in descending order of frequencies */
		for (i = 0, freq = -1; i <= cpufreq_dev->max_level; i++) {
			freq = find_next_max(policy->freq_table, freq);
			cpufreq_dev->freq_table[i].frequency = freq;

			/* Warn for duplicate entries */
			if (!freq)
				pr_warn("%s: table has duplicate entries\n", __func__);
			else
				pr_debug("%s: freq:%u KHz\n", __func__, freq);
		}

		ret = freq_qos_add_request(&policy->constraints,
				cpufreq_dev->qos_req, FREQ_QOS_MAX,
				cpufreq_dev->freq_table[0].frequency);
		if (ret < 0) {
			pr_err("%s: Failed to add freq constraint (%d)\n", __func__,
					ret);
			return ret;
		}
		mutex_lock(&cpufreq_list_lock);
		list_add(&cpufreq_dev->node, &cpufreq_dev_list);
		mutex_unlock(&cpufreq_list_lock);
	}
	return ret;
}
//end for mtk cpufreq

static void do_frame_limit_freq(void)
{
	unsigned int i;
	for_each_possible_cpu(i) {
		if(i==7){
			cpu_limits_set_level(i,cpufreq_table7[set_freq_level]);
		}else if(i>3){
			cpu_limits_set_level(i,cpufreq_table4[set_freq_level]);
		}
	}
}

static void do_maxfreq_release(void)
{
	unsigned int i;
	for_each_possible_cpu(i) {
		if(i>3){
			cpu_limits_set_level(i,FREQ_MAX_DEFAULT_VALUE);
		}
	}
}

static void do_maxfreq_release_work(struct work_struct *work)
{
	pr_debug("do_maxfreq_release_work\n");
	do_maxfreq_release();
}

static int left_shift(s64 *str, int len)
{
	int i;
	long sum=0;

	for(i = 1; i <= len; i++)
	{
		str[i-1] = str[i];
		sum+=str[i];
	}
	return sum;
}

static int calulate_fps(s64 frame_usecs64,struct connected_buffer *cur){
	int i;
	int target_fps=0;
	int count_time=0;
	int value_min_120=(FRAME_COUNT_MAX-5)*M*M;
	int value_max_120=(FRAME_COUNT_MAX+8)*M*M;
	int value_min_90=(FRAME_COUNT_MAX-2)*M*M;
	int value_max_90=(FRAME_COUNT_MAX+12)*M*M;
	int value_min_60=(FRAME_COUNT_MAX-2)*M*M;
	int value_max_60=(FRAME_COUNT_MAX+10)*M*M;

	if(cur==NULL)
		return 0;
	if(cur->target_fps==0){
		for(i=0;i<FRAME_UNIT;i++){
			cur->count_time+=cur->last_frame_unit[i];
		}
		count_time=cur->count_time*6;
		cur->frame_count=0;
		cur->count_time=0;
		goto caculate_fps;
	}else{
		cur->frame_count++;
		cur->count_time+= frame_usecs64;
		if(cur->frame_count==FRAME_COUNT_MAX){
			pr_debug(" count30 %s called  cur->count_time=%d\n",
                                 __func__,cur->count_time);
			count_time=cur->count_time;
			cur->frame_count=0;
			cur->count_time=0;
			goto caculate_fps;
		}else{
			return 0;
		}
	}
caculate_fps:
	if((count_time*FPS_120 < value_max_120) && (count_time*FPS_120 >= value_min_120)){
		target_fps = 120;
	}
	else if((count_time*FPS_90 < value_max_90) && (count_time*FPS_90 >= value_min_90)){
		target_fps = 90;
	}
	else if((count_time*FPS_60 <= value_max_60) && (count_time*FPS_60 >= value_min_60)){
		target_fps = 60;
	}
	else{
		pr_debug("unknow fps\n");
		target_fps=-1;
	}
	return target_fps;
}

static int frame_time(int target_fps){
	if(target_fps==120)
		return 8333;
	else if(target_fps==90)
		return 11111;
	else if(target_fps==60)
		return 16666;
	else
		return 11111;
}

void perfmgr_do_policy_V20(struct connected_buffer *cur)
{
	int target_fps=0;
	ktime_t current_time;
	s64 frame_usecs64;
	s64 frame_jank_usecs64;
	s64 frame_four_usecs64;
	s64 frame_unit_usecs64;
	static ktime_t last_limit_time;

	if(cur==NULL)
		return;
	cancel_delayed_work(&maxfreq_release_work);
	current_time = ktime_get();
	frame_usecs64 = ktime_to_us(ktime_sub(current_time, cur->last_time));
	cur->last_time = current_time;
	pr_debug(" %s cpufreq do_qbuffer_work -----frame_usecs64=%d- pid=%d identifier：%llu\n",
                 __func__,frame_usecs64,cur->pid,cur->identifier);
	queue_delayed_work(cpufreq_wq, &maxfreq_release_work,msecs_to_jiffies(80));

	if((cur->target_fps==0)&&(cur->frame_count < FRAME_UNIT)){
		cur->last_frame_unit[cur->frame_count]=frame_usecs64;
		cur->frame_count++;
	}else{
		frame_four_usecs64=left_shift(cur->last_frame_unit, FRAME_UNIT-1);
		cur->last_frame_unit[FRAME_UNIT-1]=frame_usecs64;
		frame_unit_usecs64=frame_four_usecs64+frame_usecs64;
		if(fixed_target_fps==-1){
			target_fps=calulate_fps(frame_usecs64,cur);
			if(target_fps==-1){
				set_freq_level=0;
				pr_debug("cpufreq unkown fps release maxfreq return,frame_usecs64=%d\n",
                                         frame_usecs64);
				return;
			}
			else if(target_fps==0){
				//do nothing
			}
			else
				cur->target_fps=target_fps;
		}else{
			cur->target_fps=fixed_target_fps;
		}
		pr_debug("target_fps=%d pid=%d identifier：%llu cpufreq  %d    %d\n",
                         cur->target_fps,cur->pid,cur->identifier,(frame_usecs64*(cur->target_fps)),(1*M*M+5*M*N));
		if((frame_usecs64*(cur->target_fps)) > (3*M*M+5*M*N)){
			set_freq_level=0;//release cpufreq
			pr_debug("cpufreq drop more than 3 frame,release freq return %d %d\n",
                                 frame_usecs64*(cur->target_fps),(2*M*M+5*M*N));
			return;
		} else if((frame_usecs64*(cur->target_fps)) > (1*M*M+7*M*N)){//drop
			jank_happened=1;
			cur->rescue_keep_count=0;
			frame_jank_usecs64  = ktime_to_us(ktime_sub(current_time, cur->last_jank_time));
			cur->last_jank_time = current_time;
			if(frame_jank_usecs64*(cur->target_fps) < jank_frame_delta*M*M){
				set_freq_level = last_freq_level-rescue_freq_level-1;
				if(load_reset){
					cur->last_frame_unit[FRAME_UNIT-1]=(1+load_scaling_y*(1>>load_scaling_x))*frame_time(cur->target_fps);
				}
				if(set_freq_level < min_freq_limit_level){
					set_freq_level=min_freq_limit_level;
				}
				do_frame_limit_freq();
				last_freq_level = set_freq_level;
				pr_debug("cpufreq 2up %d    %d\n",
                                         frame_usecs64*(cur->target_fps),(1*M*M+5*M*N));
			}
		}else {
			if((frame_unit_usecs64*(cur->target_fps)) < (FRAME_UNIT*M*M-scaling_b*M)){
				cur->rescue_keep_count++;
				if(jank_happened){
					cur->rescue_keep_total_count = normal_frame_keep_count+jank_frame_keep_count;
				}else{
					cur->rescue_keep_total_count = normal_frame_keep_count;
				}
				if((cur->rescue_keep_count >= cur->rescue_keep_total_count)&&((current_time-last_limit_time)*(cur->target_fps)) > (scaling_c*M*M)){
					jank_happened=0;
					cur->rescue_keep_count=0;
					set_freq_level = last_freq_level + 1;
					if(set_freq_level > max_freq_limit_level)
						set_freq_level = max_freq_limit_level;
					do_frame_limit_freq();
					last_freq_level = set_freq_level;
					last_limit_time = current_time;
					pr_debug("cpufreq down %d    %d\n",
                                                 frame_unit_usecs64*(cur->target_fps),(FRAME_UNIT*M*M-scaling_b*M));
				}
			}else if((frame_unit_usecs64*(cur->target_fps)) > (FRAME_UNIT*M*M+scaling_a*M)){//
				set_freq_level = last_freq_level-rescue_freq_level -1;
				if(load_reset){
					cur->last_frame_unit[FRAME_UNIT-1]=(1+load_scaling_y*(1>>load_scaling_x))*frame_time(cur->target_fps);
				}
				if(set_freq_level < min_freq_limit_level){
					set_freq_level=min_freq_limit_level;
				}
				do_frame_limit_freq();
				last_freq_level = set_freq_level;
				pr_debug("cpufreq 1up %d    %d\n",
                                         frame_unit_usecs64*(cur->target_fps),(FRAME_UNIT*M*M+scaling_a*M));
			}else{
				pr_debug("cpufreq keep\n");
			}
		}
	}
}

void perfmgr_do_policy_V10(struct connected_buffer *cur)
{
	int target_fps=0;
	ktime_t current_time;
	s64 frame_usecs64;

	if(cur==NULL)
		return;
	cancel_delayed_work(&maxfreq_release_work);
	current_time = ktime_get();
	frame_usecs64 = ktime_to_us(ktime_sub(current_time, cur->last_time));
	cur->last_time = current_time;
	queue_delayed_work(cpufreq_wq, &maxfreq_release_work,msecs_to_jiffies(80));
	pr_debug("%s cpufreq do_qbuffer_work -----frame_usecs64=%d- pid=%d-\n",
                 __func__,frame_usecs64,cur->pid);

	if(fixed_target_fps==-1){
		target_fps=calulate_fps(frame_usecs64,cur);
		if(target_fps==-1){
			pr_debug("cpufreq unkown fps\n");
			return;
		}
		else if(target_fps==0){
			pr_debug("cpufreq none target fps\n");
			//do nothing
		}
		else
		     cur->target_fps=target_fps;
	}else{
		cur->target_fps=fixed_target_fps;
	}

	if((frame_usecs64*(cur->target_fps)) > (1*M*M+scaling_d*M)){
		jank_happened=1;
          	cur->rescue_keep_count=0;
		set_freq_level = last_freq_level-rescue_freq_level -1;
		if(set_freq_level < 0)
			set_freq_level=0;
		do_frame_limit_freq();
		last_freq_level = set_freq_level;
		pr_debug("cpufreq %dup frametime=%d \n",
                         (rescue_freq_level+1),frame_usecs64);
	}
	else{
          	cur->rescue_keep_count++;
		if(jank_happened){
			cur->rescue_keep_total_count = normal_frame_keep_count+jank_frame_keep_count;
		}else{
			cur->rescue_keep_total_count = normal_frame_keep_count;
		}
		if(cur->rescue_keep_count >= cur->rescue_keep_total_count){
			jank_happened=0;
			cur->rescue_keep_count=0;
			set_freq_level = last_freq_level + 1;
			if(set_freq_level > max_freq_limit_level){
				set_freq_level = max_freq_limit_level;
			}
			do_frame_limit_freq();
			last_freq_level = set_freq_level;
			pr_debug("cpufreq down frametime=%d \n",
                                 frame_usecs64);
		}
	}
}

void perfmgr_do_policy(struct connected_buffer *cur)
{
    if(used_version_20)
	perfmgr_do_policy_V20(cur);
    else
        perfmgr_do_policy_V10(cur);
}


int perfmgr_policy_init(void)
{

	cpu4_table_length= sizeof(cpufreq_table4)/sizeof(cpufreq_table4[0]);
	cpu7_table_length= sizeof(cpufreq_table7)/sizeof(cpufreq_table7[0]);
	if(cpu4_table_length < cpu7_table_length){
		if(max_freq_limit_level>cpu4_table_length-1)
			max_freq_limit_level=cpu4_table_length-1;
	}else{
		if(max_freq_limit_level>cpu7_table_length-1)
			max_freq_limit_level=cpu7_table_length-1;
	}
	pr_debug("%s cpu4_table_length=%d cpu7_table_length=%d max_freq_limit_level=%d end\n",
                __func__,cpu4_table_length,cpu7_table_length,max_freq_limit_level);

	cpufreq_wq = alloc_workqueue("cpufreq_wq", WQ_HIGHPRI, 0);
	if (!cpufreq_wq)
		return -EFAULT;
	INIT_DELAYED_WORK(&maxfreq_release_work, do_maxfreq_release_work);

	cpu_policy_init();
	perfmgr_policy_kobj = kobject_create_and_add("perfmgr",
						&cpu_subsys.dev_root->kobj);
	return 0;
}

