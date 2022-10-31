//
// Created by didi on 2022/10/25.
//
/**
 * @file running_performance.c
 * @author chenjixin@didiglobal.com
 * @brief 运行性能模块接口，要求linux内核版本>2.6，非线程安全
 *          主要分析cpu、mem、disk、net，如果全使用，则可以调用performanceInit初始化后，调用performanceUpdate更新数据，更新完成后，全局变量memOut、CpuOut、diskOut中即是所需要的数据
 *          如果只使用某个模块。则调用对应的init初始化后，调用update
 * @version 0.1
 * @date 2022-05-31
 *
 * @copyright Copyright (c) 2022
 *
 */

#include <linux/magic.h>
#include <sys/statvfs.h>
#include <errno.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/vfs.h>
#include <sys/sysinfo.h>
#include <sys/io.h>
#include <string.h>
#include <linux/major.h>
#include "RunningPerformance_linux.h"


// char buffer[256];
static FILE* cpuFp = NULL; // "/proc/stat"
static FILE* ncpuFp = NULL; // "/proc/cpuinfo"
static int cpuNum = 0;
static cpus_proc_info_t *cpuLast = NULL;
static cpus_proc_info_t *cpuNow = NULL;
cpus_out_info_t *cpuOut = NULL;


mem_out_info_t memOut;


static FILE *netFp = NULL; // "/proc/net/dev"
static net_procs_info_t *netLast = NULL;
static net_procs_info_t *netNow = NULL;
net_out_info_t netOut;


static FILE* diskFp = NULL; // "/proc/diskstats"
static disks_proc_info_t *diskLast = NULL;
static disks_proc_info_t *diskNow = NULL;
disks_out_info_t diskOut;

#ifndef IDE_DISK_MAJOR
#define IDE_DISK_MAJOR(M) ((M) == IDE0_MAJOR || (M) == IDE1_MAJOR || \
			   (M) == IDE2_MAJOR || (M) == IDE3_MAJOR || \
			   (M) == IDE4_MAJOR || (M) == IDE5_MAJOR || \
			   (M) == IDE6_MAJOR || (M) == IDE7_MAJOR || \
			   (M) == IDE8_MAJOR || (M) == IDE9_MAJOR)
#endif	/* !IDE_DISK_MAJOR */

#define MMC_DISK_MAJOR(M) ((M) == MMC_BLOCK_MAJOR)

#ifndef SCSI_DISK_MAJOR
#ifndef SCSI_DISK8_MAJOR
#define SCSI_DISK8_MAJOR 128
#endif
#ifndef SCSI_DISK15_MAJOR
#define SCSI_DISK15_MAJOR 135
#endif
#define SCSI_DISK_MAJOR(M) ((M) == SCSI_DISK0_MAJOR || \
			   ((M) >= SCSI_DISK1_MAJOR && \
			    (M) <= SCSI_DISK7_MAJOR) || \
			   ((M) >= SCSI_DISK8_MAJOR && \
			    (M) <= SCSI_DISK15_MAJOR))
#endif	/* !SCSI_DISK_MAJOR */

#define IS_DIGIT(M) ((M)>='0' && (M)<='9')

/**
 * @brief 获取系统启动后jiffies秒数
 *
 * @return time_t
 */
time_t getCurSec()
{
    struct timespec tp;

    clock_gettime(CLOCK_MONOTONIC, &tp);
    return tp.tv_sec;
}

/**
 * @brief Get the Num Cpu object
 *
 * @return int
 */
int getCpuNum()
{
    char buffer[4096] = {0};
    int ncpu = 0;

    fflush(ncpuFp);
    rewind(cpuFp);
    while (fgets(buffer, sizeof(buffer), ncpuFp))
    {
        if(strncmp(buffer, "processor\t:", strlen("processor\t:")) == 0)
            ncpu++;
    }
    return ncpu;
}

/******************************************************************/
/**
 * @brief 获取cpus消息
 *
 * @param cpu
 * @return int
 */
