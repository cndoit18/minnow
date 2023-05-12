#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message, Reassembler& reassembler, Writer& inbound_stream )
{
  // Your code here.
  if ( message.SYN ) {
    zero_point_ = message.seqno;
  }

  if ( !zero_point_.has_value() ) {
    return;
  }
  reassembler.insert( message.seqno.unwrap( zero_point_.value(), checkpoint_ ) + ( message.SYN ? 0 : -1 ),
                      message.payload,
                      message.FIN,
                      inbound_stream );
  checkpoint_ = inbound_stream.bytes_pushed();
}

TCPReceiverMessage TCPReceiver::send( const Writer& inbound_stream ) const
{
  auto message = TCPReceiverMessage {
    .window_size = static_cast<uint16_t>(
      ( inbound_stream.available_capacity() > UINT16_MAX ) ? UINT16_MAX : inbound_stream.available_capacity() ),
  };

  if ( zero_point_.has_value() ) {
    message.ackno = Wrap32::wrap( checkpoint_, zero_point_.value() ) + 1 + ( inbound_stream.is_closed() ? 1 : 0 );
  }
  return message;
}
