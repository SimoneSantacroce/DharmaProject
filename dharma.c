/**
 * Dharma main module
 */

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Benjamin Linux");


/* The operations */

static int dharma_open(struct inode *inode, struct file *file)
{
   return 0;
}

static int dharma_release(struct inode *inode, struct file *file)
{
   return 0;
}

static ssize_t dharma_write(struct file *filp,
   const char *buff,
   size_t len,
   loff_t *off)
{
   return 0;
}

static long dharma_ioctl(struct file *filp,
	unsigned int cmd,
	unsigned long arg)
{
	int minor = iminor(filp->f_path.dentry->d_inode);
	int size;

	switch (cmd)
	{
		case DHARMA_GET_MAX_BUFFER_SIZE:
		break;
	}
}

int init_module(void)
{
	major = register_chrdev(0, DEVICE_NAME, &fops);

	if (major < 0) {
	  printk("Registering noiser device failed\n");
	  return major;
	}

	printk(KERN_INFO "Broadcast device registered, it is assigned major number %d\n", major);

	return 0;
}

void cleanup_module(void)
{
	unregister_chrdev(major, DEVICE_NAME);

	printk(KERN_INFO "Broadcast device unregistered, it was assigned major number %d\n", Major);
}
