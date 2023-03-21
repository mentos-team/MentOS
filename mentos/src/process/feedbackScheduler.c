//#include "../fs/open.c"
//NB diocaro: dopo 2 ore ho scoperto che il bro ha ridefinito le open, le close, le write con la dicitura "sys_" davanti
//sta tutto in /mentos/src/fs/.....

#include "fcntl.h"
#include "stdio.h"
#include "fs/vfs.h"

/// Size of the buffer.
#define BUFFER_SIZE 256

int writeFeedback()
{
    mode_t mode = 000777;
    char buffer[BUFFER_SIZE];
    const char *name = "feedback.txt";

    // NOTE: You should check if the file exists, or create it, during the boot
    // phase of the system. Choose a place where to put it, a location like
    // `/var/scheduling_feedback.txt`.
    vfs_file_t *file = vfs_open(name, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);

    if (file == NULL) {
        printf("Error: Failed to open feedback file.");
        return 0;
    }

    // NOTE: You need also to save the offset where you are reading or writing
    // the file. You are not using the simplified read() or write(), this one is
    // a kernel-side function.
    ssize_t numRead, offset = 0;

    // Reading up to MAX_READ bytes from STDIN.
    numRead         = vfs_read(file, buffer, offset, BUFFER_SIZE);
    buffer[numRead] = '\0';
    printf("Input data: %s\n", buffer);

    //write to file
    //vfs_write(file,"ciao",4);
    //printf("suca"); //questo va
    vfs_close(file); //il problema è qui, se chiudiamo il file non arriviamo al login, se non chiudiamo il file invece proviamo a fare login ma non riusciamo perchè non riesce ad aprire passwd
    return 1;
}