int getCupsMsg(cpus_proc_info_t *proc)
{
    const char* scanf_fmt = "%s %u %u %u %u";
    int i;
    char buffer[4096] = {0};
    char back[1024] = {0};

    memmove(back, proc, sizeof(sizeof(cpus_proc_info_t)+cpuNum*sizeof(cpu_proc_info_t)));

    fflush(cpuFp);
    rewind(cpuFp);
    for (i = 0; i < proc->ncpu; i++)
    {
        fgets(buffer, sizeof(buffer), cpuFp);
        if( sscanf(buffer, scanf_fmt, &proc->cpus[i].name, &proc->cpus[i].user,
                   &proc->cpus[i].nice, &proc->cpus[i].system, &proc->cpus[i].idle) != 5)
        {
            perror("getCupsMsg");
            memmove(proc, back, sizeof(sizeof(cpus_proc_info_t)+cpuNum*sizeof(cpu_proc_info_t)));
            return -1;
        }
    }
    proc->sec = getCurSec();
    proc->ncpu = cpuNum;

    return 0;
}

void printCpusProc(const cpus_proc_info_t *proc)
{
    printf("\nname \t user \t nice \t system \t idle \n");
    for (int i = 0; i < proc->ncpu; i++)
    {
        printf("%s \t %u \t %u \t %u \t %u \n",
               proc->cpus[i].name, proc->cpus[i].user, proc->cpus[i].nice, proc->cpus[i].system, proc->cpus[i].idle);
    }
}

void printCpusOut(const cpus_out_info_t *out)
{
    printf("\nname \t load \n");
    for (int i = 0; i < out->ncpu; i++)
    {
        if( i == 0)
            printf("cpu \t %f \n", out->load[i]);
        else
            printf("cpu%d \t %f \n", i-1, out->load[i]);
    }

}

int calCpuLoad(const cpus_proc_info_t *last, const cpus_proc_info_t *now, cpus_out_info_t *out)
{
    if(now->ncpu != last->ncpu)
    {
        perror("calCpuLoad now.ncpu != last.ncpu");
        return -1;
    }

    out->val = now->sec - last->sec;
    out->ncpu = now->ncpu;

    // printCpusProc(last);
    // printCpusProc(now);
    for(int i = 0; i < out->ncpu; i++)
    {
        uint64_t nowTotal, lastTotal;
        uint64_t nowUser, lastUser;

        nowUser = now->cpus[i].user + now->cpus[i].system;
        nowTotal = now->cpus[i].user + now->cpus[i].system + now->cpus[i].nice + now->cpus[i].idle;
        lastUser = last->cpus[i].user + last->cpus[i].system;
        lastTotal = last->cpus[i].user + last->cpus[i].system + last->cpus[i].nice + last->cpus[i].idle;

        if(nowTotal != lastTotal)
            out->load[i] = (float)(nowUser-lastUser) / (float)(nowTotal-lastTotal);
        else
            out->load[i] = 0;
    }
    // printCpusLoad(cpuOut);

    return 0;
}

int updateCpuLoad()
{
    int ret;
    if(getCupsMsg(cpuNow) < 0)
        return -1;

    ret = calCpuLoad(cpuLast, cpuNow, cpuOut);
    memmove(cpuLast, cpuNow, sizeof(cpus_proc_info_t)+cpuNum*sizeof(cpu_proc_info_t));

    return ret;
}

int cupInit()
{
    cpuFp = fopen("/proc/stat", "r");
    if(cpuFp == NULL)
        return -1;

    ncpuFp = fopen("/proc/cpuinfo", "r");
    if(ncpuFp == NULL)
        return -1;

    cpuNum = getCpuNum() + 1;

    cpuNow = calloc(1, sizeof(cpus_proc_info_t)+cpuNum*sizeof(cpu_proc_info_t));
    if(cpuNow == NULL)
        return -1;
    cpuNow->ncpu = cpuNum;

    cpuLast = calloc(1, sizeof(cpus_proc_info_t)+cpuNum*sizeof(cpu_proc_info_t));
    if(cpuLast == NULL)
        return -1;
    cpuLast->ncpu = cpuNum;

    cpuOut = calloc(1, sizeof(cpus_out_info_t)+cpuNum*sizeof(float));
    if(cpuOut == NULL)
        return -1;
    cpuOut->ncpu = cpuNum;

    if(getCupsMsg(cpuLast) == -1)
        return -1;

    return 0;
}

