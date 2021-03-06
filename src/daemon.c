#define _GNU_SOURCE
#include <errno.h>
#include <stdlib.h>
#include <sys/file.h>
#include <unistd.h>
#include "config.h"
#include "main.h"
#include "controller.h"
#include "errors.h"
#include "log.h"
#include "net.h"
#include "pid.h"
#include "signals.h"

int main(int argc, char **argv)
{
	int e, errsv;
	int lockfd;

	e = sighandlers();
	if (e != 0) {
		goto main_fail;
	}

	/* initialize config mutex */
	if (pthread_mutex_init(&config_mutex, NULL) != 0) {
		goto main_fail;
	}

	/* set config defaults, before overriding them with any options */
	config_defaults();

	/* read config */
	if ((e = config_read(NULL))) {
		switch (e) {
		case LC_ERROR_CONFIG_READFAIL:
			logmsg(LOG_WARNING, "Unable to read config file.  Skipping");
			break;
		default:
			goto main_fail;
		}
	}

	/* obtain lockfile, but don't write pid until after we fork() */
	lockfd = obtain_lockfile(O_RDWR | O_CREAT | O_TRUNC);
	if (lockfd == -1) {
		errno = 0;
		e = LC_ERROR_PID_OPEN;
		goto main_fail;
	}
	else if (flock(lockfd, LOCK_EX|LOCK_NB) != 0) {
		e = LC_ERROR_ALREADY_RUNNING;
		goto main_fail;
	}

	/* TODO: open syslogger */

	/* start controller process */
	controller_start(lockfd);

	/* not reached unless all controller threads ever exit, uncancelled */
	main_free();

	return 0;

main_fail:
	errsv = errno;
	lc_print_error(e, errsv, "main");
	config_free();
	return e;
}

void main_free()
{
	controller_shutdown();
	config_free();
	pthread_mutex_destroy(&config_mutex);
	net_free();
}
