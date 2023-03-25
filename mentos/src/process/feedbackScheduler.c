/// @file feedbackScheduler.c
/// @brief Manage the current PID for the scheduler feedback session
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "fcntl.h"
#include "stdio.h"
#include "fs/vfs.h"

//Variabili Globali
// Size of the buffer.
//#define BUFFER_SIZE 256
#define MAX_STORAGE 50
int count = 0;

int flag = 0;
int count2 = 0;
pid_t salvati[10] = {0};
int i = 0;



int countChar(char[]);


/*
    PID 1 --> INIT
    PID 2 --> KTHREADD
*/

/*
Funzione che viene chiamata da scheduler_algorithm dopo aver scelto il prossimo processo da eseguire
*/
void writeFeedback(pid_t pid, char name[])
{   
    int brake = 0;

    char start[] = "start";

    //analizzo se nome del PID passato come argomento corrisponde al comando START
    for(int j = 0 ; j < countChar(name) && brake == 0; j++)
    {
        if(start[j] != name[j])
        {
            brake = 1;
        }
    }

    //Entra solo una volta, ovvero la prima volta che viene dato start
    if(brake == 0 && count == MAX_STORAGE)
    {
        //RESET BUFFER
    }

    //Se prima volta e' start bene, proseguiamo siccome brake e' zero, 
    //le successive brake NON zero ma count > 0 poiche start passato in precedenza
    //Opportuno controllo per non sforare il MAX_STORAGE della struttura dati che utilizziamo
    if(count != MAX_STORAGE && (count != 0 || brake == 0 ))
    {
        //REGISTRAZIONE PID IN BUFFER
    }

    //QUI SE BUFFER PIENO SCRIVO SU FILE
    //QUI SOTTO PROBABILMENTE QUASI TUTTO DA CANCELLARE

    if(pid != 1 && pid != 2)
    {
        flag = 1;
    }
    else if(i<10)
    {
        salvati[i] = pid;
        i++;
    }
    

    if(count2%10 == 0)
    {
        if(flag == 1 && pid != 1 && pid != 2)
        {
            //printf("%s con %d caratteri\n",name,countChar(name));
            //printf("%s\n",start);
        //printf("%i\n",pid);
        //printf("Ho stampato: %s\n", name);
        }
    }
    count2++;


    //ROBA PER LAVORARE SUI FILE
    /*     
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

    //write to file
    //vfs_write(file,"ciao",4);
    vfs_close(file); 
    }
    */
} 


/*
Funzione che conta caratteri del array di char passato
*/

int countChar(char name[]){
    int ct = 0;               
    while(name[ct]!='\0'){
        ct++;
    }
    return ct;
}