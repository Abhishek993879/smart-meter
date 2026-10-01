// SPDX-License-Identifier: GPL-2.0
/* Smallest possible kernel module: proves your VM can build and load modules. */
#include <linux/init.h>
#include <linux/module.h>

static int __init hello_init(void)
{
	pr_info("hello: module loaded\n");
	return 0;
}

static void __exit hello_exit(void)
{
	pr_info("hello: module unloaded\n");
}

module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Abhishek Mohapatra");
MODULE_DESCRIPTION("Hello world kernel module");
