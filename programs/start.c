/// @file start.c
/// @brief Start the scheduler feedback session
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include <sys/unistd.h>
#include <sys/wait.h>
#include <string.h>

/*
Start, funzionamento: L'utente scrivendo start da shell (interna a MentOS) avviera' la registrazione di un numero
fissato di PID. 
Tre tipologie di parametri/flag accettati dalla start:

    -start --> avvia la recording senza alcun parametro (ci aspettiamo file con PID 1 e 2 alternati)
    -start -p --> avvia la recording utilizzando anche la funzione fork, cosi da far vedere nel file PID diversi (almeno inizialmente)
    -start -f file.c --> avvia la recording lanciando prima il file passato come parametro
    
*/

//int clonaccio(int, char *);

int main(int argc, char *argv[])
{
    //N.B va implementato il reset della struttura che alloca i dati(PID)
   
    //aka utente ha inserito un comando start con flag errati
    if(argc > 1 && (strcmp(argv[1],"-p") || strcmp(argv[1],"-f"))){ 
        printf("start has no command '%s'\n\n", argv[1]);
        execl("../../bin/startERR","error", NULL, NULL);
        return -1;
    }

    printf("Start Recording\n");

    if(argc == 2){

        if(!(strcmp(argv[1],"-p"))){

            for(int i = 0; i < 5; i++){

                if(fork()==0){
                    
                    //volendo no fork qua, ma solo chiamata a t_fork 5 con una exec
                    //execl("../../bin/start0", "figlio", NULL, NULL);
                    exit(1);

                }

            }

            for(int i = 0; i < 5; i++){
                wait(NULL);
            }

        }
    }
    printf("End Recording\n");
    return 0;
}