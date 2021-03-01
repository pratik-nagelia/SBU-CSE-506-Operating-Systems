#include <linux/module.h>	/* Needed by all modules */
#include <linux/kernel.h> 	/* KERN_INFO */
#include <linux/init.h> 	/* Init and exit macros */
static char *int_str = "default value";

static int __init lkp_init(void)
{
    printk(KERN_INFO "Module loaded ...\n");
    printk(KERN_INFO "The init string is %s ...\n", int_str);
    return 0; 
}

static void __exit lkp_exit(void)
{
    printk(KERN_INFO "Module exiting ...\n");
}
module_init(lkp_init); /* lkp_init() will be called at loading the module */
module_exit(lkp_exit); /* lkp_exit() will be called at unloading the module */

MODULE_AUTHOR("Pratik Nagelia <pnagelia@cs.stonybrook.edu");
MODULE_DESCRIPTION("Assignment kds module");