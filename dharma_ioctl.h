#include <linux/ioctl.h>

/* IOCTL */
#define DHARMA_GET_READ_MODE	_IOR(250, 0, int)
#define DHARMA_SET_READ_MODE	_IOW(250, 1, int)
#define DHARMA_SET_BLOCKING 	_IO(250, 2)
#define DHARMA_RESET_BLOCKING 	_IO(250, 3)
