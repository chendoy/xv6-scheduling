// test for assignment 1 task 4

#include "user.h"
//#include "defs.h"
#include "sched.h"

void print_headline(void);
void child_work(void);
void print_stats(void);

int
main (int argc, char *argv[]) {
    print_headline();
    if(fork()==0) //child 1, low
    {     
        set_ps_priority(10);
        set_cfs_priority(3);
        child_work();
        print_stats();
        exit(0);
    }
    else //parent
    {
        if(fork()==0) //child 2, medium
        {
            set_ps_priority(5);
            set_cfs_priority(2);
            child_work();
            print_stats();
            exit(0);
        }
        else //parent
        {
            if(fork()==0){//child 3, high
                set_ps_priority(1);
                set_cfs_priority(1);
                child_work();
                print_stats();
                exit(0);
            }
        }
    }
    int status;
    wait(&status);
    wait(&status);
    wait(&status);
    exit(0); //parent exits
}

void child_work(void) 
{
    int i = 10000000;
    int dummy = 0;
    while(i--){
        printf(1,"");
        dummy += i;
    }
}

void print_stats(void) 
{
    struct perf *performance = malloc(sizeof(struct perf));
    performance->ps_priority = query_perf(0);
    performance->stime = query_perf(1);
    performance->retime = query_perf(2);
    performance->rtime = query_perf(3);
    proc_info(performance);
    free(performance);
}

void print_headline(void) {
    printf(1,"%s   %s   %s   %s   %s\n", "PID", "PS_PRIORITY", "STIME", "RETIME", "RTIME");
}

int
main2(int argc, char *argv[])
{
  int ps_prt, cfs_prt, pid;
  struct perf p;
  int i = 10000000;
  int dummy = 0;


  printf(1, "PID\tPS_PRIORITY\tSTIME\tRETIME\tRTIME\n");

  pid = getpid();
  ps_prt = pid % 7 + 3;
  cfs_prt = ps_prt / 3;
  for (int j=0; j<20; j++) {
      if ((pid = fork()) == 0) {
        break;
      }
      ps_prt = pid % 7 + 3;
      cfs_prt = ps_prt / 3;
  }
  if (pid) {
    while (wait(&dummy) != -1) {}
    exit(0);
  }
  
  set_ps_priority(ps_prt);
  set_cfs_priority(cfs_prt);

  while(i--)
    dummy+=i;


  pid = getpid();
  // sleep(pid*10);
  proc_info(&p);
  printf(1, "[%d]\t%d\t\t%d\t%d\t%d\n",
  pid, p.ps_priority, p.stime, p.retime, p.rtime);

  exit(0);
}