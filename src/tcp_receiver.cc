#include "tcp_receiver.hh"
#include "debug.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  // Your code here.
  debug( "unimplemented receive() called" );
  (void)message;
  if ( writer().has_error() ) {
    return;
  }
  if ( message.RST ) {
    reassembler_.reader().set_error();
    return;
  }
  if ( message.SYN ) {
    ISN = message.seqno;
  }
  if ( ISN.has_value() ) {
    cp = writer().bytes_pushed() + static_cast<uint32_t>( message.SYN );
    auto stream_index = message.seqno.unwrap( ISN.value(), cp ) + static_cast<uint64_t>( message.SYN ) - 1;
    reassembler_.insert( stream_index, message.payload, message.FIN );
  }
}

TCPReceiverMessage TCPReceiver::send() const
{
  // Your code here.
  debug( "unimplemented send() called" );
  uint16_t window_size
    = static_cast<uint16_t>( min( writer().available_capacity(), static_cast<uint64_t>( UINT16_MAX ) ) );
  bool reset = writer().has_error();
  if ( ISN.has_value() ) {
    Wrap32 ackno
      = Wrap32::wrap( writer().bytes_pushed() + static_cast<uint64_t>( writer().is_closed() ), ISN.value() ) + 1;
    return { ackno, window_size, reset };
  } else {
    return { nullopt, window_size, reset };
  }
}
