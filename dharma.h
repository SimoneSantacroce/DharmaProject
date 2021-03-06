/*
* The darma header
*/
#include <linux/ioctl.h>

#define EXPORT_SYMTAB

#define DEVICE_NAME "dharma"
#define DEVICE_MAX_NUMBER 256
#define BUFFER_SIZE 40
#define PACKET_SIZE 4

/* IOCTL related macros */
#define DHARMA_MAJOR 				250
#define DHARMA_SET_PACKET_MODE 		_IO(DHARMA_MAJOR, 0)
#define DHARMA_SET_STREAM_MODE 		_IO(DHARMA_MAJOR, 1)
#define DHARMA_SET_BLOCKING 		_IO(DHARMA_MAJOR, 2)
#define DHARMA_SET_NONBLOCKING 		_IO(DHARMA_MAJOR, 3)

//MODIFICA SARA
#define DHARMA_PACKET_MODE 1
#define DHARMA_STREAM_MODE 0
