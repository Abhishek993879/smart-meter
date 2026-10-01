// SPDX-License-Identifier: GPL-2.0
/*
 * pulsecnt - simulated smart-meter pulse counter
 *
 * A high-resolution timer plays the role of the meter's pulse output
 * (for example a GPIO interrupt). Every pulse is timestamped and stored in a
 * ring buffer. Userspace reads the pulses from /dev/pulsecnt.
 *
 *   read()   returns struct pulsecnt_sample records, blocks until one exists
 *   poll()   readable when at least one pulse is waiting
 *   ioctl()  reset the counter, change the simulated load, read statistics
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/fs.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/math64.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/wait.h>

#include "pulsecnt_ioctl.h"

#define RING_SIZE	256	/* must be a power of two */
#define MAX_PPK		100000

static unsigned int watts = 1500;
module_param(watts, uint, 0444);
MODULE_PARM_DESC(watts, "Simulated load in watts (1-100000)");

static unsigned int pulses_per_kwh = 3600;
module_param(pulses_per_kwh, uint, 0444);
MODULE_PARM_DESC(pulses_per_kwh, "Meter constant: pulses per kWh (1-100000)");

struct pulsecnt_dev {
	struct hrtimer timer;
	spinlock_t lock;		/* protects the fields below */
	wait_queue_head_t waitq;
	struct pulsecnt_sample ring[RING_SIZE];
	unsigned int head;		/* free-running write index */
	unsigned int tail;		/* free-running read index */
	u64 count;
	u64 dropped;
	u32 watts;
	u32 ppk;
};

static struct pulsecnt_dev pdev;

/*
 * Time between pulses: 3600 s/h * 1000 W/kW / (watts * pulses_per_kwh) seconds
 * = 3.6e15 / (watts * pulses_per_kwh) nanoseconds.
 */
static u64 interval_ns(u32 w, u32 ppk)
{
	return div64_u64(3600000000000000ULL, (u64)w * ppk);
}

static bool ring_has_data(struct pulsecnt_dev *d)
{
	return READ_ONCE(d->head) != READ_ONCE(d->tail);
}

/* Runs in timer (interrupt) context: no sleeping allowed. */
static enum hrtimer_restart pulse_timer_fn(struct hrtimer *t)
{
	struct pulsecnt_dev *d = container_of(t, struct pulsecnt_dev, timer);
	unsigned long flags;
	u64 ns;

	spin_lock_irqsave(&d->lock, flags);

	d->count++;
	d->ring[d->head & (RING_SIZE - 1)].timestamp_ns = ktime_get_real_ns();
	d->ring[d->head & (RING_SIZE - 1)].count = d->count;
	d->head++;
	if (d->head - d->tail > RING_SIZE) {	/* full: drop the oldest sample */
		d->tail++;
		d->dropped++;
	}
	ns = interval_ns(d->watts, d->ppk);

	spin_unlock_irqrestore(&d->lock, flags);

	wake_up_interruptible(&d->waitq);
	hrtimer_forward_now(t, ns_to_ktime(ns));
	return HRTIMER_RESTART;
}

static int pulsecnt_open(struct inode *inode, struct file *file)
{
	return nonseekable_open(inode, file);
}

static ssize_t pulsecnt_read(struct file *file, char __user *buf,
			     size_t len, loff_t *off)
{
	struct pulsecnt_dev *d = &pdev;
	struct pulsecnt_sample sample;
	unsigned long flags;
	size_t done = 0;
	int ret;

	if (len < sizeof(sample))
		return -EINVAL;

	for (;;) {
		while (done + sizeof(sample) <= len) {
			spin_lock_irqsave(&d->lock, flags);
			if (d->head == d->tail) {
				spin_unlock_irqrestore(&d->lock, flags);
				break;
			}
			sample = d->ring[d->tail & (RING_SIZE - 1)];
			d->tail++;
			spin_unlock_irqrestore(&d->lock, flags);

			/* copy_to_user may sleep, so it is done outside the lock */
			if (copy_to_user(buf + done, &sample, sizeof(sample)))
				return done ? (ssize_t)done : -EFAULT;
			done += sizeof(sample);
		}

		if (done)
			return done;
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;

		ret = wait_event_interruptible(d->waitq, ring_has_data(d));
		if (ret)
			return ret;	/* interrupted by a signal, for example Ctrl+C */
	}
}

