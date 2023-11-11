/**
 * File: pthread.c
 * Credit: based on RT embedded systems starter code
 * Author: Brad Waggle
 * Description: Assignment-4
 * Date: November 11, 2023
 */

#define _GNU_SOURCE
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <sched.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys_logger.h>


// Course attribtues
#define COURSE 1
#define ASSIGNMENT 4

// Thread attributes
#define NUM_THREADS 128
#define NUM_CPUS 4
#define SCHED_POLICY SCHED_FIFO

// Paramter structure to keep track of each threads' thread index
typedef struct
{
    int threadIdx;
} threadParams_t;


// POSIX thread declarations and scheduling attributes

pthread_t threads[NUM_THREADS + 1]; // holds attributes for all threads
pthread_t mainthread; // main thread
pthread_t startthread; // starter thread (starts NUM_THREADS threads)
threadParams_t threadParams[NUM_THREADS + 1]; // Array that holds thread params for all threads
int rt_max_prio, rt_min_prio; // min and max thread priority

pthread_attr_t fifo_sched_attr; // scheduling attributes for starter thread
pthread_attr_t rt_sched_attr[NUM_THREADS + 1]; // real time scheduling attributes
struct sched_param rt_param[NUM_THREADS + 1]; // real time scheduling paramters

pid_t mainpid; // Keep track of the main process
struct sched_param main_param; // Keep track of the main processes' parameters


/**
 * print_scheduler - Print the scheduling policy of the current process.
 *
 * This function retrieves and prints the scheduling policy of the current process.
 * It uses the `sched_getscheduler` function to determine the policy and displays it.
 */
void print_scheduler(void)
{
    int schedType = sched_getscheduler(getpid()); // Get the scheduling policy of the current process.

    // Check the scheduling policy and print the corresponding message.
    switch (schedType)
    {
        case SCHED_FIFO:
            printf("Pthread policy is SCHED_FIFO\n"); // Print message for SCHED_FIFO policy.
            break;
        case SCHED_OTHER:
            printf("Pthread policy is SCHED_OTHER\n"); // Print message for SCHED_OTHER policy.
            break;
        case SCHED_RR:
            printf("Pthread policy is SCHED_RR\n"); // Print message for SCHED_RR policy.
            break;
        default:
            printf("Pthread policy is UNKNOWN\n"); // Print a message for an unknown policy.
    }
}


/**
 * counterThread - Thread function that calculates the sum and logs it.
 *
 * This function calculates the sum of numbers from 1 to the thread's index,
 * constructs a syslog message, and logs it with specific course and assignment
 * identifiers.
 *
 * @param threadp: Pointer to thread-specific data (threadParams_t structure).
 *
 * @return: This function does not return a value.
 */
void *counterThread(void *threadp)
{
   int sum = 0, i; // Declare integer variables 'sum' and 'i'.
   threadParams_t *threadParams = (threadParams_t *)threadp; // Cast the input parameter to 'threadParams_t' structure.
   char msg[512]; // Declare a character array 'msg' for syslog message.

   // Calculate the sum of numbers from 1 to the thread index.
   for (i = 1; i < (threadParams->threadIdx) + 1; i++)
      sum = sum + i;

   // Construct a syslog message string with thread-specific information.
   sprintf(msg, "Thread idx=%d, sum[1...%d]=%d Running on core: %d",
            threadParams->threadIdx,
            threadParams->threadIdx,
            sum,
            sched_getcpu());

   // Log the message to the syslog with specified course and assignment identifiers.
   log_sys(msg, COURSE, ASSIGNMENT);
}


/**
 * starterThread - Entry point for creating and managing multiple threads.
 *
 * This function creates and manages multiple threads, each with its own
 * scheduling attributes, CPU core assignment, and priority settings.
 *
 * @param threadp: Pointer to thread-specific data (not used in this function).
 *
 * @return: Returns a pointer to the thread exit status (not used in this function).
 */
