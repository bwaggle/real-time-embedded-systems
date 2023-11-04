/****************************************************************************/
/* Function: nanosleep and POSIX 1003.1b RT clock demonstration             */
/*                                                                          */
/* Sam Siewert - 02/05/2011                                                 */
/*                                                                          */
/****************************************************************************/

#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>

#define NSEC_PER_SEC (1000000000)  // Number of nanoseconds in a second
#define NSEC_PER_MSEC (1000000)    // Number of nanoseconds in a millisecond
#define NSEC_PER_USEC (1000)       // Number of nanoseconds in a microsecond
#define ERROR (-1)                 // A constant representing an error condition
#define OK (0)                     // A constant representing a successful or non-error condition
#define TEST_SECONDS (0)           // The number of seconds for a test (initially set to 0)
#define TEST_NANOSECONDS (NSEC_PER_MSEC * 10)  // The number of nanoseconds for a test (initially set to 10 milliseconds)

void end_delay_test(void);  // Function declaration for 'end_delay_test'

// Static variables for working with time and sleep intervals
static struct timespec sleep_time = {0, 0};           // Represents the time interval to sleep
static struct timespec sleep_requested = {0, 0};      // Represents the requested sleep time
static struct timespec remaining_time = {0, 0};        // Represents the remaining time after a sleep operation

static unsigned int sleep_count = 0;  // Counter to keep track of the number of sleep operations

// Thread-related variables
pthread_t main_thread;            // Represents the main thread
pthread_attr_t main_sched_attr;   // Thread attribute structure for the main thread
int rt_max_prio, rt_min_prio, min; // Variables related to real-time scheduling priorities
struct sched_param main_param;    // Scheduling parameters for the main thread



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
 * Calculate the time difference between two timespec structures in seconds.
 *
 * This function takes two pointers to timespec structures, `fstart` and `fstop`, and
 * calculates the time difference in seconds between them.
 *
 * @param fstart Pointer to the starting timespec structure.
 * @param fstop Pointer to the ending timespec structure.
 *
 * @return The time difference in seconds (double precision).
 */
double d_ftime(struct timespec *fstart, struct timespec *fstop)
{
  // Convert the starting timespec to a double in seconds.
  double dfstart = ((double)(fstart->tv_sec) + ((double)(fstart->tv_nsec) / 1000000000.0));
  
  // Convert the ending timespec to a double in seconds.
  double dfstop = ((double)(fstop->tv_sec) + ((double)(fstop->tv_nsec) / 1000000000.0));

  // Calculate and return the time difference in seconds.
  return (dfstop - dfstart);
}


/**
 * Calculate the time difference (delta_t) between two timespec structures 'start' and 'stop'.
 *
 * @param stop     Pointer to a 'struct timespec' representing the end time.
 * @param start    Pointer to a 'struct timespec' representing the start time.
 * @param delta_t  Pointer to a 'struct timespec' where the time difference will be stored.
 *
 * @return OK (0) if the calculation is successful; ERROR (-1) if an error occurs (e.g., if 'stop' is earlier than 'start').
 */
int delta_t(struct timespec *stop, struct timespec *start, struct timespec *delta_t)
{
  int dt_sec = stop->tv_sec - start->tv_sec;    // Calculate the difference in seconds between 'stop' and 'start'
  int dt_nsec = stop->tv_nsec - start->tv_nsec;  // Calculate the difference in nanoseconds between 'stop' and 'start

  // Case 1 - Less than a second of change
  if (dt_sec == 0)
  {
    // printf("dt less than 1 second\n"); 

    if (dt_nsec >= 0 && dt_nsec < NSEC_PER_SEC)
    {
      // printf("nanosec greater at stop than start\n"); 
      delta_t->tv_sec = 0;
      delta_t->tv_nsec = dt_nsec;
    }
    else if (dt_nsec > NSEC_PER_SEC)
    {
      // printf("nanosec overflow\n"); 
      delta_t->tv_sec = 1;
      delta_t->tv_nsec = dt_nsec - NSEC_PER_SEC;
    }
    else
    {
      printf("stop is earlier than start\n");  // Print an error message when 'stop' is earlier than 'start'
      return ERROR;  // Return an error code
    }
  }

  // Case 2 - More than a second of change, check for roll-over
  else if (dt_sec > 0)
  {
    // printf("dt more than 1 second\n"); 

    if (dt_nsec >= 0 && dt_nsec < NSEC_PER_SEC)
    {
      // printf("nanosec greater at stop than start\n"); 
      delta_t->tv_sec = dt_sec;
      delta_t->tv_nsec = dt_nsec;
    }
    else if (dt_nsec > NSEC_PER_SEC)
    {
      // printf("nanosec overflow\n"); 
      delta_t->tv_sec = delta_t->tv_sec + 1;
      delta_t->tv_nsec = dt_nsec - NSEC_PER_SEC;
    }
    else
    {
      // printf("nanosec roll over\n"); 
      delta_t->tv_sec = dt_sec - 1;
      delta_t->tv_nsec = NSEC_PER_SEC + dt_nsec;
    }
  }

  return OK;  // Return a success code
}


