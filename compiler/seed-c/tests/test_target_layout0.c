#include "w_seed_target_layout0.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static const char *const windows_triple = "x86_64-pc-windows-msvc";
static const char *const linux_triple = "x86_64-unknown-linux-gnu";
static const char *const windows_data_layout =
  "e-m:w-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128";
static const char *const linux_data_layout =
  "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128";

static unsigned int failures;

static void check(bool condition, const char *label) {
  if (!condition) {
    (void)fprintf(stderr, "target-layout0: FAIL: %s\n", label);
    ++failures;
  }
}

static void check_status(w_seed_target_layout0_status actual,
  w_seed_target_layout0_status expected, const char *label) {
  if (actual != expected) {
    (void)fprintf(stderr,
      "target-layout0: FAIL: %s (got %s, expected %s)\n",
      label,
      w_seed_target_layout0_status_name(actual),
      w_seed_target_layout0_status_name(expected));
    ++failures;
  }
}

static bool scalar_layout_equals(
  const w_seed_target_layout0_scalar_layout *layout,
  w_seed_target_layout0_scalar_kind kind,
  uint16_t logical_width_bits,
  uint16_t store_width_bits,
  uint16_t allocation_width_bits,
  uint16_t llvm_abi_alignment_bits) {
  return layout->kind == kind &&
    layout->logical_width_bits == logical_width_bits &&
    layout->store_width_bits == store_width_bits &&
    layout->allocation_width_bits == allocation_width_bits &&
    layout->llvm_abi_alignment_bits == llvm_abi_alignment_bits;
}

static void check_layout_family(
  const w_seed_target_layout0_profile *profile,
  w_seed_target_layout0_scalar_kind kind,
  const uint16_t *widths,
  size_t count,
  const char *label) {
  size_t index;

  for (index = 0u; index < count; ++index) {
    w_seed_target_layout0_scalar_layout actual = {0};
    const uint16_t width = widths[index];
    const w_seed_target_layout0_status status =
      w_seed_target_layout0_scalar_layout_for(profile, kind, width, width,
        &actual);
    if (status != W_SEED_TARGET_LAYOUT0_OK ||
        !scalar_layout_equals(&actual, kind, width, width, width, width)) {
      (void)fprintf(stderr,
        "target-layout0: FAIL: %s width %u (status %s)\n",
        label, (unsigned int)width,
        w_seed_target_layout0_status_name(status));
      ++failures;
    }
  }
}

