#include "w_seed_target_layout0.h"

#include <stdbool.h>
#include <string.h>

#define W_SEED_TARGET_LAYOUT0_MAX_TOOLCHAIN_IDENTITY 255u

typedef struct w_seed_target_layout0_target_record {
  w_seed_target_layout0_target target;
  const char *triple;
  const char *data_layout;
  uint16_t pointer_width_bits;
  w_seed_target_layout0_endianness endianness;
} w_seed_target_layout0_target_record;

/* Exact LLVM data-layout strings emitted by the selected target triples. */
static const w_seed_target_layout0_target_record target_records[] = {
  {
    W_SEED_TARGET_LAYOUT0_TARGET_X86_64_WINDOWS_MSVC,
    "x86_64-pc-windows-msvc",
    "e-m:w-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128",
    64u,
    W_SEED_TARGET_LAYOUT0_ENDIANNESS_LITTLE
  },
  {
    W_SEED_TARGET_LAYOUT0_TARGET_X86_64_LINUX_GNU,
    "x86_64-unknown-linux-gnu",
    "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128",
    64u,
    W_SEED_TARGET_LAYOUT0_ENDIANNESS_LITTLE
  }
};

static const w_seed_target_layout0_target_record *find_record_by_triple(
  const char *triple) {
  size_t index;

  if (triple == NULL) return NULL;
  for (index = 0u; index < sizeof(target_records) / sizeof(target_records[0]);
       ++index) {
    if (strcmp(triple, target_records[index].triple) == 0)
      return &target_records[index];
  }
  return NULL;
}

static const w_seed_target_layout0_target_record *find_record_by_target(
  w_seed_target_layout0_target target) {
  size_t index;

  for (index = 0u; index < sizeof(target_records) / sizeof(target_records[0]);
       ++index) {
    if (target == target_records[index].target) return &target_records[index];
  }
  return NULL;
}

static w_seed_target_layout0_status validate_toolchain_identity(
  const char *identity) {
  const unsigned char *cursor;
  size_t length = 0u;
  bool contains_non_space = false;

  if (identity == NULL || identity[0] == '\0')
    return W_SEED_TARGET_LAYOUT0_MISSING_TOOLCHAIN_IDENTITY;

  for (cursor = (const unsigned char *)identity; *cursor != 0u; ++cursor) {
    if (*cursor < 0x20u || *cursor > 0x7eu)
      return W_SEED_TARGET_LAYOUT0_INVALID_TOOLCHAIN_IDENTITY;
    if (*cursor != (unsigned char)' ') contains_non_space = true;
    ++length;
    if (length > W_SEED_TARGET_LAYOUT0_MAX_TOOLCHAIN_IDENTITY)
      return W_SEED_TARGET_LAYOUT0_INVALID_TOOLCHAIN_IDENTITY;
  }

  if (!contains_non_space || identity[0] == ' ' || identity[length - 1u] == ' ')
    return W_SEED_TARGET_LAYOUT0_INVALID_TOOLCHAIN_IDENTITY;
  return W_SEED_TARGET_LAYOUT0_OK;
}

static w_seed_target_layout0_status validate_profile(
  const w_seed_target_layout0_profile *profile) {
  const w_seed_target_layout0_target_record *record;
  w_seed_target_layout0_status status;

  if (profile == NULL) return W_SEED_TARGET_LAYOUT0_INVALID_ARGUMENT;
  record = find_record_by_target(profile->target);
  if (record == NULL || profile->target_triple == NULL ||
      profile->llvm_data_layout == NULL)
    return W_SEED_TARGET_LAYOUT0_INVALID_PROFILE;

  status = validate_toolchain_identity(profile->toolchain_identity);
  if (status != W_SEED_TARGET_LAYOUT0_OK)
    return W_SEED_TARGET_LAYOUT0_INVALID_PROFILE;
  if (strcmp(profile->target_triple, record->triple) != 0 ||
      strcmp(profile->llvm_data_layout, record->data_layout) != 0 ||
      profile->pointer_width_bits != record->pointer_width_bits ||
      profile->endianness != record->endianness)
    return W_SEED_TARGET_LAYOUT0_INVALID_PROFILE;
  return W_SEED_TARGET_LAYOUT0_OK;
}

static bool supported_alignment(uint16_t alignment_bits) {
  return alignment_bits == 8u || alignment_bits == 16u ||
    alignment_bits == 32u || alignment_bits == 64u ||
    alignment_bits == 128u;
}

