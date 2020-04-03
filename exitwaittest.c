// test for assignment 1 task 3

#include "user.h"

int
main (int argc, char *argv[]) {
    printf(1,"forking...\n");
    if(fork()==0) //child
    {
        exit(20);
    }
    else //parent
    {
        if(fork()==0) //another child
        {
            exit(30);
        }
        else
        {
            int status;
            wait(&status);
            printf(1,"Collected exit code: %d\n",status);
            wait(&status);
            printf(1,"Collected another exit code: %d\n",status);
            exit(0);
        }
        

    }
    
}