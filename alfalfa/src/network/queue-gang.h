#ifndef QUEUE_GANG_HH
#define QUEUE_GANG_HH

#include<string>
#include<map>
#include<queue>
#include"ingress-queue.h"
#include"pkt-classifier.h"
#include"codel.h"

using namespace std;

class QueueGang {
 private :
   static const int MTU_SIZE = 1434;

   typedef uint8_t flowid_t;
   std::map<flowid_t,IngressQueue> _flow_queues;
   PktClassifier _classifier; 

   /* DRR bookkeeping */
   std::map<flowid_t,double>  _flow_credits;
   std::map<flowid_t,double>  _flow_quantums;
   std::queue<flowid_t> _active_list;
   std::map<flowid_t,bool> _active_indicator;
   uint32_t _current_flow;

   /* Optional CoDel servo bank for all flows */
   std::map<flowid_t,CoDel> _codel_servo_bank;
   int _qdisc;

   /* SproutAQM functions */
   unsigned int _current_qlimit;
   unsigned int aggregate_length( void );
   flowid_t longest_queue( void );

   /* deque from a particular flow */
   string deque( flowid_t flow_id );

   /* Create a new queue for a flow */
   void create_new_queue( flowid_t flow_id );

   /* Have I already seen the flow */
   bool already_seen( flowid_t flow_id );

 public :
   /* Constructor */
   QueueGang( bool t_codel_enabled );

   /* Check if all queues are empty */
   bool empty();

   /* Interface to the outside world */
   void enque( string packet );
   string get_next_packet( void ); /* Use DRR */

   /* Set qlimit from sproutbt2.cc */
   void set_qlimit( unsigned int qlimit ) { _current_qlimit = qlimit; };
};

#endif
