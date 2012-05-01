#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <unistd.h>

long pidfile_timeout;

void usage(void);

int
main(int argc, char **argv)
{
	int c;
	char *ep;

	while ((c = getopt(argc, argv, "T:")) != -1) {
		switch (c) {
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

	if (argc <= 0)
		usage();
}

void
usage()
{
	errx(EX_USAGE, "Usage: supervise [-T timeout] service");
}
