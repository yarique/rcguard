#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <unistd.h>

/*
 * XXX Assumptions made and corners cut:
 *
 * rc.d script name == $name set in it
 *
 *   This is mostly true except in several historical cases.
 *   One big exception is sendmail.  It effectively handles
 *   several services with different names.  Ideally, those
 *   should have separate rc.d scripts.
 */

long pidfile_timeout;
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
	FILE *fp;
	long slept;
	long t;
	pid_t pid;

	for (pid = slept = 0;;) {
		if ((fp = fopen(pidfile, "r")) == NULL)
			goto retry;
retry:
		/* Exponential backoff */
		t = slept ? slept : 1;
		sleep(t);
		slept += t;
		if (slept >= timeout)
			break;
	}

	return (pid);
}

void
usage(void)
{
	errx(EX_USAGE, "Usage: supervise [-T timeout] -p pidfile service");
}