static void check_serialization(
  const w_seed_target_layout0_profile *profile,
  uint16_t width_bits,
  const char *label) {
  uint8_t value[16];
  uint8_t output[16];
  uint8_t expected_big[16];
  uint8_t overlap[17];
  const size_t byte_count = (size_t)(width_bits / 8u);
  size_t index;

  for (index = 0u; index < byte_count; ++index)
    value[index] = (uint8_t)(index + 1u);
  for (index = 0u; index < byte_count; ++index)
    expected_big[index] = value[byte_count - 1u - index];

  memset(output, 0xa5, sizeof(output));
  check_status(w_seed_target_layout0_serialize_scalar(profile, width_bits,
    W_SEED_TARGET_LAYOUT0_BYTE_ORDER_LITTLE, value, byte_count, output,
    byte_count), W_SEED_TARGET_LAYOUT0_OK, "little serialization");
  check(memcmp(output, value, byte_count) == 0, "little byte order");

  memset(output, 0xa5, sizeof(output));
  check_status(w_seed_target_layout0_serialize_scalar(profile, width_bits,
    W_SEED_TARGET_LAYOUT0_BYTE_ORDER_BIG, value, byte_count, output,
    byte_count), W_SEED_TARGET_LAYOUT0_OK, "big serialization");
  check(memcmp(output, expected_big, byte_count) == 0, "big byte order");

  memset(output, 0xa5, sizeof(output));
  check_status(w_seed_target_layout0_serialize_scalar(profile, width_bits,
    W_SEED_TARGET_LAYOUT0_BYTE_ORDER_NATIVE, value, byte_count, output,
    byte_count), W_SEED_TARGET_LAYOUT0_OK, "native serialization");
  check(memcmp(output, value, byte_count) == 0,
    "native resolves from selected little-endian target");

  /* Staging preserves the output on malformed lengths, including aliasing. */
  memset(output, 0xa5, sizeof(output));
  check_status(w_seed_target_layout0_serialize_scalar(profile, width_bits,
    W_SEED_TARGET_LAYOUT0_BYTE_ORDER_BIG, value, byte_count - 1u, output,
    byte_count), W_SEED_TARGET_LAYOUT0_SERIALIZATION_LENGTH_MISMATCH,
    "short input length rejected");
  check(output[0] == 0xa5u, "short input leaves output unchanged");
  check_status(w_seed_target_layout0_serialize_scalar(profile, width_bits,
    W_SEED_TARGET_LAYOUT0_BYTE_ORDER_BIG, value, byte_count + 1u, output,
    byte_count), W_SEED_TARGET_LAYOUT0_SERIALIZATION_LENGTH_MISMATCH,
    "long input length rejected");
  check(output[0] == 0xa5u, "long input leaves output unchanged");
  check_status(w_seed_target_layout0_serialize_scalar(profile, width_bits,
    W_SEED_TARGET_LAYOUT0_BYTE_ORDER_BIG, value, byte_count, output,
    byte_count - 1u), W_SEED_TARGET_LAYOUT0_SERIALIZATION_LENGTH_MISMATCH,
    "short output length rejected");
  check(output[0] == 0xa5u, "short output leaves output unchanged");
  check_status(w_seed_target_layout0_serialize_scalar(profile, width_bits,
    W_SEED_TARGET_LAYOUT0_BYTE_ORDER_BIG, value, byte_count, output,
    byte_count + 1u), W_SEED_TARGET_LAYOUT0_SERIALIZATION_LENGTH_MISMATCH,
    "long output length rejected");
  check(output[0] == 0xa5u, "long output leaves output unchanged");
  check_status(w_seed_target_layout0_serialize_scalar(profile, width_bits,
    (w_seed_target_layout0_byte_order)99, value, byte_count, output,
    byte_count), W_SEED_TARGET_LAYOUT0_UNSUPPORTED_BYTE_ORDER,
    "unknown byte order rejected");
  check(output[0] == 0xa5u, "unknown byte order leaves output unchanged");

  memset(output, 0xa5, sizeof(output));
  check_status(w_seed_target_layout0_serialize_scalar(profile, width_bits,
    W_SEED_TARGET_LAYOUT0_BYTE_ORDER_BIG, NULL, byte_count, output,
    byte_count), W_SEED_TARGET_LAYOUT0_INVALID_ARGUMENT,
    "null input rejected");
  check(output[0] == 0xa5u, "null input leaves output unchanged");
  check_status(w_seed_target_layout0_serialize_scalar(profile, width_bits,
    W_SEED_TARGET_LAYOUT0_BYTE_ORDER_BIG, value, byte_count, NULL,
    byte_count), W_SEED_TARGET_LAYOUT0_INVALID_ARGUMENT,
    "null output rejected");

  memcpy(output, value, byte_count);
  check_status(w_seed_target_layout0_serialize_scalar(profile, width_bits,
    W_SEED_TARGET_LAYOUT0_BYTE_ORDER_BIG, output, byte_count, output,
    byte_count), W_SEED_TARGET_LAYOUT0_OK, "in-place serialization");
  check(memcmp(output, expected_big, byte_count) == 0,
    "in-place serialization is staged");

  memset(overlap, 0xa5, sizeof(overlap));
  memcpy(overlap, value, byte_count);
  check_status(w_seed_target_layout0_serialize_scalar(profile, width_bits,
    W_SEED_TARGET_LAYOUT0_BYTE_ORDER_BIG, overlap, byte_count, overlap + 1u,
    byte_count), W_SEED_TARGET_LAYOUT0_OK,
    "partially overlapping serialization");
  check(memcmp(overlap + 1u, expected_big, byte_count) == 0,
    "partially overlapping serialization is staged");

  (void)label;
}

