// assignment 1 task 4


#define MIN_POLICY 0
#define MAX_POLICY 2
#include "user.h"

int
main(int argc, char *argv[])
{
    int policy_id;
    if(argc < 2) {
        printf(1,"Usage: policy <id>\n");
        exit(1);
    }
    else {
        policy_id = atoi(argv[1]);
        int code = policy(policy_id);
        exit(code);
        
    }
}