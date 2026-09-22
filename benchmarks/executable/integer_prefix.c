// C23 reference for the fixed-width integer prefix family.
// Expected exit: 0
// Expected stdout:
// i8 -7/-43
// i16 -7/-43
// i32 -7/-43
// i64 -7/-43
// Int -7/-43
// u8 170
// u16 65450
// u32 4294967210
// u64 18446744073709551530
// UInt 18446744073709551530
// literal -7

#include <stdint.h>
#include <stdio.h>

int main(void) {
  volatile int8_t signed8 = INT8_C(7);
  volatile uint8_t unsigned8 = UINT8_C(0x55);
  volatile int8_t bits8 = INT8_C(42);
  volatile int16_t signed16 = INT16_C(7);
  volatile uint16_t unsigned16 = UINT16_C(0x55);
  volatile int16_t bits16 = INT16_C(42);
  volatile int32_t signed32 = INT32_C(7);
  volatile uint32_t unsigned32 = UINT32_C(0x55);
  volatile int32_t bits32 = INT32_C(42);
  volatile int64_t signed64 = INT64_C(7);
  volatile uint64_t unsigned64 = UINT64_C(0x55);
  volatile int64_t bits64 = INT64_C(42);
  volatile int64_t int_value = INT64_C(7);
  volatile uint64_t uint_value = UINT64_C(0x55);
  volatile int64_t int_bits = INT64_C(42);

  printf("i8 %d/%d\n", -(int)signed8, (int)(int8_t)~bits8);
  printf("i16 %d/%d\n", -(int)signed16, (int)(int16_t)~bits16);
  printf("i32 %ld/%ld\n", (long)-signed32, (long)(int32_t)~bits32);
  printf("i64 %lld/%lld\n", (long long)-signed64,
         (long long)(int64_t)~bits64);
  printf("Int %lld/%lld\n", (long long)-int_value,
         (long long)(int64_t)~int_bits);
  printf("u8 %u\n", (unsigned)(uint8_t)~unsigned8);
  printf("u16 %u\n", (unsigned)(uint16_t)~unsigned16);
  printf("u32 %lu\n", (unsigned long)(uint32_t)~unsigned32);
  printf("u64 %llu\n", (unsigned long long)(uint64_t)~unsigned64);
  printf("UInt %llu\n", (unsigned long long)(uint64_t)~uint_value);
  printf("literal -7\n");
  return 0;
}
