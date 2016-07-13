/*
* The darma header
*/

#define EXPORT_SYMTAB

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/fcntl.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include <asm/atomic.h>

#include "dharma_ioctl.h"

#define DEVICE_NAME "dharma"
#define DEVICE_MAX_NUMBER 256
#define BUFFER_SIZE 20
#define PACKET_SIZE 5

// Fabrizio: Magic Number Removed
char* minorArray[DEVICE_MAX_NUMBER];

static int major;
static spinlock_t buffer_lock[DEVICE_MAX_NUMBER];

/*Pos_mod is the position mod BUFFER_SIZE, Pos is without mod, used to check that 
 read is always less than write */
/* Fabrizio: Ci servono davvero 4 indici? readPos_mod e writePos_mod non possono
 * essere trasformate in variabili locali ai metodi read e write? */
int readPos_mod;
int readPos;
int writePos_mod;
int writePos;

static int dharma_open(struct inode *, struct file *);
static int dharma_release(struct inode *, struct file *);
static ssize_t dharma_write(struct file *, const char *, size_t, loff_t *);
static ssize_t dharma_read(struct file *, char *, size_t, loff_t *);
static long dharma_ioctl(struct file *f, unsigned int cmd, unsigned long arg);

static struct file_operations fops = {
	.read			= dharma_read,
	.write			= dharma_write,
	.open			= dharma_open,
	.release		= dharma_release,
	.unlocked_ioctl	= dharma_ioctl
};
