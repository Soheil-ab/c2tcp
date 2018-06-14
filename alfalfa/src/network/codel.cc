#include "codel.h"

CoDel::dodeque_result CoDel::dodeque ( IngressQueue & ingress_queue ) 
  {
      uint64_t now = Network::timestamp();
      dodeque_result r = { ingress_queue.deque(), false };
      if (r.p.contents.size() == 0 ) {
            first_above_time = 0;
      } else {
            uint64_t sojourn_time = now - r.p.tstamp;
            if (sojourn_time < target || bytes( ingress_queue ) < maxpacket) {
                  // went below so we'll stay below for at least interval
                  first_above_time = 0;
            } else {
                  if (first_above_time == 0) {
                        // just went above from below. if we stay above
                        // for at least interval we'll say it's ok to drop
                        first_above_time = now + interval;
                  } else if (now >= first_above_time) {
                        r.ok_to_drop = true;
                  }
            }
      }
      return r; 
  }


TrackedPacket CoDel::deque( IngressQueue & ingress_queue )
{
      uint64_t now = Network::timestamp();
      dodeque_result r = dodeque( ingress_queue );
      if (r.p.contents.size() == 0 ) {
            // an empty queue takes us out of dropping state
            dropping = 0;
            return r.p;
      }
      if (dropping) {
            if (! r.ok_to_drop) {
                  // sojourn time below target - leave dropping state
                  dropping = 0;
            } else if (now >= drop_next) {
                  while (now >= drop_next && dropping) {
                        drop(r.p);
                        ++count;
                        r = dodeque( ingress_queue );
                        if (! r.ok_to_drop)
                              // leave dropping state
                              dropping = 0;
                        else
                              // schedule the next drop.
                              drop_next = control_law(drop_next);
                        /* execute exactly once on limbo queue */
                  }
            }
      } else if (r.ok_to_drop &&
                         ((now - drop_next < interval) ||
                          (now - first_above_time >= interval))) {
                   drop(r.p);
                   r = dodeque( ingress_queue );
                   dropping = 1;
                   // If we're in a drop cycle, the drop rate that controlled the queue
                   // on the last cycle is a good starting point to control it now.
                   if (now - drop_next < interval)
                           count = count>2? count-2 : 1;
                   else
                           count = 1;
                   drop_next = control_law(now);
             }
             return (r.p);
}

void CoDel::drop (TrackedPacket p) 
{ 
  drop_count++;
  fprintf(stderr," Codel dropped a packet with size %ld, count now at %d \n",p.contents.size(),drop_count);
}

void CoDel::enque( IngressQueue & ingress_queue, std::string payload )
{
  ingress_queue.enque( payload );
}

CoDel::CoDel() :
  first_above_time( 0),
  drop_next(0),
  count(0),
  dropping(false),
  drop_count(0)
{}
