
#include<linux/module.h>  
#include<linux/kernel.h>  
#include<linux/init.h>  
#include<linux/proc_fs.h>
#include<linux/seq_file.h>
MODULE_LICENSE("GPL");  

static ssize_t dump_mem_write(struct file *filp, const char __user *user, size_t size, loff_t *off) {
	unsigned long addr;
	unsigned long *content; 
	int rc;
	rc = kstrtoul_from_user(user, size, 0, &addr);
	content = addr; 
	printk("lwg:%s:dumping [%08lx] = %08lx\n", __func__, addr,*content);
	return size;
}


static const struct file_operations dump_mem_ops = {
	.write = dump_mem_write,
};


static int __init lkp_init(void)  
{  
	proc_create("dump_mem", 0666, NULL, &dump_mem_ops);
	return 0;  
}  

static void __exit lkp_cleanup(void)  
{  
	remove_proc_entry("dump_mem", NULL);
}  

module_init(lkp_init);  
module_exit(lkp_cleanup);  
MODULE_AUTHOR("lwg");  
MODULE_DESCRIPTION("dump_mem@addr");  
