/**
 * Dharma main module
 */
#include "dharma.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Benjamin Linux");

char* minorArray[DEVICE_MAX_NUMBER];

int major;
spinlock_t buffer_lock[DEVICE_MAX_NUMBER];

/*Pos_mod is the position mod BUFFER_SIZE, Pos is without mod, used to check that 
 read is always less than write */
 /* these are all 0-initialized upon module mounting */
int readPos_mod[DEVICE_MAX_NUMBER];
int readPos[DEVICE_MAX_NUMBER];
int writePos_mod[DEVICE_MAX_NUMBER];
int writePos[DEVICE_MAX_NUMBER];

DECLARE_WAIT_QUEUE_HEAD(read_queue);
DECLARE_WAIT_QUEUE_HEAD(write_queue);

#define IS_EMPTY(minor) (readPos[minor] == writePos[minor])
#define O_PACKET 0x80000000


/* The operations */

static int dharma_open(struct inode *inode, struct file *file)
{
    int minor;
    try_module_get(THIS_MODULE);

    minor = iminor(inode);
    
    if (minor < DEVICE_MAX_NUMBER && minor >= 0) {
        if (minorArray[minor] == NULL)
            minorArray[minor] = kmalloc(BUFFER_SIZE, GFP_KERNEL);
            // Note: default is stream mode, blocking
        return 0;
    }
    else {
        return -EBUSY; // Why busy?
    }
}

static int dharma_release(struct inode *inode, struct file *file)
{
    module_put(THIS_MODULE);
    printk("releasing\n");
    // close does NOT invalidate the buffer
    // buffer stays in memory until the module is unmounted
    return 0;
}

static ssize_t dharma_write(struct file *filp, const char *buff, size_t count, loff_t *off){
    /*  Two valid alternatives for implementing the write; if there isn't enough space to write all the data: 
     *      1) I don't want to write only a part of the message
                => FAIL in non-blocking mode, put in a wait-queue in blocking mode
     *      2) I'm ok with writing only a part of the message
                => I will neither fail nor put the process into a wait queue,
                but I'll simply write what I can, and return "success"
     */

    int minor=iminor(filp->f_path.dentry->d_inode);
    int res = 0;
    
    printk("Write was called on dharma-device %d\n", minor);

    // acquire spinlock
    spin_lock(&(buffer_lock[minor]));
    
    // fail if the process wants to write data bigger than the buffer size
    if (count > BUFFER_SIZE) {
        return -EINVAL;
    }
    
    printk("Before space check\n");

    // check if there's sufficient space to perform the write
    while (readPos[minor]+BUFFER_SIZE < writePos[minor]+count) {
        printk("Not enough space available\n");
        //release the spinlock for writing
        spin_unlock(&(buffer_lock[minor]));
        //op mode is NON BLOCKING
        printk("Should we block?\n");
        if (filp->f_flags & O_NONBLOCK) {
            printk("No blocking\n");
            return -EAGAIN; //no data can be written, the buffer is full
        }
        //else op mode is BLOCKING
        printk("Yep. Sleeping on the write queue\n");
        if (wait_event_interruptible(write_queue, readPos[minor]+BUFFER_SIZE >= writePos[minor]+count))
                return -ERESTARTSYS; //-ERESTARTSYS is returned iff the thread is woken up by a signal
        // otherwise loop, but first re-acquire spinlock
        spin_lock(&(buffer_lock[minor]));
    }

    /* If we reach this point, we have exclusive access to the buffer
     * AND there is sufficient room into the buffer -> we can move on
     */
     
    printk("After space check\n");
    
    // Case 1) one copy_from_user is required
    if(count <= (BUFFER_SIZE - writePos_mod[minor])){
        res = copy_from_user((char*)(&(minorArray[minor][writePos_mod[minor]])), buff, count);
    }
    // Case 2) two copy_from_user are required
    else{
        int partial_count = BUFFER_SIZE-writePos_mod[minor];
        res  = copy_from_user((char*)(&(minorArray[minor][writePos_mod[minor]])), buff, partial_count);
        res += copy_from_user((char*)(&(minorArray[minor][0])), buff+partial_count, count - partial_count);
    }

    if(res != 0){
        res = -EINVAL; // if copy_from_user didn't return 0, there was a problem in the parameters. 
    }

    // If the copy_from_user succeeded (i.e., it returned 0), we need to update the write file pointer.
    if( res == 0 ){
        writePos[minor] += count;
        writePos_mod[minor] = writePos[minor] % BUFFER_SIZE;
        wake_up_interruptible(&read_queue); // also wake up reading processes
    }

    spin_unlock(&(buffer_lock[minor]));
    printk("Leaving the write\n");
    return count;
}

