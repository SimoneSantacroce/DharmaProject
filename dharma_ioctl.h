#include <linux/ioctl.h>

/* IOCTL */
#define DHARMA_TYPE 			250
#define DHARMA_GET_READ_MODE 	_IOR(DHARMA_TYPE, 0, int)
#define DHARMA_SET_READ_MODE 	_IOW(DHARMA_TYPE, 1, int)
#define DHARMA_SET_BLOCKING 	_IO(DHARMA_TYPE, 2)
#define DHARMA_RESET_BLOCKING 	_IO(DHARMA_TYPE, 3)

#define DHARMA_STREAM_MODE 0
#define DHARMA_PACKET_MODE 1
