#include "pkt-classifier.h"
#include <net/ethernet.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

uint16_t PktClassifier::get_eth_header( std::string ethernet_frame ) const
{
  /* Seek to the beginning of the eth frame */
  const char* eth_hdr_with_data = ethernet_frame.c_str();
  const struct ether_header* eth_hdr;
  eth_hdr = (struct ether_header*) (eth_hdr_with_data); 

  return ntohs(eth_hdr->ether_type);
}

flowid_t PktClassifier::get_flow_id( std::string packet_str ) const
{
  /* Parse eth frame first */
  auto eth_type = get_eth_header( packet_str );
  if (eth_type == ETHERTYPE_ARP ) {
    return (uint8_t)-1;
  } else if (eth_type == ETHERTYPE_IP ) {
    /* Seek to the beginning of the IP header */
    const char* packet = packet_str.c_str();
    const struct ip* ip_hdr;
    ip_hdr = ( struct ip*) ( packet + sizeof( struct ether_header ) );
    auto hdr_len  = ip_hdr->ip_hl;
    auto protocol = ip_hdr->ip_p;
  
    return protocol;
  } else {
    printf( " Some other protocol type, packet is %x \n", packet_str.c_str() );
    return (uint8_t)-1;
  }
}

std::string PktClassifier::get_udp_header( std::string udp_packet ) const
{
  auto udp_hdr_with_data = udp_packet.c_str();
  const struct udphdr* udp_hdr = (struct udphdr*) ( udp_hdr_with_data );
  auto sport = ntohs( udp_hdr -> source );
  auto dport = ntohs( udp_hdr -> dest );
  return "";
}

std::string PktClassifier::get_tcp_header( std::string tcp_packet ) const
{
  auto tcp_hdr_with_data = tcp_packet.c_str();
  const struct tcphdr* tcp_hdr = (struct tcphdr*) ( tcp_hdr_with_data );
  auto sport = ntohs( tcp_hdr -> source );
  auto dport = ntohs( tcp_hdr -> dest );
  return "";
}

std::string PktClassifier::pkt_hash( std::string packet ) const
{
  return (packet.size() >= HASH_SIZE) ? packet.substr( packet.size() - HASH_SIZE, HASH_SIZE ) : packet ;
}

PktClassifier::PktClassifier()
{}
