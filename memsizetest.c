// assignment 1 task 2

#include "user.h"
#define K 1024

int
main(int argc, char *argv[])
{
    printf(1,"The process is using: %dB\n",memsize());
    printf(1,"Allocating more memory\n");
    void *mem = malloc(2*K);
    printf(1,"The process is using: %dB\n",memsize());
    printf(1,"Freeing memory\n");
    free(mem);
    printf(1,"The process is using: %dB\n",memsize());
    exit(0);
}