/*
 * The darma header
 */

#define DEVICE_NAME "dharma"
#define DEVICE_MAX_NUMBER 256

static int major;

static int dharma_open(struct inode *, struct file *);
static int dharma_release(struct inode *, struct file *);
static ssize_t dharma_write(struct file *, const char *, size_t, loff_t *);
static ssize_t dharma_read(struct file *, char *, size_t, loff_t *);
static long dharma_ioctl(struct file *f, unsigned int cmd, unsigned long arg);

static struct file_operations fops = {
	.read			= dharma_read,
	.write			= dharma_write,
	.open			=  dharma_open,
	.release		= dharma_release,
	.unlocked_ioctl = mailbox_ioctl
};
