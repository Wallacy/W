#include "w_seed_hlo1.h"

#include <limits.h>
#include <string.h>

_Static_assert(CHAR_BIT == 8, "w-seed HLO1 requires 8-bit bytes");

enum {
  HLO1_DIGEST_BYTES = 32,
  HLO1_NEWLINE_BYTES = 1,
  HLO1_PAYLOAD_BYTES = 13,
  HLO1_STDOUT_BYTES = 14,
};

static const char HLO0_SCHEMA[] = "w-seed-hlo0-1";
static const char HLO0_PROFILE[] = "native-process@1";
static const char HLO0_SLOT[] = ".default";
static const char HLO0_ENTRY[] = "main";
static const char HLO0_CALLEE[] = "print";
static const char HLO0_REQUIREMENT[] = "Console";
static const uint8_t HLO0_PAYLOAD[] = {
    0x48u, 0x65u, 0x6cu, 0x6cu, 0x6fu, 0x2cu, 0x20u,
    0x77u, 0x6fu, 0x72u, 0x6cu, 0x64u, 0x21u,
};

static const char C_PREFIX[] =
    "/* " W_SEED_HLO1_SCHEMA_VERSION " */\n"
    "#include <stdio.h>\n"
    "#if defined(_WIN32)\n"
    "#include <fcntl.h>\n"
    "#include <io.h>\n"
    "#endif\n"
    "\n"
    "int main(void) {\n"
    "  static const unsigned char w_output[] = {\n"
    "    ";
static const char C_MIDDLE[] =
    "\n"
    "  };\n"
    "#if defined(_WIN32)\n"
    "  if (_setmode(_fileno(stdout), _O_BINARY) == -1) {\n"
    "    return 1;\n"
    "  }\n"
    "#endif\n"
    "  if (fwrite(w_output, 1u, sizeof(w_output), stdout) !=\n"
    "      sizeof(w_output)) {\n"
    "    return 1;\n"
    "  }\n"
    "  if (fflush(stdout) != 0) {\n"
    "    return 1;\n"
    "  }\n"
    "  return 0;\n"
    "}\n";

enum {
  HLO1_HEX_BYTE_CHARS = 4,
  HLO1_HEX_SEPARATOR_CHARS = 2,
};

_Static_assert(
    sizeof(C_PREFIX) - 1u +
            ((size_t)HLO1_STDOUT_BYTES * HLO1_HEX_BYTE_CHARS) +
            ((size_t)(HLO1_STDOUT_BYTES - 1u) * HLO1_HEX_SEPARATOR_CHARS) +
            sizeof(C_MIDDLE) - 1u <=
        W_SEED_HLO1_MAX_C_BYTES,
    "w-seed HLO1 artifact template exceeds its bound");

static const char HEX[] = "0123456789abcdef";

static bool bounded_length(const char *text, size_t capacity,
                           size_t *length) {
  if (text == NULL || length == NULL) return false;
  for (size_t index = 0u; index < capacity; index += 1u) {
    if (text[index] == '\0') {
      *length = index;
      return true;
    }
  }
  return false;
}

static bool plan_text_is(const char *text, size_t capacity,
                         const char *expected) {
  if (expected == NULL) return false;
  size_t length = 0u;
  if (!bounded_length(text, capacity, &length)) return false;
  const size_t expected_length = strlen(expected);
  return length == expected_length &&
         memcmp(text, expected, expected_length) == 0;
}

static bool digest_equal(const uint8_t *left, const uint8_t *right,
                         size_t length) {
  return left != NULL && right != NULL && memcmp(left, right, length) == 0;
}

static bool validate_plan(const w_seed_hlo0_plan *plan) {
  if (plan == NULL ||
      !plan_text_is(plan->schema, sizeof(plan->schema), HLO0_SCHEMA) ||
      !plan_text_is(plan->profile, sizeof(plan->profile), HLO0_PROFILE) ||
      !plan_text_is(plan->slot, sizeof(plan->slot), HLO0_SLOT) ||
      !plan_text_is(plan->entry_target, sizeof(plan->entry_target), HLO0_ENTRY) ||
      !plan_text_is(plan->handler, sizeof(plan->handler), HLO0_ENTRY) ||
      !plan_text_is(plan->callee, sizeof(plan->callee), HLO0_CALLEE) ||
      !plan_text_is(plan->requirement, sizeof(plan->requirement),
                    HLO0_REQUIREMENT) ||
      plan->is_async || plan->is_throws || plan->is_unsafe ||
      plan->has_borrow_clause || !plan->zero_parameters ||
      !plan->unit_return ||
      plan->newline_policy != W_SEED_HLO0_NEWLINE_ADD_LF ||
      plan->payload_bytes != HLO1_PAYLOAD_BYTES ||
      memcmp(plan->payload, HLO0_PAYLOAD, HLO1_PAYLOAD_BYTES) != 0 ||
      plan->stdout_bytes != HLO1_STDOUT_BYTES || !plan->exit_success) {
    return false;
  }

  w_seed_sha256_state state;
  uint8_t expected_digest[HLO1_DIGEST_BYTES];
  static const uint8_t line_feed = 0x0au;
  w_seed_sha256_init(&state);
  w_seed_sha256_update(&state, plan->payload, plan->payload_bytes);
  w_seed_sha256_update(&state, &line_feed, HLO1_NEWLINE_BYTES);
  w_seed_sha256_final(&state, expected_digest);
  return digest_equal(plan->stdout_sha256, expected_digest,
                      HLO1_DIGEST_BYTES);
}

