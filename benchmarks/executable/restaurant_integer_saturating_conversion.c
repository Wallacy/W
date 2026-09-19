// C23 reference for representative Restaurant integer saturating conversions.
// Expected exit: 0
// Expected stdout:
// ss -128/7/127; us 7/127/127; su 0/200/255; uu 7/255/255; UInt->Int 9223372036854775807

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static volatile int16_t signed16[] = {-129, 7, 128, -1, 200, 256};
static volatile uint16_t unsigned16[] = {7u, 127u, 128u, 255u, 256u};
static volatile uint64_t unsigned64_maximum = UINT64_MAX;

static int8_t saturate_signed_to_signed_i8(int16_t value) {
  if (value < INT8_MIN) return INT8_MIN;
  if (value > INT8_MAX) return INT8_MAX;
  return (int8_t)value;
}

static int8_t saturate_unsigned_to_signed_i8(uint16_t value) {
  return value > (uint16_t)INT8_MAX ? INT8_MAX : (int8_t)value;
}

static uint8_t saturate_signed_to_unsigned_u8(int16_t value) {
  if (value < 0) return 0u;
  if (value > UINT8_MAX) return UINT8_MAX;
  return (uint8_t)value;
}

static uint8_t saturate_unsigned_to_unsigned_u8(uint16_t value) {
  return value > UINT8_MAX ? UINT8_MAX : (uint8_t)value;
}

static int64_t saturate_uint_to_int(uint64_t value) {
  return value > (uint64_t)INT64_MAX ? INT64_MAX : (int64_t)value;
}

int main(void) {
#ifdef _WIN32
  if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif
  if (printf("ss %d/%d/%d; us %d/%d/%d; su %u/%u/%u; uu %u/%u/%u; "
             "UInt->Int %" PRId64 "\n",
             (int)saturate_signed_to_signed_i8(signed16[0]),
             (int)saturate_signed_to_signed_i8(signed16[1]),
             (int)saturate_signed_to_signed_i8(signed16[2]),
             (int)saturate_unsigned_to_signed_i8(unsigned16[0]),
             (int)saturate_unsigned_to_signed_i8(unsigned16[1]),
             (int)saturate_unsigned_to_signed_i8(unsigned16[2]),
             (unsigned)saturate_signed_to_unsigned_u8(signed16[3]),
             (unsigned)saturate_signed_to_unsigned_u8(signed16[4]),
             (unsigned)saturate_signed_to_unsigned_u8(signed16[5]),
             (unsigned)saturate_unsigned_to_unsigned_u8(unsigned16[0]),
             (unsigned)saturate_unsigned_to_unsigned_u8(unsigned16[3]),
             (unsigned)saturate_unsigned_to_unsigned_u8(unsigned16[4]),
             saturate_uint_to_int(unsigned64_maximum)) < 0)
    return 1;
  return 0;
}
