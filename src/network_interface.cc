#include "network_interface.hh"

#include "address.hh"
#include "arp_message.hh"
#include "ethernet_frame.hh"
#include "ethernet_header.hh"
#include "ipv4_datagram.hh"
#include "parser.hh"
#include <cstddef>
#include <deque>
#include <iostream>
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

  mapper_[ip_address_.ip()] = Expiration<EthernetAddress> {
    .obj = ethernet_address_,
    .tick = 0,
  };

  auto iter = mapper_.find( next_hop.ip() );
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
    next_.emplace_back( dgram, next_hop );
    return;
  }

  frame.header.dst = iter->second.obj;
  sendqueue_.emplace_back( frame );
}

// frame: the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  if ( frame.header.dst != ethernet_address_ && frame.header.dst != ETHERNET_BROADCAST ) {
    return nullopt;
  }

  switch ( frame.header.type ) {
    case EthernetHeader::TYPE_IPv4: {
      InternetDatagram dgram;
      parse( dgram, frame.payload );
      return dgram;
    }
    case EthernetHeader::TYPE_ARP: {
      ARPMessage arp;
      if ( parse( arp, frame.payload ) ) {
        mapper_[Address::from_ipv4_numeric( arp.sender_ip_address ).ip()] = Expiration<EthernetAddress> {
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

  for ( auto nex = next_.begin(); nex != next_.end(); ) {
    auto iter = mapper_.find( nex->second.ip() );
    if ( iter == mapper_.end() ) {
      ++nex;
      continue;
    }
    EthernetFrame ff;
    ff.header.src = ethernet_address_;
    ff.header.dst = iter->second.obj;
    ff.header.type = EthernetHeader::TYPE_IPv4;
    ff.payload = serialize( InternetDatagram { nex->first } );
    nex = next_.erase( nex );
    sendqueue_.emplace_back( ff );
  }

  return nullopt;
}

// ms_since_last_tick: the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  std::vector<std::string> keys_to_remove {};
  for ( auto& pair : mapper_ ) {
    if ( pair.second.tick < ms_since_last_tick ) {
      keys_to_remove.push_back( pair.first );
      continue;
    }
    pair.second.tick -= ms_since_last_tick;
  }

  for ( auto& del : keys_to_remove ) {
    mapper_.erase( del );
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
