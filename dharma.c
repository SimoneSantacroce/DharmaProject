/**
 * Dharma main module
 */

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Benjamin Linux");


/* The operations */

static int dharma_open(struct inode *inode, struct file *file)
{
	try_module_get(THIS_MODULE);

	// return the minor number
	int minor = iminor(filp->f_path.dentry->d_inode);
	if (minor < DEVICE_MAX_NUMBER) {
		minorArray[minor]=kmalloc(BUFFER_SIZE);
		return 0;
	}
	else {
		return -EBUSY; // Why busy?
	}
}

static int dharma_release(struct inode *inode, struct file *file)
{
   	module_put(THIS_MODULE);
   	// close does NOT invalidate the buffer
   	// buffer stays in memory until the module is unmounted
	return 0;
}

static ssize_t dharma_write(struct file *filp,
   const char *buff,
   size_t len,
   loff_t *off)
{
   return 0;
}

static ssize_t dharma_read(struct file *filp, char *out_buffer, size_t size, loff_t *offset) {
	// should we check if file is open/valid?
}

static ssize_t dharma_read_packet(struct file *filp, char *out_buffer, size_t size, loff_t *offset) {
	int minor=iminor(filp->f_path.dentry->d_inode);
	// acquire spinlock
	spin_lock(&(buffer_lock[minor]));
	int ret_val = 0;
	DECLARE_WAIT_QUEUE_HEAD(the_queue);//here we use a private queue since wakeup is selective via wake up process

	// N.B. buffer check should be atomic too
	if (buffer_is_empty) {
		// release spinlock (look at STEEEVE)
		spin_unlock(&(buffer_lock[minor]));
		if (!BLOCKING) {
			return -1;
		} else {
			// insert into read wait queue
			if(wait_event_interruptible(the_queue, !buffer_is_empty))
				return -1;
			//acquire spinlock
			spin_lock(&(buffer_lock[minor]));
		}
	} 
	copy_to_user(out_buffer, minorArray[minor], PACKET_SIZE);
	
}

static ssize_t dharma_read_stream(struct file *filp, char *out_buffer, size_t size, loff_t *offset) {
	
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

int init_module(void){

	// dynamic allocation of the major number (through the first parameter = 0)
	major = register_chrdev(0, DEVICE_NAME, &fops);

	if (major < 0) {
	  printk("Registering noiser device failed\n");
	  return major;
	}

	printk(KERN_INFO "Broadcast device registered, it is assigned major number %d\n", major);

	return 0;
}

void cleanup_module(void){

	unregister_chrdev(major, DEVICE_NAME);

	printk(KERN_INFO "Broadcast device unregistered, it was assigned major number %d\n", major);
}
