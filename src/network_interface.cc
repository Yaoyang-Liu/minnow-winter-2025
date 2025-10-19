#include <iostream>

#include "arp_message.hh"
#include "debug.hh"
#include "ethernet_frame.hh"
#include "exception.hh"
#include "helpers.hh"
#include "network_interface.hh"

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  debug( "unimplemented send_datagram called" );
  const uint32_t ip = next_hop.ipv4_numeric();
  auto it = arp_map_.find( ip );

  if ( it == arp_map_.end() ) {
    bool is_first = !broadcast_waitlist_.contains( ip ) || broadcast_waitlist_[ip].empty();

    // 将数据报追加到等待列表（无论是否首次，都需要缓存）
    broadcast_waitlist_[ip].emplace_back( dgram, timer_ );

    if ( is_first ) {
      ARPMessage arp_request = { .opcode = ARPMessage::OPCODE_REQUEST,
                                 .sender_ethernet_address = ethernet_address_,
                                 .sender_ip_address = ip_address_.ipv4_numeric(),
                                 .target_ethernet_address = ETHERNET_REQUEST_ADDRESS,
                                 .target_ip_address = ip };
      EthernetHeader eth_header
        = { .dst = ETHERNET_BROADCAST, .src = ethernet_address_, .type = EthernetHeader::TYPE_ARP };
      transmit( EthernetFrame { .header = eth_header, .payload = serialize( arp_request ) } );
    }
  } else {
    EthernetHeader eth_header
      = { .dst = it->second.first, .src = ethernet_address_, .type = EthernetHeader::TYPE_IPv4 };
    transmit( EthernetFrame { .header = eth_header, .payload = serialize( dgram ) } );
  }
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( EthernetFrame frame )
{
  debug( "unimplemented recv_frame called" );
  if ( frame.header.dst != ethernet_address_ && frame.header.dst != ETHERNET_BROADCAST ) {
    return;
  }

  if ( frame.header.type == EthernetHeader::TYPE_IPv4 ) {
    InternetDatagram data;
    if ( parse( data, frame.payload ) ) {
      datagrams_received_.push( data );
    }
  } else if ( frame.header.type == EthernetHeader::TYPE_ARP ) {
    ARPMessage msg;
    if ( !parse( msg, frame.payload ) ) {
      return;
    }

    // 无论是否存在，都更新 ARP 缓存（刷新时间戳）
    arp_map_[msg.sender_ip_address] = { msg.sender_ethernet_address, timer_ };
    if ( msg.opcode == ARPMessage::OPCODE_REQUEST && msg.target_ip_address == ip_address_.ipv4_numeric() ) {
      ARPMessage arp_reply = { .opcode = ARPMessage::OPCODE_REPLY,
                               .sender_ethernet_address = ethernet_address_,
                               .sender_ip_address = ip_address_.ipv4_numeric(),
                               .target_ethernet_address = msg.sender_ethernet_address,
                               .target_ip_address = msg.sender_ip_address };
      EthernetHeader arp_eth_header
        = { .dst = msg.sender_ethernet_address, .src = ethernet_address_, .type = EthernetHeader::TYPE_ARP };
      transmit( EthernetFrame { .header = arp_eth_header, .payload = serialize( arp_reply ) } );
    }

    auto waitlist_it = broadcast_waitlist_.find( msg.sender_ip_address );
    if ( waitlist_it != broadcast_waitlist_.end() ) {
      // 遍历发送该 IP 对应的所有缓存数据报
      for ( const auto& [dgram, _] : waitlist_it->second ) {
        EthernetHeader dgram_eth_header
          = { .dst = msg.sender_ethernet_address, .src = ethernet_address_, .type = EthernetHeader::TYPE_IPv4 };
        transmit( EthernetFrame { .header = dgram_eth_header, .payload = serialize( dgram ) } );
      }
      broadcast_waitlist_.erase( waitlist_it );
    }
  }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  debug( "unimplemented tick {} called", ms_since_last_tick );
  timer_ += ms_since_last_tick;

  for ( auto it = arp_map_.begin(); it != arp_map_.end(); ) {
    if ( timer_ - it->second.second > ARP_MAP_TTL ) {
      it = arp_map_.erase( it );
    } else {
      ++it;
    }
  }

  for ( auto it = broadcast_waitlist_.begin(); it != broadcast_waitlist_.end(); ) {
    if ( !it->second.empty() ) {
      // 以首个数据报的时间戳判断是否超时
      auto& [first_dgram, first_time] = it->second.front();
      if ( timer_ - first_time > ARP_RETX_PERIOD ) {
        it = broadcast_waitlist_.erase( it );
        continue;
      }
    }
    ++it;
  }
}