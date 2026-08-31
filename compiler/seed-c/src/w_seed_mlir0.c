#include "w_seed_mlir0.h"

#include <limits.h>
#include <string.h>

#include "w_seed_sha256.h"

enum {
  MLIR0_DIGEST_BYTES = 32,
  MLIR0_NEWLINE_BYTES = 1,
  MLIR0_ESCAPE_BYTES_PER_INPUT = 3,
  MLIR0_DECIMAL_FIELDS = 4,
  MLIR0_DECIMAL_MAX_BYTES = 3,
  MLIR0_MAX_STDOUT_BYTES = W_SEED_HLO0_MAX_PAYLOAD + MLIR0_NEWLINE_BYTES,
};

static const char MLIR0_SCHEMA_COMMENT[] =
    "// " W_SEED_MLIR0_SCHEMA_VERSION "\n";
static const char MLIR0_PREFIX[] =
    "module attributes {llvm.target_triple = \"" W_SEED_MLIR0_TARGET_TRIPLE
    "\"} {\n"
    "  llvm.mlir.global private constant @w_seed_mlir0_payload(\"";
static const char MLIR0_GLOBAL_MIDDLE[] =
    "\") : !llvm.array<";
static const char MLIR0_GLOBAL_SUFFIX[] =
    " x i8>\n"
    "  llvm.func @write(%fd: i32, %buffer: !llvm.ptr, %count: i64) -> i64\n"
    "  llvm.func @main() -> i32 {\n"
    "    %fd = llvm.mlir.constant(1 : i32) : i32\n"
    "    %length = llvm.mlir.constant(";
static const char MLIR0_LENGTH_MIDDLE[] =
    " : i64) : i64\n"
    "    %zero = llvm.mlir.constant(0 : i32) : i32\n"
    "    %one = llvm.mlir.constant(1 : i32) : i32\n"
    "    %base = llvm.mlir.addressof @w_seed_mlir0_payload : !llvm.ptr\n"
    "    %data = llvm.getelementptr %base[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<";
static const char MLIR0_GEP_SUFFIX[] =
    " x i8>\n"
    "    %written = llvm.call @write(%fd, %data, %length) : (i32, !llvm.ptr, i64) -> i64\n"
    "    %expected = llvm.mlir.constant(";
static const char MLIR0_RETURN_SUFFIX[] =
    " : i64) : i64\n"
    "    %equal = llvm.icmp \"eq\" %written, %expected : i64\n"
    "    %status = llvm.select %equal, %zero, %one : i1, i32\n"
    "    llvm.return %status : i32\n"
    "  }\n"
    "}\n";

static const char MLIR0_HEX[] = "0123456789abcdef";

#define MLIR0_FIXED_LITERAL_BYTES                                            \
  ((sizeof(MLIR0_SCHEMA_COMMENT) - 1u) + (sizeof(MLIR0_PREFIX) - 1u) +       \
   (sizeof(MLIR0_GLOBAL_MIDDLE) - 1u) + (sizeof(MLIR0_GLOBAL_SUFFIX) - 1u) + \
   (sizeof(MLIR0_LENGTH_MIDDLE) - 1u) + (sizeof(MLIR0_GEP_SUFFIX) - 1u) +    \
   (sizeof(MLIR0_RETURN_SUFFIX) - 1u))
#define MLIR0_VARIABLE_ESCAPED_BYTES                                         \
  (((size_t)W_SEED_HLO0_MAX_PAYLOAD + MLIR0_NEWLINE_BYTES) *                 \
   MLIR0_ESCAPE_BYTES_PER_INPUT)
#define MLIR0_DECIMAL_BYTES                                                   \
  ((size_t)MLIR0_DECIMAL_FIELDS * MLIR0_DECIMAL_MAX_BYTES)

_Static_assert(CHAR_BIT == 8, "w-seed MLIR0 requires 8-bit bytes");
_Static_assert(MLIR0_MAX_STDOUT_BYTES <= 999u,
               "w-seed MLIR0 decimal fields must cover the bounded stdout size");
_Static_assert(
    MLIR0_FIXED_LITERAL_BYTES + MLIR0_VARIABLE_ESCAPED_BYTES +
            MLIR0_DECIMAL_BYTES <=
        W_SEED_MLIR0_MAX_BYTES,
    "w-seed MLIR0 artifact bound must cover every literal, escape and decimal");

static bool range_end(uintptr_t start, size_t length, uintptr_t *end) {
  if (end == NULL || length > UINTPTR_MAX - start) return false;
  *end = start + (uintptr_t)length;
  return true;
}

