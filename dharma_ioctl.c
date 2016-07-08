#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>

#include "dharma_ioctl.h"

void get_read_mode(int fd)
{
	int res;
	if (ioctl(fd, DHARMA_GET_READ_MODE, &res) == -1)
	{
		perror("dharma ioctl get read mode\n");
	}
	else
	{
		printf("Read mode : %d\n", res);
	}
}

void set_read_mode(int fd , char *argv)
{
	int res = atoi(argv);

	if (ioctl(fd, DHARMA_SET_READ_MODE, &res) == -1)
	{
		perror("dharma ioctl set read mode\n");
	}
	else
	{
		printf("Read mode set to: %d\n", res);
	}
}

void set_blocking_flag(int fd)
{
	int res;
	if (ioctl(fd, DHARMA_SET_BLOCKING, &res) == -1)
	{
		perror("dharma ioctl set blocking flag\n");
	}
	else
	{
		printf("Blocking flag setted\n");
	}
}

void reset_blocking_flag(int fd)
{
	int res;
	if (ioctl(fd, DHARMA_RESET_BLOCKING, &res) == -1)
	{
		perror("dharma ioctl reset blocking flag\n");
	}
	else
	{
		printf("Blocking flag resetted\n");
	}
}

int main(int argc, char const *argv[])
{
	char *file_name = "/dev/dharma";

	/* Set the flags */

	return 0;
}
