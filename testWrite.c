#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include "dharma.h"

/** 
 * Set a file descriptor to blocking or non-blocking mode
 * using fcntl library function.
 *
 * @param fd The file descriptor
 * @param blocking 0:non-blocking mode, 1:blocking mode
 *
 * @return 1:success, 0:failure.
 **/

int main(void)
{
    int filedesc = open("/dev/dharma0", O_RDWR);
 
    if (filedesc < 0) {
		printf("There was an error opening dharma0\n");
        return -1;
    }
 
	printf("Testing write and stream, non-blocking read...\n");
	
	int wrote = 0;
    if ((wrote = write(filedesc, "123456789", 9)) != 9) {
        printf("There was an error writing to dharma0; wrote: %d\n", wrote);
        return -1;
    }
    
	close(filedesc);
 
    return 0;
}
