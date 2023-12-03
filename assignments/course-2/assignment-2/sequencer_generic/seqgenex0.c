//
// Sequencer Generic to emulate Example 0 assuming millisec time resolution
//
// Service_1, S1, T1=2,  C1=1, D=T
// Service_2, S2, T2=10, C2=1, D=T
// Service_3, S3, T3=15, C3=2, D=T
//
// Sequencer - 100 Hz [gives semaphores to all other services]
// Service_1 - 50 Hz, every other Sequencer loop
// Service_2 - 10 Hz, every 10th Sequencer loop 
// Service_3 - 6.67 Hz, every 15th Sequencer loop
//
// With the above, priorities by RM policy would be:
//
// Sequencer = RT_MAX	@ 100 Hz, T= 1
// Servcie_1 = RT_MAX-1	@ 50 Hz,  T= 2
// Service_2 = RT_MAX-2	@ 10 Hz,  T=10
// Service_3 = RT_MAX-3	@ 6.67 Hz T=15 
//
// Here are a few hardware/platform configuration settings
// that you should also check before running this code:
//
// 1) Check to ensure all your CPU cores on in an online state.
//
// 2) Check /sys/devices/system/cpu or do lscpu.
//
//    echo 1 > /sys/devices/system/cpu/cpu1/online
//    echo 1 > /sys/devices/system/cpu/cpu2/online
//    echo 1 > /sys/devices/system/cpu/cpu3/online
//
// 3) Check for precision time resolution and support with cat /proc/timer_list
//
// 4) Ideally all printf calls should be eliminated as they can interfere with
//    timing.  They should be replaced with an in-memory event logger or at
//    least calls to syslog.
//

// This is necessary for CPU affinity macros in Linux
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <semaphore.h>

#include <syslog.h>
#include <sys/time.h>
#include <errno.h>
#include "seqgen.h"
#include <sys/sysinfo.h>
#include <sys_logger.h>

// Course attribtues
#define COURSE 2        // course number
#define ASSIGNMENT 2   // assignment number

#define ABS_DELAY  // Flag for absolute delay
#define DRIFT_CONTROL // Flag for drift control
#define NUM_THREADS (3+1) // Number of threads

int abortTest=FALSE; // When true, aborts Service
int abortS1=FALSE, abortS2=FALSE, abortS3=FALSE; // When true, aborts Service
sem_t semS1, semS2, semS3; // Service semaphores
static double start_time = 0; // Start time

pthread_t threads[NUM_THREADS]; // Thread array
pthread_attr_t rt_sched_attr[NUM_THREADS]; // Thread scheduling attributes
pthread_attr_t main_attr; // Main thread attributes
int rt_max_prio, rt_min_prio; // Max in Min priority
struct sched_param rt_param[NUM_THREADS]; // Store scheduling parameters
threadParams_t threadParams[NUM_THREADS]; // Store thread parameters


