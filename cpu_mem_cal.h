



#ifndef __CPU_MEM_CAL_H__
#define __CPU_MEM_CAL_H__




#include <stdio.h>
#include <string.h>
#include <utmp.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <errno.h>
  

/*
 * 本文件用于计算cpu的使用率和内存的使用率，2023-0419
 * */

//****************************************************************************************************************
//cpu和内存使用率计算，这一块代码的计算是独立的，跟其他模块关系不大

typedef struct jiffy_counts_t {
	/* Linux 2.4.x has only first four */
	unsigned long long usr, nic, sys, idle;
	unsigned long long iowait, irq, softirq, steal;
	unsigned long long total;
	unsigned long long busy;
} jiffy_counts_t;



struct globals {
	unsigned long total_kb, available_kb;
};


#define LINE_BUF_SIZE 256

int read_cpu_jiffy(jiffy_counts_t *p_jif);

int parse_meminfo(struct globals *g);

/*
	 通过参数
 */
int parse_cputemp(int *temp);


int parse_gputemp(int *temp);


int get_cpu_freq(int cpu_n,int *freq);

/*
 * 计算cpu和内存的使用率，正常是1秒定时调用一次。
 * 
 * 参数 data，传出参数，用于把数据记录到全局结构体变量中。
 * 
 * */
void cpu_mem_calc(void*data);

//****************************************************************************************************************
#endif



