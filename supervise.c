#include <sys/types.h>
#include <sys/event.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sysexits.h>
#include <syslog.h>
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
 * of the service with a pid value now reused by an unrelated
 * process
 *
 *   This is an obvious race condition: Should a stale pid
 *   file exist, it will be impossible to reliably tell if
 *   it came from the current or previous instance of the
 *   service.  Hence the assumption.  Of course, it would
 *   be better just to remove any stale pidfiles in rc.subr
 *   before starting the service.
 *
 * pid value is written atomically
 *
 *   E.g., there should be no chance to read just "12" from
 *   the pidfile if the pid value is 12345.
 */

#define PATH_SERVICE	"/usr/sbin/service"
#define RCD_CMD		"restart"	/* rc.d command to restart service */

int foreground = 0;
long pidfile_timeout = 60;	/* seconds */
const char *service_name;
const char *service_pidfile = NULL;
int verbose = 0;

pid_t get_pid_from_file(const char *, long);
void usage(void);
int watch_pid(pid_t);

int
main(int argc, char **argv)
{
	char *ep;
	const char *p;
	int c;
	pid_t pid;

	while ((c = getopt(argc, argv, "fp:T:v")) != -1) {
		switch (c) {
		case 'f':
			foreground = 1;
			break;
		case 'p':
			service_pidfile = optarg;
			if (service_pidfile[0] == '\0')
				errx(EX_USAGE, "null pidfile name");
			break;
		case 'T':
			pidfile_timeout = strtol(optarg, &ep, 10);
			if (pidfile_timeout <= 0 || *ep != '\0')
				errx(EX_USAGE,
				    "invalid timeout value: %s", optarg);
			break;
		case 'v':
			verbose++;
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
	if (service_name[0] == '\0')
		errx(EX_USAGE, "null service name");

	if (verbose) {
		printf("Service: %s\n", service_name);
		printf("Pidfile: %s\n", service_pidfile);
		printf("Timeout: %ld\n", pidfile_timeout);
	}

	pid = get_pid_from_file(service_pidfile, pidfile_timeout);

	if (!foreground) {
		verbose = 0;	/* no stdio */
		daemon(0, 0);
	}

	openlog("supervise", LOG_CONS | LOG_PID, LOG_DAEMON);

	/* Get basename for a nicer proctitle */
	p = strrchr(service_name, '/');
	if (p == NULL || *(++p) == '\0')
		p = service_name;
	setproctitle("%s", p);

	c = watch_pid(pid);
	if (WIFSIGNALED(c))
		syslog(LOG_WARNING, "%s terminated on signal %d",
		    p, WTERMSIG(c));
	else if (WIFEXITED(c))
		syslog(LOG_WARNING, "%s exited with status %d",
		    p, WEXITSTATUS(c));
	else
		syslog(LOG_WARNING, "%s ceased with unknown status %d",
		    p, c);
	if (1) {	/* XXX cases where no restart needed? */
		syslog(LOG_WARNING, "Restarting %s", p);
		if (verbose)
			printf("Restarting %s\n", service_name);
		if (service_name[0] == '/') {
			if (verbose)
				printf("Running '%s %s'\n",
				    service_name, RCD_CMD);
			c = execl(service_name, service_name, RCD_CMD,
			    (char *)NULL);
		} else {
			if (verbose)
				printf("Running '%s %s %s'\n",
				    PATH_SERVICE, service_name, RCD_CMD);
			c = execl(PATH_SERVICE, PATH_SERVICE,
			    service_name, RCD_CMD, (char *)NULL);
		}
		if (c == -1)
			syslog(LOG_ERR, "exec failed: %m");
		else
			syslog(LOG_ERR, "exec returned %d", c);
		exit(EX_OSERR);
	}

	exit(EX_OK);

	return (0);	/* dummy */
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
	struct stat st;

	for (pid = slept = 0;;) {
		if ((fp = fopen(pidfile, "r")) == NULL) {
			if (verbose)
				printf("Failed to open %s: %s\n",
				    pidfile, strerror(errno));
			goto retry;	/* Not created yet? */
		}
		if (fgets(buf, sizeof(buf), fp) == NULL) {
			if (verbose)
				printf("Read nothing from %s\n", pidfile);
			fclose(fp);
			goto retry;	/* Not written yet? */
		}
		if (verbose > 1)
			printf("Got 1st line from pidfile %s:\n%s\n",
			    pidfile, buf);
		pid = strtol(buf, &ep, 10);
		if (pid <= 0 || !(*ep == '\0' || *ep == '\n' ||
		    *ep == '\t' || *ep == ' '))
			errx(EX_DATAERR,
			    "no pid in pidfile %s", pidfile);
		if (verbose)
			printf("Got pid %ld from %s\n", pid, pidfile);
		if (fstat(fileno(fp), &st) != 0) {
			if (verbose)
				printf("Failed to stat %s: %s\n",
				    pidfile, strerror(errno));
			fclose(fp);
			goto retry;	/* File system gone? */
		}
		fclose(fp);
		if (kill(pid, 0) != 0) {
			if (errno != ESRCH)
				err(EX_NOPERM, "failed to check pid %ld", pid);
			if (verbose)
				printf("No process with pid %ld yet\n", pid);
			goto retry;	/* Stale pidfile? */
		}
		t = time(NULL) - st.st_mtime;
		if (t >= timeout)
			warnx("pidfile %s might be stale, age %ld seconds",
			    pidfile, t);
		break;
retry:
		/* Exponential backoff */
		t = slept ? slept : 1;
		if (verbose)
			printf("Sleeping for %ld seconds...\n", t);
		sleep(t);
		slept += t;
		if (verbose > 1)
			printf("Slept for %ld seconds so far\n", slept);
		if (slept >= timeout)
			errx(EX_UNAVAILABLE,
			    "timeout waiting for pidfile %s", pidfile);
		if (verbose)
			printf("Retrying...\n");
	}

	return (pid);
}

void
usage(void)
{
	fprintf(stderr,
	    "Usage: supervise [-fv] [-T timeout] -p pidfile service\n");
	exit(EX_USAGE);
}

int
watch_pid(pid_t pid)
{
	int kq;
	struct kevent kev;

	if ((kq = kqueue()) == -1) {
		syslog(LOG_ERR, "kqueue: %m");
		exit(EX_OSERR);
	}

	EV_SET(&kev, pid, EVFILT_PROC, EV_ADD | EV_ONESHOT, NOTE_EXIT, 0, NULL);

	if (verbose)
		printf("Waiting for kevent on pid %ld...\n", (long)pid);

	switch (kevent(kq, &kev, 1, &kev, 1, NULL)) {
	case -1:
		syslog(LOG_ERR, "kevent: %m");
		exit(EX_OSERR);
	case 0:
		syslog(LOG_ERR, "kevent returned 0");
		exit(EX_OSERR);
	}

	if ((long)kev.ident != (long)pid) {
		syslog(LOG_ERR, "kevent fired on pid %ld not %ld",
		    (long)kev.ident, (long)pid);
		exit(EX_OSERR);
	}

	if (verbose)
		printf("Got exit status %d\n", (int)kev.data);

	return (kev.data);
}