void cpuDeinit()
{
    if(cpuFp)
        fclose(cpuFp);
    if(ncpuFp)
        fclose(ncpuFp);

    if(cpuNow)
        free(cpuNow);
    if(cpuLast)
        free(cpuLast);
    if(cpuOut)
        free(cpuOut);
}

/******************************************************************/
void printMemOut(mem_out_info_t *mem)
{
    printf("\n total \t\t used \t\t free \n");
    printf("mem \t %0.2f \t %0.2f \t %0.2f \n", mem->total, mem->used, mem->free);
}

int updateMemInfo()
{
    struct sysinfo sinfo;

    if (sysinfo(&sinfo) == -1)
        return -1;

    memOut.total = (double)sinfo.totalram/1024/1024;
    memOut.free = (double)sinfo.freeram/1024/1024;
    memOut.used = memOut.total - memOut.free;

    return 0;
}


/******************************************************************/
void printDiskProc(disks_proc_info_t *proc)
{
    printf("\n%6s %6s %6s %10s %10s %10s %10s\n", "major", "minor", "name", "rd_ios", "rd_sectors", "wr_ios", "wr_sectors");
    for (int i = 0; i < proc->ndisk; i++)
    {
        printf("%6u %6u %6s %10u %10llu %10u %10llu\n",
               proc->disk[i].major, proc->disk[i].minor, proc->disk[i].name,
               proc->disk[i].rd_ios, proc->disk[i].rd_sectors,
               proc->disk[i].wr_ios, proc->disk[i].wr_sectors);
    }
}

void printDiskOut(disks_out_info_t *out)
{
    printf("\n%6s %9s %9s %9s %9s %9s %9s\n", "name", "total", "free", "used", "tps", "wr_disk", "rd_disk");
    printf("%6s %9.1f %9.1f %9.1f %10d %9.1f %9.1f\n", "all", out->all.total, out->all.free, out->all.used, out->tps, out->write, out->read);
}

/**
 * @brief 获取设备的disk信息
 *
 * @param proc
 * @return int fail:-1,else ndisk
 */
int getDiskMsg(disks_proc_info_t *proc)
{
    const char *scan_fmt = "%4d %4d %s %u %u %llu %u %u %u %llu %u %u %u %u";
    char buffer[4096];
    disk_proc_info_t disk;
    int pos = 0;
    int items;

    rewind(diskFp);
    while (pos < DISK_MAX_NUM)
    {
        memset(buffer, 0, 4096);
        if (fgets(buffer, 4096, diskFp) == NULL)
            break;

        items = sscanf(buffer, scan_fmt, &disk.major, &disk.minor, disk.name,
                       &disk.rd_ios, &disk.rd_merges, &disk.rd_sectors, &disk.rd_ticks,
                       &disk.wr_ios, &disk.wr_merges, &disk.wr_sectors, &disk.wr_ticks,
                       &disk.deal_io, &disk.ticks, &disk.aveq);

        if(items != 14)
            continue;

        if(!IS_DIGIT(disk.name[strlen(disk.name)-1]))
            continue;

        if(SCSI_DISK_MAJOR(disk.major) || IDE_DISK_MAJOR(disk.major) || MMC_DISK_MAJOR(disk.major))
        {
            memmove(&proc->disk[pos], &disk, sizeof(disk_proc_info_t));
            pos++;
        }
    }

    if(pos == 0)
        return -1;
    proc->sec = getCurSec();
    proc->ndisk = pos;

    return pos;
}

/**
 * @brief 获取disk相关数据，简化实现
 *        --后续需要解析/proc/mounts,再和/proc/diskstat中name匹配，计算所有挂在硬盘的容量
 *
 * @param last
 * @param now
 * @param out
 * @return int
 */