void main(void)
{
    double current_time; // Variable to hold current time
    struct timespec rt_res, monotonic_res; // Structs to hold resolution of different clocks
    int i, rc, cpuidx; // Integer variables for loop, return codes, CPU index
    cpu_set_t threadcpu; // CPU set for thread affinity
    struct sched_param main_param; // Scheduler parameters for main thread
    pid_t mainpid; // Process ID of the main thread

    clear_syslog(); // Clear the system log
    log_uname(COURSE, ASSIGNMENT); // Log machine information with course and assignment details

    start_time = getTimeMsec(); // Record the start time in milliseconds

    usleep(1000000); // Delay the start for a second

    printf("Starting High Rate Sequencer Example\n"); // Print a message indicating the start of the sequencer example
    get_cpu_core_config(); // Get CPU core configuration details

    clock_getres(CLOCK_REALTIME, &rt_res); // Get resolution of the real-time clock and store it
    printf("RT clock resolution is %d sec, %d nsec\n", rt_res.tv_sec, rt_res.tv_nsec); // Print the resolution of the real-time clock

    printf("System has %d processors configured and %d available.\n", get_nprocs_conf(), get_nprocs()); // Print system processor configuration details

    // Initialize sequencer semaphores
    if (sem_init(&semS1, 0, 0)) { printf("Failed to initialize S1 semaphore\n"); exit(-1); }
    if (sem_init(&semS2, 0, 0)) { printf("Failed to initialize S2 semaphore\n"); exit(-1); }
    if (sem_init(&semS3, 0, 0)) { printf("Failed to initialize S3 semaphore\n"); exit(-1); }

    mainpid = getpid(); // Get the process ID of the main thread

    rt_max_prio = sched_get_priority_max(SCHED_FIFO); // Get maximum priority for FIFO scheduling
    rt_min_prio = sched_get_priority_min(SCHED_FIFO); // Get minimum priority for FIFO scheduling

    rc = sched_getparam(mainpid, &main_param); // Get scheduling parameters for the main thread
    main_param.sched_priority = rt_max_prio; // Set main thread priority to the maximum priority
    rc = sched_setscheduler(getpid(), SCHED_FIFO, &main_param); // Set scheduling policy and parameters for the main thread
    if (rc < 0) perror("main_param"); // Print an error message if setting scheduler parameters fails

    print_scheduler(); // Print scheduler information

    printf("rt_max_prio=%d\n", rt_max_prio); // Print maximum priority
    printf("rt_min_prio=%d\n", rt_min_prio); // Print minimum priority

    // Set up attributes and parameters for multiple threads
    for (i = 0; i < NUM_THREADS; i++) {
        CPU_ZERO(&threadcpu); // Clear the CPU set
        cpuidx = (3); // Set CPU index
        CPU_SET(cpuidx, &threadcpu); // Set CPU affinity for the thread

        rc = pthread_attr_init(&rt_sched_attr[i]); // Initialize thread attributes
        rc = pthread_attr_setinheritsched(&rt_sched_attr[i], PTHREAD_EXPLICIT_SCHED); // Set explicit scheduling inheritance
        rc = pthread_attr_setschedpolicy(&rt_sched_attr[i], SCHED_FIFO); // Set scheduling policy
        rc = pthread_attr_setaffinity_np(&rt_sched_attr[i], sizeof(cpu_set_t), &threadcpu); // Set thread affinity

        rt_param[i].sched_priority = rt_max_prio - i; // Set thread priorities
        pthread_attr_setschedparam(&rt_sched_attr[i], &rt_param[i]); // Set thread scheduling parameters

        threadParams[i].threadIdx = i; // Set thread index
    }

    printf("Service threads will run on %d CPU cores\n", CPU_COUNT(&threadcpu)); // Print the number of CPU cores threads will run on

    current_time = getTimeMsec(); // Get current time in milliseconds

    // Create service threads with different priorities and frequencies
    // Service_1 = RT_MAX-1 @ 50 Hz
    rt_param[1].sched_priority = rt_max_prio - 1; // Set priority for Service_1
    pthread_attr_setschedparam(&rt_sched_attr[1], &rt_param[1]); // Set scheduling parameters for thread 1
    rc = pthread_create(&threads[1], &rt_sched_attr[1], Service_1, (void *)&(threadParams[1])); // Create thread for Service_1
    if (rc < 0) perror("pthread_create for service 1"); // Check for thread creation errors for Service_1
    else printf("pthread_create successful for service 1\n"); // Print success message if thread creation for Service_1 is successful

    // Service_2 = RT_MAX-2 @ 10 Hz
    rt_param[2].sched_priority = rt_max_prio - 2; // Set priority for Service_2
    pthread_attr_setschedparam(&rt_sched_attr[2], &rt_param[2]); // Set scheduling parameters for thread 2
    rc = pthread_create(&threads[2], &rt_sched_attr[2], Service_2, (void *)&(threadParams[2])); // Create thread for Service_2
    if (rc < 0) perror("pthread_create for service 2"); // Check for thread creation errors for Service_2
    else printf("pthread_create successful for service 2\n"); // Print success message if thread creation for Service_2 is successful

    // Service_3 = RT_MAX-3 @ 6.67 Hz
    rt_param[3].sched_priority = rt_max_prio - 3; // Set priority for Service_3
    pthread_attr_setschedparam(&rt_sched_attr[3], &rt_param[3]); // Set scheduling parameters for thread 3
    rc = pthread_create(&threads[3], &rt_sched_attr[3], Service_3, (void *)&(threadParams[3])); // Create thread for Service_3
    if (rc < 0) perror("pthread_create for service 3"); // Check for thread creation errors for Service_3
    else printf("pthread_create successful for service 3\n"); // Print success message if thread creation for Service_3 is successful

    // Create Sequencer thread with the highest priority and frequency
    printf("Start sequencer\n"); // Print message indicating the start of the sequencer
    threadParams[0].sequencePeriods = RTSEQ_PERIODS; // Set sequence periods for the sequencer

    // Sequencer = RT_MAX @ 1000 Hz
    rt_param[0].sched_priority = rt_max_prio; // Set priority for the sequencer
    pthread_attr_setschedparam(&rt_sched_attr[0], &rt_param[0]); // Set scheduling parameters for the sequencer
    rc = pthread_create(&threads[0], &rt_sched_attr[0], Sequencer, (void *)&(threadParams[0])); // Create thread for the sequencer
    if (rc < 0) perror("pthread_create for sequencer service 0"); // Check for thread creation errors for the sequencer
    else printf("pthread_create successful for sequencer service 0\n"); // Print success message if thread creation for the sequencer is successful

    // Wait for all threads to complete
    for (i = 0; i < NUM_THREADS; i++)
        pthread_join(threads[i], NULL); // Wait for each thread to complete before proceeding

    // Copy the updated syslog to the current project directory
    copy_syslog(COURSE, ASSIGNMENT); // Copy syslog to the current project directory

    printf("\nTEST COMPLETE\n"); // Print message indicating completion of the test

}

