//test
#include "ipc/sem.h"
#include "stdio.h"
int main()
{

    key_t i = ftok("/test80.txt", 5);  //checking if the syscalls are already linked
    //long i = semget(0,0,0);
    
    if (i == -1){
        printf("Error with IPC_KEY\n");
        return -1;
    }
    
    printf("IPC_KEY: %d\n", i);
    return 0;
}