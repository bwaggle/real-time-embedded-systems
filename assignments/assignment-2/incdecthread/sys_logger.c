/**
 * File: sys_logger.c
 * Author: Brad Waggle
 * Description: Utilites for clearing, writing, copying the syslog.
 * Date: October 27, 2023
 */

#include <stdio.h>
#include <syslog.h>
#include <stdlib.h>
#include <string.h>

// Log a message to the syslog with course number and assignment number
// Example format: 
// <System Time> <Host Name> [COURSE:1][ASSIGNMENT:2]: <msg>
void log_sys(const char *msg, int course_num, int assignment_num) {
    openlog("pthread", LOG_PID, LOG_USER);

    syslog(LOG_INFO, "[COURSE:%d][ASSIGNMENT:%d]%s", 
            course_num,
            assignment_num, 
            msg);

    closelog();
}

// Log machine info to the syslog
void log_uname(int course, int assignment) {
    // Construct the command to send to the console
    FILE *uname_command = popen("uname -a", "r");
    if (uname_command == NULL) {
        perror("popen");
        return;
    }

    char uname_output[256]; // Stores the output of the uname_command
    size_t len = 0;

    while (fgets(uname_output, sizeof(uname_output), uname_command) != NULL) {
        // Remove the newline character from the uname output
        len = strlen(uname_output);
        if (len > 0 && uname_output[len - 1] == '\n') {
            uname_output[len - 1] = '\0';
        }
        // Log the output of the uname command to the syslog
        log_sys(uname_output, course, assignment);
    }

    pclose(uname_command);
}

// Empties the syslog 
void clear_syslog() {
    // Note: permissions on the syslog must be 664 
    // (i.e. chmod 664 /var/log/syslog) 
    int status = system("sh -c 'echo > /var/log/syslog'");

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        printf("Syslog cleared successfully.\n");
    } else {
        printf("Failed to clear syslog.\n");
    }
}

// Copies the syslog to the current assignment directory
void copy_syslog(int course, int assignment) {
    char filename[100]; 

    // Format the filename based on course and assignment
    snprintf(filename, sizeof(filename), "syslog-prog-%d.%d.txt", course, assignment);

    // Use the system command to copy syslog to the specified filename
    int status = system("cp /var/log/syslog .");  // Copy syslog to current directory

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        printf("Syslog copied to current directory successfully.\n");
        // Rename the copied syslog file
        int rename_status = rename("syslog", filename);
        if (rename_status == 0) {
            printf("Renamed to %s.\n", filename);
        } else {
            printf("Failed to rename the copied syslog file.\n");
        }
    } else {
        printf("Failed to copy syslog.\n");
    }
}