#ifndef W_SEED_TARGET_LAYOUT0_H
#define W_SEED_TARGET_LAYOUT0_H

#include <stddef.h>
#include <stdint.h>

/*
 * Compiler-lifecycle-only scalar storage facts derived from exact LLVM
 * target data layouts. These records do not describe a W/C calling ABI,
 * aggregate layout, or a public target-support claim.
 */
typedef enum w_seed_target_layout0_target {
  W_SEED_TARGET_LAYOUT0_TARGET_INVALID = 0,
  W_SEED_TARGET_LAYOUT0_TARGET_X86_64_WINDOWS_MSVC = 1,
  W_SEED_TARGET_LAYOUT0_TARGET_X86_64_LINUX_GNU = 2
} w_seed_target_layout0_target;

typedef enum w_seed_target_layout0_endianness {
  W_SEED_TARGET_LAYOUT0_ENDIANNESS_INVALID = 0,
  W_SEED_TARGET_LAYOUT0_ENDIANNESS_LITTLE = 1,
  W_SEED_TARGET_LAYOUT0_ENDIANNESS_BIG = 2
} w_seed_target_layout0_endianness;

typedef enum w_seed_target_layout0_byte_order {
  W_SEED_TARGET_LAYOUT0_BYTE_ORDER_LITTLE = 1,
  W_SEED_TARGET_LAYOUT0_BYTE_ORDER_BIG = 2,
  W_SEED_TARGET_LAYOUT0_BYTE_ORDER_NATIVE = 3
} w_seed_target_layout0_byte_order;

typedef enum w_seed_target_layout0_scalar_kind {
  W_SEED_TARGET_LAYOUT0_SCALAR_BOOL = 1,
  W_SEED_TARGET_LAYOUT0_SCALAR_INTEGER = 2,
  W_SEED_TARGET_LAYOUT0_SCALAR_FLOAT = 3
} w_seed_target_layout0_scalar_kind;

typedef enum w_seed_target_layout0_status {
  W_SEED_TARGET_LAYOUT0_OK = 0,
  W_SEED_TARGET_LAYOUT0_INVALID_ARGUMENT,
  W_SEED_TARGET_LAYOUT0_MISSING_TARGET,
  W_SEED_TARGET_LAYOUT0_MISSING_TOOLCHAIN_IDENTITY,
  W_SEED_TARGET_LAYOUT0_MISSING_DATA_LAYOUT,
  W_SEED_TARGET_LAYOUT0_INVALID_TOOLCHAIN_IDENTITY,
  W_SEED_TARGET_LAYOUT0_UNSUPPORTED_TARGET,
  W_SEED_TARGET_LAYOUT0_DATA_LAYOUT_MISMATCH,
  W_SEED_TARGET_LAYOUT0_INVALID_PROFILE,
  W_SEED_TARGET_LAYOUT0_UNSUPPORTED_SCALAR_KIND,
  W_SEED_TARGET_LAYOUT0_UNSUPPORTED_WIDTH,
  W_SEED_TARGET_LAYOUT0_UNSUPPORTED_ALIGNMENT,
  W_SEED_TARGET_LAYOUT0_ABI_ALIGNMENT_MISMATCH,
  W_SEED_TARGET_LAYOUT0_UNSUPPORTED_BYTE_ORDER,
  W_SEED_TARGET_LAYOUT0_SERIALIZATION_LENGTH_MISMATCH
} w_seed_target_layout0_status;

/*
 * A successful bind stores canonical target/layout pointers and borrows the
 * caller's toolchain_identity string. Keep that string alive for the profile's
 * lifetime. Public fields are revalidated by every operation so a changed
 * target, layout, pointer width, or byte order fails closed.
 */
typedef struct w_seed_target_layout0_profile {
  w_seed_target_layout0_target target;
  const char *target_triple;
  const char *toolchain_identity;
  const char *llvm_data_layout;
  uint16_t pointer_width_bits;
  w_seed_target_layout0_endianness endianness;
} w_seed_target_layout0_profile;

typedef struct w_seed_target_layout0_scalar_layout {
  w_seed_target_layout0_scalar_kind kind;
  uint16_t logical_width_bits;
  uint16_t store_width_bits;
  uint16_t allocation_width_bits;
  uint16_t llvm_abi_alignment_bits;
} w_seed_target_layout0_scalar_layout;

/* Bind only the two exact triples and the exact target-derived data layouts. */
w_seed_target_layout0_status w_seed_target_layout0_bind(
  const char *target_triple,
  const char *toolchain_identity,
  const char *llvm_data_layout,
  w_seed_target_layout0_profile *out_profile);

/* Validate one scalar fact and an independently supplied LLVM ABI alignment. */
w_seed_target_layout0_status w_seed_target_layout0_scalar_layout_for(
  const w_seed_target_layout0_profile *profile,
  w_seed_target_layout0_scalar_kind kind,
  uint16_t logical_width_bits,
  uint16_t expected_llvm_abi_alignment_bits,
  w_seed_target_layout0_scalar_layout *out_layout);

/* Resolve `.native` from the selected profile, never from the compiler host. */
w_seed_target_layout0_status w_seed_target_layout0_resolve_byte_order(
  const w_seed_target_layout0_profile *profile,
  w_seed_target_layout0_byte_order requested_order,
  w_seed_target_layout0_endianness *out_order);

/*
 * value_little_endian is a canonical bit-pattern byte sequence, independent
 * of host memory. Input and output lengths must both equal width / 8 exactly.
 */
w_seed_target_layout0_status w_seed_target_layout0_serialize_scalar(
  const w_seed_target_layout0_profile *profile,
  uint16_t logical_width_bits,
  w_seed_target_layout0_byte_order requested_order,
  const uint8_t *value_little_endian,
  size_t value_length,
  uint8_t *out_bytes,
  size_t out_length);

const char *w_seed_target_layout0_status_name(
  w_seed_target_layout0_status status);

#endif
