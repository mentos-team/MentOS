/// @file t_msgget.c
/// @brief This program creates a message queue and demonstrates sending and
/// receiving messages using System V message queues.
/// @details This program creates a message queue, sends and receives messages,
/// and then deletes the message queue. It demonstrates basic message queue
/// operations in C, including error handling for message queue operations. The
/// program also includes a child process that sends a message to the parent
/// process.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <sys/unistd.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>

#define MESSAGE_LEN 100

/// @brief Structure for message queue.
typedef struct {
    long mesg_type;              ///< Message type.
    char mesg_text[MESSAGE_LEN]; ///< Message text.
} message_t;

/// @brief Send a message to the message queue.
/// @param msqid The message queue identifier.
/// @param mtype The message type.
/// @param message The message structure.
/// @param msg The message text.
static inline void __send_message(int msqid, long mtype, message_t *message, const char *msg)
{
    // Set the type.
    message->mesg_type = mtype;
    // Set the content.
    strncpy(message->mesg_text, msg, MESSAGE_LEN);
    // Send the message.
    if (msgsnd(msqid, message, sizeof(message->mesg_text), 0) < 0) {
        perror("Failed to send the message");
    } else {
        printf("[%2d] Message sent (%2d) `%s`\n", getpid(), message->mesg_type, message->mesg_text);
    }
}

/// @brief Receive a message from the message queue.
/// @param msqid The message queue identifier.
/// @param mtype The message type.
/// @param message The message structure.
static inline void __receive_message(int msqid, long mtype, message_t *message)
{
    // Clear the user-defined message.
    memset(message->mesg_text, 0, sizeof(char) * MESSAGE_LEN);
    // Receive the message.
    if (msgrcv(msqid, message, sizeof(message->mesg_text), mtype, 0) < 0) {
        perror("Failed to receive the message");
    } else {
        printf("[%2d] Message received (%2d) `%s` (Query: %2d)\n", getpid(), message->mesg_type, message->mesg_text, mtype);
    }
}

int main(int argc, char *argv[])
{
    message_t message;
    long ret;
    key_t key;
    int msqid;

    // ========================================================================
    // Generating a key using ftok
    key = ftok("/README", 5);
    if (key < 0) {
        perror("Failed to generate key using ftok");
        return EXIT_FAILURE;
    }
    printf("Generated key using ftok (key = %d)\n", key);

    // ========================================================================
    // Create the first message queue.
    msqid = msgget(key, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    if (msqid < 0) {
        perror("Failed to create message queue");
        return EXIT_FAILURE;
    }
    printf("Created message queue (id : %d)\n", msqid);

    // ========================================================================
    // Send the message.
    __send_message(msqid, 1, &message, "Hello there!");
    // Receive the message.
    __receive_message(msqid, 1, &message);
    // Create child process.
    if (!fork()) {
        sleep(3);
        // Send the message.
        __send_message(msqid, 1, &message, "General Kenobi...");
        return EXIT_SUCCESS;
    }
    // Receive the message.
    __receive_message(msqid, 1, &message);
    sleep(3);

    // ========================================================================
    // Send multiple messages.
    __send_message(msqid, 7, &message, "course, ");
    __send_message(msqid, 9, &message, "cheers!");
    __send_message(msqid, 1, &message, "From the operating");
    __send_message(msqid, 3, &message, "systems");

    // Receive multiple messages.
    __receive_message(msqid, 1, &message);
    __receive_message(msqid, -8, &message);
    __receive_message(msqid, -8, &message);
    __receive_message(msqid, 0, &message);

    // ========================================================================
    // Delete the message queue.
    ret = msgctl(msqid, IPC_RMID, NULL);
    if (ret < 0) {
        perror("Failed to remove message queue.");
        return EXIT_FAILURE;
    }
    printf("Correctly removed message queue.\n");
    return EXIT_SUCCESS;
}