static bool ranges_overlap(const void *left, size_t left_length,
                           const void *right, size_t right_length) {
  if (left == NULL || right == NULL || left_length == 0u ||
      right_length == 0u)
    return false;
  const uintptr_t left_start = (uintptr_t)left;
  const uintptr_t right_start = (uintptr_t)right;
  uintptr_t left_end = 0u;
  uintptr_t right_end = 0u;
  if (!range_end(left_start, left_length, &left_end) ||
      !range_end(right_start, right_length, &right_end))
    return true;
  return left_start < right_end && right_start < left_end;
}

static bool append_bytes(uint8_t *buffer, size_t capacity, size_t *offset,
                         const void *bytes, size_t length) {
  if (buffer == NULL || offset == NULL ||
      (length != 0u && bytes == NULL) || *offset > capacity ||
      length > capacity - *offset)
    return false;
  if (length != 0u) (void)memcpy(buffer + *offset, bytes, length);
  *offset += length;
  return true;
}

static bool append_literal(uint8_t *buffer, size_t capacity, size_t *offset,
                           const char *literal) {
  return literal != NULL &&
         append_bytes(buffer, capacity, offset, literal, strlen(literal));
}

static bool append_size(uint8_t *buffer, size_t capacity, size_t *offset,
                        size_t value) {
  char digits[3u * sizeof(size_t) + 1u];
  size_t length = 0u;
  do {
    digits[length] = (char)('0' + value % 10u);
    value /= 10u;
    length += 1u;
  } while (value != 0u);
  for (size_t index = 0u; index < length / 2u; index += 1u) {
    const char swap = digits[index];
    digits[index] = digits[length - index - 1u];
    digits[length - index - 1u] = swap;
  }
  return append_bytes(buffer, capacity, offset, digits, length);
}

static bool append_hex_byte(uint8_t *buffer, size_t capacity, size_t *offset,
                            uint8_t value) {
  const uint8_t escaped[] = {'\\', (uint8_t)MLIR0_HEX[value >> 4u],
                             (uint8_t)MLIR0_HEX[value & 0x0fu]};
  return append_bytes(buffer, capacity, offset, escaped, sizeof(escaped));
}

static bool append_escaped_bytes(uint8_t *buffer, size_t capacity,
                                  size_t *offset, const uint8_t *bytes,
                                  size_t length) {
  if (bytes == NULL && length != 0u) return false;
  for (size_t index = 0u; index < length; index += 1u)
    if (!append_hex_byte(buffer, capacity, offset, bytes[index])) return false;
  return true;
}

static bool target_is_supported(const w_seed_mlir0_target *target) {
  return target != NULL &&
         target->kind == W_SEED_MLIR0_TARGET_X86_64_UNKNOWN_LINUX_GNU;
}

bool w_seed_mlir0_target_is_supported(const w_seed_mlir0_target *target) {
  return target_is_supported(target);
}

static bool build_artifact(const w_seed_hlo0_plan *plan,
                           const w_seed_mlir0_target *target, uint8_t *artifact,
                           size_t capacity, size_t *written,
                           uint8_t digest[MLIR0_DIGEST_BYTES]) {
  if (plan == NULL || !target_is_supported(target) || artifact == NULL ||
      written == NULL || digest == NULL || !w_seed_hlo0_verify_plan(plan) ||
      plan->stdout_bytes < MLIR0_NEWLINE_BYTES ||
      plan->stdout_bytes > (size_t)UINT32_MAX)
    return false;
  size_t offset = 0u;
  if (!append_literal(artifact, capacity, &offset, MLIR0_SCHEMA_COMMENT) ||
      !append_literal(artifact, capacity, &offset, MLIR0_PREFIX) ||
      !append_escaped_bytes(artifact, capacity, &offset, plan->payload,
                             plan->payload_bytes) ||
      !append_hex_byte(artifact, capacity, &offset, 0x0au) ||
      !append_literal(artifact, capacity, &offset, MLIR0_GLOBAL_MIDDLE) ||
      !append_size(artifact, capacity, &offset, plan->stdout_bytes) ||
      !append_literal(artifact, capacity, &offset, MLIR0_GLOBAL_SUFFIX) ||
      !append_size(artifact, capacity, &offset, plan->stdout_bytes) ||
      !append_literal(artifact, capacity, &offset, MLIR0_LENGTH_MIDDLE) ||
      !append_size(artifact, capacity, &offset, plan->stdout_bytes) ||
      !append_literal(artifact, capacity, &offset, MLIR0_GEP_SUFFIX) ||
      !append_size(artifact, capacity, &offset, plan->stdout_bytes) ||
      !append_literal(artifact, capacity, &offset, MLIR0_RETURN_SUFFIX))
    return false;
  *written = offset;
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  w_seed_sha256_update(&state, artifact, offset);
  w_seed_sha256_final(&state, digest);
  return true;
}

