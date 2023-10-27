#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <sched.h>
#include <syslog.h>
#include <string.h>
#include <sys_logger.h>

#define COURSE 1
#define ASSIGNMENT 2

#define COUNT  1000
#define NUM_THREADS 128

typedef struct
{
    int threadIdx;
} threadParams_t;


// POSIX thread declarations and scheduling attributes
//
pthread_t threads[NUM_THREADS];
threadParams_t threadParams[NUM_THREADS + 1];


// Unsafe global
int gsum=0;

void *counterThread(void *threadp)
{
   int sum=0, i;
   threadParams_t *threadParams = (threadParams_t *)threadp;
   char msg[512]; // syslog message string

   // Sum from 1 to the thread index
   for(i=1; i < (threadParams->threadIdx)+1; i++)
      sum=sum+i;
   
   // Construct the syslog message string
   sprintf(msg, "Thread idx=%d, sum[1...%d]=%d",
            threadParams->threadIdx,
            threadParams->threadIdx,
            sum );

   log_sys(msg, COURSE, ASSIGNMENT);
}


int main (int argc, char *argv[])
{
   int rc;
   int i;

   // Clear the syslog before starting
   clear_syslog();

   // Log machine info to the syslog
   log_uname(COURSE, ASSIGNMENT);

   // Create a number of a threads
   for(i=1; i <= NUM_THREADS; i++) {

      // Populate the parameters to pass in
      threadParams[i].threadIdx=i;

      // Create the thread
      pthread_create(&threads[i],   // pointer to thread descriptor
                     (void *)0,     // use default attributes
                     counterThread, // thread function entry point
                     (void *)&(threadParams[i]) // parameters to pass in
                     );
   }

   // Wait for threads to complete
   for(i=0; i<NUM_THREADS; i++)
     pthread_join(threads[i], NULL);

   // Copy the updated syslog to the current project directory
   copy_syslog(COURSE, ASSIGNMENT);

   printf("TEST COMPLETE\n");
}
