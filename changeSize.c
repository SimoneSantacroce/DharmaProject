#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include "dharma.h"
#include <stdlib.h>


/** 
 * Set a file descriptor to blocking or non-blocking mode
 * using fcntl library function.
 *
 * @param fd The file descriptor
 * @param blocking 0:non-blocking mode, 1:blocking mode
 *
 * @return 1:success, 0:failure.
 **/

int main( int argc, char *argv[] )
{
    int numBytes = 0;
    
    int filedesc = open("/dev/dharma0", O_RDWR);
    
    if (filedesc < 0) {
		printf("There was an error opening dharma0\n");
        return -1;
    }
    
    if( argc == 2 ) {
        // Get the new buffer size from cli.
        numBytes = atoi(argv[1]);
        if( ioctl(filedesc, DHARMA_SET_BUFFER_SIZE, &numBytes) == 0){
            printf("Set buffer size = %d.\n", numBytes);
            if( ioctl(filedesc, DHARMA_GET_BUFFER_SIZE, &numBytes) == 0)
                printf("New buffer size is %d.\n", numBytes);
        }
    }
    else {
        printf("Usage: changeSize n, where:\n");
        printf("- n is the new size of the buffer (in bytes).\n");
        printf("Exiting.\n");
        return 0;
    }
    
    return 0;
}
