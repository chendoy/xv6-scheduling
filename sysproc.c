#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  int status;

  if(argint(0, &status) < 0)
    return -1;
  exit(status);
  return 0;  // not reached
}

int
sys_wait(void)
{
  int *status = null; 

  if(argptr(0, (char**)&status, 4) < 0)
    return -1;
  return wait(status);
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// assignment 1 task 2
uint
sys_memsize(void)
{
  return myproc()->sz;
}

int
sys_set_ps_priority(void)
{
  int priority;

  if(argint(0, &priority) < 0)
    return -1;
  return set_ps_priority(priority);
}

int
sys_policy(void)
{
  int policy_id;

  if(argint(0, &policy_id) < 0)
    return -1;
  return policy(policy_id);
}

int
sys_set_cfs_priority(void) 
{
  int priority;

  if(argint(0, &priority) < 0)
    return -1;

  return set_cfs_priority(priority);
}

int
sys_proc_info(void) {
  struct perf *performance;
  int perf_size = sizeof(performance);

  if(argptr(0, (char**)&performance, perf_size) < 0)
    return -1;

  return proc_info(performance);
}

int
sys_query_perf(void) {

  int flag;

  if(argint(0, &flag) < 0)
    return -1;

  return query_perf(flag);
}