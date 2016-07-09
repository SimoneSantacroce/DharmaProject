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
	if (argc < 3)
		goto help;

	int minor = argv[1];

	int mode_stream = 0;
	int mode_packet = 0;
	int blocking = 0;
	int not_blocking = 0;

	int n = 2;

	while ( n < argc )
	{
		if ( !strncmp(argv[n], "-s", 2) || !strncmp(argv[n], "-S", 2) )
		{
			mode_stream = 1;
		}

		if ( !strncmp(argv[n], "-p", 2) || !strncmp(argv[n], "-P", 2) )
		{
			mode_packet = 1;
		}

		if ( !strncmp(argv[n], "-b", 2) || !strncmp(argv[n], "-B", 2) )
		{
			blocking = 1;
		}

		if ( !strncmp(argv[n], "-nb", 3) || !strncmp(argv[n], "-NB", 3) )
		{
			not_blocking = 1;
		}

		++n;
	}

	if (mode_stream && mode_packet)
		goto help;

	if (blocking && not_blocking)
		goto help;

	char *file_name = "/dev/dharma";
	char new_file_name[strlen(file_name) + 1];
	int fd;

	strcpy(new_file_name, file_name);
	strcat(new_file_name, minor);

	fd = open(new_file_name, O_RDWR);

	if (fd == -1)
	{
		perror("ioctl open error\n");
		return 2;
	}

	if (mode_stream)
		set_read_mode(fd, DHARMA_STREAM_MODE);

	if (mode_packet)
		set_read_mode(fd, DHARMA_PACKET_MODE);

	if(blocking)
		set_blocking_flag(fd);

	if(not_blocking)
		reset_blocking_flag(fd);

	return 0;

help:
	fprintf(stderr, "Usage: %s minor [-s | -p] [-b | -nb]\n", argv[0]);
	return 1;
}
