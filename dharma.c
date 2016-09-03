/**
 * Dharma main module
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/fcntl.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include <asm/atomic.h>

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

/* Fabrizio: L'idea è di utilizzare un array minorSize di "buffer_size"
 * per tenere traccia della taglia dei buffer per i vari minor.
 * Ho scelto di farlo atomico per essere più parato... 
 * ma credo che un semplice int vada bene lo stesso...
 * minorSize è iniziaizzato a BUFFER_SIZE. */
atomic_t minorSize[DEVICE_MAX_NUMBER];

DECLARE_WAIT_QUEUE_HEAD(read_queue);
DECLARE_WAIT_QUEUE_HEAD(write_queue);

#define IS_EMPTY(minor) (readPos[minor] == writePos[minor])
#define O_PACKET 0x80000000


/* The operations */

static int dharma_open(struct inode *inode, struct file *file)
{
    int minor;
    try_module_get(THIS_MODULE);

    printk(KERN_INFO "open called\n");

    minor = iminor(inode);
    printk(KERN_INFO "minor is %d\n", minor);
    if (minor < DEVICE_MAX_NUMBER && minor >= 0) {
        if (minorArray[minor] == NULL)
            minorArray[minor] = kmalloc(BUFFER_SIZE, GFP_KERNEL);
            // Fabrizio: Inizializzazione minorSize[minor]
            atomic_set(&(minorSize[minor]), BUFFER_SIZE);
            // Note: default is stream mode, blocking
        return 0;
    }
    else {
        printk(KERN_INFO "minor not allowed\n");
        return -ENODEV; // No such device
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

    int minor = iminor(filp->f_path.dentry->d_inode);
    int res = 0;

    printk("Write was called on dharma-device %d\n", minor);



    // fail if the process wants to write data bigger than the buffer size
    if (count > BUFFER_SIZE) {
        return -EINVAL;
    }

    // acquire spinlock
    spin_lock(&(buffer_lock[minor]));

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
        printk("buffer address at first copy is %p, at second is %p\n", buff, buff+partial_count);
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
        printk("write pos mod is %d\n", writePos_mod[minor]);
        wake_up_interruptible(&read_queue); // also wake up reading processes
    }

    spin_unlock(&(buffer_lock[minor]));
    printk("Leaving the write\n");
    return count;
}

static ssize_t dharma_read_packet(struct file *filp, char *out_buffer, size_t size, loff_t *offset) {

    int minor=iminor(filp->f_path.dentry->d_inode);
    int res = 0;
    int residual;
    int to_end;
    int byte_read = 0;
    printk("Read-Packet was called on dharma-device %d\n", minor);


    // acquire spinlock
    spin_lock(&(buffer_lock[minor]));

    printk("Before buffer check\n");

    while (IS_EMPTY(minor)) {
        printk("Buffer is empty\n");
        // release spinlock
        spin_unlock(&(buffer_lock[minor]));
        //if op is non blocking. EGAIN:resource is temporarily unavailable
        printk("Should we block?\n");
        if (filp->f_flags & O_NONBLOCK) {
            printk("No blocking\n");
            return -EAGAIN; //mettere 0 per evitare @______@ 
        }

        /* insert into wait queue. wait_event_interruptible returns ERESTARTSYS
         * if it is interrupted by a signal. The system call can be re-executed if there is some
         * interruption*/
         /*questo è quello che ho capito da qui:
          * http://stackoverflow.com/questions/9576604/what-does-erestartsys-used-while-writing-linux-driver */
        printk("Sleeping on the read queue\n");
        if(wait_event_interruptible(read_queue, !IS_EMPTY(minor)))
            return -ERESTARTSYS;
        // otherwise loop, but first re-acquire spinlock
        spin_lock(&(buffer_lock[minor]));
        // this way once a process ceases to spin, it will actually check if buffer is still non-empty
        // or if the previous guy consumed everything
    }
    // if we get here, then data is in the buffer AND we have exclusive access to it: we're ready to go.

    printk("After buffer check\n");

    residual = PACKET_SIZE-readPos_mod[minor]%PACKET_SIZE; // how many bytes we have to read effectively

    //bytes that are missing to get to the end of the packet.Used later.
    to_end = residual;

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

    /* NOTA DI ROB: prima di chiamare copy_to_user, dobbiamo controllare che il chiamante abbia
     * spazio a sufficienza in out_buffer per contenere il pacchetto. Se questo spazio non c'è,
     * bisogna ridurre ulteriormente il valore di residual.
     */
    if( residual > size ){
        //residual = size;
	res= copy_to_user(out_buffer, (char *)(&(minorArray[minor][readPos_mod[minor]])), size);
	byte_read=size;
    }
    else {
        res= copy_to_user(out_buffer, (char *)(&(minorArray[minor][readPos_mod[minor]])), residual);
	byte_read=residual;
    }
    //if res>0, it means an unexpected error happened, so we abort the operation (=not update pointers)
    if(res!=0){
        //as if 0 bytes were read. Exit
        spin_unlock(&(buffer_lock[minor]));
        return -EINVAL;
    }
    //Note: if we arrive here, everything was read. OK

    //update readPos
    /* if I read less than a packet(or its residual), it means that readPos (line before) was updated
     * in a way it does not coincide with the end of the packet, so I must
     * add to readPos the quantity it needs to arrive at the end of the packet, and since writePos
     * too is not placed at the end of the frame, I update it to make it coincide with the new readPos,
     * that means the buffer is now empty*/
    /* NOTA DI SARA: se entriamo nel blocco if alla riga 221 (ovvero 'if(residual>size)'),
     * allora potremmo non entrare nel ramo if qui sotto (nel caso in cui writePos[minor]-readPos[minor] > size).
     * Quindi alla fine della read_packet, ci ritroveremmo quindi con un indice di lettura
     * non posizionato all'inizio di un pacchetto.
     * SOLUZIONE PROPOSTA DA ROB: sostituiamo la condizione nell'if qui sotto con 'if(residual < to_end)'
     * (dove 'to_end' in precedenza viene settato alla taglia del prossimo pacchetto).
     * In questo modo, entriamo nell'if anche nel caso di cui parlava Sara.   
     */
    if(residual < to_end){ 
        readPos[minor] += to_end;
        writePos[minor] = readPos[minor];
        writePos_mod[minor] = writePos[minor] % BUFFER_SIZE;
    }
    else{
  	    readPos[minor] += residual;
    }

    readPos_mod[minor] = readPos[minor] % BUFFER_SIZE;

    // VEDI NOTA DI ROB nella read_stream (LOC 395)
    wake_up_interruptible(&write_queue);

    spin_unlock(&(buffer_lock[minor]));
    return byte_read;


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
        //I want to read a fraction of packet smaller than the already existent residual.
        //But since this is read packet I cannot leave residuals, so readPos will jump to the next packet
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
        //read size bytes; if size is not multiple of a packet, discard the rest of the packet and
        //update readPos as if an entire packet was read. Out buffer must be incremented because I
        //read the residual
        ret=copy_to_user((char *)(&(out_buffer[residual])), (char *)(&(minorArray[minor][readPos_mod])), size-residual);

        //update reading position. +1 is here because I discard the residual of bytes of the packet I
        //did not read
        readPos+=((size/PACKET_SIZE)+1)*PACKET_SIZE;
        readPos_mod=readPos%BUFFER_SIZE;

        //release spinlock
        spin_unlock(&(buffer_lock[minor]));
    }
    return ret;

    */

}

