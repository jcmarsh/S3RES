/*
 *  pint-debug.c - supposed to help me debug the PINT fork() issue
 */

#include <linux/module.h>
#include <linux/kernel.h>

int init_module(void) {
  printk(KERN_INFO "Inited pint-debug\n");

  return 0;
}

void cleanup_module(void) {
  printk(KERN_INFO "Cleanup pint-debug\n");
}
