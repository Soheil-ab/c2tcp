#ifndef PKT_CLASSIFIER_HH
#define PKT_CLASSIFIER_HH

#include<string>

typedef u_int8_t  flowid_t;
typedef u_int16_t source_t;
typedef u_int16_t dest_t;

class PktClassifier {
  public :
    static const uint16_t TCP_PROTOCOL_NUM = 6;
    static const uint16_t UDP_PROTOCOL_NUM = 17;
    static const uint16_t ICMP_PROTOCOL_NUM = 1;
    static const uint16_t HASH_SIZE = 64;

    uint16_t get_eth_header( std::string ethernet_frame ) const;
    flowid_t  get_flow_id( std::string packet_str ) const;
    std::string get_tcp_header( std::string ip_payload ) const;
    std::string get_udp_header( std::string ip_payload ) const;
    PktClassifier();
    std::string pkt_hash( std::string packet ) const;
};


#endif
