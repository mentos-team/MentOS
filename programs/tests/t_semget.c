//test
#include <ipc/sem.h>
int main()
{
    long i = semget(0,0,0);  //checking if the syscalls are already linked
    
    return 0;
}