int calDiskInfo(const disks_proc_info_t *last, const disks_proc_info_t *now, disks_out_info_t *out)
{
    struct statfs s;
    uint64_t totalIOs = 0;
    uint64_t totalIOsRd = 0;
    uint64_t totalIOsWr = 0;
    // double totalDisk = 0;
    // double totalFreeDisk = 0;

    if(now->ndisk != last->ndisk)
        return -1;

    for(int i=0; i < now->ndisk; i++)
    {
        totalIOs += (now->disk[i].rd_ios + now->disk[i].wr_ios - last->disk[i].rd_ios - last->disk[i].wr_ios);
        totalIOsRd += (now->disk[i].rd_sectors - last->disk[i].rd_sectors);
        totalIOsWr += (now->disk[i].wr_sectors - last->disk[i].wr_sectors);
    }


    if (statfs("/", &s) == -1)
    {
        printf("statfs errno[%d]\n", errno);
        return -1;
    }
    float blocksize = (float)s.f_bsize/1024.0/1024.0;// 每个block里面包含的字节数

    out->val = now->sec - last->sec;
    out->ndisk = now->ndisk;

    out->all.total = s.f_blocks * blocksize;
    out->all.free = s.f_ffree * blocksize;
    out->all.used = out->all.total - out->all.free;
    if(out->val != 0)
    {
        out->tps = (double)totalIOs / out->val;
        out->write = (double)totalIOsWr / out->val;
        out->read = (double)totalIOsWr / out->val;
    }
    else
    {
        out->tps = (double)totalIOs;
        out->write = (double)totalIOsWr;
        out->read = (double)totalIOsWr;
    }

    return 0;
}

int updateDiskInfo()
{
    int ret;

    if(getDiskMsg(diskNow) == -1)
        return -1;

    ret = calDiskInfo(diskLast, diskNow, &diskOut);
    memmove(diskLast, diskNow, sizeof(disks_proc_info_t)+DISK_MAX_NUM*sizeof(disk_proc_info_t));

    return ret;
}

int diskInit()
{
    diskFp = fopen("/proc/diskstats", "r");
    if(diskFp == NULL)
        return -1;

    // mountFp = fopen("/proc/mounts", "r");
    // if(mountFp == NULL)
    //     return -1;

    diskNow = calloc(1, sizeof(disks_proc_info_t)+DISK_MAX_NUM*sizeof(disk_proc_info_t));
    if(diskNow == NULL)
        return -1;

    diskLast = calloc(1, sizeof(disks_proc_info_t)+DISK_MAX_NUM*sizeof(disk_proc_info_t));
    if(diskLast == NULL)
        return -1;

    if(getDiskMsg(diskLast) == -1)
        return -1;

    return 0;
}

int diskDeinit()
{
    if(diskFp)
        fclose(diskFp);
    if(diskNow)
        free(diskNow);
    if(diskLast)
        free(diskLast);

    return 0;
}

/******************************************************************/
void printNetProc(net_procs_info_t *proc)
{
    printf("\n%10s %10s %10s %10s %10s\n", "name", "rxByte", "rxPacket", "txByte", "txPacket");
    for(int i = 0; i < proc->nnet; i++)
    {
        printf("%10s %10llu %10llu %10llu %10llu\n",
               proc->net[i].name, proc->net[i].rxBytes, proc->net[i].rxPackets, proc->net[i].txBytes, proc->net[i].txPackets);
    }
}

void printNetOut(net_out_info_t *out)
{
    printf("\n%10s %10s %10s %10s %10s\n", "name", "rxByte", "rxPacket", "txByte", "txPacket");
    printf("%10s %10.1f %10d %10.1f %10d\n", "total", out->rx_byte, out->rx_packets, out->tx_byte, out->tx_packets);
}

int getNetMsg(net_procs_info_t *proc)
{
    int pos = 0;
    int delRow = 2;
    char buffer[4096];
    int items;
    const char *scan_fmt = "%s %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu";

    rewind(netFp);
    while (pos < NET_MAX_NUM)
    {
        memset(buffer, 0, 4096);
        if (fgets(buffer, 4096, netFp) == NULL)
            break;
        if (delRow--)
            continue;
        memset(proc->net[pos].name, 0, 32);
        items = sscanf(buffer, scan_fmt, proc->net[pos].name,
                       &proc->net[pos].rxBytes, &proc->net[pos].rxPackets, &proc->net[pos].rxErr, &proc->net[pos].rxDrop,
                       &proc->net[pos].rxFifo, &proc->net[pos].rxFrame, &proc->net[pos].rxCompressed, &proc->net[pos].rxMulticast,
                       &proc->net[pos].txBytes, &proc->net[pos].txPackets, &proc->net[pos].txErr, &proc->net[pos].txDrop,
                       &proc->net[pos].txFifo, &proc->net[pos].txColls, &proc->net[pos].txCarrier, &proc->net[pos].txCompressed);
        if (items != 17)
            continue;
        pos++;
    }
    if (pos == 0)
    {
        printf("getNetMsg pos == 0\n");
        return -1;
    }

    proc->sec = getCurSec();
    proc->nnet = pos;
    return 0;
}