static bool supported_fixed_width(uint16_t width_bits) {
  return width_bits == 8u || width_bits == 16u || width_bits == 32u ||
    width_bits == 64u || width_bits == 128u;
}

static w_seed_target_layout0_status scalar_facts(
  w_seed_target_layout0_scalar_kind kind,
  uint16_t width_bits,
  uint16_t *out_store_width_bits,
  uint16_t *out_allocation_width_bits,
  uint16_t *out_alignment_bits) {
  if (kind == W_SEED_TARGET_LAYOUT0_SCALAR_BOOL) {
    if (width_bits != 1u) return W_SEED_TARGET_LAYOUT0_UNSUPPORTED_WIDTH;
    *out_store_width_bits = 8u;
    *out_allocation_width_bits = 8u;
    *out_alignment_bits = 8u;
    return W_SEED_TARGET_LAYOUT0_OK;
  }

  if (kind != W_SEED_TARGET_LAYOUT0_SCALAR_INTEGER &&
      kind != W_SEED_TARGET_LAYOUT0_SCALAR_FLOAT)
    return W_SEED_TARGET_LAYOUT0_UNSUPPORTED_SCALAR_KIND;
  if (!supported_fixed_width(width_bits))
    return W_SEED_TARGET_LAYOUT0_UNSUPPORTED_WIDTH;
  if (kind == W_SEED_TARGET_LAYOUT0_SCALAR_FLOAT && width_bits == 8u)
    return W_SEED_TARGET_LAYOUT0_UNSUPPORTED_WIDTH;

  *out_store_width_bits = width_bits;
  *out_allocation_width_bits = width_bits;
  *out_alignment_bits = width_bits;
  return W_SEED_TARGET_LAYOUT0_OK;
}

w_seed_target_layout0_status w_seed_target_layout0_bind(
  const char *target_triple,
  const char *toolchain_identity,
  const char *llvm_data_layout,
  w_seed_target_layout0_profile *out_profile) {
  const w_seed_target_layout0_target_record *record;
  w_seed_target_layout0_profile profile;
  w_seed_target_layout0_status status;

  if (out_profile == NULL) return W_SEED_TARGET_LAYOUT0_INVALID_ARGUMENT;
  if (target_triple == NULL || target_triple[0] == '\0')
    return W_SEED_TARGET_LAYOUT0_MISSING_TARGET;
  if (llvm_data_layout == NULL || llvm_data_layout[0] == '\0')
    return W_SEED_TARGET_LAYOUT0_MISSING_DATA_LAYOUT;

  status = validate_toolchain_identity(toolchain_identity);
  if (status != W_SEED_TARGET_LAYOUT0_OK) return status;

  record = find_record_by_triple(target_triple);
  if (record == NULL) return W_SEED_TARGET_LAYOUT0_UNSUPPORTED_TARGET;
  if (strcmp(llvm_data_layout, record->data_layout) != 0)
    return W_SEED_TARGET_LAYOUT0_DATA_LAYOUT_MISMATCH;

  profile.target = record->target;
  profile.target_triple = record->triple;
  profile.toolchain_identity = toolchain_identity;
  profile.llvm_data_layout = record->data_layout;
  profile.pointer_width_bits = record->pointer_width_bits;
  profile.endianness = record->endianness;
  *out_profile = profile;
  return W_SEED_TARGET_LAYOUT0_OK;
}

w_seed_target_layout0_status w_seed_target_layout0_scalar_layout_for(
  const w_seed_target_layout0_profile *profile,
  w_seed_target_layout0_scalar_kind kind,
  uint16_t logical_width_bits,
  uint16_t expected_llvm_abi_alignment_bits,
  w_seed_target_layout0_scalar_layout *out_layout) {
  w_seed_target_layout0_scalar_layout layout;
  w_seed_target_layout0_status status;

  if (out_layout == NULL) return W_SEED_TARGET_LAYOUT0_INVALID_ARGUMENT;
  status = validate_profile(profile);
  if (status != W_SEED_TARGET_LAYOUT0_OK) return status;
  status = scalar_facts(kind, logical_width_bits, &layout.store_width_bits,
    &layout.allocation_width_bits, &layout.llvm_abi_alignment_bits);
  if (status != W_SEED_TARGET_LAYOUT0_OK) return status;
  if (!supported_alignment(expected_llvm_abi_alignment_bits))
    return W_SEED_TARGET_LAYOUT0_UNSUPPORTED_ALIGNMENT;
  if (expected_llvm_abi_alignment_bits != layout.llvm_abi_alignment_bits)
    return W_SEED_TARGET_LAYOUT0_ABI_ALIGNMENT_MISMATCH;

  layout.kind = kind;
  layout.logical_width_bits = logical_width_bits;
  *out_layout = layout;
  return W_SEED_TARGET_LAYOUT0_OK;
}

