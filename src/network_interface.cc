#include "network_interface.hh"

#include "address.hh"
#include "arp_message.hh"
#include "ethernet_frame.hh"
#include "ethernet_header.hh"
#include "parser.hh"
#include <cstddef>
#include <deque>
#include <optional>
#include <vector>

using namespace std;

// ethernet_address: Ethernet (what ARP calls "hardware") address of the interface
// ip_address: IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( const EthernetAddress& ethernet_address, const Address& ip_address )
  : ethernet_address_( ethernet_address ), ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

// dgram: the IPv4 datagram to be sent
// next_hop: the IP address of the interface to send it to (typically a router or default gateway, but
// may also be another host if directly connected to the same network as the destination)

// Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) by using the
// Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  EthernetFrame frame;
  frame.header.src = ethernet_address_;
  frame.header.type = EthernetHeader::TYPE_IPv4;
  frame.payload = serialize( dgram );

  mapper_[to_string( ip_address_.ipv4_numeric() )] = Expiration<EthernetAddress> {
    .obj = ethernet_address_,
    .tick = 0,
  };

  auto iter = mapper_.find( to_string( next_hop.ipv4_numeric() ) );
  if ( iter == mapper_.end() ) {
    EthernetFrame arp;
    arp.header.src = ethernet_address_;
    arp.header.dst = EthernetAddress { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
    arp.header.type = EthernetHeader::TYPE_ARP;
    ARPMessage msg;
    msg.opcode = ARPMessage::OPCODE_REQUEST;
    msg.sender_ethernet_address = ethernet_address_;
    msg.sender_ip_address = ip_address_.ipv4_numeric();
    msg.target_ip_address = next_hop.ipv4_numeric();
    arp.payload = serialize( msg );
    sendqueue_.emplace_back( arp );
    next_[dgram.header.dst] = next_hop.ipv4_numeric();
  } else {
    frame.header.dst = iter->second.obj;
  }
  sendqueue_.emplace_back( frame );
}

// frame: the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  if ( frame.header.dst != ethernet_address_
       && frame.header.dst != EthernetAddress { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff } ) {
    return nullopt;
  }

  switch ( frame.header.type ) {
    case EthernetHeader::TYPE_IPv4: {
      InternetDatagram dgram;
      if ( parse( dgram, frame.payload ) ) {
        return dgram;
      }
      return nullopt;
    }
    case EthernetHeader::TYPE_ARP: {
      ARPMessage arp;
      if ( parse( arp, frame.payload ) ) {
        mapper_[to_string( arp.sender_ip_address )] = Expiration<EthernetAddress> {
          .obj = arp.sender_ethernet_address,
          .tick = 30000,
        };
        if ( arp.opcode == ARPMessage::OPCODE_REQUEST && arp.target_ip_address == ip_address_.ipv4_numeric() ) {
          EthernetFrame reply;
          reply.header.src = ethernet_address_;
          reply.header.dst = arp.sender_ethernet_address;
          reply.header.type = EthernetHeader::TYPE_ARP;
          ARPMessage msg;
          msg.opcode = ARPMessage::OPCODE_REPLY;
          msg.sender_ethernet_address = ethernet_address_;
          msg.sender_ip_address = ip_address_.ipv4_numeric();

          msg.target_ip_address = arp.sender_ip_address;
          msg.target_ethernet_address = arp.sender_ethernet_address;
          reply.payload = serialize( msg );
          sendqueue_.emplace_back( reply );
        }
      }
    }
  }
  return nullopt;
}

// ms_since_last_tick: the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  for ( auto& pair : mapper_ ) {
    if ( pair.second.tick < ms_since_last_tick ) {
      mapper_.erase( pair.first );
      continue;
    }
    pair.second.tick -= ms_since_last_tick;
  }

  for ( auto begin = sended_.begin(); begin != sended_.end(); ) {
    if ( begin->tick < ms_since_last_tick ) {
      begin = sended_.erase( begin );
      continue;
    }
    begin->tick -= ms_since_last_tick;
    begin++;
  }
}

optional<EthernetFrame> NetworkInterface::maybe_send()
{
  for ( auto begin = sendqueue_.begin(); begin != sendqueue_.end(); begin++ ) {
    if ( begin->header.dst == EthernetAddress {} ) {
      InternetDatagram dgram;
      parse( dgram, begin->payload );
      auto it = next_.find( dgram.header.dst );
      if ( it != next_.end() ) {
        auto iter = mapper_.find( to_string( it->second ) );
        if ( iter != mapper_.end() ) {
          begin->header.dst = iter->second.obj;
          --begin;
        }
      }
      continue;
    }
    auto result = optional<EthernetFrame> { *begin };
    begin = sendqueue_.erase( begin );
    if ( result->header.type == EthernetHeader::TYPE_ARP ) {
      ARPMessage a;
      parse( a, result->payload );
      if ( a.opcode == ARPMessage::OPCODE_REQUEST ) {
        for ( auto& send : sended_ ) {
          if ( send.obj.header.to_string() == result->header.to_string() ) {
            return nullopt;
          }
        }
        sended_.emplace_back( Expiration<EthernetFrame> { *result, 5000 } );
      }
    }
    return result;
  }
  return nullopt;
}