static ssize_t dharma_read_packet(struct file *filp, char *out_buffer, size_t size, loff_t *offset) {

    int minor=iminor(filp->f_path.dentry->d_inode);
    int res = 0;

    // acquire spinlock
    spin_lock(&(buffer_lock[minor]));

    while (IS_EMPTY(minor)) {
        // release spinlock
        spin_unlock(&(buffer_lock[minor]));
        //if op is non blocking. EGAIN:resource is temporarily unavailable
        if (filp->f_flags & O_NONBLOCK)
            return -EAGAIN;
            
        /* insert into wait queue. wait_event_interruptible returns ERESTARTSYS
         * if it is interrupted by a signal. The system call can be re-executed if there is some
         * interruption*/
         /*questo è quello che ho capito da qui:
          * http://stackoverflow.com/questions/9576604/what-does-erestartsys-used-while-writing-linux-driver */
        if(wait_event_interruptible(read_queue, !IS_EMPTY(minor)))
            return -ERESTARTSYS;
        // otherwise loop, but first re-acquire spinlock
        spin_lock(&(buffer_lock[minor]));
        // this way once a process ceases to spin, it will actually check if buffer is still non-empty
        // or if the previous guy consumed everything
    }
    // if we get here, then data is in the buffer AND we have exclusive access to it: we're ready to go.
    
    int residual = PACKET_SIZE-readPos_mod[minor]%PACKET_SIZE; // how many bytes we have to read effectively

    //bytes that are missing to get to the end of the packet.Used later.
    int to_end = residual;

    //check there are residual bytes available: writePos-readPos are the unread bytes
    /* Note from Rob: we've to use writePos and readPos (instead of writePos_mod and readPos_mod)
     * because we could have a situation where writePos_mod points to the beginning of the buffer
     * and readPos_mod points to the end of the buffer. In these cases, residual would become negative. 
     */
    if(residual > writePos[minor]-readPos[minor]){
        /*CHOICE: can be discussed. If there are less than residual bytes, I read all bytes available,
         even if they are less than a packet*/
        residual = writePos[minor]-readPos[minor];
    }
    res= copy_to_user(out_buffer, (char *)(&(minorArray[minor][readPos_mod[minor]])), residual);
    
    //if res>0, it means an unexpected error happened, so we abort the operation (=not update pointers)
    if(res!=0){
        //as if 0 bytes were read. Exit
        spin_unlock(&(buffer_lock[minor]));
        return -EINVAL;
    }
    //Note: if we arrive here, everything was read. OK
    
    wake_up_interruptible(&write_queue);
    
    //update readPos
    readPos[minor] += residual;
    /*if I read less than a packet(or its residual), it means that readPos (line before) was updated 
     * in a way it does not coincide with the end of the packet, so I must
     * add to readPos the quantity it needs to arrive at the end of the packet, and since writePos 
     * too is not placed at the end of the frame, I update it to make it coincide with the new readPos, 
     * that means the buffer is now empty*/
    if(residual == writePos[minor] - readPos[minor]){
        readPos[minor] += (to_end-residual);
        writePos[minor] = readPos[minor];
        writePos_mod[minor] = writePos[minor] % BUFFER_SIZE;
    }

    readPos_mod[minor] = readPos[minor] % BUFFER_SIZE;

    spin_unlock(&(buffer_lock[minor]));
    return residual;


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

    while (IS_EMPTY(minor)) {
        // release spinlock
        spin_unlock(&(buffer_lock[minor]));
        //if op is non blocking
        if (filp->f_flags & O_NONBLOCK)
            // different choice with respect to the read_packet case, because:
            // return -1 represents an error, instead I think that in this case
            // we have to return "no bytes read" = "0 bytes read"
            return 0;
        
        if(wait_event_interruptible(read_queue, !IS_EMPTY(minor)))
            return -ERESTARTSYS;
        // otherwise loop, but first re-acquire spinlock
        spin_lock(&(buffer_lock[minor]));
        // this way once a process ceases to spin, it will actually check if buffer is still non-empty
        // or if the previous guy consumed everything
    }
    // if we get here, then data is in the buffer AND we have exclusive access to it: we're ready to go.

    //return value
    int res = 0;

    // how many bytes we can read
    int readableBytes = writePos[minor] - readPos[minor];

    /*
        In the previous version the operation was: int readableBytes = writePos_mod - readPos_mod
        but in this way, if the write pointer precedes the read pointer (legal case if the writePos > readPos)
        we will obtain a negative value of readableBytes!
    */

    // amount of bytes to read (updated during the control phase)
    int bytesToRead = 0;

    // we compare the readable bytes (an int value) with the number of bytes the user
    // wants to read (a size_t value)
    if(readableBytes >= 0 && (size_t)readableBytes < size){
        // if the user wants to read a number of bytes greater than the possible amount
        // then we read the possible amount and set the new read pointer
        bytesToRead = readableBytes;
    }
    else{
        // the user wants to read an amount of bytes which does not exceed the write pointer
        bytesToRead = size;
    }

    if( readPos_mod[minor] + readableBytes < BUFFER_SIZE ){
        // before reading, we control whether the amount to be read
        // is contained in the interval between readPos_mod and the end of the buffer
        res = copy_to_user(out_buffer, (char *)(&(minorArray[minor][readPos_mod[minor]])), bytesToRead);
    }
    else{
        // in this case, we need to read the last bytes of the buffer and go back at the begin of the buffer
        // in order to complete the read

        // leggiamo gli ultimi bytes disponibili dal buffer
        // plus the leftover (which consists of the initial part of the buffer)
        res = copy_to_user(out_buffer, (char *)(&(minorArray[minor][readPos_mod[minor]])), BUFFER_SIZE - readPos_mod[minor] );

        //we compute the number of bytes to read at the begin of the buffer
        int leftover = bytesToRead - ( BUFFER_SIZE - readPos_mod[minor] );
        res += copy_to_user(out_buffer, (char *)(&(minorArray[minor][0])), leftover );
    }
    
    //if res>0, it means an unexpected error happened, so we abort the operation (=not update pointers)
    if(res != 0){
        //as if 0 bytes were read. Exit
        spin_unlock(&(buffer_lock[minor]));
        return -EINVAL;
    }
    
    //Note: if we arrive here, everything was read. OK
    
    wake_up_interruptible(&write_queue);
    
    readPos[minor] += bytesToRead;

    // we update the read module-pointer
    // in this case the write pointer is the same as before
    readPos_mod[minor] = readPos[minor] % BUFFER_SIZE;

    spin_unlock(&(buffer_lock[minor]));
    return bytesToRead;
}

