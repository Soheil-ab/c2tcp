#ifndef TAP_DEVICE_HH
#define TAP_DEVICE_HH

#include<string.h>
#include<stdio.h>
#include<net/if.h>
#include<unistd.h>
#include<fcntl.h>
#include<linux/if_tun.h>
#include<errno.h>
#include<stdlib.h>
#include<sys/ioctl.h>

int tun_alloc(char *dev, int flags) ;
int setup_tap( void );

#endif
