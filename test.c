#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
 
int main(void)
{
    int filedesc = open("/dev/dharma0", O_RDWR);
 
    if (filedesc < 0) {
		printf("There was an error opening dharma0\n");
        return -1;
    }
 
	int wrote = 0;
    if ((wrote = write(filedesc, "This will be output to dharma0\n", 31)) != 31) {
        printf("There was an error writing to dharma0; wrote: %d\n", wrote);
        return -1;
    }
    
    char data[128];
 
    if(read(filedesc, data, 128) < 0)
		printf("An error occurred in the read.\n");
		
	printf("Read data: %s\n", data);
 
    return 0;
}
