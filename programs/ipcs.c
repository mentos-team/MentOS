/// @file ipcs.c
/// @brief
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#if 0
static inline void print_sem_stat()
{
    printf("Semaphores: \n");
    printf("%-10s %-10s %-10s %-10s %-10s %-10s \n", "T", "ID", "KEY", "MODE", "OWNER", "GROUP");
    printf("%-20s %-10s %-20s \n\n", "", "Semaphores not implemented", "");
}

static inline void print_msg_stat()
{
    printf("Message Queues: \n");
    printf("%-10s %-10s %-10s %-10s %-10s %-10s \n", "T", "ID", "KEY", "MODE", "OWNER", "GROUP");
    printf("%-20s %-10s %-20s \n\n", "", "Message Queues not implemented", "");
}

static inline void print_shm_stat()
{
    printf("Shared Memory: \n");
    printf("%-10s %-10s %-10s %-10s %-10s %-10s \n", "T", "ID", "KEY", "MODE",           "OWNER", "GROUP");
    printf("%-20s %-10s %-20s \n\n", "", "Shared Memory not implemented", "");
}
#endif

int main(int argc, char **argv)
{
#if 0
    if (argc > 2) {
        printf("Too much arguments.\n");

        return;
    }

    char datehour[100] = "";
    strdatehour(datehour);

    printf("IPC status from " OS_NAME " as of %s\n", datehour);

    if (argc == 2) {
        if (strcmp(argv[1], "-s") == 0) {
            print_sem_stat();
        } else if (strcmp(argv[1], "-m") == 0) {
            print_shm_stat();
        } else if (strcmp(argv[1], "-q") == 0) {
            print_msg_stat();
        } else {
            printf("Option not recognize.\n");
        }
    } else {
        print_sem_stat();
        print_shm_stat();
        print_msg_stat();
    }

    return;
#endif
}