void *Sequencer(void *threadp) {
    // Declare and initialize a structure for delaying time
    struct timespec delay_time = {0, RTSEQ_DELAY_NSEC};
    // Declare and initialize a structure for standard delaying time
    struct timespec std_delay_time = {0, RTSEQ_DELAY_NSEC};
    // Declare and initialize a structure for the current time value
    struct timespec current_time_val = {0, 0};

    // Declare structures and variables for time
    struct timespec remaining_time;
    double current_time, last_time, scaleDelay;
    double delta_t = (RTSEQ_DELAY_NSEC / (double)NANOSEC_PER_SEC);
    double scale_dt;
    int rc, delay_cnt = 0;
    unsigned long long seqCnt = 0;
    // Declare a pointer to threadParams_t and assign the input threadp to it
    threadParams_t *threadParams = (threadParams_t *)threadp;
    // Declare a character array 'msg' for syslog message with a size of 512
    char msg[512];

    // Get the current time in milliseconds and assign it to current_time
    current_time = getTimeMsec();
    // Set last_time to the current_time minus delta_t
    last_time = current_time - delta_t;

    // syslog(LOG_CRIT, "RTSEQ: start on cpu=%d @ sec=%lf after %lf with dt=%lf\n", sched_getcpu(), current_time, last_time, delta_t);

    // Start a do-while loop
    do {
        // Update the current time with the current time in milliseconds
        current_time = getTimeMsec();
        // Reset delay count
        delay_cnt = 0;

        // Check for DRIFT_CONTROL flag to determine delay_time
#ifdef DRIFT_CONTROL
        // Calculate scale_dt based on time differences
        scale_dt = (current_time - last_time) - delta_t;
        // Adjust delay_time based on scale_dt, uncertainties, and CLOCK_BIAS_NANOSEC
        delay_time.tv_nsec = std_delay_time.tv_nsec - (scale_dt * (NANOSEC_PER_SEC + DT_SCALING_UNCERTAINTY_NANOSEC)) - CLOCK_BIAS_NANOSEC;
        //syslog(LOG_CRIT, "RTSEQ: scale dt=%lf @ sec=%lf after=%lf with dt=%lf\n", scale_dt, current_time, last_time, delta_t);
#else
        // If not using DRIFT_CONTROL, set delay_time and scale_dt to default values
        delay_time = std_delay_time;
        scale_dt = delta_t;
#endif

        // Check for ABS_DELAY flag to conditionally update delay_time
#ifdef ABS_DELAY
        // Get the current real-time clock value and add it to delay_time
        clock_gettime(CLOCK_REALTIME, &current_time_val);
        delay_time.tv_sec = current_time_val.tv_sec;
        delay_time.tv_nsec = current_time_val.tv_nsec + delay_time.tv_nsec;

        // Adjust delay_time if the nanosecond part exceeds a second
        if (delay_time.tv_nsec > NANOSEC_PER_SEC) {
            delay_time.tv_sec = delay_time.tv_sec + 1;
            delay_time.tv_nsec = delay_time.tv_nsec - NANOSEC_PER_SEC;
        }
        //syslog(LOG_CRIT, "RTSEQ: cycle %08llu delay for dt=%lf @ sec=%d, nsec=%d to sec=%d, nsec=%d\n", seqCnt, scale_dt, current_time_val.tv_sec, current_time_val.tv_nsec, delay_time.tv_sec, delay_time.tv_nsec);
#endif

        // Start a delay loop with checks for early wake-up
        do {
#ifdef ABS_DELAY
            // Sleep until the specified absolute time with absolute delay
            rc = clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &delay_time, (struct timespec *)0);
#else
            // Sleep until the specified time with relative delay
            rc = clock_nanosleep(CLOCK_REALTIME, 0, &delay_time, &remaining_time);
#endif

            // Check if interrupted by a signal
            if (rc == EINTR) {
                syslog(LOG_CRIT, "RTSEQ: EINTR @ sec=%lf\n", current_time);
                delay_cnt++;
            }
            // Check for other errors during sleep
            else if (rc < 0) {
                perror("RTSEQ: nanosleep");
                exit(-1);
            }

            //syslog(LOG_CRIT, "RTSEQ: WOKE UP\n");
        } while (rc == EINTR);

        // syslog(LOG_CRIT, "RTSEQ: cycle %08llu @ sec=%lf, last=%lf, dt=%lf, sdt=%lf\n", seqCnt, current_time, last_time, (current_time-last_time), scale_dt);

        // Release services at specific rates based on the sequence count

        // Service_1 = RT_MAX-1 @ 50 Hz
        if ((seqCnt % 2) == 0) {
            // Construct a message string for syslog with thread-specific information
            sprintf(msg, "Thread 1 start %d @ %lf on core %d \n",
                    seqCnt + 1,
                    current_time,
                    sched_getcpu());
            // Log the message to syslog with specified course and assignment identifiers
            log_sys(msg, COURSE, ASSIGNMENT);
            // Post the semaphore for Service_1
            sem_post(&semS1);
        }
        // Service_2 = RT_MAX-2 @ 10 Hz
        if ((seqCnt % 5) == 0) {
            // Construct a message string for syslog with thread-specific information
            sprintf(msg, "Thread 2 start %d @ %lf on core %d \n",
                    seqCnt + 1,
                    current_time,
                    sched_getcpu());
            // Log the message to syslog with specified course and assignment identifiers
            log_sys(msg, COURSE, ASSIGNMENT);
            // Post the semaphore for Service_2
            sem_post(&semS2);
        }
        // Service_3 = RT_MAX-3 @ 6.67 Hz
        if ((seqCnt % 15) == 0) {
            // Construct a message string for syslog with thread-specific information
            sprintf(msg, "Thread 3 start %d @ %lf on core %d \n",
                    seqCnt + 1,
                    current_time,
                    sched_getcpu());
            // Log the message to syslog with specified course and assignment identifiers
            log_sys(msg, COURSE, ASSIGNMENT);
            // Post the semaphore for Service_3
            sem_post(&semS3);
        }

        // Increment sequence count and update last_time
        seqCnt++;
        last_time = current_time;

    } while (!abortTest && (seqCnt < threadParams->sequencePeriods));

    // Post semaphores and set abort flags before exiting the thread
    sem_post(&semS1);
    sem_post(&semS2);
    sem_post(&semS3);
    abortS1 = TRUE;
    abortS2 = TRUE;
    abortS3 = TRUE;

    // Exit the thread
    pthread_exit((void *)0);
}

