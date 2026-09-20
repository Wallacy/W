// C23 correctness reference for exact implicit numeric widening.
// Expected exit: 0
// Expected stdout:
// Numeric widen ok

#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static volatile int16_t signed_f32_input = -32767;
static volatile uint16_t unsigned_f32_input = UINT16_MAX;
static volatile int16_t returned_f32_input = -123;
static volatile int32_t signed_f64_input = INT32_C(-2147483647);
static volatile uint32_t called_f64_input = UINT32_MAX;
static volatile float widened_float_input = 1.5f;
static volatile float explicit_float_input = 1.25f;
static volatile int32_t mixed_integer_input = 2;
static volatile float mixed_float_input = 1.5f;

static float return_f32(int16_t value) { return value; }

static double accept_f64(double value) { return value; }

int main(void) {
#ifdef _WIN32
  if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif
  const float signed_f32 = signed_f32_input;
  const float unsigned_f32 = unsigned_f32_input;
  const float returned_f32 = return_f32(returned_f32_input);
  const double signed_f64 = signed_f64_input;
  const double called_f64 = accept_f64(called_f64_input);
  const double widened_float = widened_float_input;
  const double explicit_float = explicit_float_input;
  const double mixed_integer = mixed_integer_input + 0.5;
  const double mixed_float = mixed_float_input + 2.25;
  const int mixed_comparison = unsigned_f32_input == 65535.0f;
  const int valid =
      signed_f32 == -32767.0f && unsigned_f32 == 65535.0f &&
      returned_f32 == -123.0f && signed_f64 == -2147483647.0 &&
      called_f64 == 4294967295.0 && widened_float == 1.5 &&
      explicit_float == 1.25 && mixed_integer == 2.5 &&
      mixed_float == 3.75 && mixed_comparison;
  return fputs(valid ? "Numeric widen ok\n" : "Numeric widen bad\n", stdout) ==
         EOF;
}
