// C23 reference for the Restaurant integer bitwise family.
// Expected exit: 0
// Expected stdout:
// i8 10/-81/-91
// u8 10/175/165
// i16 2570/-20561/-23131
// u16 2570/44975/42405
// i32 168430090/-1347440721/-1515870811
// u32 168430090/2947526575/2779096485
// i64 723401728380766730/-5787213827046133841/-6510615555426900571
// u64 723401728380766730/12659530246663417775/11936128518282651045
// Int 723401728380766730/-5787213827046133841/-6510615555426900571
// UInt 723401728380766730/12659530246663417775/11936128518282651045
// Widened -13
// Mixed 255

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static volatile int8_t i8_inputs[] = {-86, 15};
static volatile uint8_t u8_inputs[] = {UINT8_C(170), UINT8_C(15)};
static volatile int16_t i16_inputs[] = {-INT16_C(21846), INT16_C(3855)};
static volatile uint16_t u16_inputs[] = {UINT16_C(43690), UINT16_C(3855)};
static volatile int32_t i32_inputs[] = {-INT32_C(1431655766), INT32_C(252645135)};
static volatile uint32_t u32_inputs[] = {UINT32_C(2863311530), UINT32_C(252645135)};
static volatile int64_t i64_inputs[] = {
    -INT64_C(6148914691236517206), INT64_C(1085102592571150095),
};
static volatile uint64_t u64_inputs[] = {
    UINT64_C(12297829382473034410), UINT64_C(1085102592571150095),
};
static volatile intptr_t int_inputs[] = {
    -INT64_C(6148914691236517206), INT64_C(1085102592571150095),
};
static volatile uintptr_t uint_inputs[] = {
    UINT64_C(12297829382473034410), UINT64_C(1085102592571150095),
};
static volatile int8_t widened_left = -INT8_C(16);
static volatile int32_t widened_right = INT32_C(3);
static volatile uint8_t mixed_left = UINT8_C(240);
static volatile int16_t mixed_right = INT16_C(15);

_Static_assert(sizeof(intptr_t) == sizeof(int64_t), "Int must be 64-bit");
_Static_assert(sizeof(uintptr_t) == sizeof(uint64_t), "UInt must be 64-bit");

#define PRINT_SIGNED_ROW(label, type, inputs)                                  \
  do {                                                                         \
    const type a = (inputs)[0];                                                \
    const type b = (inputs)[1];                                                \
    const type bit_and = (type)(a & b);                                        \
    const type bit_or = (type)(a | b);                                         \
    const type bit_xor = (type)(a ^ b);                                        \
    if (printf("%s %" PRIdMAX "/%" PRIdMAX "/%" PRIdMAX "\n", label,     \
               (intmax_t)bit_and, (intmax_t)bit_or, (intmax_t)bit_xor) < 0)   \
      return 1;                                                                \
  } while (0)

#define PRINT_UNSIGNED_ROW(label, type, inputs)                               \
  do {                                                                         \
    const type a = (inputs)[0];                                                \
    const type b = (inputs)[1];                                                \
    const type bit_and = (type)(a & b);                                        \
    const type bit_or = (type)(a | b);                                         \
    const type bit_xor = (type)(a ^ b);                                        \
    if (printf("%s %" PRIuMAX "/%" PRIuMAX "/%" PRIuMAX "\n", label,     \
               (uintmax_t)bit_and, (uintmax_t)bit_or, (uintmax_t)bit_xor) < 0)\
      return 1;                                                                \
  } while (0)

int main(void) {
#ifdef _WIN32
  if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif
  PRINT_SIGNED_ROW("i8", int8_t, i8_inputs);
  PRINT_UNSIGNED_ROW("u8", uint8_t, u8_inputs);
  PRINT_SIGNED_ROW("i16", int16_t, i16_inputs);
  PRINT_UNSIGNED_ROW("u16", uint16_t, u16_inputs);
  PRINT_SIGNED_ROW("i32", int32_t, i32_inputs);
  PRINT_UNSIGNED_ROW("u32", uint32_t, u32_inputs);
  PRINT_SIGNED_ROW("i64", int64_t, i64_inputs);
  PRINT_UNSIGNED_ROW("u64", uint64_t, u64_inputs);
  PRINT_SIGNED_ROW("Int", intptr_t, int_inputs);
  PRINT_UNSIGNED_ROW("UInt", uintptr_t, uint_inputs);
  const int32_t widened = (int32_t)widened_left | widened_right;
  if (printf("Widened %" PRId32 "\n", widened) < 0) return 1;
  const int16_t mixed = (int16_t)mixed_left | mixed_right;
  if (printf("Mixed %" PRId16 "\n", mixed) < 0) return 1;
  return 0;
}
