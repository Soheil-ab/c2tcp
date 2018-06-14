#include <sys/socket.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/types.h>
#include <string.h>
#include <arpa/inet.h>
#include <assert.h>

#include "packetsocket.hh"

using namespace std;

int PacketSocket::get_index( const std::string & name ) const
{
  /* Find index number from name */
  struct ifreq ifr;

  strncpy( ifr.ifr_name, name.c_str(), IFNAMSIZ );
  if ( ioctl( sock, SIOCGIFINDEX, &ifr ) < 0 ) {
    perror( "ioctl" );
    exit( 1 );
  }

  fprintf( stderr, "Interface %s has index %d\n", name.c_str(), ifr.ifr_ifindex );
  
  return ifr.ifr_ifindex;
}

PacketSocket::PacketSocket( const std::string & s_interface,
			    const std::string & s_from_filter,
			    const std::string & s_to_filter )
  : sock( socket( AF_PACKET, SOCK_RAW, htons( ETH_P_ALL ) ) ),
    _from_filter( MACAddress::parse_human( s_from_filter ) ),
    _to_filter( MACAddress::parse_human( s_to_filter ) )
{
  /* create packet socket */

  if ( sock < 0 ) {
    perror( "socket" );
    exit( 1 );
  }

  /* Get interface index */
  int index = get_index( s_interface );

  /* Bind packet socket to interface */
  struct sockaddr_ll sll;
  sll.sll_family = AF_PACKET;
  sll.sll_protocol = htons( ETH_P_ALL );
  sll.sll_ifindex = index;

  if ( bind( sock, (struct sockaddr *)&sll, sizeof( sll ) ) < 0 ) {
    perror( "bind" );
    exit( 1 );
  }

  /* Set to promiscuous mode */
  struct packet_mreq pmr;
  pmr.mr_ifindex = index;
  pmr.mr_type = PACKET_MR_PROMISC;

  if ( setsockopt( sock, SOL_SOCKET, PACKET_ADD_MEMBERSHIP, &pmr, sizeof( pmr ) ) < 0 ) {
    perror( "setsockopt promisc" );
    exit( 1 );
  }
}

PacketSocket::PacketSocket()
  : sock ( -1 ),
    _from_filter( MACAddress::parse_human( "" ) ),
    _to_filter( MACAddress::parse_human(""))
{}

vector< string > PacketSocket::recv_raw( void )
{
  vector< string > ret;

  const int BUFFER_SIZE = 2048;

  char buf[ BUFFER_SIZE ];

  ssize_t bytes_read = recvfrom( sock, buf, BUFFER_SIZE, MSG_TRUNC, nullptr, nullptr );
  if ( bytes_read < 0 ) {
    perror( "recvfrom" );
    exit( 1 );
  } else if ( bytes_read > BUFFER_SIZE ) {
    fprintf( stderr, "Received size (%ld) too long (> %d)!\n",
	     bytes_read, BUFFER_SIZE );
    exit( 1 );
  }

  const string packet( buf, bytes_read );

  assert( packet.size() > 12 );

  const MACAddress destination_address( packet.substr( 0, 6 ) );
  const MACAddress source_address( packet.substr( 6, 6 ) );

  if (  _to_filter.matches( destination_address )
	&& _from_filter.matches( source_address ) ) {
    ret.push_back( packet );
  }

  return ret;
}

void PacketSocket::send_raw( const std::string & input )
{
  ssize_t bytes_sent = send( sock, input.data(), input.size(), 0 );
  if ( bytes_sent < 0 ) {
    perror( "send" );
    exit( 1 );
  }
}

MACAddress::MACAddress( const std::string & s_addr )
  : octets()
{
  assert( s_addr.size() == 6 );
  for ( int i = 0; i < 6; i++ ) {
    octets[ i ] = s_addr[ i ];
  }
}

bool MACAddress::is_broadcast( void ) const
{
  for ( int i = 0; i < 6; i++ ) {
    if ( octets[ i ] != 0xff ) {
      return false;
    }
  }
  return true;
}

bool MACAddress::matches( const MACAddress & other ) const
{
  return ( is_broadcast() || other.is_broadcast() || (0 == memcmp( octets, other.octets, 6 )) );
}

std::string MACAddress::parse_human( const std::string & with_colons )
{
  string ret( 6, 0 );

  unsigned int octets[ 6 ];

  if ( with_colons.empty() ) {
    for ( int i = 0; i < 6; i++ ) {
      octets[ i ] = 0xff;
    }
  } else {
    int items_matched = sscanf( with_colons.c_str(),
				"%x:%x:%x:%x:%x:%x",
				&octets[ 0 ],
				&octets[ 1 ],
				&octets[ 2 ],
				&octets[ 3 ],
				&octets[ 4 ],
				&octets[ 5 ] );
    assert( items_matched == 6 );
  }

  for ( int i = 0; i < 6; i++ ) {
    assert( octets[ i ] <= 255 );
    ret[ i ] = octets[ i ];
  }

  return ret;
}

string MACAddress::pp( void ) const
{
  char tmp[ 64 ];
  snprintf( tmp, 64, "%x:%x:%x:%x:%x:%x",
	    octets[ 0 ],
	    octets[ 1 ],
	    octets[ 2 ],
	    octets[ 3 ],
	    octets[ 4 ],
	    octets[ 5 ] );
  return string( tmp );
}
