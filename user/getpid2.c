#include<kernel/types.h>
#include<user/user.h>

int 
main(void){
    int pid = getpid();
    int pid2 = getpid2();
    printf("pid = %d, pid2 = %d\n", pid, pid2);
    exit(0);
}