## How to compile and run

### Compiling

The module can be *compiled* with the command:

```shell
make
```

It is possible to clean the compiled binaries with:

```shell
make clean
```


### Installing

Let's first install the module with:

```shell
sudo make insert
```

Then create a `dharma` device (e.g. with major `250` and minor `0`) using:

```shell
sudo mknod /dev/dharma0 c 250 0
```

Finally change the permissions to use the device:

```shell
sudo chgrp staff /dev/dharma0
sudo chmod 666 /dev/dharma0
```

It is possible to delete the devices with:

```shell
sudo rm -f /dev/dharma0
```


### Running

**TODO!** (please fill this in)
