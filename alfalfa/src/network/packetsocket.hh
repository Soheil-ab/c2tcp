#ifndef PACKETSOCKET_HH
#define PACKETSOCKET_HH

#include <string>
#include <vector>

class MACAddress
{
private:
  uint8_t octets[ 6 ];

public:
  MACAddress( const std::string & s_addr );
  bool matches( const MACAddress & other ) const;

  static std::string parse_human( const std::string & with_colons );
  std::string pp( void ) const;

  bool is_broadcast( void ) const;
};

class PacketSocket
{
private:
  int sock;

  MACAddress _from_filter;
  MACAddress _to_filter;

  int get_index( const std::string & name ) const;

public:
  PacketSocket();
  PacketSocket( const std::string & s_interface,
		const std::string & s_from_filter,
		const std::string & s_to_filter );

  std::vector< std::string > recv_raw( void );
  void send_raw( const std::string & input );
  int fd( void ) const { return sock; }
};

#endif
