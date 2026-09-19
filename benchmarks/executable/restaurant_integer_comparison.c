// C23 reference for Restaurant fixed-width integer comparisons.
// Expected exit: 0
// Expected stdout:
// i8 false/true/true/true/false/false
// u8 false/true/false/false/true/true
// i16 true/false/false/true/false/true
// u16 false/true/true/true/false/false
// i32 false/true/true/true/false/false
// u32 false/true/false/false/true/true
// i64 false/true/true/true/false/false
// u64 false/true/false/false/true/true
// Int false/true/true/true/false/false
// UInt true/false/false/true/false/true
// widen u8->i16 true

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static volatile int8_t i8_input[] = {-1, 1};
static volatile uint8_t u8_input[] = {250u, 10u};
static volatile int16_t i16_input[] = {77, 77};
static volatile uint16_t u16_input[] = {3u, 50000u};
static volatile int32_t i32_input[] = {-120000, 120000};
static volatile uint32_t u32_input[] = {UINT32_C(3000000000), 1u};
static volatile int64_t i64_input[] = {-INT64_C(9000000000), INT64_C(9000000000)};
static volatile uint64_t u64_input[] = {10u, 9u};
static volatile int64_t int_input[] = {-7, 12};
static volatile uint64_t uint_input[] = {7u, 7u};
static volatile uint8_t widen_input = 200u;

static const char *bool_text(bool value) {
  return value ? "true" : "false";
}

static int report_i8(int8_t left, int8_t right) {
  return printf("i8 %s/%s/%s/%s/%s/%s\n",
                bool_text(left == right), bool_text(left != right),
                bool_text(left < right), bool_text(left <= right),
                bool_text(left > right), bool_text(left >= right)) < 0;
}

static int report_u8(uint8_t left, uint8_t right) {
  return printf("u8 %s/%s/%s/%s/%s/%s\n",
                bool_text(left == right), bool_text(left != right),
                bool_text(left < right), bool_text(left <= right),
                bool_text(left > right), bool_text(left >= right)) < 0;
}

static int report_i16(int16_t left, int16_t right) {
  return printf("i16 %s/%s/%s/%s/%s/%s\n",
                bool_text(left == right), bool_text(left != right),
                bool_text(left < right), bool_text(left <= right),
                bool_text(left > right), bool_text(left >= right)) < 0;
}

static int report_u16(uint16_t left, uint16_t right) {
  return printf("u16 %s/%s/%s/%s/%s/%s\n",
                bool_text(left == right), bool_text(left != right),
                bool_text(left < right), bool_text(left <= right),
                bool_text(left > right), bool_text(left >= right)) < 0;
}

static int report_i32(int32_t left, int32_t right) {
  return printf("i32 %s/%s/%s/%s/%s/%s\n",
                bool_text(left == right), bool_text(left != right),
                bool_text(left < right), bool_text(left <= right),
                bool_text(left > right), bool_text(left >= right)) < 0;
}

static int report_u32(uint32_t left, uint32_t right) {
  return printf("u32 %s/%s/%s/%s/%s/%s\n",
                bool_text(left == right), bool_text(left != right),
                bool_text(left < right), bool_text(left <= right),
                bool_text(left > right), bool_text(left >= right)) < 0;
}

static int report_i64(int64_t left, int64_t right) {
  return printf("i64 %s/%s/%s/%s/%s/%s\n",
                bool_text(left == right), bool_text(left != right),
                bool_text(left < right), bool_text(left <= right),
                bool_text(left > right), bool_text(left >= right)) < 0;
}

static int report_u64(uint64_t left, uint64_t right) {
  return printf("u64 %s/%s/%s/%s/%s/%s\n",
                bool_text(left == right), bool_text(left != right),
                bool_text(left < right), bool_text(left <= right),
                bool_text(left > right), bool_text(left >= right)) < 0;
}

static int report_int(int64_t left, int64_t right) {
  return printf("Int %s/%s/%s/%s/%s/%s\n",
                bool_text(left == right), bool_text(left != right),
                bool_text(left < right), bool_text(left <= right),
                bool_text(left > right), bool_text(left >= right)) < 0;
}

static int report_uint(uint64_t left, uint64_t right) {
  return printf("UInt %s/%s/%s/%s/%s/%s\n",
                bool_text(left == right), bool_text(left != right),
                bool_text(left < right), bool_text(left <= right),
                bool_text(left > right), bool_text(left >= right)) < 0;
}

static int report_widened(int16_t value) {
  return printf("widen u8->i16 %s\n", bool_text(value > 199)) < 0;
}

int main(void) {
#ifdef _WIN32
  if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif
  if (report_i8(i8_input[0], i8_input[1]) ||
      report_u8(u8_input[0], u8_input[1]) ||
      report_i16(i16_input[0], i16_input[1]) ||
      report_u16(u16_input[0], u16_input[1]) ||
      report_i32(i32_input[0], i32_input[1]) ||
      report_u32(u32_input[0], u32_input[1]) ||
      report_i64(i64_input[0], i64_input[1]) ||
      report_u64(u64_input[0], u64_input[1]) ||
      report_int(int_input[0], int_input[1]) ||
      report_uint(uint_input[0], uint_input[1]) ||
      report_widened(widen_input))
    return 1;
  return 0;
}
