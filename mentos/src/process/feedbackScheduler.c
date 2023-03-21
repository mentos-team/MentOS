//#include "../fs/open.c"
//sta tutto in /mentos/src/fs/.....

#include "fcntl.h"
#include "stdio.h"
#include "fs/vfs.h"

/// Size of the buffer.
#define BUFFER_SIZE 256
int count = 0;

int writeFeedback()
{
   if(count == 300000)
   {
    mode_t mode = 000777;
    char buffer[BUFFER_SIZE];
    const char *name = "/home/user/feedback.txt";

    // NOTE: You should check if the file exists, or create it, during the boot
    // phase of the system. Choose a place where to put it, a location like
    // `/var/scheduling_feedback.txt`.
    vfs_file_t *file = vfs_open(name, O_RDWR, mode);

    if (file == NULL) {
        printf("Error: Failed to open feedback file.");
        return 0;
    }

    // NOTE: You need also to save the offset where you are reading or writing
    // the file. You are not using the simplified read() or write(), this one is
    // a kernel-side function.
    ssize_t offset = 0;
    buffer[0] = 'P';
    buffer[1] = 'i';
    buffer[2] = 'd';
    buffer[3] = '\0';
    vfs_write(file, buffer, offset, 2);

    offset = 0;
    // Reading up to MAX_READ bytes from STDIN.
    //numRead         = vfs_read(file, buffer, offset, BUFFER_SIZE);
    //buffer[numRead+1] = '5';
    printf("luca");

    //write to file
    //vfs_write(file,"ciao",4);
    vfs_close(file); 
   }
   count++;
   return 1;
} 