static ssize_t dharma_read(struct file *filp, char *out_buffer, size_t size, loff_t *offset) {
    if ((unsigned long)filp->private_data & O_PACKET)
        return dharma_read_packet(filp, out_buffer, size, offset);
    return dharma_read_stream(filp, out_buffer, size, offset);
}

static long dharma_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
    int minor = iminor(filp->f_path.dentry->d_inode);
    switch(cmd){
    case DHARMA_SET_PACKET_MODE :
            printk("Packet mode now is active on dharma-device %d\n", minor);
            filp->private_data = (void *) ((unsigned long)filp->private_data | O_PACKET);
            break;
        case DHARMA_SET_STREAM_MODE :
            printk("Stream mode now is active on dharma-device %d\n", minor);
            filp->private_data = (void *) ((unsigned long)filp->private_data & ~O_PACKET);
            break;
        
        case DHARMA_SET_BLOCKING :
            printk("Blocking mode now is active on dharma-device %d\n", minor);
            filp->f_flags &= ~O_NONBLOCK;
            break;
        case DHARMA_SET_NONBLOCKING :
            printk("Non blocking mode now is active on dharma-device %d\n", minor);
            filp->f_flags |= O_NONBLOCK;
            break;
        default :
            return -EINVAL; // invalid argument
    }
    return 0;
}

int init_module(void){

    // Dynamic allocation of the major number (through the first parameter = 0).
    // The function registers a range of 256 minor numbers; the first minor number is 0.
    major = register_chrdev(0, DEVICE_NAME, &fops);

    if (major < 0) {
        printk("Registering dharma device failed\n");
        return major;
    }

    printk(KERN_INFO "Dharma device registered, it is assigned major number %d\n", major);

    return 0;
}

void cleanup_module(void){

    unregister_chrdev(major, DEVICE_NAME);

    printk(KERN_INFO "Dharma device unregistered, it was assigned major number %d\n", major);
}
