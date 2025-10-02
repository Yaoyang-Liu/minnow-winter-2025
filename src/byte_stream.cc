#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity )
{
  buffer_.reserve( capacity );
}

void Writer::push( string data )
{
  (void)data; // Your code here.
  size_t available_capacity = this->available_capacity();
  size_t bytes_to_copy = min( data.size(), available_capacity );
  buffer_ += data.substr( 0, bytes_to_copy );
  bytes_pushed_ += bytes_to_copy;
}

void Writer::close()
{
  // Your code here.
  closed_ = true;
}

bool Writer::is_closed() const
{
  return this->closed_; // Your code here.
}

uint64_t Writer::available_capacity() const
{
  return capacity_ - buffer_.size(); // Your code here.
}

uint64_t Writer::bytes_pushed() const
{
  return bytes_pushed_; // Your code here.
}

string_view Reader::peek() const
{
  string_view sv( buffer_.data(), min( static_cast<uint64_t>( 1000 ), this->bytes_buffered() ) );
  return sv; // Your code here.
}

void Reader::pop( uint64_t len )
{
  (void)len; // Your code here.
  buffer_.erase( buffer_.begin(), buffer_.begin() + len );
  bytes_popped_ += len;
}

bool Reader::is_finished() const
{
  return closed_ == true && this->bytes_buffered() == 0; // Your code here.
}

uint64_t Reader::bytes_buffered() const
{
  return buffer_.size(); // Your code here.
}

uint64_t Reader::bytes_popped() const
{
  return bytes_popped_; // Your code here.
}
