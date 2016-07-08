DHARMA PROJECT
========================

```c
init_mod()  // simoneOne the night king
{
	array[256];
}
```

```c
clean_mod()
{
	if(active_devices > 0)
	{
		kfree(array);
	}

	reutrn 0;
}
```

```c
open()
{
	if(array[minor] != NULL)
	{
		init_array(minor);
	}

	return 0;
}
```

```c
close()
{
	return ;
}
```

```c
read_stream()
{
	if(! buffer_is_empty())
	{
		copy_to_user();
		rd += size;
	} else {
		if (BLOCKING)
			wait_queue();

		if (NON_BLOCKING)
			return 0;
	}
}
```

```c
read_packet()
{
	if(! buffer_is_empty())
	{
		copy_to_user();
		rd += packet_size;
	} else {
		if (BLOCKING)
			wait_queue();

		if (NON_BLOCKING)
			return 0;
	}
}
```

```c
write()
{
	if(BLOCKING)
	{
		copy_from_user();
		wr += size;
		wait_queue();
	}

	if(NON_BLOCKING)
	{
		if(size > available)
		{
			errno = ENO_STRUNZ;
			return -1;
		}

		copy_from_user();
		wr += size;
	}
}
```
