#ifndef __DAEMON_H
#define __DAEMON_H

void daemonize();
void write_pid(const char *pid_file, const char *user, const char *group);
void drop_privileges(const char *user, const char *group);
void nchown(const char *filename, const char *user, const char *group);

#endif
