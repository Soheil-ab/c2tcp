# verus
Verus is an adaptive congestion control protocol that is custom designed for cellular networks.

### Build Instructions:
Required packages: libtbb libasio libalglib libboost-system

Steps (tested on Ubuntu 14.04.1):
```sh
$ sudo apt-get install build-essential autoconf libtbb-dev libasio-dev libalglib-dev libboost-system-dev
$ autoreconf -i
$ ./configure
$ make
```
