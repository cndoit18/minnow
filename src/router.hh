#pragma once

#include "address.hh"
#include "arp_message.hh"
#include "ethernet_header.hh"
#include "network_interface.hh"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <queue>
#include <string>
#include <utility>

// A wrapper for NetworkInterface that makes the host-side
// interface asynchronous: instead of returning received datagrams
// immediately (from the `recv_frame` method), it stores them for
// later retrieval. Otherwise, behaves identically to the underlying
// implementation of NetworkInterface.
class AsyncNetworkInterface : public NetworkInterface
{
  std::queue<InternetDatagram> datagrams_in_ {};

public:
  using NetworkInterface::NetworkInterface;

  // Construct from a NetworkInterface
  explicit AsyncNetworkInterface( NetworkInterface&& interface ) : NetworkInterface( interface ) {}

  // \brief Receives and Ethernet frame and responds appropriately.

  // - If type is IPv4, pushes to the `datagrams_out` queue for later retrieval by the owner.
  // - If type is ARP request, learn a mapping from the "sender" fields, and send an ARP reply.
  // - If type is ARP reply, learn a mapping from the "target" fields.
  //
  // \param[in] frame the incoming Ethernet frame
  void recv_frame( const EthernetFrame& frame )
  {
    auto optional_dgram = NetworkInterface::recv_frame( frame );
    if ( optional_dgram.has_value() ) {
      datagrams_in_.push( std::move( optional_dgram.value() ) );
    }
  };

  // Access queue of Internet datagrams that have been received
  std::optional<InternetDatagram> maybe_receive()
  {
    if ( datagrams_in_.empty() ) {
      return {};
    }

    InternetDatagram datagram = std::move( datagrams_in_.front() );
    datagrams_in_.pop();
    return datagram;
  }
};

class MatchRouter;
// A router that has multiple network interfaces and
// performs longest-prefix-match routing between them.
class Router
{
  // The router's collection of network interfaces
  std::vector<AsyncNetworkInterface> interfaces_ {};
  std::vector<MatchRouter> routes_ {};
  std::map<std::string, uint32_t> mapper_ {};

public:
  // Add an interface to the router
  // interface: an already-constructed network interface
  // returns the index of the interface after it has been added to the router
  size_t add_interface( AsyncNetworkInterface&& interface )
  {
    interfaces_.push_back( std::move( interface ) );
    return interfaces_.size() - 1;
  }

  // Access an interface by index
  AsyncNetworkInterface& interface( size_t N ) { return interfaces_.at( N ); }

  // Add a route (a forwarding rule)
  void add_route( uint32_t route_prefix,
                  uint8_t prefix_length,
                  std::optional<Address> next_hop,
                  size_t interface_num );

  // Route packets between the interfaces. For each interface, use the
  // maybe_receive() method to consume every incoming datagram and
  // send it on one of interfaces to the correct next hop. The router
  // chooses the outbound interface and next-hop as specified by the
  // route with the longest prefix_length that matches the datagram's
  // destination address.
  void route();
};

class MatchRouter
{
private:
  Router& router_;
  uint32_t route_prefix_;
  uint8_t prefix_length_;
  size_t interface_num_;
  std::optional<Address> next_hop_;

public:
  friend class Router;
  MatchRouter( Router& router,
               const uint32_t& route_prefix,
               const uint8_t prefix_length,
               const std::optional<Address>& next_hop,
               const size_t& interface_num )
    : router_( router )
    , route_prefix_( route_prefix )
    , prefix_length_( prefix_length )
    , interface_num_( interface_num )
    , next_hop_( next_hop )
  {
    uint32_t prefix_mask = static_cast<uint64_t>( -1 ) << ( 32 - prefix_length_ );
    std::cerr << "DEBUG: adding match route " << Address::from_ipv4_numeric( route_prefix_ ).ip() << "/" << std::hex
              << prefix_mask << " => " << ( next_hop_.has_value() ? next_hop_->ip() : "(direct)" )
              << " on interface " << interface_num_ << "\n";
  }

  MatchRouter& operator=( const MatchRouter& other )
  {
    if ( &other != this ) {
      router_ = other.router_;
      route_prefix_ = other.route_prefix_;
      prefix_length_ = other.prefix_length_;
      interface_num_ = other.interface_num_;
      next_hop_ = other.next_hop_;
    }
    return *this;
  };

  MatchRouter( const MatchRouter& other ) = default;
  bool match_router( InternetDatagram& dgram, uint32_t next_hop_uint32 )
  {
    uint32_t prefix_mask = static_cast<uint64_t>( -1 ) << ( 32 - prefix_length_ );
    auto next_hop = Address::from_ipv4_numeric( next_hop_uint32 );
    if ( ( prefix_mask & next_hop.ipv4_numeric() ) != ( route_prefix_ & prefix_mask ) ) {
      return false;
    }

    if ( next_hop_.has_value() ) {
      next_hop = next_hop_.value();
    }

    if ( dgram.header.ttl <= 1 ) {
      return true;
    }
    --dgram.header.ttl;
    router_.interface( interface_num_ ).send_datagram( dgram, next_hop );
    return true;
  }
};
