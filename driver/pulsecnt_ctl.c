/*
 * pulsecnt_ctl - command-line tool for /dev/pulsecnt
 *
 *   ./pulsecnt_ctl stats        show counters and settings
 *   ./pulsecnt_ctl reset        reset the counter
 *   ./pulsecnt_ctl watts 2500   change the simulated load
 */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "pulsecnt_ioctl.h"

static void usage(const char *prog)
{
	fprintf(stderr, "usage: %s stats | reset | watts <W>\n", prog);
}

int main(int argc, char **argv)
{
	int fd, rc = 0;

	if (argc < 2) {
		usage(argv[0]);
		return 2;
	}

	fd = open("/dev/pulsecnt", O_RDONLY);
	if (fd < 0) {
		perror("open /dev/pulsecnt");
		return 1;
	}

	if (strcmp(argv[1], "stats") == 0) {
		struct pulsecnt_stats st;

		if (ioctl(fd, PULSECNT_IOC_GET_STATS, &st) < 0) {
			perror("GET_STATS");
			rc = 1;
		} else {
			printf("count=%llu dropped=%llu watts=%u pulses_per_kwh=%u\n",
			       (unsigned long long)st.count,
			       (unsigned long long)st.dropped,
			       st.watts, st.pulses_per_kwh);
		}
	} else if (strcmp(argv[1], "reset") == 0) {
		if (ioctl(fd, PULSECNT_IOC_RESET) < 0) {
			perror("RESET");
			rc = 1;
		} else {
			puts("counter reset");
		}
	} else if (strcmp(argv[1], "watts") == 0 && argc == 3) {
		__u32 w = (__u32)strtoul(argv[2], NULL, 10);

		if (ioctl(fd, PULSECNT_IOC_SET_WATTS, &w) < 0) {
			perror("SET_WATTS");
			rc = 1;
		} else {
			printf("load set to %u W\n", w);
		}
	} else {
		usage(argv[0]);
		rc = 2;
	}

	close(fd);
	return rc;
}
