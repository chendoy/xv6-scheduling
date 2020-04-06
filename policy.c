// assignment 1 task 4

#include "user.h"
//#include "defs.h"

int
main(int argc, char *argv[])
{
    int policy_id;
    char *policy_str;
    if(argc < 2) {
        printf(1,"Usage: policy <id>\n");
        exit(1);
    }
    else {
        policy_id = atoi(argv[1]);

        if (policy_id < 0 || policy_id > 2){
            printf(1,"Error replacing policy, no such a policy number (%d)\n",policy_id);
            exit(1);
        }
        else {
            switch(policy_id) {
                case 0:
                    policy_str = "Default Policy";
                break;
                case 1:
                    policy_str = "Priority Policy";
                break;
                case 2:
                    policy_str = "CFS Policy";
                break;
            }
            policy(policy_id);
            printf(1,"Policy has been successfully changed to %s\n",policy_str);
            exit(0);
        }
    }
}