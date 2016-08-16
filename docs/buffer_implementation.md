# Buffer implementation

Implementation based on memory frames analogy:
a buffer is like a memory bank divided into *frames*.

A frame contains a data segment (i.e. a *packet*), thus *FRAME_SIZE = PACKET_SIZE*.

The buffer is managed (read and write) in a *circular* fashion:
- all accesses are __*atomic*__
- a *write* operation simply writes the indicated amount of bytes, starting from the first *free __byte__* in the buffer (obviously enough free, *circularly-contiguous* buffer space is necessary to carry out the operation);  
  N.B. the *write* is completely *frame-agnostic*: we give no specification on where the written stuff falls inside the buffer (appropriate placement, if necessary, is responsibility of the user)
- a *read_stream* operation reads *up to* the requested amount of bytes (blocks only if *none* are available, if the op is blocking), and returns the amount of bytes read
- a *read_packet* operation reads the (residual) contents of the *oldest, non-empty* frame (blocks only if all frames are empty, if the op is blocking), and returns the amount of bytes read (may be less than *FRAME_SIZE*, for example if we interleaved stream and packet reads)

```c
/* CONSTANTS */
// proposed values (not set in stone)
int FRAME_SIZE = 4096; // 4KB
int BUFFER_SIZE = 10 * FRAME_SIZE;
```

```c
/* NECESSARY INFO for a buffer */
char *read_ptr;
char *write_ptr;
// that's it! info on "what packet (fragment) to return" can be computed
// easily from the read_ptr and the fact that packets begin at multiples of FRAME_SIZE:
// e.g. we return all bytes in [read_ptr, (read_ptr + FRAME_SIZE)/FRAME_SIZE*FRAME_SIZE)
```
