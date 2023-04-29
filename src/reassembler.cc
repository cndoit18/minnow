#include "reassembler.hh"
#include <sstream>

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring, Writer& output )
{
  // Your code here.
  if ( is_last_substring ) {
    end_index_ = first_index + data.size();
  }

  auto size = confirm_index_ + output.available_capacity();
  data = data.substr( 0, size > first_index ? size - first_index : 0 );

  if ( !data.empty() && ( first_index + data.size() > confirm_index_ + data_.size() ) ) {
    data_.resize( first_index + data.size(), optional<char> {} );
  }

  for ( auto current : data ) {
    if ( first_index < confirm_index_ ) {
      first_index++;
      continue;
    }

    if ( !data_[first_index - confirm_index_].has_value() ) {
      data_[first_index - confirm_index_] = current;
      pedding_++;
    }
    first_index++;
  }

  string buffer {};
  auto current { data_.begin() };
  for ( ; current != data_.end() && current->has_value(); current++ ) {
    buffer.push_back( current->value() );
    confirm_index_++;
    pedding_--;
  }
  data_.erase( data_.begin(), current );
  output.push( buffer );

  if ( end_index_.has_value() && end_index_.value() <= confirm_index_ ) {
    output.close();
  }
}

uint64_t Reassembler::bytes_pending() const
{
  return pedding_;
}
