#include "reassembly.hh"

Reassembly::Reassembly() :
  hole_map( std::map<uint32_t,std::vector<Hole>> () ),
  reassembly_buffer( std::map<uint32_t,std::string> () )
{}

void Reassembly::add_fragment( fragment::PacketFragment rx )
{
  fprintf( stderr, "RECEIVED packet of seq_num %u, size %lu from remote \n", rx.id(), rx.payload().size() );

  /* Step 1 : Have you seen this packet earlier ? */
  auto seq_num = rx.id();
  if ( hole_map.find( seq_num ) == hole_map.end() ) {
    Hole entire(0,1600);
    hole_map[ seq_num ].push_back( entire );
    reassembly_buffer[ seq_num ] = std::string(1600,'\0');
  }

  /* Step 2: Iterate through the holes for this packet */
  auto fragment_first = rx.offset();
  auto fragment_last  = rx.offset() + rx.payload().size() - 1 ;
  uint32_t i = 0;
  while( i < hole_map[ seq_num ].size() ) {
    auto hole = hole_map[ seq_num ].at(i);

    /* No overlap, move on */
    if ( ( fragment_first > hole.last ) or ( fragment_last < hole.first ) ) {
      i++;
      continue;
    }
    auto hole_first = hole.first;
    auto hole_last = hole.last;
    hole_map[ seq_num ].erase( hole_map[ seq_num ].begin() + i );

    /* Split existing hole */
    if ( fragment_first > hole_first ) {
      hole_map[ seq_num ].push_back( Hole( hole_first, fragment_first - 1 ) );
    }
    if ( (fragment_last < hole_last) and rx.more_frags() ) {
      hole_map[ seq_num ].push_back( Hole( fragment_last + 1, hole_last ) );
    }
    i++;
  }

  /* Step 3 : Write into reassembly buffer */
  reassembly_buffer[ seq_num ].replace( rx.offset(), rx.payload().size(), rx.payload() );
}

std::string Reassembly::ready_to_deliver( uint32_t seq_num )
{
  /* Check if there are no more holes */
  if ( hole_map[ seq_num ].size() == 0 ) {
   return reassembly_buffer[ seq_num ];
  } else {
   return "";
  }
}