void *starterThread(void *threadp)
{
   int i, rc; // Declare integer variables 'i' and 'rc'.
   cpu_set_t threadcpu; // Declare a variable 'threadcpu' of type 'cpu_set_t'.
   int coreid; // Declare an integer variable 'coreid'.
   int numberOfProcessors; // Declare an integer variable 'numberOfProcessors'.

   // Get the number of processors configured in the system.
   numberOfProcessors = get_nprocs_conf();

   // Print the CPU on which the starter thread is running.
   printf("starter thread running on CPU=%d\n", sched_getcpu());

   for (i = 0; i < NUM_THREADS; i++) // Loop from 0 to the number of threads (NUM_THREADS).
   {
       CPU_ZERO(&threadcpu); // Initialize the 'threadcpu' CPU set to empty.

       coreid = 3; // Set core affinity to 3.

       // Print the core assignment for the current thread.
       printf("Setting thread %d to core %d\n", i, coreid);

       CPU_SET(coreid, &threadcpu); // Add the core to the 'threadcpu' CPU set.

       // Initialize and configure thread attributes.
       rc = pthread_attr_init(&rt_sched_attr[i]); // Initialize thread attribute.
       rc = pthread_attr_setinheritsched(&rt_sched_attr[i], PTHREAD_EXPLICIT_SCHED); // Set scheduling inheritance.
       rc = pthread_attr_setschedpolicy(&rt_sched_attr[i], SCHED_POLICY); // Set scheduling policy.
       rc = pthread_attr_setaffinity_np(&rt_sched_attr[i], sizeof(cpu_set_t), &threadcpu); // Set thread affinity.

       rt_param[i].sched_priority = rt_max_prio - i - 1; // Set thread priority.
       pthread_attr_setschedparam(&rt_sched_attr[i], &rt_param[i]); // Set thread scheduling parameters.

       threadParams[i].threadIdx = i; // Set the thread index in threadParams.

       // Create a new thread with specified attributes and the 'counterThread' function.
       pthread_create(&threads[i], &rt_sched_attr[i], counterThread, (void *)&(threadParams[i]));
   }

   for (i = 0; i < NUM_THREADS; i++) // Loop to wait for all threads to finish.
       pthread_join(threads[i], NULL);
}


int main(int argc, char *argv[])
{
   int rc; // Holds error return code
   int i, j; // Counters 'i' and 'j'
   int numberOfProcessors; // number of processors in the system

   // Clear the syslog before starting
   clear_syslog();

   // Log machine info to the syslog
   log_uname(COURSE, ASSIGNMENT);

   // Get the number of processors configured in the system.
   numberOfProcessors = get_nprocs_conf();

   // Print the number of processors and the number of available processors.
   printf("This system has %d processors with %d available\n", numberOfProcessors, get_nprocs());
   printf("The worker thread created will be run on a specific core based on thread index\n");

   // Obtain the maximum and minimum real-time priority values for the scheduling policy.
   rt_max_prio = sched_get_priority_max(SCHED_POLICY); // typically 99
   rt_min_prio = sched_get_priority_min(SCHED_POLICY); // typically 1

   // Get the process ID of the main thread.
   mainpid = getpid();

   // Get scheduling parameters for the main thread.
   rc = sched_getparam(mainpid, &main_param);

   // Set the scheduling priority of the main thread to the maximum real-time priority.
   main_param.sched_priority = rt_max_prio;

   // Print the CPU on which the main thread is running.
   printf("The main thread is running on CPU=%d\n", sched_getcpu());

   // Check if the scheduling policy is not SCHED_OTHER.
   if (SCHED_POLICY != SCHED_OTHER)
   {
       // Set the scheduling policy and parameters for the current process.
       if (rc = sched_setscheduler(getpid(), SCHED_POLICY, &main_param) < 0)
           perror("******** WARNING: sched_setscheduler");
   }

   // Print the scheduling information.
   print_scheduler();
   printf("rt_max_prio=%d\n", rt_max_prio);
   printf("rt_min_prio=%d\n", rt_min_prio);

   // Create a new thread with specific attributes and the 'starterThread' function.
   pthread_create(&startthread, &fifo_sched_attr, starterThread, (void *)0);

   // Wait for the newly created thread to finish.
   pthread_join(startthread, NULL);

   // Copy the updated syslog to the current project directory.
   copy_syslog(COURSE, ASSIGNMENT);

   printf("TEST COMPLETE\n");
}

