#include "tcp_sender.hh"
#include "tcp_config.hh"

#include <algorithm>
#include <optional>
#include <random>

using namespace std;

/* TCPSender constructor (uses a random ISN if none given) */
TCPSender::TCPSender( uint64_t initial_RTO_ms, optional<Wrap32> fixed_isn )
  : isn_( fixed_isn.value_or( Wrap32 { random_device()() } ) ), initial_RTO_ms_( initial_RTO_ms )
{}

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return unacknowledged_ - acknowledged_;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  // Your code here.
  return retransmissions_;
}

optional<TCPSenderMessage> TCPSender::maybe_send()
{
  // Your code here.
  for ( const auto& msg : messages_ ) {
    auto isn = msg.seqno.unwrap( isn_, acknowledged_ ) + msg.sequence_length() + 1;
    if ( isn > send_ ) {
      send_ = isn;
      if ( !RTO_ms_.has_value() ) {
        RTO_ms_ = optional<uint64_t> { initial_RTO_ms_ };
      }
      return optional<TCPSenderMessage> { msg };
    }
  }

  if ( expire_ ) {
    expire_ = false;
    return optional<TCPSenderMessage> { messages_.front() };
  }
  return nullopt;
}

void TCPSender::push( Reader& outbound_stream )
{
  if ( try_send_ && windows_size_ == 0 ) {
    windows_size_ = 1;
  }
  while ( windows_size_ > 0 && !is_close_ ) {
    TCPSenderMessage message {
      .seqno = Wrap32::wrap( unacknowledged_, isn_ ),
    };

    if ( try_send_ ) {
      try_msg_ = unacknowledged_;
    }

    if ( unacknowledged_ == 0 && windows_size_ > 0 ) {
      windows_size_--;
      message.SYN = true;
    }
    auto n = min( outbound_stream.bytes_buffered(), min( windows_size_, TCPConfig::MAX_PAYLOAD_SIZE ) );
    message.payload = Buffer( string( outbound_stream.peek().substr( 0, n ) ) );
    windows_size_ -= message.payload.size();
    outbound_stream.pop( message.payload.size() );

    if ( !is_close_ && outbound_stream.is_finished() && windows_size_ > 0 ) {
      windows_size_--;
      message.FIN = true;
      is_close_ = true;
    }

    unacknowledged_ += message.sequence_length();

    if ( message.sequence_length() == 0 ) {
      break;
    }

    try_send_ = false;
    messages_.push_back( message );
  }
}

TCPSenderMessage TCPSender::send_empty_message() const
{
  // Your code here.
  return TCPSenderMessage {
    .seqno = Wrap32::wrap( unacknowledged_, isn_ ),
  };
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  if ( msg.ackno.has_value() ) {
    auto isn = msg.ackno.value().unwrap( isn_, acknowledged_ );
    if ( isn > acknowledged_ && isn <= unacknowledged_ ) {
      acknowledged_ = max( acknowledged_, isn );
      messages_.erase( remove_if( messages_.begin(),
                                  messages_.end(),
                                  [this]( const TCPSenderMessage& m ) {
                                    return m.seqno.unwrap( isn_, acknowledged_ ) + m.sequence_length()
                                           <= acknowledged_;
                                  } ),
                       messages_.end() );
      RTO_ms_ = optional<uint64_t> { initial_RTO_ms_ };
      retransmissions_ = 0;
      if ( try_msg_.has_value() && try_msg_.value() <= isn ) {
        try_msg_ = nullopt;
      }
    }
  }
  windows_size_
    = msg.window_size < sequence_numbers_in_flight() ? 0 : msg.window_size - sequence_numbers_in_flight();
  if ( msg.window_size == 0 ) {
    try_send_ = true;
  }

  if ( sequence_numbers_in_flight() == 0 ) {
    RTO_ms_ = nullopt;
  }
}

void TCPSender::tick( const size_t ms_since_last_tick )
{
  // Your code here.
  if ( !RTO_ms_.has_value() ) {
    return;
  }

  auto ms = RTO_ms_.value() < ms_since_last_tick ? 0 : RTO_ms_.value() - ms_since_last_tick;
  if ( !( ms == 0 ) ) {
    RTO_ms_ = optional<uint64_t> { ms };
    return;
  }

  if ( sequence_numbers_in_flight() != 0 ) {
    expire_ = true;
    auto isn = messages_.front().seqno.unwrap( isn_, acknowledged_ );
    if ( !try_msg_.has_value() || isn != try_msg_.value() ) {
      retransmissions_++;
    }
    RTO_ms_ = optional<uint64_t> { initial_RTO_ms_ << retransmissions_ };
  }
}
