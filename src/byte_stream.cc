#include <stdexcept>

#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

void Writer::push( string data )
{
  // Your code here.
  for ( auto current { data.begin() }; current != data.end() && capacity_ > 0; current++ ) {
    data_.push_back( *current );
    capacity_--;
    bytes_written_++;
  }
}

void Writer::close()
{
  // Your code here.
  close_ = true;
}

void Writer::set_error()
{
  // Your code here.
  error_ = true;
}

bool Writer::is_closed() const
{
  // Your code here.
  return close_;
}

uint64_t Writer::available_capacity() const
{
  // Your code here.
  return capacity_;
}

uint64_t Writer::bytes_pushed() const
{
  // Your code here.
  return bytes_written_;
}

string_view Reader::peek() const
{
  // Your code here.
  return data_;
}

bool Reader::is_finished() const
{
  // Your code here.
  return close_ && data_.empty();
}

bool Reader::has_error() const
{
  // Your code here.
  return error_;
}

void Reader::pop( uint64_t len )
{
  // Your code here.
  uint64_t pos { len };
  if ( len > data_.size() ) {
    pos = data_.size();
  }
  data_ = data_.substr( pos );
  capacity_ += pos;
  bytes_read_ += pos;
}

uint64_t Reader::bytes_buffered() const
{
  // Your code here.
  return data_.size();
}

uint64_t Reader::bytes_popped() const
{
  // Your code here.
  return bytes_read_;
}
