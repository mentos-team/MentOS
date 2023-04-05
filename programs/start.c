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
#include <fcntl.h>

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
    int flag = 0;
    
    if(argc == 1){
        printf("Start Recording\n");
        printf("End Recording\n");
        return 0;
    }
    else if(argc == 2 && !(strcmp(argv[1],"-p"))){

        printf("Start Recording\n");

        for(int i = 0; i < 5; i++){

            if(fork()==0){
                exit(1);
            }

        }

        for(int i = 0; i < 5; i++){
            wait(NULL);
        }   

        printf("End Recording\n");
        return 0;

    } 
    else if( argc==3 && !strcmp(argv[1],"-f") ){

            char destination[] = "../../bin/tests/";
            strcat(destination,argv[2]);
            if(fork()==0){
                flag = execl(destination, "customProgram", NULL, NULL);
                if(flag == -1){
                    execl("../../bin/startERR","error", NULL, NULL);
                }
            }
            wait(NULL);

            /*
            if(flag != -1)
            {
                printf("Start Recording\n");
                printf("End Recording\n");
            }*/
            
            int fd;
            mode_t flag1 = 000111;
            char destination1[] = "../../bin/tests/";
            strcat(destination1,argv[2]);
            fd = open(destination1, S_IRUSR, flag1);
                if( fd != -1 ){
                    printf("Start Recording\n");
                    printf("End Recording\n");
                    close(fd);
                }
                else{
                    printf("start: file '%s' not found!\n\n", destination1);
                }


            return 0;
    }
    
    if(argc != 1){
        //se arriviamo qui, significa che sono stati passati parametri errati
        if(strcmp(argv[1],"-f")){
            printf("start: start has no command '%s'\n\n", argv[1]);
            execl("../../bin/startERR","error", NULL, NULL);
        }
        else{
            printf("start: missing FILE for OPTION '%s'\n\n", argv[1]);
            execl("../../bin/startERR","error", NULL, NULL);
        }
    }


    /*
    if(argc > 1 && (strcmp(argv[1],"-p") && strcmp(argv[1],"-f"))){  //da rafforzare
        
    }*/

}