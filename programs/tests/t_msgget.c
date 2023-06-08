/// @file t_msgget.c
/// @brief This program creates a message queue.
/// @copyright (c) 2014-2023 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "string.h"
#include "sys/unistd.h"
#include "sys/errno.h"
#include "sys/msg.h"
#include "sys/ipc.h"
#include "stdlib.h"
#include "fcntl.h"
#include "stdio.h"

#define MESSAGE_LEN 100

// structure for message queue
typedef struct {
    long mesg_type;
    char mesg_text[MESSAGE_LEN];
} message_t;

int main(int argc, char *argv[])
{
    message_t message;
    long ret, msqid;
    key_t key;

    // ========================================================================
    // Generating a key using ftok
    key = ftok("/home/user/test7.txt", 5);
    if (key < 0) {
        perror("Failed to generate key using ftok");
        return 1;
    }
    printf("Generated key using ftok (key = %d)\n", key);

    // ========================================================================
    // Create the first message queue.
    msqid = msgget(key, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    if (msqid < 0) {
        perror("Failed to create message queue");
        return 1;
    }
    printf("Created message queue (id : %d)\n", msqid);

    // Set the type.
    message.mesg_type = 1;
    // Set the content.
    strcpy(message.mesg_text, "Hello there!");

    // Send the message.
    if (msgsnd(msqid, &message, sizeof(message), 0) < 0) {
        perror("Failed to send the message");
        return 1;
    }
    // Display the message.
    printf("We sent the message `%s`\n", message.mesg_text);

    // Clear the user-defined message.
    memset(message.mesg_text, 0, sizeof(char) * MESSAGE_LEN);
    // Receive the message.
    if (msgrcv(msqid, &message, sizeof(message), 1, 0) < 0) {
        perror("Failed to receive the message");
        return 1;
    }
    // Display the message.
    printf("We received the message `%s`\n", message.mesg_text);

    // Clear the user-defined message.
    memset(message.mesg_text, 0, sizeof(char) * MESSAGE_LEN);
    // Receive the message.
    if (msgrcv(msqid, &message, sizeof(message), 1, 0) < 0) {
        perror("Failed to receive the message");
        return 1;
    }
    // Display the message.
    printf("We received the message `%s`\n", message.mesg_text);

    return 0;
}
