/* tapclient.c */
#include<string.h>
#include<stdio.h>
#include<net/if.h>
#include<unistd.h>
#include<fcntl.h>
#include<linux/if_tun.h>
#include<errno.h>
#include<stdlib.h>
#include<sys/ioctl.h>

/* Borrowed heavily from http://backreference.org/2010/03/26/tuntap-interface-tutorial/ */

int tun_alloc(char *dev, int flags) {
	struct ifreq ifr;
	int fd;
	int err;
	char *clone_dev = "/dev/net/tun";

	/* open the clone device */
	if( (fd = open(clone_dev, O_RDWR)) < 0 ) {
		return fd;
	}

	/* preparation of the struct ifr, of type "struct ifreq" */
	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags = flags;   /* IFF_TUN or IFF_TAP, plus maybe IFF_NO_PI */

	/* if a device name was specified, put it in the structure */
	if (*dev) {
		strncpy(ifr.ifr_name, dev, IFNAMSIZ);
	}

	/* try to create the device */
	if( (err = ioctl(fd, TUNSETIFF, (void *) &ifr)) < 0 ) {
		close(fd);
		return err;
	}

	/* if the operation was successful, write back the name of the
	 * interface to the variable "dev", so the caller can know
	 * it. Note that the caller MUST reserve space in *dev */
	strcpy(dev, ifr.ifr_name);

	/* return file descriptor to tap device */
	return fd;
}

int main() {
	char tap_name[IFNAMSIZ];
	char tap_name1[IFNAMSIZ];
	/* Connect to the device */
	strcpy(tap_name, "tap0");
	int tap_fd = tun_alloc(tap_name, IFF_TAP | IFF_NO_PI);  /* tun interface */

	strcpy(tap_name1, "tap1");
	int tap_fd2 = tun_alloc(tap_name1, IFF_TAP | IFF_NO_PI);  /* tun interface */

	if(tap_fd < 0){
		perror("Allocating interface");
		exit(1);
	}

	/* Now read data coming from the kernel */
	while(1) {
		/* Note that "buffer" should be at least the MTU size of the interface, eg 1500 bytes */
		char buffer[2000];
		int nread = read(tap_fd,(void *)buffer,sizeof(buffer));
		if(nread < 0) {
			perror("Reading from interface");
			close(tap_fd);
			exit(1);
		}

		/* Do whatever with the data */
		printf("Read %d bytes from device %s\n", nread, tap_name);
		buffer[10]='h';
		write( tap_fd2, (void*) buffer, nread);
	}
}
