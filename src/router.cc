#include "router.hh"
#include "debug.hh"

#include <iostream>

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

  debug( "unimplemented add_route() called" );
  routes.emplace_back( route_prefix, prefix_length, next_hop, interface_num );
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route()
{
  debug( "unimplemented route() called" );
  for ( auto& interface_ : interfaces_ ) {
    while ( !interface_->datagrams_received().empty() ) {
      auto datagram = interface_->datagrams_received().front();
      interface_->datagrams_received().pop();
      auto dst_ip = datagram.header.dst;
      auto cur_best_match = routes.end();
      for ( auto it = routes.begin(); it != routes.end(); ++it ) {
        uint32_t netmask = ( it->prefix_length == 0 ) ? 0 : ( 0xFFFFFFFF << ( 32 - it->prefix_length ) );
        if ( ( it->route_prefix & netmask ) == ( dst_ip & netmask ) ) {
          if ( cur_best_match == routes.end() || it->prefix_length > cur_best_match->prefix_length ) {
            cur_best_match = it;
          }
        }
      }
      if ( cur_best_match == routes.end() || datagram.header.ttl <= 1 ) {
        continue;
      }
      datagram.header.ttl--;
      datagram.header.compute_checksum();
      auto next_interface = interface( cur_best_match->interface_idx );
      if ( cur_best_match->next_hop.has_value() ) {
        next_interface->send_datagram( datagram, cur_best_match->next_hop.value() );
      } else {
        next_interface->send_datagram( datagram, Address::from_ipv4_numeric( dst_ip ) );
      }
    }
  }
}