static void check_invalid_inputs(
  const w_seed_target_layout0_profile *profile,
  const char *triple,
  const char *toolchain_identity,
  const char *data_layout,
  const char *other_target_data_layout) {
  w_seed_target_layout0_profile sentinel = {
    W_SEED_TARGET_LAYOUT0_TARGET_X86_64_WINDOWS_MSVC,
    "sentinel-triple",
    "sentinel-toolchain",
    "sentinel-layout",
    777u,
    W_SEED_TARGET_LAYOUT0_ENDIANNESS_BIG
  };
  w_seed_target_layout0_scalar_layout layout = {
    W_SEED_TARGET_LAYOUT0_SCALAR_FLOAT, 99u, 98u, 97u, 96u
  };
  w_seed_target_layout0_endianness resolved =
    W_SEED_TARGET_LAYOUT0_ENDIANNESS_BIG;

  check_status(w_seed_target_layout0_bind(NULL, toolchain_identity,
    data_layout, &sentinel), W_SEED_TARGET_LAYOUT0_MISSING_TARGET,
    "absent target rejected");
  check(strcmp(sentinel.target_triple, "sentinel-triple") == 0 &&
    sentinel.pointer_width_bits == 777u,
    "failed bind leaves profile unchanged");
  check_status(w_seed_target_layout0_bind(triple, toolchain_identity, NULL,
    &sentinel), W_SEED_TARGET_LAYOUT0_MISSING_DATA_LAYOUT,
    "absent data layout rejected");
  check_status(w_seed_target_layout0_bind(triple, NULL, data_layout,
    &sentinel), W_SEED_TARGET_LAYOUT0_MISSING_TOOLCHAIN_IDENTITY,
    "absent toolchain identity rejected");
  check_status(w_seed_target_layout0_bind("x86_64-unknown-none",
    toolchain_identity, data_layout, &sentinel),
    W_SEED_TARGET_LAYOUT0_UNSUPPORTED_TARGET, "unknown target rejected");
  check_status(w_seed_target_layout0_bind(triple, toolchain_identity,
    "e-m:generic-test-layout", &sentinel),
    W_SEED_TARGET_LAYOUT0_DATA_LAYOUT_MISMATCH,
    "generic/mismatched layout rejected");
  check_status(w_seed_target_layout0_bind(triple, toolchain_identity,
    other_target_data_layout, &sentinel),
    W_SEED_TARGET_LAYOUT0_DATA_LAYOUT_MISMATCH,
    "layout for another target rejected");
  check_status(w_seed_target_layout0_bind(triple, "  ", data_layout,
    &sentinel), W_SEED_TARGET_LAYOUT0_INVALID_TOOLCHAIN_IDENTITY,
    "blank toolchain identity rejected");

  check_status(w_seed_target_layout0_scalar_layout_for(profile,
    W_SEED_TARGET_LAYOUT0_SCALAR_INTEGER, 24u, 24u, &layout),
    W_SEED_TARGET_LAYOUT0_UNSUPPORTED_WIDTH, "unsupported width rejected");
  check(scalar_layout_equals(&layout, W_SEED_TARGET_LAYOUT0_SCALAR_FLOAT,
    99u, 98u, 97u, 96u), "failed scalar lookup leaves output unchanged");
  check_status(w_seed_target_layout0_scalar_layout_for(profile,
    W_SEED_TARGET_LAYOUT0_SCALAR_INTEGER, 64u, 24u, &layout),
    W_SEED_TARGET_LAYOUT0_UNSUPPORTED_ALIGNMENT,
    "unsupported alignment rejected");
  check_status(w_seed_target_layout0_scalar_layout_for(profile,
    W_SEED_TARGET_LAYOUT0_SCALAR_INTEGER, 64u, 128u, &layout),
    W_SEED_TARGET_LAYOUT0_ABI_ALIGNMENT_MISMATCH,
    "supported but mismatched alignment rejected");
  check_status(w_seed_target_layout0_scalar_layout_for(profile,
    (w_seed_target_layout0_scalar_kind)99, 64u, 64u, &layout),
    W_SEED_TARGET_LAYOUT0_UNSUPPORTED_SCALAR_KIND,
    "unknown scalar kind rejected");
  check_status(w_seed_target_layout0_scalar_layout_for(profile,
    W_SEED_TARGET_LAYOUT0_SCALAR_FLOAT, 8u, 8u, &layout),
    W_SEED_TARGET_LAYOUT0_UNSUPPORTED_WIDTH, "f8 storage is not implied");

  check_status(w_seed_target_layout0_resolve_byte_order(profile,
    (w_seed_target_layout0_byte_order)99, &resolved),
    W_SEED_TARGET_LAYOUT0_UNSUPPORTED_BYTE_ORDER,
    "unknown byte order cannot resolve");
  check(resolved == W_SEED_TARGET_LAYOUT0_ENDIANNESS_BIG,
    "failed byte-order resolution leaves output unchanged");

  {
    w_seed_target_layout0_profile changed = *profile;
    changed.pointer_width_bits = 32u;
    check_status(w_seed_target_layout0_resolve_byte_order(&changed,
      W_SEED_TARGET_LAYOUT0_BYTE_ORDER_NATIVE, &resolved),
      W_SEED_TARGET_LAYOUT0_INVALID_PROFILE,
      "profile pointer-width mutation rejected");
  }
  {
    w_seed_target_layout0_profile changed = *profile;
    changed.target_triple = profile->target ==
      W_SEED_TARGET_LAYOUT0_TARGET_X86_64_WINDOWS_MSVC
      ? linux_triple : windows_triple;
    check_status(w_seed_target_layout0_resolve_byte_order(&changed,
      W_SEED_TARGET_LAYOUT0_BYTE_ORDER_NATIVE, &resolved),
      W_SEED_TARGET_LAYOUT0_INVALID_PROFILE,
      "profile target mutation rejected");
  }
  {
    w_seed_target_layout0_profile changed = *profile;
    changed.endianness = W_SEED_TARGET_LAYOUT0_ENDIANNESS_BIG;
    check_status(w_seed_target_layout0_resolve_byte_order(&changed,
      W_SEED_TARGET_LAYOUT0_BYTE_ORDER_NATIVE, &resolved),
      W_SEED_TARGET_LAYOUT0_INVALID_PROFILE,
      "profile endianness mutation rejected");
  }
}

