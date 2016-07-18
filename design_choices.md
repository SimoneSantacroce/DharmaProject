Design Choices
==============

*All or Nothing* v.s. *Best Effort*
------------------------------------

Since the project specification didn't put any contraints on the chosen semantic for the 
*read* and *write* operations (except for the **FIFO** rule) there are two possibilities
for the case in which from *user level* is requested to read/write **count** bytes while
the available amounts of bytes to be read/written is **less** than this amount.

So there are 2 possibilities for us to deal with this situation:

- **All or Nothing**: If the available bytes are **greather** than *count*, then the
  operation can be carried out, otherwise the operation **fails** and according to the
  mode, the process can either be put into a *wait queue* (**Blocking**) or return with
  an error to the user (**Non Blocking**).
- **Best Effort**: Even in the available bytes are **less** than *count*, the operation
  is still carried out up to the current capabilities of the buffer (so only an amount of
  *available* bytes is read/write from/to the buffer).
  
At the moment the design choiches we made are:

- The **read** (stream) is **Best Effort**
- The **write** is **All or Nothing**