void *Service_1(void *threadp)
{
    // Store current time 
    double current_time;

    // Initialize a counter for Service 1
    unsigned long long S1Cnt = 0;

     // Casts the input thread parameter to a specific structure type
    threadParams_t *threadParams = (threadParams_t *)threadp;

    // Continuously execute the following block until abort
    while (!abortS1)
    {
        // Wait for a semaphore signal to proceed
        sem_wait(&semS1);

        // Increment the counter for Service 1
        S1Cnt++;

        // Get the current time in milliseconds
        current_time = getTimeMsec();

        // syslog(LOG_CRIT, "S1: release %llu @ sec=%lf\n", S1Cnt, current_time);
    }

    // Exit the thread with a return value of 0
    pthread_exit((void *)0);
}

void *Service_2(void *threadp)
{
    // Store current time 
    double current_time;

    // Initialize a counter for Service 1
    unsigned long long S2Cnt = 0;

    // Casts the input thread parameter to a specific structure type
    threadParams_t *threadParams = (threadParams_t *)threadp;

    // Continuously execute the following block until abort
    while (!abortS2)
    {
        // Wait for a semaphore signal to proceed
        sem_wait(&semS2);

        // Increment the counter for Service 2
        S2Cnt++;

        // Get the current time in milliseconds
        current_time = getTimeMsec();

        // syslog(LOG_CRIT, "S2: release %llu @ sec=%lf\n", S2Cnt, current_time);
    }

    // Exits the thread with a return value of 0
    pthread_exit((void *)0);
}


