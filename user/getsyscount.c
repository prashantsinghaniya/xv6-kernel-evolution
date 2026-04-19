#include<kernel/types.h>
#include<user/user.h>

int 
main(void){
    
    int sys_count =  getsyscount();
    // int ppid = getppid();
    int sys_count2 =  getsyscount();
    printf("Number of system calls: %d, %d\n", sys_count, sys_count2);  
    exit(0);
}