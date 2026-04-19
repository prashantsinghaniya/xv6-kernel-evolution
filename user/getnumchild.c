#include<kernel/types.h>
#include<user/user.h>

int 
main(void){
    int numchild = getnumchild();
    printf("Number of children: %d\n", numchild);
    exit(0);
}