void *Service_3(void *threadp)
{
    // Store current time 
    double current_time;

    // Initialize a counter for Service 2
    unsigned long long S3Cnt=0;

    // Casts the input thread parameter to a specific structure type
    threadParams_t *threadParams = (threadParams_t *)threadp;

    // Continuously execute the following block until abort 
    while(!abortS3)
    {
        // Wait for a semaphore signal to proceed
        sem_wait(&semS3);

        // Increment the counter for Service 3
        S3Cnt++;

        // Get the current time in milliseconds
        current_time=getTimeMsec();
        
        // syslog(LOG_CRIT, "S3: release %llu @ sec=%lf\n", S3Cnt, current_time);
    }

    // Exit the thread with a return value of 0
    pthread_exit((void *)0);
}


// Function to calculate time in milliseconds
// global start_time must be set on first call
double getTimeMsec(void)
{
  // Structure to hold time information
  struct timespec event_ts = {0, 0};
  // Variable to store the calculated time in milliseconds
  double event_time = 0;

  // Obtaining the current time using CLOCK_REALTIME
  clock_gettime(CLOCK_REALTIME, &event_ts);
  
  // Calculating the time in seconds with nanoseconds converted to seconds
  event_time = ((event_ts.tv_sec) + ((event_ts.tv_nsec) / (double)NANOSEC_PER_SEC));
  
  // Returning the time difference between the current time and the start time
  return (event_time - start_time);
}



void print_scheduler(void)
{
   int schedType, scope; // Schedule type and scope

   schedType = sched_getscheduler(getpid()); // Get scheduling policy of the current process

   switch(schedType) // Scheduling policy
   {
       case SCHED_FIFO: // If scheduling policy is FIFO
           printf("Pthread Policy is SCHED_FIFO\n");
           break;
       case SCHED_OTHER: // If scheduling policy is OTHER
           printf("Pthread Policy is SCHED_OTHER\n"); 
           exit(-1); 
           break;
       case SCHED_RR: // If scheduling policy is Round Robin
           printf("Pthread Policy is SCHED_RR\n"); 
           exit(-1);
           break;
       default: // If scheduling policy is unknown
           printf("Pthread Policy is UNKNOWN\n"); 
           exit(-1); 
   }

   pthread_attr_getscope(&main_attr, &scope); // Get the scope of the main attribute

   if(scope == PTHREAD_SCOPE_SYSTEM)
       printf("PTHREAD SCOPE SYSTEM\n"); 
   else if (scope == PTHREAD_SCOPE_PROCESS) 
       printf("PTHREAD SCOPE PROCESS\n"); 
   else 
       printf("PTHREAD SCOPE UNKNOWN\n"); 
}



void get_cpu_core_config(void)
{
   cpu_set_t cpuset; // Declare a CPU set
   pthread_t callingThread; // Store the ID of the calling thread
   int rc, idx; // Return codes and indexing

   CPU_ZERO(&cpuset); // Initialize the CPU set to be empty

   callingThread = pthread_self(); // Get the ID of the current thread

   rc = pthread_getaffinity_np(callingThread, sizeof(cpu_set_t), &cpuset); // Get the CPU affinity mask of the current thread
   if (rc != 0)
       perror("pthread_getaffinity_np"); // Print an error message if getting CPU affinity fails
   else
   {
       printf("thread running on CPU=%d, CPUs =", sched_getcpu()); // Print the CPU on which the thread is running

       for (idx = 0; idx < CPU_SETSIZE; idx++) // Iterate through the CPUs in the set
           if (CPU_ISSET(idx, &cpuset)) // Check if CPU 'idx' is present in 'cpuset'
               printf(" %d", idx); // Print the CPU number if it's in the set

       printf("\n");
   }

   printf("Using CPUS=%d from total available.\n", CPU_COUNT(&cpuset)); // Print the count of CPUs in 'cpuset'
}