int calNetInfo(const net_procs_info_t *last, const net_procs_info_t *now, net_out_info_t *out)
{
    uint64_t rxByteTotal = 0;
    uint64_t rxPacketTotal = 0;
    uint64_t txByteTotal = 0;
    uint64_t txPacketTotal = 0;

    if(last->nnet != now->nnet)
    {
        printf("calNetInfo last->nnet != now->nnet");
        return -1;
    }

    for (int i = 0; i < now->nnet; i++)
    {
        rxByteTotal += (now->net[i].rxBytes - last->net[i].rxBytes);
        rxPacketTotal += (now->net[i].rxPackets - last->net[i].rxPackets);
        txByteTotal += (now->net[i].txBytes - last->net[i].txBytes);
        txPacketTotal += (now->net[i].txPackets - last->net[i].txPackets);
    }
    out->val = now->sec - last->sec;
    out->nnet = now->nnet;
    if(out->val != 0)
    {
        out->rx_byte = (float)rxByteTotal / (float)out->val;
        out->tx_byte = (float)txByteTotal / (float)out->val;
        out->rx_packets = rxPacketTotal / out->val;
        out->tx_packets = txPacketTotal / out->val;
    }
    else
    {
        out->rx_byte = (float)rxByteTotal;
        out->tx_byte = (float)txByteTotal;
        out->rx_packets = rxPacketTotal;
        out->tx_packets = txPacketTotal;
    }

    return 0;
}

int updateNetInfo()
{
    int ret;

    if(getNetMsg(netNow) == -1)
        return -1;

    ret = calNetInfo(netLast, netNow, &netOut);
    memmove(netLast, netNow, sizeof(net_procs_info_t)+sizeof(net_proc_info_t)*NET_MAX_NUM);

    return ret;
}

int netInit()
{
    netFp = fopen("/proc/net/dev", "r");
    if(netFp == NULL)
        return -1;

    netLast = calloc(1, sizeof(net_procs_info_t)+sizeof(net_proc_info_t)*NET_MAX_NUM);
    if(netLast == NULL)
        return -1;

    netNow = calloc(1, sizeof(net_procs_info_t)+sizeof(net_proc_info_t)*NET_MAX_NUM);
    if(netNow == NULL)
        return -1;

    getNetMsg(netLast);

    return 0;
}

void netDeinit()
{
    if(netFp)
        fclose(netFp);
    if(netLast)
        free(netLast);
    if(netNow)
        free(netNow);
}


int performanceInit()
{
    return (cupInit() || diskInit() || netInit());
}

void performanceDeinit()
{
    cpuDeinit();
    diskDeinit();
    netDeinit();
}

int performanceUpdate()
{
    return (updateCpuLoad() || updateMemInfo() || updateDiskInfo() || updateNetInfo());
}

 int main()
 {
     int num = 10;
/*    if( performanceInit() != 0)
    {
        printf("init error");
        return -1;
    }
    while (num--)
    {
        sleep(1);
        performanceUpdate();
        system("clear");
        printCpusOut(cpuOut);
        printMemOut(&memOut);
        printDiskOut(&diskOut);
    }
    performanceDeinit();*/

  /*  if (cupInit() == -1)
        exit(EXIT_FAILURE);
    while (num--)
    {
        sleep(1);
        updateCpuLoad();
    }
    cpuDeinit();*/

    while (num--)
    {
        sleep(1);
        updateMemInfo();
        system("clear");
        printMemOut(&memOut);
    }
/*
    diskInit();
    while (num--)
    {
        sleep(1);
        system("clear");
        printDiskProc(diskLast);
        updateDiskInfo();
        printDiskProc(diskNow);
        printDiskOut(&diskOut);
    }
    diskDeinit();*/

 /*   netInit();
    while (num--)
    {
        sleep(1);
        system("clear");
        printNetProc(netLast);
        updateNetInfo();
        printNetProc(netNow);
        printNetOut(&netOut);
    }
    netDeinit();*/
     return 0;
 }
