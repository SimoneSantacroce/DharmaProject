/**
 * Dharma main module
 */
#include "dharma.h"
#include "dharma_ioctl.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Benjamin Linux");

//solo per compilare.
#define buffer_is_empty (readPos_mod==writePos_mod)


/* The operations */

static int dharma_open(struct inode *inode, struct file *file)
{
	try_module_get(THIS_MODULE);

	// return the minor number
	int minor = iminor(file->f_path.dentry->d_inode);
	if (minor < DEVICE_MAX_NUMBER) {
		minorArray[minor]=kmalloc(BUFFER_SIZE, GFP_ATOMIC);
		readPos=0;
		readPos_mod=0;
		writePos=0;
		writePos_mod=0;
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
	return 0;
}

static ssize_t dharma_read_packet(struct file *filp, char *out_buffer, size_t size, loff_t *offset) {
	int minor=iminor(filp->f_path.dentry->d_inode);
	// acquire spinlock
	spin_lock(&(buffer_lock[minor]));

	DECLARE_WAIT_QUEUE_HEAD(the_queue);

	// N.B. buffer check should be atomic too.
	if (buffer_is_empty) {
		// release spinlock
		spin_unlock(&(buffer_lock[minor]));
		//if op is non blocking. EGAIN:resource is temporarily unavailable
		if (filp->f_flags & O_NONBLOCK) {
			return -EAGAIN;
		} else {
			/* insert into wait queue. wait_event_interruptible returns ERESTARTSYS 
			 * if it is interrupted by a signal. The system call can be re-executed if there is some 
			 * interruption*/
			 /*questo è quello che ho capito da qui:
			  * http://stackoverflow.com/questions/9576604/what-does-erestartsys-used-while-writing-linux-driver */
			if(wait_event_interruptible(the_queue, !buffer_is_empty))
				return -ERESTARTSYS;
			//acquire spinlock
			spin_lock(&(buffer_lock[minor]));
		}
	}
	//return value
	int res=0;
	
	/*residual. if there is no real residual, it is equal to PACKET_SIZE.
	 * residual = how many bytes we have to read effectively */
	int residual=PACKET_SIZE-readPos_mod%PACKET_SIZE;
	//bytes that are missing to get to the end of the packet.Used later.
	int to_end=residual;
	
	//check there are residual bytes available: writePos_mod-readPos_mod are the unread bytes
	if(residual> writePos_mod-readPos_mod){
		/*CHOICE: can be discussed. If there are less than residual bytes, I read all bytes available, 
		 even if they are less than a packet*/
		residual=writePos_mod-readPos_mod;
	}
	res= copy_to_user(out_buffer, (char *)(&(minorArray[minor][readPos_mod])), residual);
	//update readPos
	readPos+=residual;
	/*if I read less than a packet(or its residual), it means that readPos (line before) was updated 
	 * in a way it does not coincide with the end of the packet, so I must
	 * add to readPos the quantity it needs to arrive at the end of the packet, and since writePos 
	 * too is not placed at the end of the frame, I update it to make it coincide with the new readPos, 
	 * that means the buffer is now empty*/
	if(residual==writePos_mod-readPos_mod){
		//now I am sure readPos is at the end of the packet
		readPos+=(to_end-residual);
		writePos=readPos;
		writePos_mod=writePos%BUFFER_SIZE;
	}
	
	readPos_mod=readPos%BUFFER_SIZE;
	spin_unlock(&(buffer_lock[minor]));
	return residual-res;
	
	
	/* DA BUTTARE SE LEGGIAMO SOLO UN PACCHETTO AL PIÙ	
	
	//read the residual, if there is, but check if size is bigger than the residual
	if(residual!=PACKET_SIZE && size>residual){		
		res= copy_to_user(out_buffer, (char *)(&(minorArray[minor][readPos_mod])), residual);
		//update readPos
		readPos+=residual;
		readPos_mod=readPos%BUFFER_SIZE;
		return res;
	}
	else if (size<=residual){
		/*I want to read a fraction of packet smaller than the already existent residual.
		 But since this is read packet I cannot leave residuals, so readPos will jump to the next packet 
		ret=copy_to_user(out_buffer, (char *)(&(minorArray[minor][readPos_mod])), size);
		//update readPos as if I read the whole packet
		readPos+=residual;
		readPos_mod=readPos%BUFFER_SIZE;
		return ret;
	}
	 
	//Now I have read residual bytes. Note: residual can be also 0.
	
	//check that I want less bytes than how many there are in the buffer
	if(size-residual> writePos-readPos){
		size=residual+writePos-readPos;
	}
	
	//if the remaining bytes to read are more than what there is up to the end of the buffer
	if(size-residual>BUFFER_SIZE-readPos_mod){
		//gestione buffer circolare
	}
	else{
		/*read size bytes; if size is not multiple of a packet, discard the rest of the packet and
		update readPos as if an entire packet was read. Out buffer must be incremented because I
		read the residual
		ret=copy_to_user((char *)(&(out_buffer[residual])), (char *)(&(minorArray[minor][readPos_mod])), size-residual);
		
		/*update reading position. +1 is here because I discard the residual of bytes of the packet I
		did not read
		readPos+=((size/PACKET_SIZE)+1)*PACKET_SIZE;
		readPos_mod=readPos%BUFFER_SIZE;
		
		//release spinlock
		spin_unlock(&(buffer_lock[minor]));
	}
	return ret;*/
	
}

static ssize_t dharma_read_stream(struct file *filp, char *out_buffer, size_t size, loff_t *offset) {
	int minor=iminor(filp->f_path.dentry->d_inode);
	// acquire spinlock
	spin_lock(&(buffer_lock[minor]));
	DECLARE_WAIT_QUEUE_HEAD(the_queue);

	if(buffer_is_empty){
		// release spinlock
		spin_unlock(&(buffer_lock[minor]));
		//if op is non blocking
		if (filp->f_flags & O_NONBLOCK) {
			// different choice with respect to the read_packet case, because:
			// return -1 represents an error, instead I think that in this case
			// we have to return "no bytes read" = "0 bytes read"
			return 0;
		} else {
			// insert into wait queue
			if(wait_event_interruptible(the_queue, !buffer_is_empty))
				return -1;
			//acquire spinlock
			spin_lock(&(buffer_lock[minor]));
		}
	}
	// ...like in the read_packet case

	//return value
	int res=0;

	// how many bytes we can read
	int readableBytes = writePos - readPos;

	/*
		In the previous version the operation was: int readableBytes = writePos_mod - readPos_mod
		but in this way, if the write pointer precedes the read pointer (legal case if the writePos > readPos)
		we will obtain a negative value of readableBytes!
	*/

	// amount of bytes to read (updated during the control phase)
	int bytesToRead = 0;

	// we compare the readable bytes (an int value) with the number of bytes the user
	// wants to read (a size_t value)
	if(readableBytes>=0 && (size_t)readableBytes < size){
		// if the user wants to read a number of bytes greater than the possible amount
		// then we read the possible amount and set the new read pointer
		bytesToRead = readableBytes;
	}
	else{
		// the user wants to read an amount of bytes which does not exceed the write pointer
		bytesToRead = size;
	}

	if( readPos_mod + readableBytes < BUFFER_SIZE ){
		// before reading, we control whether the amount to be read 
		// is contained in the interval between readPos_mod and the end of the buffer
		res= copy_to_user(out_buffer, (char *)(&(minorArray[minor][readPos_mod])), bytesToRead);
	}
	else{
		// in this case, we need to read the last bytes of the buffer and go back at the begin of the buffer
		// in order to complete the read

		// leggiamo gli ultimi bytes disponibili dal buffer
		// plus the leftover (which consists of the initial part of the buffer)
		res= copy_to_user(out_buffer, (char *)(&(minorArray[minor][readPos_mod])), BUFFER_SIZE - readPos_mod );

		//we compute the number of bytes to read at the begin of the buffer
		int leftover = bytesToRead - ( BUFFER_SIZE - readPos_mod );
		res+= copy_to_user(out_buffer, (char *)(&(minorArray[minor][0])), leftover );
	}
	readPos += bytesToRead;

	// we update the read module-pointer 
	// in this case the write pointer is the same as before
	readPos_mod=readPos%BUFFER_SIZE;

	/*res is the total of bytes unread: bytesToRead is the total of bytes that should have been read.
	 * so this is the total of bytes read. Va bene che dite? */
	return bytesToRead-res;
}

static long dharma_ioctl(struct file *filp,
	unsigned int cmd,
	unsigned long arg)
{
	int minor = iminor(filp->f_path.dentry->d_inode);
	int size;

	switch (cmd)
	{
		case DHARMA_SET_BLOCKING:
		break;
	}
	//solo per compilare
	return 0;
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