static __poll_t pulsecnt_poll(struct file *file, poll_table *wait)
{
	struct pulsecnt_dev *d = &pdev;

	poll_wait(file, &d->waitq, wait);
	return ring_has_data(d) ? (EPOLLIN | EPOLLRDNORM) : 0;
}

static long pulsecnt_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct pulsecnt_dev *d = &pdev;
	unsigned long flags;

	switch (cmd) {
	case PULSECNT_IOC_RESET:
		spin_lock_irqsave(&d->lock, flags);
		d->count = 0;
		d->dropped = 0;
		d->head = 0;
		d->tail = 0;
		spin_unlock_irqrestore(&d->lock, flags);
		return 0;

	case PULSECNT_IOC_SET_WATTS: {
		__u32 w;
		u64 ns;

		if (copy_from_user(&w, (void __user *)arg, sizeof(w)))
			return -EFAULT;
		if (w < PULSECNT_MIN_WATTS || w > PULSECNT_MAX_WATTS)
			return -EINVAL;

		spin_lock_irqsave(&d->lock, flags);
		d->watts = w;
		ns = interval_ns(w, d->ppk);
		spin_unlock_irqrestore(&d->lock, flags);

		/* Re-arm now, otherwise a slow old interval would delay the change. */
		hrtimer_cancel(&d->timer);
		hrtimer_start(&d->timer, ns_to_ktime(ns), HRTIMER_MODE_REL);
		return 0;
	}

	case PULSECNT_IOC_GET_STATS: {
		struct pulsecnt_stats st;

		spin_lock_irqsave(&d->lock, flags);
		st.count = d->count;
		st.dropped = d->dropped;
		st.watts = d->watts;
		st.pulses_per_kwh = d->ppk;
		spin_unlock_irqrestore(&d->lock, flags);

		if (copy_to_user((void __user *)arg, &st, sizeof(st)))
			return -EFAULT;
		return 0;
	}

	default:
		return -ENOTTY;
	}
}

static const struct file_operations pulsecnt_fops = {
	.owner		= THIS_MODULE,
	.open		= pulsecnt_open,
	.read		= pulsecnt_read,
	.poll		= pulsecnt_poll,
	.unlocked_ioctl	= pulsecnt_ioctl,
};

static struct miscdevice pulsecnt_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "pulsecnt",
	.fops	= &pulsecnt_fops,
	.mode	= 0666,		/* demo only: lets the agent run without root */
};

static int __init pulsecnt_init(void)
{
	struct pulsecnt_dev *d = &pdev;
	int ret;

	if (watts < PULSECNT_MIN_WATTS || watts > PULSECNT_MAX_WATTS ||
	    pulses_per_kwh < 1 || pulses_per_kwh > MAX_PPK) {
		pr_err("invalid parameters: watts=%u pulses_per_kwh=%u\n",
		       watts, pulses_per_kwh);
		return -EINVAL;
	}

	spin_lock_init(&d->lock);
	init_waitqueue_head(&d->waitq);
	d->watts = watts;
	d->ppk = pulses_per_kwh;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 13, 0)
	hrtimer_setup(&d->timer, pulse_timer_fn, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
#else
	hrtimer_init(&d->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	d->timer.function = pulse_timer_fn;
#endif

	ret = misc_register(&pulsecnt_misc);
	if (ret) {
		pr_err("misc_register failed: %d\n", ret);
		return ret;
	}

	hrtimer_start(&d->timer, ns_to_ktime(interval_ns(d->watts, d->ppk)),
		      HRTIMER_MODE_REL);
	pr_info("loaded: /dev/pulsecnt, %u W, %u pulses/kWh\n", d->watts, d->ppk);
	return 0;
}

static void __exit pulsecnt_exit(void)
{
	misc_deregister(&pulsecnt_misc);
	hrtimer_cancel(&pdev.timer);
	pr_info("unloaded after %llu pulses\n", (unsigned long long)pdev.count);
}

module_init(pulsecnt_init);
module_exit(pulsecnt_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Abhishek Mohapatra");
MODULE_DESCRIPTION("Simulated smart-meter pulse counter");
