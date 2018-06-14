#ifndef REASSEMBLY_HH
#define REASSEMBLY_HH

#include<map>
#include<vector>
#include<stdint.h>
#include<string>
#include"packet-fragment.pb.h"

/* Implements RFC 815 for reassembling fragmented packets */

class Reassembly
{
private :
  class Hole {
    public :
      uint16_t first;
      uint16_t last;
      Hole( uint16_t t_first, uint16_t t_last ) :
        first( t_first ),
        last( t_last )
        {}
      bool operator==(const Hole &other) const { return ( (first==other.first) and (last==other.last) ); }
  };

  std::map<uint32_t,std::vector<Hole>> hole_map;
  std::map<uint32_t,std::string> reassembly_buffer;

public :
  Reassembly();

  void add_fragment( fragment::PacketFragment p );

  std::string ready_to_deliver( uint32_t seq_num );
};

#endif
