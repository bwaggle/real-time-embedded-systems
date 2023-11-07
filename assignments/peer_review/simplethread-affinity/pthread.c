/**
 * File: pthread.c
 * Credit: based on RT embedded systems starter code
 * Author: Brad Waggle
 * Description: Commented for Peer Review walk through
 * Date: November 4, 2023
 */
#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sched.h>

// Define the number of threads to be created
#define NUM_THREADS 64

// Define the number of CPUs available for scheduling
#define NUM_CPUS 8

// Define a structure to hold per-thread parameters, specifically an integer for the thread index
typedef struct
{
    int threadIdx;  // An integer to store the thread index
} threadParams_t;


pthread_t threads[NUM_THREADS]; // Holds thread IDs for multiple threads
pthread_t mainthread; // Holds ID for the main thread
pthread_t startthread; // Holds thread ID for a starting thread
threadParams_t threadParams[NUM_THREADS]; // Holds thread parameters, one for each thread
pthread_attr_t fifo_sched_attr; // Attributes for FIFO scheduling policy
pthread_attr_t orig_sched_attr; // Original scheduling attributes
struct sched_param fifo_param; // Scheduling parameters for FIFO scheduling policy

// Define the scheduling policy as SCHED_FIFO (FIFO scheduling)
#define SCHED_POLICY SCHED_FIFO

// Constant representing the maximum number of iterations
#define MAX_ITERATIONS (1000000)



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
 * Set the scheduler policy to SCHED_FIFO and configure it for a specific CPU core.
 * This function sets the scheduler policy, CPU affinity, and priority for the current process.
 */
void set_scheduler(void)
{
    int max_prio, scope, rc, cpuidx;
    cpu_set_t cpuset;

    // Print the initial scheduler configuration
    printf("INITIAL "); print_scheduler();

    // Initialize thread attributes for SCHED_FIFO scheduling policy
    pthread_attr_init(&fifo_sched_attr);
    
    // Set the scheduling policy to SCHED_FIFO
    pthread_attr_setinheritsched(&fifo_sched_attr, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&fifo_sched_attr, SCHED_POLICY);

    // Zero out the CPU core mask
    CPU_ZERO(&cpuset);

    // Set the CPU core to run on core 3
    cpuidx = 3;
    CPU_SET(cpuidx, &cpuset);
    
    // Set the CPU affinity for the thread attributes
    pthread_attr_setaffinity_np(&fifo_sched_attr, sizeof(cpu_set_t), &cpuset);

    // Get the maximum priority for the specified scheduling policy
    max_prio = sched_get_priority_max(SCHED_POLICY);
    
    // Set the thread priority to the maximum
    fifo_param.sched_priority = max_prio;

    // Set the scheduler policy of the main process thread to SCHED_FIFO
    if ((rc = sched_setscheduler(getpid(), SCHED_POLICY, &fifo_param)) < 0)
        perror("sched_setscheduler");

    // Set the scheduling parameters for the thread attributes
    pthread_attr_setschedparam(&fifo_sched_attr, &fifo_param);

    // Print the adjusted scheduler configuration
    printf("ADJUSTED "); print_scheduler();
}


// Function: counterThread
// Description: Calculates the sum of integers from 1 to threadParams->threadIdx
//              over MAX_ITERATIONS iterations and measures the execution time.
// Parameters:
//   - threadp: A pointer to threadParams_t structure containing thread-specific parameters.
// Returns: None (void)
void *counterThread(void *threadp)
{
    int sum = 0, i, iterations;
    threadParams_t *threadParams = (threadParams_t *)threadp;
    pthread_t mythread;
    double start = 0.0, stop = 0.0;
    struct timeval startTime, stopTime;

    // Record the start time using gettimeofday and convert it to seconds.
    gettimeofday(&startTime, 0);
    start = ((startTime.tv_sec * 1000000.0) + startTime.tv_usec) / 1000000.0;

    // Perform the summation over MAX_ITERATIONS iterations.
    for (iterations = 0; iterations < MAX_ITERATIONS; iterations++) {
        sum = 0;
        for (i = 1; i < (threadParams->threadIdx) + 1; i++)
            sum = sum + i;
    }

    // Record the stop time using gettimeofday and convert it to seconds.
    gettimeofday(&stopTime, 0);
    stop = ((stopTime.tv_sec * 1000000.0) + stopTime.tv_usec) / 1000000.0;

    // Print thread-specific information, including the thread index, the calculated sum, the CPU it's running on,
    // and the start and stop times.
    printf("\nThread idx=%d, sum[0...%d]=%d, running on CPU=%d, start=%lf, stop=%lf",
           threadParams->threadIdx,
           threadParams->threadIdx, sum, sched_getcpu(),
           start, stop);
}


/**
 * Function: starterThread
 *
 * This function is a starting point for a multi-threaded program.
 * It creates and manages multiple threads using the POSIX threads library.
 *
 * @param threadp: A pointer to the thread parameter data.
 *
 * @return: None (void function).
 */
void *starterThread(void *threadp)
{
    int i, rc;

    // Print the current CPU on which the starter thread is running.
    printf("starter thread running on CPU=%d\n", sched_getcpu());

    // Create multiple threads using a loop.
    for (i = 0; i < NUM_THREADS; i++)
    {
        // Set thread index for thread parameters.
        threadParams[i].threadIdx = i;

        pthread_create(&threads[i], // pointer to the thread descriptor
                       &fifo_sched_attr, // use SCHED_POLICY attributes (i.e. SCHED_FIFO)
                       counterThread, // thread function entry point
                       (void *)&(threadParams[i])); // parameters to pass into the thread
    }

    // Wait for all created threads to complete before exiting.
    for (i = 0; i < NUM_THREADS; i++)
        pthread_join(threads[i], NULL);
}

int main(int argc, char *argv[]) {
    int rc;
    int i, j;
    cpu_set_t cpuset;
    pthread_t mainthread, startthread;
    
    set_scheduler(); // Set scheduler policy and priority

    CPU_ZERO(&cpuset); // Initialize the CPU affinity mask

    // Get the identifier for the main thread
    mainthread = pthread_self();

    // Check the affinity mask assigned to the thread
    rc = pthread_getaffinity_np(mainthread, sizeof(cpu_set_t), &cpuset);
    if (rc != 0)
        perror("pthread_getaffinity_np");
    else {
        printf("main thread running on CPU=%d, CPUs =", sched_getcpu());

        // Print the CPUs in the affinity mask
        for (j = 0; j < CPU_SETSIZE; j++)
            if (CPU_ISSET(j, &cpuset))
                printf(" %d", j);

        printf("\n");
    }

    // Create a new thread with FIFO real-time scheduling attributes
    // The start thread spins off a number of additional worker threads
    pthread_create(&startthread, &fifo_sched_attr, starterThread, (void *)0);

    // Wait for the start thread to complete
    pthread_join(startthread, NULL);

    printf("\nTEST COMPLETE\n");
    return 0;
}
