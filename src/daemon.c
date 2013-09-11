
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <libgen.h>

#include "memory.h"
#include "daemon.h"
#include "logger.h"
#include "utils.h"

void daemonize() {
	int i;
	pid_t sid, pid;

	pid = fork();
	if (pid < 0) {
		panic("Could not fork");
	}

	if (pid > 0) {
		exit(EXIT_SUCCESS); // parent
    }

	// Get new session ID so we are not killed when the parent is
	sid = setsid();

	if (sid < 0) {
		panic("Could not get new session ID");
	}

	if (chdir("/") < 0) {
		panic("Changing background process direcotry");
	}

	umask(0022);

	fclose(stdin);
	fclose(stdout);
	fclose(stderr);

	i = open("/dev/null", O_RDWR);
	i = dup(i);
	i = dup(i);
}

void write_pid(const char *pid_file, const char *user, const char *group) {
	pid_t pid = getpid();
    int f;
    char pidstr[32];
    char *pid_dir;

    pid_dir = strdup(pid_file);
    pid_dir = dirname(pid_dir);

    if (!file_exists(pid_dir)) {
        if (mkdir(pid_dir, 0755) != 0) {
            panic("Could not create directory for PID file");
        }
    }
    nchown(pid_dir, user, group);
    free(pid_dir);

    if ((f = open(pid_file, O_CREAT | O_EXCL | O_WRONLY, 00644)) < 0) {
        if (errno == EEXIST) {
            panic("PID file already exists");
        }
        else {
            panic("Could not create PID file");
        }
    }

	snprintf(pidstr, sizeof(pidstr), "%d", pid);

    if (write(f, pidstr, strlen(pidstr)) != strlen(pidstr)) {
        panic("Could not write to PID file");
    }

	close(f);
}

void drop_privileges(const char *user, const char *group) {
    struct passwd *pw;
    struct group *gr;

    if (!strlen(user) || !strlen(group)) {
        return;
    }

    if ((pw = getpwnam(user)) == NULL) {
        panic("Desired user does not exist");
    }

    if ((gr = getgrnam(group)) == NULL) {
        panic("Desired group does not exist");
    }

    if (getuid() == 0) {
        if (setgid(gr->gr_gid) != 0) {
            panic("Unable to drop group privileges");
        }

        if (setuid(pw->pw_uid) != 0) {
            panic("Unable to drop user privileges");
        }

        if (setuid(0) != -1) {
             panic("Managed to regain root privileges");
        }

    }
}

void nchown(const char *filename, const char *user, const char *group) {
    struct passwd *pw;
    struct group *gr;

    if (getuid() != 0) {
        return;
    }

    if (!strlen(user) || !strlen(group)) {
        return;
    }

    if ((pw = getpwnam(user)) == NULL) {
        panic("Desired user does not exist");
    }

    if ((gr = getgrnam(group)) == NULL) {
        panic("Desired group does not exist");
    }

    if (0 != chown(filename, pw->pw_uid, gr->gr_gid)) {
        panic("Could not chown file");
    }
}

