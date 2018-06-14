#include <algorithm>
#include "queue-gang.h"
#include "tracked-packet.h"

QueueGang::QueueGang( bool qdisc ) :
  _flow_queues( std::map<flowid_t,IngressQueue>() ),
  _flow_credits(  std::map<flowid_t,double>() ),
  _flow_quantums( std::map<flowid_t,double>() ),
  _active_list( std::queue<flowid_t>() ),
  _active_indicator( std::map<flowid_t,bool>() ),
  _current_flow( 0 ),
  _codel_servo_bank( std::map<flowid_t,CoDel> () ),
  _qdisc( qdisc ),
  _current_qlimit( 0 )
{}

unsigned int QueueGang::aggregate_length( void )
{
  return std::accumulate( _flow_queues.begin(), _flow_queues.end(),
                          0, [&] (unsigned int acc, const std::pair<flowid_t,IngressQueue> & q)
                             { return ( q.second.total_length() + acc ); } );
}

flowid_t QueueGang::longest_queue( void )
{
  typedef std::pair<flowid_t,IngressQueue> FlowQ;
  auto it = std::max_element( _flow_queues.begin(), _flow_queues.end(),
                              [&] (const FlowQ &q1, const FlowQ &q2 )
                              { return q1.second.total_length() < q2.second.total_length() ; } );
  return it->first;
}

bool QueueGang::empty( void )
{
  return   std::accumulate( _flow_queues.begin(), _flow_queues.end(),
                            true, [&] (bool acc, const std::pair<flowid_t,IngressQueue> & q)
                                  { return (q.second.empty() and acc); } );
}

void QueueGang::create_new_queue( flowid_t flow_id )
{
  assert( _flow_queues.find( flow_id ) == _flow_queues.end() );
  _flow_queues[ flow_id ] = IngressQueue();
  _flow_credits[ flow_id ] = 0;
  _flow_quantums[ flow_id ] = MTU_SIZE;
  _active_indicator[ flow_id ] = false;
  if ( _qdisc == IngressQueue::QDISC_CODEL ) {
    _codel_servo_bank[ flow_id ] = CoDel();
  }

}

bool QueueGang::already_seen( flowid_t flow_id )
{
  return _flow_queues.find( flow_id ) != _flow_queues.end();
}

void QueueGang::enque( string packet )
{
  /* find flow id from packet */
  auto flow_id =  _classifier.get_flow_id( packet );

  /* Check if queue exists, if not make one */
  if ( !already_seen( flow_id ) ) {
    create_new_queue( flow_id );
  }

  /* enque into the right queue */
  if ( _qdisc == IngressQueue::QDISC_SPROUT ) {
    assert( _current_qlimit > 0 );
    if ( aggregate_length() > _current_qlimit ) {
      /* Drop from front of the longest queue */
      auto hol_pkt = _flow_queues.at( longest_queue() ).front();
      _flow_queues.at( longest_queue() ).pop();
      fprintf( stderr, "Sprout AQM dropped packet of size %lu \n", hol_pkt.contents.size() );
    }
    _flow_queues.at( flow_id ).enque( packet );
  } else { 
    _flow_queues.at( flow_id ).enque( packet );
  }

  /* Update DRR structures */
  if ( _active_indicator.at ( flow_id ) == false ) {
  	_active_indicator.at( flow_id ) = true;
  	_active_list.push( flow_id );
  	_flow_credits.at( flow_id )=0;
  }
}

string QueueGang::deque( flowid_t flow_id )
{
  assert( !_flow_queues.at( flow_id ).empty() );
  /* CoDel specific stuff on each queue */
  if ( _qdisc == IngressQueue::QDISC_CODEL ) {
    TrackedPacket packet = _codel_servo_bank.at( flow_id ).deque( _flow_queues.at( flow_id ) );
    if(packet.contents.size() > 0) {
      string to_send = packet.contents;
      return to_send;
    } else {
      return "";
    }
  } else {
    TrackedPacket tracked_pkt = _flow_queues.at( flow_id ).deque();
    return tracked_pkt.contents;
  }
}


string QueueGang::get_next_packet()
{
  string p;
  if ( _active_list.empty() ) {
    return p;
  }
  _current_flow = _active_list.front();
  uint32_t pkt_size = _flow_queues.at( _current_flow ).front().contents.size();
  if ( _flow_credits.at ( _current_flow ) < pkt_size ) { /* do not add until you deplete credits */
    _flow_credits.at( _current_flow ) += _flow_quantums.at( _current_flow );
  }
  printf("Flow Qs : ");
  std::for_each( _flow_queues.begin(), _flow_queues.end(),
                 [&] (const std::pair<flowid_t,IngressQueue> & p ) { printf (" %u:%u ", p.first, p.second.total_length() ); } ); 
  printf("\n");
  assert ( !_flow_queues.at( _current_flow ).empty() ) ;
  assert ( pkt_size <= _flow_credits.at( _current_flow ) ) ;
  _flow_credits.at( _current_flow ) -= pkt_size;
  p=deque( _current_flow );
  if ( _flow_queues.at( _current_flow ).empty() ) {
    _flow_credits.at( _current_flow ) = 0;
    _active_list.pop();
    _active_indicator.at( _current_flow )=false;
  } else if ( _flow_credits.at( _current_flow ) < pkt_size ) {
    _active_list.pop();
    _active_list.push( _current_flow );
  }
  return p;
}
