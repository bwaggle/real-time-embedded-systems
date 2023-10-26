#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <sched.h>
#include <syslog.h>
#include <time.h>

#define NUM_THREADS 12
#define COURSE_NUM 1
#define ASSIGNMENT_NUM 1 

typedef struct
{
    int threadIdx;
} threadParams_t;


// POSIX thread declarations and scheduling attributes
//
pthread_t threads[NUM_THREADS];
threadParams_t threadParams[NUM_THREADS];

void log(const char *msg) {
    openlog("pthread", LOG_PID, LOG_USER);

    time_t current_time;
    struct tm *time_info;
    char time_str[20];

    time(&current_time);
    time_info = localtime(&current_time);
    strftime(time_str, 20, "%b %d %H:%M:%S", time_info);

    // syslog(LOG_INFO, "%s raspberrypi pthread: %s", time_str, msg);
    syslog(LOG_INFO, "[COURSE:%d][ASSIGNMENT:%d]%s", COURSE_NUM, ASSIGNMENT_NUM, msg);

    closelog();
}

void log_uname() {
    FILE *unameCommand = popen("uname -a", "r");
    if (unameCommand == NULL) {
        perror("popen");
        return;
    }

    char unameOutput[256]; // Adjust buffer size as needed
    size_t len = 0;

    while (fgets(unameOutput, sizeof(unameOutput), unameCommand) != NULL) {
        // Remove the newline character from the uname output
        len = strlen(unameOutput);
        if (len > 0 && unameOutput[len - 1] == '\n') {
            unameOutput[len - 1] = '\0';
        }
        
        log(unameOutput);
    }

    pclose(unameCommand);
}

void *counterThread(void *threadp)
{
    int sum=0, i;
    threadParams_t *threadParams = (threadParams_t *)threadp;

    if (threadParams->threadIdx == 0) {
        log("Hello World from Thread!");
    }

    for(i=1; i < (threadParams->threadIdx)+1; i++)
        sum=sum+i;
 
    printf("Thread idx=%d, sum[0...%d]=%d\n", 
           threadParams->threadIdx,
           threadParams->threadIdx, sum);
}




int main (int argc, char *argv[])
{
   int rc;
   int i;

    log_uname();
   log("Hello World from Main!");

   for(i=0; i < NUM_THREADS; i++)
   {
       threadParams[i].threadIdx=i;

       pthread_create(&threads[i],   // pointer to thread descriptor
                      (void *)0,     // use default attributes
                      counterThread, // thread function entry point
                      (void *)&(threadParams[i]) // parameters to pass in
                     );

   }

   for(i=0;i<NUM_THREADS;i++)
       pthread_join(threads[i], NULL);

   printf("TEST COMPLETE\n");
}
