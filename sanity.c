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
        set_ps_priority(1);
        set_cfs_priority(1);
        child_work();
        print_stats();
        exit(0);
    }
    else //parent
    {
        if(fork()==0) //child 2, medium
        {
            set_ps_priority(1);
            set_cfs_priority(1);
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