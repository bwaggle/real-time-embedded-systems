#ifndef SYSLOGGER_H
#define SYSLOGGER_H

void log_sys(const char *msg, int course_num, int assignment_num);
void log_uname(int course_num, int assignment_num);
void clear_syslog();
void copy_syslog(int course, int assignment);

#endif
