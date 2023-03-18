//#include "../fs/open.c"
//NB diocaro: dopo 2 ore ho scoperto che il bro ha ridefinito le open, le close, le write con la dicitura "sys_" davanti
//sta tutto in /mentos/src/fs/.....

#include "fcntl.h"
#include "stdio.h"


int writeFeedback()
{
   mode_t mode = 000777;
   char buffer[100 + 1];
   const char* name = "feedback.txt";
   

   int fptr = 0;
   //fptr = sys_open("/../../../files/home/feedback.txt", O_WRONLY | O_CREAT | O_TRUNC , mode);
   //fptr = sys_open("/../../../files/home/feedback.txt", O_RDONLY | O_CREAT | O_EXCL  , mode);
   //fptr = sys_open("/../../../files/home/feedback.txt", O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
   fptr = sys_open(name, O_CREAT | O_WRONLY , S_IRUSR | S_IWUSR);

   if(fptr == -1){
      printf("suca");
      return 0;
   }
   

   // Reading up to MAX_READ bytes from STDIN.
   ssize_t numRead = sys_read(fptr, buffer, 100);

   buffer[numRead] = '\0';
   printf("Input data: %s\n", buffer);


   //write to file
   //sys_write(fptr,"ciao",4);
   //printf("suca"); //questo va
   sys_close(fptr); //il problema è qui, se chiudiamo il file non arriviamo al login, se non chiudiamo il file invece proviamo a fare login ma non riusciamo perchè non riesce ad aprire passwd
   return 1; 
}