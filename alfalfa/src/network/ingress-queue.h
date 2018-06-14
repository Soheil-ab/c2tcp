#ifndef INGRESS_QUEUE_HH
#define INGRESS_QUEUE_HH

#include <queue>
#include "network.h"
#include "tracked-packet.h"

using namespace std;

class IngressQueue {
  private:
    queue< TrackedPacket > _packets;
    unsigned int _total_length;
  
  public:
    static const int QDISC_CODEL  = 0;
    static const int QDISC_SPROUT = 1;
    IngressQueue() : _packets(), _total_length() {}
    TrackedPacket front( void ) const { return _packets.front(); }
    void pop( void ) { _total_length -= _packets.front().contents.size(); _packets.pop(); }
    void push( const TrackedPacket & s ) { _total_length += s.contents.size(); _packets.push( s ); }
    unsigned int total_length( void ) const { return _total_length; }
    bool empty( void ) const { return _packets.empty(); }
  
    TrackedPacket deque( void ) { 
      if( empty() ) {
        return TrackedPacket((uint64_t)-1,"");
      } else {
        auto pkt = front();
        pop();
        return pkt;
      }
    }
  
    void enque( std::string payload ) {
      TrackedPacket pkt( Network::timestamp(), payload );
      push( pkt ); 
    }
};

#endif