static bool measure_aliases(const w_seed_hlo0_plan *plan,
                            const w_seed_mlir0_target *target,
                            const w_seed_mlir0_counts *counts,
                            const w_seed_mlir0_result *result) {
  return ranges_overlap(plan, sizeof(*plan), counts, sizeof(*counts)) ||
         ranges_overlap(plan, sizeof(*plan), result, sizeof(*result)) ||
         ranges_overlap(target, sizeof(*target), counts, sizeof(*counts)) ||
         ranges_overlap(target, sizeof(*target), result, sizeof(*result)) ||
         ranges_overlap(counts, sizeof(*counts), result, sizeof(*result));
}

static bool emit_descriptor_aliases(const w_seed_hlo0_plan *plan,
                                    const w_seed_mlir0_target *target,
                                    const w_seed_mlir0_output *output,
                                    const w_seed_mlir0_result *result) {
  if (ranges_overlap(plan, sizeof(*plan), result, sizeof(*result)) ||
      ranges_overlap(target, sizeof(*target), result, sizeof(*result)))
    return true;
  if (output == NULL) return false;
  if (ranges_overlap(plan, sizeof(*plan), output, sizeof(*output)) ||
      ranges_overlap(target, sizeof(*target), output, sizeof(*output)) ||
      ranges_overlap(result, sizeof(*result), output, sizeof(*output)))
    return true;
  return false;
}

static bool emit_buffer_aliases(const w_seed_hlo0_plan *plan,
                                const w_seed_mlir0_target *target,
                                const w_seed_mlir0_output *output,
                                const w_seed_mlir0_result *result,
                                size_t bytes) {
  if (output == NULL) return false;
  return ranges_overlap(plan, sizeof(*plan), output->bytes, bytes) ||
         ranges_overlap(target, sizeof(*target), output->bytes, bytes) ||
         ranges_overlap(result, sizeof(*result), output->bytes, bytes) ||
         ranges_overlap(output, sizeof(*output), output->bytes, bytes);
}

w_seed_mlir0_status w_seed_mlir0_measure(
    const w_seed_hlo0_plan *plan, const w_seed_mlir0_target *target,
    w_seed_mlir0_counts *counts, w_seed_mlir0_result *result) {
  if (counts == NULL || result == NULL) return W_SEED_MLIR0_INVALID_PLAN;
  if (measure_aliases(plan, target, counts, result))
    return W_SEED_MLIR0_ALIAS;
  if (!target_is_supported(target)) return W_SEED_MLIR0_UNSUPPORTED;
  if (plan == NULL || !w_seed_hlo0_verify_plan(plan))
    return W_SEED_MLIR0_INVALID_PLAN;
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  uint8_t digest[MLIR0_DIGEST_BYTES];
  size_t written = 0u;
  if (!build_artifact(plan, target, artifact, sizeof(artifact), &written,
                      digest))
    return W_SEED_MLIR0_INVALID_PLAN;
  const w_seed_mlir0_counts candidate_counts = {written};
  w_seed_mlir0_result candidate_result;
  (void)memset(&candidate_result, 0, sizeof(candidate_result));
  candidate_result.status = W_SEED_MLIR0_OK;
  candidate_result.required = candidate_counts;
  (void)memcpy(candidate_result.mlir_sha256, digest,
               sizeof(candidate_result.mlir_sha256));
  *counts = candidate_counts;
  *result = candidate_result;
  return W_SEED_MLIR0_OK;
}

w_seed_mlir0_status w_seed_mlir0_emit(
    const w_seed_hlo0_plan *plan, const w_seed_mlir0_target *target,
    const w_seed_mlir0_output *output, w_seed_mlir0_result *result) {
  if (result == NULL || plan == NULL || target == NULL)
    return W_SEED_MLIR0_INVALID_PLAN;
  if (emit_descriptor_aliases(plan, target, output, result))
    return W_SEED_MLIR0_ALIAS;
  if (!target_is_supported(target)) return W_SEED_MLIR0_UNSUPPORTED;
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  uint8_t digest[MLIR0_DIGEST_BYTES];
  size_t written = 0u;
  if (!build_artifact(plan, target, artifact, sizeof(artifact), &written,
                      digest))
    return W_SEED_MLIR0_INVALID_PLAN;
  if (emit_buffer_aliases(plan, target, output, result, written))
    return W_SEED_MLIR0_ALIAS;
  if (output == NULL || output->bytes == NULL || output->capacity < written)
    return W_SEED_MLIR0_CAPACITY;
  w_seed_mlir0_result candidate_result;
  (void)memset(&candidate_result, 0, sizeof(candidate_result));
  candidate_result.status = W_SEED_MLIR0_OK;
  candidate_result.required.mlir_bytes = written;
  candidate_result.written.mlir_bytes = written;
  (void)memcpy(candidate_result.mlir_sha256, digest,
               sizeof(candidate_result.mlir_sha256));
  (void)memcpy(output->bytes, artifact, written);
  *result = candidate_result;
  return W_SEED_MLIR0_OK;
}
