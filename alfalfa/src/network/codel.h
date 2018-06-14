#ifndef CODEL_HH
#define CODEL_HH

#include <math.h>
#include "ingress-queue.h"
#include "tracked-packet.h"

/* Class to Implement CoDel */
class CoDel {
  private :

    /* CoDel specific structure */
    typedef struct {
         TrackedPacket p; 
         bool ok_to_drop;
    } dodeque_result; 

    /* Codel - specific parameters */
    static const uint64_t    target=5 ;    /* 5   ms as per the spec */
    static const uint64_t  interval=100;   /* 100 ms as per the spec */
    static const uint16_t maxpacket=1500;    /* MTU of the link */
  
    /* Codel - specific tracker variables */
    uint64_t first_above_time;
    uint64_t drop_next;
    uint32_t count;
    bool     dropping;
    uint32_t _bytes_in_queue;
    uint32_t drop_count; /* for statistics */

    /* Codel - control rule */
    uint64_t control_law (uint64_t t) { return t + interval/sqrt(count);}

    /* Main codel routines */
    dodeque_result dodeque ( IngressQueue & ingress_queue );
    void drop ( TrackedPacket p);

    /* Number of bytes in queue */
    uint32_t bytes( const IngressQueue & ingress_queue ) { return ingress_queue.total_length(); };
    
  public :
    CoDel();
    /* Interface to the outside world */
    void enque( IngressQueue & ingress_queue, std::string payload );
    TrackedPacket deque ( IngressQueue & ingress_queue ); 

};

#endif
