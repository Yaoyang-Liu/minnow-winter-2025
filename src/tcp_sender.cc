#include "tcp_sender.hh"
#include "debug.hh"
#include "tcp_config.hh"

using namespace std;

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::sequence_numbers_in_flight() const
{
  debug( "unimplemented sequence_numbers_in_flight() called" );
  return send_cnt_ - ack_cnt_;
}

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::consecutive_retransmissions() const
{
  debug( "unimplemented consecutive_retransmissions() called" );
  return retrs_cnt_;
}

void TCPSender::push( const TransmitFunction& transmit )
{
  debug( "unimplemented push() called" );
  (void)transmit;
  while ( ( wdsz_ == 0 ? 1 : wdsz_ ) > sequence_numbers_in_flight() ) {
    if ( is_fin_ ) {
      break;
    }
    auto msg = make_empty_message();
    if ( !is_syn_ ) {
      msg.SYN = true;
      is_syn_ = true;
    }
    uint64_t remaining = ( wdsz_ == 0 ? 1 : wdsz_ ) - sequence_numbers_in_flight();
    uint64_t len = min( TCPConfig::MAX_PAYLOAD_SIZE, remaining - msg.sequence_length() );
    auto&& data = msg.payload;
    while ( reader().bytes_buffered() && data.size() < len ) {
      auto cur_data = reader().peek();
      cur_data = cur_data.substr( 0, len - data.size() );
      data += cur_data;
      reader().pop( cur_data.size() );
    }
    if ( !is_fin_ && remaining > msg.sequence_length() && reader().is_finished() ) {
      msg.FIN = true;
      is_fin_ = true;
    }
    if ( msg.sequence_length() == 0 )
      break;
    transmit( msg );
    if ( !is_timer_on_ ) {
      is_timer_on_ = true;
      timer_ = 0;
    }
    send_cnt_ += msg.sequence_length();
    retrs_queue_.push( move( msg ) );
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  debug( "unimplemented make_empty_message() called" );
  return { .seqno = Wrap32::wrap( send_cnt_, isn_ ),
           .SYN = false,
           .payload = {},
           .FIN = false,
           .RST = input_.has_error() };
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  debug( "unimplemented receive() called" );
  (void)msg;
  if ( msg.RST ) {
    input_.set_error();
    return;
  }
  wdsz_ = msg.window_size;
  if ( msg.ackno.has_value() ) {
    uint64_t recv_ackno = msg.ackno.value().unwrap( isn_, ack_cnt_ );
    if ( recv_ackno > send_cnt_ ) {
      return;
    }
    if ( recv_ackno > ack_cnt_ ) {
      ack_cnt_ = recv_ackno;
      cur_RTO_ms_ = initial_RTO_ms_;
      retrs_cnt_ = 0;
      if ( sequence_numbers_in_flight() > 0 ) {
        timer_ = 0;
        if ( !is_timer_on_ ) {
          is_timer_on_ = true;
        }
      } else {
        is_timer_on_ = false;
        timer_ = 0;
      }
    }
  }
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  debug( "unimplemented tick({}, ...) called", ms_since_last_tick );
  (void)transmit;
  if ( is_timer_on_ ) {
    timer_ += ms_since_last_tick;
  }
  if ( timer_ >= cur_RTO_ms_ ) {
    while ( !retrs_queue_.empty() ) {
      auto msg = retrs_queue_.front();
      auto sendno = msg.seqno.unwrap( isn_, send_cnt_ );
      if ( sendno + msg.sequence_length() > ack_cnt_ ) {
        transmit( msg );
        if ( wdsz_ ) {
          cur_RTO_ms_ *= 2;
        }
        timer_ = 0;
        retrs_cnt_++;
        break;
      } else {
        retrs_queue_.pop();
      }
    }
    if ( retrs_queue_.empty() ) {
      is_timer_on_ = false;
      timer_ = 0;
    }
  }
}