w_seed_target_layout0_status w_seed_target_layout0_resolve_byte_order(
  const w_seed_target_layout0_profile *profile,
  w_seed_target_layout0_byte_order requested_order,
  w_seed_target_layout0_endianness *out_order) {
  w_seed_target_layout0_endianness resolved_order;
  w_seed_target_layout0_status status;

  if (out_order == NULL) return W_SEED_TARGET_LAYOUT0_INVALID_ARGUMENT;
  status = validate_profile(profile);
  if (status != W_SEED_TARGET_LAYOUT0_OK) return status;

  switch (requested_order) {
    case W_SEED_TARGET_LAYOUT0_BYTE_ORDER_LITTLE:
      resolved_order = W_SEED_TARGET_LAYOUT0_ENDIANNESS_LITTLE;
      break;
    case W_SEED_TARGET_LAYOUT0_BYTE_ORDER_BIG:
      resolved_order = W_SEED_TARGET_LAYOUT0_ENDIANNESS_BIG;
      break;
    case W_SEED_TARGET_LAYOUT0_BYTE_ORDER_NATIVE:
      resolved_order = profile->endianness;
      break;
    default:
      return W_SEED_TARGET_LAYOUT0_UNSUPPORTED_BYTE_ORDER;
  }

  *out_order = resolved_order;
  return W_SEED_TARGET_LAYOUT0_OK;
}

w_seed_target_layout0_status w_seed_target_layout0_serialize_scalar(
  const w_seed_target_layout0_profile *profile,
  uint16_t logical_width_bits,
  w_seed_target_layout0_byte_order requested_order,
  const uint8_t *value_little_endian,
  size_t value_length,
  uint8_t *out_bytes,
  size_t out_length) {
  uint8_t serialized[16];
  size_t byte_count;
  size_t index;
  w_seed_target_layout0_endianness resolved_order;
  w_seed_target_layout0_status status;

  status = validate_profile(profile);
  if (status != W_SEED_TARGET_LAYOUT0_OK) return status;
  if (!supported_fixed_width(logical_width_bits))
    return W_SEED_TARGET_LAYOUT0_UNSUPPORTED_WIDTH;
  if (value_little_endian == NULL || out_bytes == NULL)
    return W_SEED_TARGET_LAYOUT0_INVALID_ARGUMENT;

  byte_count = (size_t)(logical_width_bits / 8u);
  if (value_length != byte_count || out_length != byte_count)
    return W_SEED_TARGET_LAYOUT0_SERIALIZATION_LENGTH_MISMATCH;
  status = w_seed_target_layout0_resolve_byte_order(
    profile, requested_order, &resolved_order);
  if (status != W_SEED_TARGET_LAYOUT0_OK) return status;

  if (resolved_order == W_SEED_TARGET_LAYOUT0_ENDIANNESS_LITTLE) {
    for (index = 0u; index < byte_count; ++index)
      serialized[index] = value_little_endian[index];
  } else {
    for (index = 0u; index < byte_count; ++index)
      serialized[index] = value_little_endian[byte_count - 1u - index];
  }
  memcpy(out_bytes, serialized, byte_count);
  return W_SEED_TARGET_LAYOUT0_OK;
}

const char *w_seed_target_layout0_status_name(
  w_seed_target_layout0_status status) {
  static const char *const names[] = {
    "ok",
    "invalid-argument",
    "missing-target",
    "missing-toolchain-identity",
    "missing-data-layout",
    "invalid-toolchain-identity",
    "unsupported-target",
    "data-layout-mismatch",
    "invalid-profile",
    "unsupported-scalar-kind",
    "unsupported-width",
    "unsupported-alignment",
    "abi-alignment-mismatch",
    "unsupported-byte-order",
    "serialization-length-mismatch"
  };

  if ((unsigned int)status >= sizeof(names) / sizeof(names[0]))
    return "unknown-status";
  return names[(size_t)status];
}
