///                MentOS, The Mentoring Operating system project
/// @file testing.c
/// @brief Commands used to test OS functionalities.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "shm.h"
#include "stdio.h"
#include "timer.h"
#include "debug.h"
#include "shell.h"
#include "video.h"
#include "assert.h"
#include "stdlib.h"
#include "unistd.h"
#include "string.h"
#include "spinlock.h"

//Function used to test vfork.
int task_test_function(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	printf("Hey there, here is %s with pid %d!\n", argv[0], getpid());
	sleep(12);
	printf("Here is %s with pid %d, I'm leaving.\n", argv[0], getpid());

	return 0;
}

void try_process(int argc, char **argv)
{
	printf("I'm %d, testing task creation functions...\n", getpid());
	pid_t cpid = vfork();

	if (cpid == 0) {
		char *_cmd[] = { "task_test_function", (char *)NULL };
		char *_env[] = { (char *)NULL };
		execve((const char *)task_test_function, _cmd, _env);
		assert(false && "I should not be here.");
	}
	printf("Executed process with pid %d...\n", cpid);
}

void try_stress_heap(int argc, char **argv)
{
	uint32_t max_element = 1000;
	if (argc >= 1) {
		int val = atoi(argv[0]);
		if (val > 0) {
			max_element = (uint32_t)val;
		}
	}

	dbg_print("Starting allocation of matrix of %d...\n", max_element);
	uint32_t **elements = malloc(max_element * sizeof(uint32_t *));
	dbg_print("Starting allocation of each vector...\n");
	for (uint32_t i = 0; i < max_element; ++i) {
		elements[i] = malloc(100 * sizeof(uint32_t));
		(*elements[i]) = i;
	}

	dbg_print("Starting de-allocation of each vector...\n");
	for (uint32_t i = 0; i < max_element; ++i) {
		free(elements[i]);
	}
	free(elements);
	dbg_print("Done\n");
}

// Share memory keys used in shm test.
#define SHMKEY1 81
#define SHMKEY2 82

// Spinlock used in the following shm test.
static spinlock_t shmspinlock1;
static spinlock_t shmspinlock2;

// @brief Function used to test the shm.
int shm_test_1(void *args)
{
	(void)args;

	printf("[T1] I am the first process to be executed.\n");

	int shmid = shmget(SHMKEY1, 128 * sizeof(char), 0777);
	if (shmid == -1) {
		printf("[T1] Error: shmget() failed!\n");

		return -1;
	}
	printf("[T1] I have got a share memory with ID %i.\n", shmid);

	char *myshm = shmat(shmid, NULL, 0);
	if (myshm == (void *)-1) {
		printf("[T1] Error: shmat() failed!\n");

		return -1;
	}
	printf("[T1] I attached the share memory in my virtual address. \n");
	printf("[T1]         SHM VIRTUAL ADDRESS %p\n", myshm);
	printf("[T1]        SHM PHYSICAL ADDRESS %p\n",
		   paging_virtual_to_physical(get_current_page_directory(), myshm));

	printf("[T1] Writing something on share memory.\n");
	memcpy(myshm, "Bella questa Share Memory!", 27);

	int ret = shmdt(myshm);
	if (ret == -1) {
		printf("[T1] Error: shmdt() failed!\n");

		return -1;
	}
	printf("[T1] Share memory detached\n");

	printf("[T2] Passing the control to Task 2.\n");
	spinlock_unlock(&shmspinlock1);

	return 0;
}

// Function used to test the shm.
int shm_test_2(void *args)
{
	(void)args;
	printf("[T2] I'm waiting that T1 finishes...\n");
	spinlock_lock(&shmspinlock1);
	printf("[T2] Now it's my turn!\n");

	int shmid = shmget(SHMKEY1, 128 * sizeof(char), 0777);
	if (shmid == -1) {
		printf("[T2] Error: shmget() failed!\n");

		return -1;
	}
	printf("[T2] I have got a share memory with ID %i.\n", shmid);

	char *myshm = shmat(shmid, NULL, 0);
	if (myshm == (void *)-1) {
		printf("[T2] Error: shmat() failed!\n");

		return -1;
	}
	printf("[T2] I attached the share memory in my virtual address. \n");
	printf("[T2]         SHM VIRTUAL ADDRESS %p\n", myshm);
	printf("[T2]        SHM PHYSICAL ADDRESS %p\n",
		   paging_virtual_to_physical(get_current_page_directory(), myshm));

	printf("[T2] I'm going to see what's inside this share memory...\n");
	printf("        << ");

	char *c = myshm;
	while (*c != '\0') {
		printf("%c", *c++);
	}
	printf(" >>\n");

	int ret = shmdt(myshm);
	if (ret == -1) {
		printf("[T2] Error: shmdt() failed!\n");

		return -1;
	}
	printf("[T2] Share memory detached\n");

	printf("[T2] Passing the control to my father\n");
	spinlock_unlock(&shmspinlock2);

	return 0;
}

