//
// Created by didi on 2022/10/25.
//

#ifndef ECLIPSE_PAHO_C_RUNNINGPERFOMANCE_LINUX_H
#define ECLIPSE_PAHO_C_RUNNINGPERFOMANCE_LINUX_H

#include <stdint.h>
#include <time.h>
#include "unistd.h"

#define DISK_MAX_NUM 10 //支持disk中最大个数
#define NET_MAX_NUM 20 //支持网卡最大个数
typedef struct cpu_proc_info_st
{
    char name[20];
    uint32_t user; //从系统启动开始累积到当前时刻,用户态的CPU时间,不包含nice值为负进程
    uint32_t nice; //nice值为负的进程所占用的CPU时间
    uint32_t system; //从系统启动开始累计到当前时刻,系统时间
    uint32_t idle; //从系统启动开始累积到当前时刻，除硬盘IO等待时间意外其他等待时间
}cpu_proc_info_t;

typedef struct cpus_proc_info_st
{
    int ncpu;
    time_t sec;
    cpu_proc_info_t cpus[0];
}cpus_proc_info_t;

typedef struct cpus_out_info_st
{
    int ncpu;
    time_t val;
    float load[0];
}cpus_out_info_t;

typedef struct mem_out_info_st
{
    float total;
    float used;
    float free;
}mem_out_info_t;

/**
 * @brief disk描述信息
 * 4.8内核新增：
 *      15 - discards completed successfully
 *      16 - discards merged
 *      17 - sectors discarded
 *      18 - time spent discarding
 * 5.5内核新增
 *      19 - flush requests completed successfully
 *      20 - time spent flushing
 *
 */
typedef struct disk_proc_info_st
{
    uint32_t major;
    uint32_t minor;
    char name[32];
    uint32_t rd_ios; //完成读总次数
    uint32_t rd_merges;	//读操作合并后次数
    uint64_t rd_sectors; //读扇区大小
    uint32_t rd_ticks;	//读花费时间
    uint32_t wr_ios;	/* Write I/O operations */
    uint32_t wr_merges;	/* Writes merged */
    uint64_t wr_sectors; /* Sectors written */
    uint32_t wr_ticks;	/* Time in queue + service for write */
    uint32_t deal_io; //正在处理io
    uint32_t ticks;	//输入输出时间
    uint32_t aveq;	//输入输出加权毫秒数
}disk_proc_info_t;

typedef struct disks_proc_info_st
{
    int ndisk;
    time_t sec;
    disk_proc_info_t disk[0];
}disks_proc_info_t;

typedef struct disk_out_info_st
{
    char name[32];
    float total; //单位m
    float used; //单位m
    float free; //单位m
}disk_out_info_t;

typedef struct disks_out_info_st
{
    time_t val;
    int ndisk;
    // disk_out_info_t disk[DISK_MAX_NUM];
    disk_out_info_t all;
    int32_t tps; //每秒io请求数
    float write; //每秒写入磁盘数，单位k
    float read; //每秒读取磁盘数，单位k
}disks_out_info_t;

typedef struct net_proc_info_st
{
    char name[32];
    uint64_t rxBytes;
    uint64_t rxPackets;
    uint64_t rxErr;
    uint64_t rxDrop;
    uint64_t rxFifo;
    uint64_t rxFrame;
    uint64_t rxCompressed;
    uint64_t rxMulticast;
    uint64_t txBytes;
    uint64_t txPackets;
    uint64_t txErr;
    uint64_t txDrop;
    uint64_t txFifo;
    uint64_t txColls;
    uint64_t txCarrier;
    uint64_t txCompressed;
}net_proc_info_t;

typedef struct net_procs_info_st
{
    time_t sec;
    int nnet;
    net_proc_info_t net[0];
}net_procs_info_t;

typedef struct net_out_info_st
{
    time_t val;
    int nnet;
    int32_t rx_packets; //每秒接收数据包
    int32_t tx_packets; //每秒发送数据包
    float rx_byte;
    float tx_byte;
}net_out_info_t;

extern cpus_out_info_t *cpuOut;
extern mem_out_info_t memOut;
extern disks_out_info_t diskOut;
extern net_out_info_t netOut;

#ifdef __cplusplus
extern "C"
{
#endif
int cupInit();
int updateCpuLoad();
void cpuDeinit();

int updateMemInfo();

int diskInit();
int updateDiskInfo();
int diskDeinit();

int performanceInit();
int performanceUpdate();
void performanceDeinit();

#ifdef __cplusplus
}
#endif

#endif //ECLIPSE_PAHO_C_RUNNINGPERFOMANCE_LINUX_H
