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
    
    if( argc == 3 ) {
        
        numBytes = atoi(argv[1]);
        
        if(strcmp("p",argv[2]) == 0){
            if( ioctl(filedesc, DHARMA_SET_PACKET_MODE, 0) == 0)
                printf("Set PACKET mode\n");
        }
    }
    else {
        printf("Usage: testRead n m, where:\n");
        printf("- n is the number of bytes you want to read.\n");
        printf("- m is the read mode: type 'p' for packet and 's' for string.\n");
        printf("The default values are n = 0 and m = s.\n");
    }

    if (filedesc < 0) {
		printf("There was an error opening dharma0\n");
        return -1;
    }
 
	char data[numBytes+1];
 
    int res2;
	res2 = read(filedesc, data, numBytes);
	if( res2 < 0 )
		printf("An error occurred in the read.");
	
	printf("Read data:\n%s\n", data);
	
	close(filedesc);
 
    return 0;
}