static struct timespec rtclk_dt = {0, 0};
static struct timespec rtclk_start_time = {0, 0};
static struct timespec rtclk_stop_time = {0, 0};
static struct timespec delay_error = {0, 0};

//#define MY_CLOCK CLOCK_REALTIME
//#define MY_CLOCK CLOCK_MONOTONIC
#define MY_CLOCK CLOCK_MONOTONIC_RAW
//#define MY_CLOCK CLOCK_REALTIME_COARSE
//#define MY_CLOCK CLOCK_MONOTONIC_COARSE

#define TEST_ITERATIONS (100)

/**
 * Delay Test Function
 * 
 * This function performs a delay test using POSIX clocks and nanosleep.
 *
 * @param threadID - A pointer to thread-specific data (not used in this function).
 *
 * @return void* - This function returns a void pointer (not used).
 */
void *delay_test(void *threadID)
{
    int idx, rc;
    unsigned int max_sleep_calls = 3;
    int flags = 0;
    struct timespec rtclk_resolution;

    sleep_count = 0;  // Initialize the sleep count.

    if (clock_getres(MY_CLOCK, &rtclk_resolution) == ERROR)
    {
        perror("clock_getres");  // Print an error message if clock_getres fails.
        exit(-1);  // Exit the program with an error code.
    }
    else
    {
        printf("\n\nPOSIX Clock demo using system RT clock with resolution:\n\t%ld secs, %ld microsecs, %ld nanosecs\n",
               rtclk_resolution.tv_sec, (rtclk_resolution.tv_nsec / 1000), rtclk_resolution.tv_nsec);
    }

    for (idx = 0; idx < TEST_ITERATIONS; idx++)
    {
        printf("test %d\n", idx);  // Print the current test iteration.

        /* run test for defined seconds */
        sleep_time.tv_sec = TEST_SECONDS;
        sleep_time.tv_nsec = TEST_NANOSECONDS;
        sleep_requested.tv_sec = sleep_time.tv_sec;
        sleep_requested.tv_nsec = sleep_time.tv_nsec;

        /* start time stamp */
        clock_gettime(MY_CLOCK, &rtclk_start_time);  // Record the start time.

        /* request sleep time and repeat if time remains */
        do
        {
            if (rc = nanosleep(&sleep_time, &remaining_time) == 0) break;

            sleep_time.tv_sec = remaining_time.tv_sec;
            sleep_time.tv_nsec = remaining_time.tv_nsec;
            sleep_count++;
        } while (((remaining_time.tv_sec > 0) || (remaining_time.tv_nsec > 0)) &&
                 (sleep_count < max_sleep_calls));

        clock_gettime(MY_CLOCK, &rtclk_stop_time);  // Record the stop time.

        delta_t(&rtclk_stop_time, &rtclk_start_time, &rtclk_dt);  // Calculate the time elapsed.
        delta_t(&rtclk_dt, &sleep_requested, &delay_error);  // Calculate the delay error.

        end_delay_test();  // Perform any necessary cleanup or reporting for the delay test.
    }

    // This function returns a void pointer, typically not used, so there's no return statement.
}


/**
 * Calculate and print the delay statistics using clock data.
 *
 * This function calculates the delay statistics between 'rtclk_start_time' and 'rtclk_stop_time',
 * then prints the results, including the real time difference and delay error.
 */
