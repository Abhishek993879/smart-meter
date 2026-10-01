/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Interface of the simulated smart-meter pulse counter (/dev/pulsecnt).
 * Shared by the kernel module and by userspace programs (the C++ agent).
 */
#ifndef PULSECNT_IOCTL_H
#define PULSECNT_IOCTL_H

#include <linux/ioctl.h>
#include <linux/types.h>

/* read() returns an array of these, one per pulse. Same layout as PulseEvent in the agent. */
struct pulsecnt_sample {
	__u64 timestamp_ns;	/* wall-clock time of the pulse, ns since 1970 */
	__u64 count;		/* total pulses since load or last reset */
};

struct pulsecnt_stats {
	__u64 count;		/* pulses generated so far */
	__u64 dropped;		/* samples lost because nobody read them in time */
	__u32 watts;		/* current simulated load */
	__u32 pulses_per_kwh;	/* meter constant */
};

#define PULSECNT_MIN_WATTS	1
#define PULSECNT_MAX_WATTS	100000

#define PULSECNT_IOC_MAGIC	'P'
#define PULSECNT_IOC_RESET	_IO(PULSECNT_IOC_MAGIC, 0)
#define PULSECNT_IOC_SET_WATTS	_IOW(PULSECNT_IOC_MAGIC, 1, __u32)
#define PULSECNT_IOC_GET_STATS	_IOR(PULSECNT_IOC_MAGIC, 2, struct pulsecnt_stats)

#endif /* PULSECNT_IOCTL_H */
