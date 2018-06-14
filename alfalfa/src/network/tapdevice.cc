#include "tapdevice.hh"

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
  
  /* zero out struct ifr, of type "struct ifreq" */
  memset(&ifr, 0, sizeof(ifr));
  ifr.ifr_flags = flags;   /* IFF_TUN or IFF_TAP, optionally IFF_NO_PI */
  
  /* if a device name is supplied, use it */
  if (*dev) {
  	strncpy(ifr.ifr_name, dev, IFNAMSIZ);
  }
  
  /* try to create the device */
  if( (err = ioctl(fd, TUNSETIFF, (void *) &ifr)) < 0 ) {
  	close(fd);
  	return err;
  }
  
  /* if successful, write back device name to the variable "dev" */
  strcpy(dev, ifr.ifr_name);
  
  /* return file descriptor to tap device */
  return fd;
}

int setup_tap( void ) {
  char tap_name[ IFNAMSIZ ];
  strcpy( tap_name, "tap0" );
  int tap_fd = tun_alloc( tap_name, IFF_TAP | IFF_NO_PI );
  if ( tap_fd < 0 ) {
    perror( "Allocating interface" );
    exit( 1 );
  }
  return tap_fd;
}
