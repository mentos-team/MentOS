/// @file start.c
/// @brief Start the scheduler feedback session
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.
#include <stdlib.h>
#include <stdio.h>
#include <time.h>


/*
Start, funzionamento: L'utente scrivendo start da shell (interna a MentOS) avviera' la registrazione di un numero
fissato di PID. 
Tre tipologie di parametri/flag accettati dalla start:

    -start --> avvia la recording senza alcun parametro (ci aspettiamo file con PID 1 e 2 alternati)
    -start -p --> avvia la recording utilizzando anche la funzione fork, cosi da far vedere nel file PID diversi (almeno inizialmente)
    -start -f file.c --> avvia la recording lanciando prima il file passato come parametro
    
*/

int main()
{
    //N.B va implementato il reset della struttura che alloca i dati(PID)
    printf("Avvio Recording\n");
    return 0;
}