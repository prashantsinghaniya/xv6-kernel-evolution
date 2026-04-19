#include<kernel/types.h>
#include<user/user.h>

int 
main( int argc, char *argv[]){
    // int pid;
    // if(argc != 2){
    //     printf("Usage: childsyscnt <pid>\n");
    //     exit(1);
    // }
    // pid = atoi(argv[1]);
    // int numchild = getchildsyscount(pid);
    // printf("No. of system calls made by children of process %d is : %d\n", pid, numchild);
    int my_pid = getpid();
    int child_pid = fork();
    getpid();
    getpid();
    getpid();
    // printf("Child process ");
    getnumchild();
    if(getpid() == my_pid){
        printf("Parent process %d made %d syscalls through its children\n", my_pid, getchildsyscount(child_pid));
    }
    exit(0);
}