#include "wrapping_integers.hh"

using namespace std;

uint64_t delta( uint64_t x, uint64_t y )
{
  if ( x > y ) {
    return x - y;
  }
  return y - x;
}

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return Wrap32 { static_cast<uint32_t>( n ) + zero_point.raw_value_ };
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  uint64_t except = raw_value_ - zero_point.raw_value_;
  if ( raw_value_ < zero_point.raw_value_ ) {
    except = ( 1UL << 32 ) + raw_value_ - zero_point.raw_value_;
  }
  except = except + ( checkpoint & ( UINT64_MAX << 32 ) );
  if ( except > ( 1UL << 32 ) ) {
    except -= ( 1UL << 32 );
  }
  while ( delta( except, checkpoint ) > delta( except + ( 1UL << 32 ), checkpoint ) ) {
    except += ( 1UL << 32 );
  }
  return except;
}