/// @file scheduler_feedback.c
/// @brief Manage the current PID for the scheduler feedback session
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

/*
HOW TO:
all'avvio di MentOS, usa comando start, verrai avvisato su terminale quando la sessione sarà terminata.
Puoi verificare i risultati a video lanciando il comando cat sul file del desktop (feedback.txt)

es:
~  start
~  cat feedback.txt

Questa e' una versione di prova in cui registriamo una brevissima sessione di soli 10 pid.
(Funzionante per ora solo con lo scheduler RR)
Istruzioni relative ai parametri della funzione start (che sono work in progress) sono in start.c
*/

#include "process/scheduler_feedback.h"

#include "fcntl.h"
#include "stdio.h"
#include "fs/vfs.h"
#include "string.h"

#define FEEDBACK_FILENAME "/home/user/feedback.txt"

// Size of the buffer.
#define BUFFER_SIZE 256

#define MAX_SIZE_NAME 20

//Variabili Globali
#define MAX_STORAGE 10
int count                     = 0;
pid_t PID_BUFFER[MAX_STORAGE] = { 0 };
char PID_NAME[MAX_STORAGE][MAX_SIZE_NAME];

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
    for (int i = 0; i < countChar(name) && brake == 0; i++) {
        if (start[i] != name[i]) {
            brake = 1;
        }
    }

    //Entra solo una volta, dopo aver finito la sessione (riempito il buffer)
    if (brake == 0 && count == MAX_STORAGE - 1) {
        //RESET BUFFER
        for (int i = 0; i < MAX_STORAGE; i++) {
            PID_BUFFER[i] = 0;
        }
        count = 0;
    }

    //Se prima volta e' start bene, proseguiamo siccome brake e' zero,
    //le successive brake NON zero ma count > 0 poiche start passato in precedenza
    //Opportuno controllo per non sforare il MAX_STORAGE della struttura dati che utilizziamo
    if (count != MAX_STORAGE - 1 && (count != 0 || brake == 0)) {
        //REGISTRAZIONE PID IN BUFFER
        PID_BUFFER[count] = pid;
        strcpy(PID_NAME[count], name);
        count++;
    }

    //QUI SE BUFFER PIENO SCRIVO SU FILE

    if (count == MAX_STORAGE - 1) {
        mode_t mode = 000777;
        char buffer[BUFFER_SIZE];
        const char *namef = "/home/user/feedback.txt";
        ssize_t offset    = sizeof(char) * 38;
        char temp[6]; //max PID 99999

        // NOTE: You should check if the file exists, or create it, during the boot
        // phase of the system. Choose a place where to put it, a location like
        // `/var/scheduling_feedback.txt`.
        vfs_file_t *file = vfs_open(namef, O_RDWR, mode);

        if (file == NULL) {
            printf("Error: Failed to open feedback file.");
        }

        for (int i = 0; i < MAX_STORAGE; i++) {
            itoa(temp, PID_BUFFER[i], 10);
            offset += vfs_write(file, temp, offset, sizeof(char) * countChar(temp));
            offset += vfs_write(file, "   ", offset, sizeof(char) * 3);
            offset += vfs_write(file, PID_NAME[i], offset, sizeof(char) * countChar(PID_NAME[i]));
            offset += vfs_write(file, "\n", offset, sizeof(char));
            //ripristino array temp
            for (int j = 0; j < 5; j++) {
                temp[j] = 0;
            }
        }
        printf("End Recording\n");
        vfs_close(file);
        count = 0;
    }
}

/*
Funzione che conta caratteri del array di char passato
*/

int countChar(char name[])
{
    int ct = 0;
    while (name[ct] != '\0') {
        ct++;
    }
    return ct;
}

int scheduler_feedback_init()
{
    //
    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    //
    vfs_file_t *feedback = vfs_creat(FEEDBACK_FILENAME, mode);
    return 1;
}