static void check_target(const char *triple, const char *data_layout,
  const char *toolchain_identity,
  w_seed_target_layout0_target expected_target) {
  static const uint16_t integer_widths[] = {8u, 16u, 32u, 64u, 128u};
  static const uint16_t float_widths[] = {16u, 32u, 64u, 128u};
  w_seed_target_layout0_profile profile = {0};
  w_seed_target_layout0_scalar_layout boolean_layout = {0};
  w_seed_target_layout0_endianness resolved =
    W_SEED_TARGET_LAYOUT0_ENDIANNESS_INVALID;
  size_t index;

  check_status(w_seed_target_layout0_bind(triple, toolchain_identity,
    data_layout, &profile), W_SEED_TARGET_LAYOUT0_OK,
    "bind exact target and compiler data layout");
  check(profile.target == expected_target, "bound target identity");
  check(strcmp(profile.target_triple, triple) == 0,
    "bound canonical target triple");
  check(strcmp(profile.toolchain_identity, toolchain_identity) == 0,
    "bound observed toolchain identity");
  check(strcmp(profile.llvm_data_layout, data_layout) == 0,
    "bound exact LLVM data layout");
  check(profile.pointer_width_bits == 64u, "target pointer width");
  check(profile.endianness == W_SEED_TARGET_LAYOUT0_ENDIANNESS_LITTLE,
    "target endianness comes from selected data layout");

  check_status(w_seed_target_layout0_scalar_layout_for(&profile,
    W_SEED_TARGET_LAYOUT0_SCALAR_BOOL, 1u, 8u, &boolean_layout),
    W_SEED_TARGET_LAYOUT0_OK, "Bool LLVM scalar storage facts");
  check(scalar_layout_equals(&boolean_layout,
    W_SEED_TARGET_LAYOUT0_SCALAR_BOOL, 1u, 8u, 8u, 8u),
    "Bool logical/store/allocation/alignment facts");
  check_layout_family(&profile, W_SEED_TARGET_LAYOUT0_SCALAR_INTEGER,
    integer_widths, sizeof(integer_widths) / sizeof(integer_widths[0]),
    "integer LLVM layout");
  check_layout_family(&profile, W_SEED_TARGET_LAYOUT0_SCALAR_FLOAT,
    float_widths, sizeof(float_widths) / sizeof(float_widths[0]),
    "float LLVM layout");

  check_status(w_seed_target_layout0_resolve_byte_order(&profile,
    W_SEED_TARGET_LAYOUT0_BYTE_ORDER_NATIVE, &resolved),
    W_SEED_TARGET_LAYOUT0_OK, "native byte order resolves");
  check(resolved == W_SEED_TARGET_LAYOUT0_ENDIANNESS_LITTLE,
    "native byte order is selected-target little endian");
  for (index = 0u; index < sizeof(integer_widths) / sizeof(integer_widths[0]);
       ++index) {
    check_serialization(&profile, integer_widths[index],
      "target scalar serialization");
  }
  check_invalid_inputs(&profile, triple, toolchain_identity, data_layout,
    expected_target == W_SEED_TARGET_LAYOUT0_TARGET_X86_64_WINDOWS_MSVC
      ? linux_data_layout : windows_data_layout);

  (void)printf("target-layout0: %s, pointer=64, endian=little, toolchain=%s\n",
    triple, toolchain_identity);
}

