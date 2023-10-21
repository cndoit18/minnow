#include "router.hh"
#include "address.hh"
#include "arp_message.hh"
#include "ethernet_frame.hh"
#include "ethernet_header.hh"

#include <iostream>
#include <limits>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";
  routes_.emplace_back( *this, route_prefix, prefix_length, next_hop, interface_num );
  sort( routes_.begin(), routes_.end(), []( MatchRouter& x, MatchRouter& y ) {
    return x.prefix_length_ > y.prefix_length_;
  } );
}

void Router::route()
{
  for ( auto& i : interfaces_ ) {
    while ( true ) {
      auto data = i.maybe_receive();
      if ( !data.has_value() ) {
        break;
      }
      for ( auto& r : routes_ ) {
        if ( r.match_router( data.value(), data->header.dst ) ) {
          break;
        }
      }
    }
  }
}
