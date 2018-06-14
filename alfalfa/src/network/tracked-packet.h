#ifndef TRACKED_PACKET_HH
#define TRACKED_PACKET_HH

#include <stdint.h>
#include <string>

using namespace std;

/* New pkt structure to track sojourn time */
class TrackedPacket {
  public:
    uint64_t tstamp; /* Time at entry */
    string contents; /* Payload */

    TrackedPacket( uint64_t s_e, const string & s_c ) : tstamp( s_e ), contents( s_c ) {}
};

#endif