void end_delay_test(void) {
    double real_dt; // Declare a variable to store the real time difference.

#if 0
    // The following code block is commented out and not used in the function.
    printf("MY_CLOCK start seconds = %ld, nanoseconds = %ld\n", 
           rtclk_start_time.tv_sec, rtclk_start_time.tv_nsec);
  
    printf("MY_CLOCK clock stop seconds = %ld, nanoseconds = %ld\n", 
           rtclk_stop_time.tv_sec, rtclk_stop_time.tv_nsec);
#endif

    // Calculate the real time difference using 'd_ftime' function and store it in 'real_dt'.
    real_dt = d_ftime(&rtclk_start_time, &rtclk_stop_time);

    // Print the clock data including seconds, milliseconds, microseconds, nanoseconds, and real time difference.
    printf("MY_CLOCK clock DT seconds = %ld, msec=%ld, usec=%ld, nsec=%ld, sec=%6.9lf\n", 
           rtclk_dt.tv_sec, rtclk_dt.tv_nsec / 1000000, rtclk_dt.tv_nsec / 1000, rtclk_dt.tv_nsec, real_dt);

#if 0
    // The following code block is commented out and not used in the function.
    printf("Requested sleep seconds = %ld, nanoseconds = %ld\n", 
           sleep_requested.tv_sec, sleep_requested.tv_nsec);

    printf("\n");
    printf("Sleep loop count = %ld\n", sleep_count);
#endif

    // Print the delay error in seconds and nanoseconds.
    printf("MY_CLOCK delay error = %ld, nanoseconds = %ld\n", 
           delay_error.tv_sec, delay_error.tv_nsec);
}


#define RUN_RT_THREAD

/**
 * Main function for a program that demonstrates scheduling policy adjustments and runs a real-time thread (if enabled).
 * The program sets the scheduling policy to SCHED_FIFO and runs a real-time thread with the specified policy.
 * It also prints the scheduler information before and after adjustments.
 * 
 * @return None
 */
void main(void)
{
   int rc, scope;

   // Print scheduler information before adjustments
   printf("Before adjustments to scheduling policy:\n");
   print_scheduler();

#ifdef RUN_RT_THREAD
   // Initialize the thread attributes for the main thread
   pthread_attr_init(&main_sched_attr);
   // Set the scheduling policy for the main thread to SCHED_FIFO
   pthread_attr_setinheritsched(&main_sched_attr, PTHREAD_EXPLICIT_SCHED);
   pthread_attr_setschedpolicy(&main_sched_attr, SCHED_FIFO);

   // Get the maximum and minimum priorities for SCHED_FIFO
   rt_max_prio = sched_get_priority_max(SCHED_FIFO);
   rt_min_prio = sched_get_priority_min(SCHED_FIFO);

   // Set the scheduling priority for the main process to the maximum priority
   main_param.sched_priority = rt_max_prio;
   rc = sched_setscheduler(getpid(), SCHED_FIFO, &main_param);

   // Check for errors in setting the scheduling policy
   if (rc)
   {
       printf("ERROR; sched_setscheduler rc is %d\n", rc);
       perror("sched_setscheduler");
       exit(-1);
   }

   // Print scheduler information after adjustments
   printf("After adjustments to scheduling policy:\n");
   print_scheduler();

   // Set the scheduling priority for the main thread to the maximum priority
   main_param.sched_priority = rt_max_prio;
   pthread_attr_setschedparam(&main_sched_attr, &main_param);

   // Create and run a real-time thread with the specified attributes
   rc = pthread_create(&main_thread, &main_sched_attr, delay_test, (void *)0);

   // Check for errors in thread creation
   if (rc)
   {
       printf("ERROR; pthread_create() rc is %d\n", rc);
       perror("pthread_create");
       exit(-1);
   }

   // Wait for the real-time thread to complete
   pthread_join(main_thread, NULL);

   // Destroy the thread attributes
   if (pthread_attr_destroy(&main_sched_attr) != 0)
     perror("attr destroy");
#else
   // Run a non-real-time test function if the RUN_RT_THREAD flag is not set
   delay_test((void *)0);
#endif

   // Print a message indicating the test is complete
   printf("TEST COMPLETE\n");
}