static bool append_bytes(uint8_t *buffer, size_t capacity, size_t *offset,
                         const void *bytes, size_t length) {
  if (buffer == NULL || offset == NULL ||
      (bytes == NULL && length != 0u) || *offset > capacity ||
      length > capacity - *offset)
    return false;
  if (length != 0u) (void)memcpy(buffer + *offset, bytes, length);
  *offset += length;
  return true;
}

static bool append_literal(uint8_t *buffer, size_t capacity, size_t *offset,
                           const char *literal) {
  if (literal == NULL) return false;
  return append_bytes(buffer, capacity, offset, literal, strlen(literal));
}

static bool append_hex_byte(uint8_t *buffer, size_t capacity, size_t *offset,
                            uint8_t value) {
  const uint8_t bytes[] = {'0', 'x', (uint8_t)HEX[value >> 4u],
                           (uint8_t)HEX[value & 0x0fu]};
  return append_bytes(buffer, capacity, offset, bytes, sizeof(bytes));
}

static bool build_artifact(const w_seed_hlo0_plan *plan, uint8_t *artifact,
                           size_t capacity, size_t *written,
                           uint8_t digest[HLO1_DIGEST_BYTES]) {
  if (plan == NULL || artifact == NULL || written == NULL || digest == NULL ||
      !validate_plan(plan))
    return false;
  size_t offset = 0u;
  if (!append_literal(artifact, capacity, &offset, C_PREFIX)) return false;
  for (size_t index = 0u; index <= plan->payload_bytes; index += 1u) {
    if (index != 0u && !append_literal(artifact, capacity, &offset, ", "))
      return false;
    const uint8_t value = index == plan->payload_bytes
                              ? (uint8_t)0x0au
                              : plan->payload[index];
    if (!append_hex_byte(artifact, capacity, &offset, value)) return false;
  }
  if (!append_literal(artifact, capacity, &offset, C_MIDDLE)) return false;
  *written = offset;
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  w_seed_sha256_update(&state, artifact, offset);
  w_seed_sha256_final(&state, digest);
  return true;
}

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

static void result_reset(w_seed_hlo1_result *result) {
  if (result != NULL) (void)memset(result, 0, sizeof(*result));
}

static bool measure_arguments_alias(const w_seed_hlo0_plan *plan,
                                    const w_seed_hlo1_counts *counts,
                                    const w_seed_hlo1_result *result) {
  return ranges_overlap(plan, sizeof(*plan), counts, sizeof(*counts)) ||
         ranges_overlap(plan, sizeof(*plan), result, sizeof(*result)) ||
         ranges_overlap(counts, sizeof(*counts), result, sizeof(*result));
}

static bool emit_records_alias(const w_seed_hlo0_plan *plan,
                               const w_seed_hlo1_output *output,
                               const w_seed_hlo1_result *result) {
  if (ranges_overlap(plan, sizeof(*plan), result, sizeof(*result)))
    return true;
  if (output == NULL) return false;
  return ranges_overlap(plan, sizeof(*plan), output, sizeof(*output)) ||
         ranges_overlap(result, sizeof(*result), output, sizeof(*output));
}

w_seed_hlo1_status w_seed_hlo1_measure(const w_seed_hlo0_plan *plan,
                                        w_seed_hlo1_counts *counts,
                                        w_seed_hlo1_result *result) {
  if (plan == NULL || counts == NULL || result == NULL) {
    return W_SEED_HLO1_INVALID_PLAN;
  }
  if (measure_arguments_alias(plan, counts, result))
    return W_SEED_HLO1_ALIAS;
  uint8_t artifact[W_SEED_HLO1_MAX_C_BYTES];
  uint8_t digest[HLO1_DIGEST_BYTES];
  size_t written = 0u;
  if (!build_artifact(plan, artifact, sizeof(artifact), &written, digest))
    return W_SEED_HLO1_INVALID_PLAN;
  result_reset(result);
  (void)memset(counts, 0, sizeof(*counts));
  counts->c_bytes = written;
  result->required = *counts;
  (void)memcpy(result->c_sha256, digest, sizeof(result->c_sha256));
  result->status = W_SEED_HLO1_OK;
  return W_SEED_HLO1_OK;
}

w_seed_hlo1_status w_seed_hlo1_emit(const w_seed_hlo0_plan *plan,
                                     const w_seed_hlo1_output *output,
                                     w_seed_hlo1_result *result) {
  if (result == NULL || plan == NULL) return W_SEED_HLO1_INVALID_PLAN;
  if (emit_records_alias(plan, output, result)) return W_SEED_HLO1_ALIAS;
  uint8_t artifact[W_SEED_HLO1_MAX_C_BYTES];
  uint8_t digest[HLO1_DIGEST_BYTES];
  size_t written = 0u;
  if (!build_artifact(plan, artifact, sizeof(artifact), &written, digest))
    return W_SEED_HLO1_INVALID_PLAN;
  if (output == NULL || output->bytes == NULL || output->capacity < written)
    return W_SEED_HLO1_CAPACITY;
  if (ranges_overlap(plan, sizeof(*plan), output->bytes, written) ||
      ranges_overlap(result, sizeof(*result), output->bytes, written) ||
      ranges_overlap(output, sizeof(*output), output->bytes, written))
    return W_SEED_HLO1_ALIAS;
  result_reset(result);
  result->required.c_bytes = written;
  (void)memcpy(result->c_sha256, digest, sizeof(result->c_sha256));
  (void)memcpy(output->bytes, artifact, written);
  result->written.c_bytes = written;
  result->status = W_SEED_HLO1_OK;
  return W_SEED_HLO1_OK;
}
