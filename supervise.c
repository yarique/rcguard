#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <unistd.h>

/*
 * Assumptions made and corners cut:
 *
 * XXX rc.d script name == $name set in it
 *
 *   This is mostly true except in several historical cases.
 *   One big exception is sendmail.  It effectively handles
 *   several services with different names.  Ideally, those
 *   should have separate rc.d scripts.
 *
 * there is no stale pidfile left from an earlier instance
 *
 *   This is an obvious race condition: Should a stale pid
 *   file exist, it will be impossible to reliably tell if
 *   it came from the current or previous instance of the
 *   service.  XXX Can its time stamp be of any help here?
 *
 * pid value is written atomically
 *
 *   E.g., there should be no chance to read just "12" from
 *   the pidfile if the pid value is 12345.
 */

long pidfile_timeout = 60;	/* seconds */
const char *service_name;
const char *service_pidfile = NULL;

pid_t get_pid_from_file(const char *, long);
void usage(void);

int
main(int argc, char **argv)
{
	char *ep;
	int c;
	pid_t pid;

	while ((c = getopt(argc, argv, "p:T:")) != -1) {
		switch (c) {
		case 'p':
			service_pidfile = optarg;
			if (service_pidfile[0] == '\0')
				errx(EX_USAGE, "Null pidfile name");
			break;
		case 'T':
			pidfile_timeout = strtol(optarg, &ep, 10);
			if (pidfile_timeout <= 0 || *ep != '\0')
				errx(EX_USAGE,
				    "Invalid timeout value: %s", optarg);
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	/* Can't supervise a service w/o knowing its pidfile */
	if (service_pidfile == NULL)
		usage();

	if (argc <= 0 || argc > 1)
		usage();
	service_name = *argv;

	pid = get_pid_from_file(service_pidfile, pidfile_timeout);

	return (0);
}

pid_t
get_pid_from_file(const char *pidfile, long timeout)
{
	char buf[32];
	char *ep;
	FILE *fp;
	long pid;	/* will be cast to pid_t on return */
	long slept;
	long t;

	for (pid = slept = 0;;) {
		if ((fp = fopen(pidfile, "r")) == NULL)
			goto retry;	/* Not created yet */
		if (fgets(buf, sizeof(buf), fp) == NULL)
			goto retry;	/* Not written yet */
		pid = strtol(buf, &ep, 10);
		if (pid <= 0 || !(*ep == '\0' || *ep == '\n' ||
		    *ep == '\t' || *ep == ' '))
			errx(EX_DATAERR, "Unsupported pidfile format");
retry:
		/* Exponential backoff */
		t = slept ? slept : 1;
		sleep(t);
		slept += t;
		if (slept >= timeout)
			errx(EX_UNAVAILABLE, "Timeout waiting for pidfile");
	}

	return (pid);
}

void
usage(void)
{
	errx(EX_USAGE, "Usage: supervise [-T timeout] -p pidfile service");
}