static ssize_t dharma_read_stream(struct file *filp, char *out_buffer, size_t size, loff_t *offset) {

    int minor=iminor(filp->f_path.dentry->d_inode);
    int res;
    int bytesToRead;
    int readableBytes;
    int leftover;

    printk("Read-Stream was called on dharma-device %d\n", minor);

    // acquire spinlock
    spin_lock(&(buffer_lock[minor]));

    printk("Before buffer check\n");

    while (IS_EMPTY(minor)) {
        printk("Buffer is empty\n");
        // release spinlock
        spin_unlock(&(buffer_lock[minor]));
        //if op is non blocking
        printk("Should we block?\n");
        if (filp->f_flags & O_NONBLOCK) {
            printk("No blocking\n");
            // different choice with respect to the read_packet case, because:
            // return -1 represents an error, instead I think that in this case
            // we have to return "no bytes read" = "0 bytes read"
            return 0;
        }

        printk("Sleeping on read queue\n");
        if(wait_event_interruptible(read_queue, !IS_EMPTY(minor)))
            return -ERESTARTSYS;
        // otherwise loop, but first re-acquire spinlock
        spin_lock(&(buffer_lock[minor]));
        // this way once a process ceases to spin, it will actually check if buffer is still non-empty
        // or if the previous guy consumed everything
    }
    // if we get here, then data is in the buffer AND we have exclusive access to it: we're ready to go.

    printk("After buffer check\n");

    //return value
    res = 0;

    // how many bytes we can read
    readableBytes = writePos[minor] - readPos[minor];

    printk("readableBytes = %d\n", readableBytes);
    /*
        In the previous version the operation was: int readableBytes = writePos_mod - readPos_mod
        but in this way, if the write pointer precedes the read pointer (legal case if the writePos > readPos)
        we will obtain a negative value of readableBytes!
    */

    // amount of bytes to read (updated during the control phase)
    bytesToRead = 0;

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

    printk("bytesToRead = %d computed.\n", bytesToRead);

    /* NOTA DI ROB: credo che qui dovrebbe essere '<=', anziché '<'
     * Vedi caso in cui readPos_mod = 0, BUFFER_SIZE = 40 e bytesToRead = 40:
     * non ci sarebbe bisogno di fare due copy_to_user (la variabile leftover sarebbe pari a 0)
     */
    if( readPos_mod[minor] + bytesToRead < BUFFER_SIZE ){
        printk("Case 1: one read is needed.\n");
        // before reading, we control whether the amount to be read
        // is contained in the interval between readPos_mod and the end of the buffer
        res = copy_to_user(out_buffer, (char *)(&(minorArray[minor][readPos_mod[minor]])), bytesToRead);
    }
    else{
        printk("Case 2: two reads are required.\n");
        // in this case, we need to read the last bytes of the buffer and go back at the begin of the buffer
        // in order to complete the read

        // leggiamo gli ultimi bytes disponibili dal buffer
        // plus the leftover (which consists of the initial part of the buffer)
        res = copy_to_user(out_buffer, (char *)(&(minorArray[minor][readPos_mod[minor]])), BUFFER_SIZE - readPos_mod[minor] );

        //we compute the number of bytes to read at the begin of the buffer
        leftover = bytesToRead - ( BUFFER_SIZE - readPos_mod[minor] );
        res += copy_to_user(out_buffer+( BUFFER_SIZE - readPos_mod[minor] ), (char *)(&(minorArray[minor][0])), leftover );
    }

    //if res>0, it means an unexpected error happened, so we abort the operation (=not update pointers)
    if(res != 0){
        //as if 0 bytes were read. Exit
        spin_unlock(&(buffer_lock[minor]));
        return -EINVAL;
    }

    //Note: if we arrive here, everything was read. OK


    readPos[minor] += bytesToRead;

    // we update the read module-pointer
    // in this case the write pointer is the same as before
    readPos_mod[minor] = readPos[minor] % BUFFER_SIZE;

    /* NOTA DI ROB: la wake-up, secondo me, dovrebbe stare qui, cioè dopo l'aggiornamento dei puntatori readPos,
     *              e non prima, altrimenti c'è il rischio che la coda si svegli e, dato che i puntatori readPos
     *              sono rimasti invariati, torni a dormire, anche se ci sarebbe spazio a sufficienza per
     *              svegliare qualcuno.
     *              Fatemi sapere che ne pensate.
     */
    wake_up_interruptible(&write_queue);

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
        case DHARMA_GET_BUFFER_SIZE : // Fabrizio: Case "GET_BUFFER_SIZE"
			printk("Returning current buffer size for dharma-device %d...\n", minor);
			int size = atomic_read(&(minorSize[minor]));
            int res1 = copy_to_user((int *) arg, &size , sizeof(int));
            if(res1 != 0)
				return -EINVAL; // if copy_from_user didn't return 0, there was a problem in the parameters.
			printk("Buffer size for dharma-device %d read.\n", minor);
			break;
		case DHARMA_SET_BUFFER_SIZE : // Fabrizio: Case "SET_BUFFER_SIZE"
			printk("Updating current buffer size for dharma-device %d...\n", minor);
			int newsize;
			int res2 = copy_from_user(&size, (int *) arg, sizeof(int));
			if(res2 != 0)
				return -EINVAL; // if copy_from_user didn't return 0, there was a problem in the parameters.
			if(newsize % PACKET_SIZE != 0) // Fabrizio: Nuova size non multipla di packet size!
				return -EINVAL;
				
			/* MODIFICA SARA */
			/* secondo me serve per evitare che read concorrenti modificano i puntatori 
			 * nel buffer */
			spin_lock(&(buffer_lock[minor]));
			
			if(newsize < writePos[minor] - readPos[minor]) // Fabrizio: Nuova size non sufficiente.
				return -EINVAL;
			int oldsize= atomic_read(&minorSize[minor]);
			atomic_set(&minorSize[minor], newsize);
			printk("Buffer size for dharma-device %d updated.\n", minor);
			
			
			/* alloco un nuovo buffer che sicuramente può contenere la roba non letta*/
			char * new_buffer=kmalloc(newsize, GFP_KERNEL);
			if(readPos_mod[minor] < writePos_mod[minor]){
				/* _____rpos*******wpos______  le stelline sono i byte da copiare */
				strncpy(new_buffer, minorArray[minor]+readPos_mod[minor], writePos_mod[minor]-readPos_mod[minor] );
			}
			else if(readPos_mod[minor] > writePos_mod[minor]){
				/* ******wpos_______rpos******  le stelline sono i byte da copiare */
				strncpy(new_buffer, minorArray[minor]+readPos_mod[minor], oldsize- readPos_mod[minor] );
				strncpy(new_buffer +oldsize- readPos_mod[minor], minorArray[minor], writePos_mod[minor] );
			}
			/* aggiorno i puntatori*/
			readPos[minor]=0;
			writePos[minor]=writePos[minor]-readPos[minor];
			
			kfree(minorArray[minor]);
			minorArray[minor]=new_buffer;
			
			spin_unlock(&(buffer_lock[minor]));
			break;
        default :
            return -EINVAL; // invalid argument
    }
    return 0;
}

// Module declarations

struct file_operations fops = {
    .read           = dharma_read,
    .write          = dharma_write,
    .open           = dharma_open,
    .release        = dharma_release,
    .unlocked_ioctl = dharma_ioctl
};

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