int main(int argc, char **argv) {
  if (argc == 1) {
    check_target(windows_triple, windows_data_layout,
      "unit-test clang 23.1.1", W_SEED_TARGET_LAYOUT0_TARGET_X86_64_WINDOWS_MSVC);
    check_target(linux_triple, linux_data_layout,
      "unit-test clang 23.1.1", W_SEED_TARGET_LAYOUT0_TARGET_X86_64_LINUX_GNU);
  } else if (argc == 7 && strcmp(argv[1], "--target") == 0 &&
      strcmp(argv[3], "--toolchain") == 0 &&
      strcmp(argv[5], "--data-layout") == 0) {
    if (strcmp(argv[2], windows_triple) == 0) {
      check_target(argv[2], argv[6], argv[4],
        W_SEED_TARGET_LAYOUT0_TARGET_X86_64_WINDOWS_MSVC);
    } else if (strcmp(argv[2], linux_triple) == 0) {
      check_target(argv[2], argv[6], argv[4],
        W_SEED_TARGET_LAYOUT0_TARGET_X86_64_LINUX_GNU);
    } else {
      check(false, "target gate only accepts exact closed triples");
    }
  } else {
    check(false,
      "usage: test [--target TRIPLE --toolchain ID --data-layout LAYOUT]");
  }

  if (failures != 0u) {
    (void)fprintf(stderr, "target-layout0: %u failure(s)\n", failures);
    return 1;
  }
  (void)puts("target-layout0: scalar layout and serialization checks passed");
  return 0;
}