void try_shm(int argc, char **argv)
{
	printf("Testing shm functions...\n");
	//    printf("[F] I am the father process.\n");
	//    printf("[F] Creating shm: shmget()\n");
	//
	//    size_t size = 128 *sizeof(char);
	//
	//    int shmid = shmget(SHMKEY1, size, IPC_CREAT | 0777);
	//    if (shmid == -1)
	//    {
	//        printf(
	//            "[F] Error: attempt to create a shared memory already created!\n");
	//
	//        return;
	//    }
	//    printf("[F] Share memory %i with ID %i and SIZE %i byte. \n", SHMKEY1,
	//           shmid, size);
	//
	//    spinlock_init(&shmspinlock1);
	//    spinlock_init(&shmspinlock2);
	//
	//    spinlock_lock(&shmspinlock1);
	//    spinlock_lock(&shmspinlock2);
	//
	//    int process1_id = execvp(shm_test_1,
	//                             "shm_test_1",
	//                             "shm_test_1");
	//    printf("[F] I have created Task 1 with pid: %d\n", process1_id);
	//    int process2_id = execvp(shm_test_2,
	//                             "shm_test_2",
	//                             "shm_test_2");
	//    printf("[F] I have created Task 2 with pid: %d\n", process2_id);
	//
	//    printf("[F] Now I have to wait child finished processing.\n");
	//    spinlock_lock(&shmspinlock2);
	//
	//    int ret = shmctl(shmid, IPC_RMID, NULL);
	//    if (ret == -1)
	//    {
	//        printf("[F] Error: shmctl() failed!\n");
	//        return;
	//    }
	//
	//    printf("[F] Share memory removed... Finished! :)\n");
}

void try_badshm()
{
	// Attempt to create a Shared Memory.
	size_t size = sizeof(int);
	mode_t mode = 0777;

	int shmid = shmget(SHMKEY2, size, IPC_CREAT | mode);
	if (shmid == -1) {
		printf("Error: attempt to create a shared memory already created!\n");

		return;
	}

	printf("I created a Shared Memory with: \n");
	printf(" -> KEY: %5i \n", SHMKEY2);
	printf(" -> ID: %5i \n", shmid);
	printf(" -> SIZE: %5i \n", size);
	printf(" -> FLAGS: %5o \n", IPC_CREAT | mode);

	char mode_str[100];
	strmode(mode, mode_str);
	printf(" -> PERMISSIONS: %5s \n", mode_str);
	printf("but I don't want to free it! \n");
	printf("Other process/functions can get this share memory. \n");
	printf("Try ipcs. \n");
}

int run_to_2(void *args)
{
	(void)args;
	for (int i = 1; i <= 2; i++) {
		sleep(i);
	}

	return 0;
}

int run_to_3(void *args)
{
	(void)args;
	(void)args;

	for (int i = 1; i <= 3; i++) {
		sleep(i);
	}

	return 0;
}

void try_scheduler()
{
	unsigned int start = timer_get_ticks();
	//    // Disable the IRQs.
	//    irq_disable();
	//    struct task_struct * process1 = kernel_create_process(run_to_2,
	//                                                 "run_to_2",
	//                                                 "run_to_2");
	//    printf("I have created Task 1 with pid: %d\n", process1->id);
	//    struct task_struct * process2 = kernel_create_process(run_to_3,
	//                                                 "run_to_3",
	//                                                 "run_to_3");
	//    printf("I have created Task 2 with pid: %d\n", process2->id);
	//    // Re-Enable the IRQs.
	//    irq_enable();
	//    wait(process1);
	//    wait(process2);
	unsigned int end = timer_get_ticks();
	printf("Total time of execution: %d\n", end - start);
}

// The maximum number of tests.
#define MAX_TEST 20

/// @brief Define testing functions.
struct {
	/// The name of the test.
	char cmd_testname[CMD_LEN];
	/// A description of the test.
	char cmd_description[DESC_LEN];

	/// A pointer to the function.
	void (*func)(int, char **);
} _testing_functions[MAX_TEST] = {
	{ "try_process", "Test multiple processes creation", try_process },
	{ "try_stress_heap", "Tries to stress the heap", try_stress_heap },
	{ "try_shm", "Test shared memory", try_shm },
	{ "try_badshm", "Test shared memory without free it", try_badshm },
	{ "try_scheduler", "Test the performance of different schduler",
	  try_scheduler }
};

void cmd_tester(int argc, char **argv)
{
	if (argc <= 1) {
		printf("Bad usage. Try '%s --help' for more info about the usage.\n",
			   argv[0]);

		return;
	}
	if (!strcmp(argv[1], "--help")) {
		printf("Testing functions.. ");
		video_set_color(RED);
		printf("Warning: for developers only!\n");
		video_set_color(GREY);
		for (size_t i = 0; i < MAX_TEST; i++) {
			if (_testing_functions[i].func == NULL) {
				break;
			}
			printf("    [%-2d] %-20s%-20s\n", i,
				   _testing_functions[i].cmd_testname,
				   _testing_functions[i].cmd_description);
		}
		video_set_color(WHITE);

		return;
	}
	int testid = atoi(argv[1]);
	for (size_t i = 0; i < MAX_TEST; i++) {
		if (testid != i) {
			continue;
		}
		if (_testing_functions[i].func == NULL) {
			break;
		}
		printf("Running test %d...\n", testid);
		++argv;
		++argv;
		(_testing_functions[i].func)(argc - 2, argv);
		printf("Done running test %d...\n", testid);

		return;
	}
	printf("Error: Test %s not found.\n", argv[1]);
	printf("       You have to provide the test id.\n");
}
