#include "w_seed_mlir0.h"

#include <limits.h>
#include <string.h>

#include "w_seed_native_subset0.h"
#include "w_seed_sha256.h"

enum {
  MLIR0_DIGEST_BYTES = 32,
  MLIR0_NEWLINE_BYTES = 1,
  MLIR0_ESCAPE_BYTES_PER_INPUT = 3,
  MLIR0_DECIMAL_FIELDS = 4,
  MLIR0_DECIMAL_MAX_BYTES = 4,
  MLIR0_MAX_STDOUT_BYTES = W_SEED_NATIVE_SUBSET0_MAX_STDOUT_BYTES,
  MLIR0_DYNAMIC_MAX_ACTIONS =
      W_SEED_NATIVE_SUBSET0_MAX_INTERPOLATION_SEGMENTS +
      (2 * W_SEED_NATIVE_SUBSET0_MAX_CALLS),
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

static const char MLIR0_RUNTIME_HELPERS[] =
    "  llvm.func internal @w_seed_copy(%destination: !llvm.ptr, %offset: i64, %source: !llvm.ptr, %length: i64) -> i64 {\n"
    "    %copy_zero = llvm.mlir.constant(0 : i64) : i64\n"
    "    %copy_one = llvm.mlir.constant(1 : i64) : i64\n"
    "    %copy_end = llvm.add %offset, %length : i64\n"
    "    %copy_empty = llvm.icmp \"eq\" %length, %copy_zero : i64\n"
    "    llvm.cond_br %copy_empty, ^copy_done, ^copy_loop(%copy_zero : i64)\n"
    "  ^copy_loop(%copy_index: i64):\n"
    "    %copy_source = llvm.getelementptr %source[%copy_index] : (!llvm.ptr, i64) -> !llvm.ptr, i8\n"
    "    %copy_byte = llvm.load %copy_source : !llvm.ptr -> i8\n"
    "    %copy_position = llvm.add %offset, %copy_index : i64\n"
    "    %copy_destination = llvm.getelementptr %destination[%copy_position] : (!llvm.ptr, i64) -> !llvm.ptr, i8\n"
    "    llvm.store %copy_byte, %copy_destination : i8, !llvm.ptr\n"
    "    %copy_next = llvm.add %copy_index, %copy_one : i64\n"
    "    %copy_more = llvm.icmp \"ult\" %copy_next, %length : i64\n"
    "    llvm.cond_br %copy_more, ^copy_loop(%copy_next : i64), ^copy_done\n"
    "  ^copy_done:\n"
    "    llvm.return %copy_end : i64\n"
    "  }\n"
    "  llvm.func internal @w_seed_append_i64(%buffer: !llvm.ptr, %offset: i64, %value: i64) -> i64 {\n"
    "    %append_zero = llvm.mlir.constant(0 : i64) : i64\n"
    "    %append_one = llvm.mlir.constant(1 : i64) : i64\n"
    "    %append_ten = llvm.mlir.constant(10 : i64) : i64\n"
    "    %append_ascii_zero = llvm.mlir.constant(48 : i64) : i64\n"
    "    %append_minus = llvm.mlir.constant(45 : i8) : i8\n"
    "    %append_negative = llvm.icmp \"slt\" %value, %append_zero : i64\n"
    "    llvm.cond_br %append_negative, ^append_sign, ^append_count(%value, %value, %offset, %append_one : i64, i64, i64, i64)\n"
    "  ^append_sign:\n"
    "    %append_sign_address = llvm.getelementptr %buffer[%offset] : (!llvm.ptr, i64) -> !llvm.ptr, i8\n"
    "    llvm.store %append_minus, %append_sign_address : i8, !llvm.ptr\n"
    "    %append_magnitude = llvm.sub %append_zero, %value : i64\n"
    "    %append_start = llvm.add %offset, %append_one : i64\n"
    "    llvm.br ^append_count(%append_magnitude, %append_magnitude, %append_start, %append_one : i64, i64, i64, i64)\n"
    "  ^append_count(%append_original: i64, %append_current: i64, %append_digit_start: i64, %append_digits: i64):\n"
    "    %append_count_quotient = llvm.udiv %append_current, %append_ten : i64\n"
    "    %append_count_more = llvm.icmp \"ne\" %append_count_quotient, %append_zero : i64\n"
    "    %append_next_digits = llvm.add %append_digits, %append_one : i64\n"
    "    llvm.cond_br %append_count_more, ^append_count(%append_original, %append_count_quotient, %append_digit_start, %append_next_digits : i64, i64, i64, i64), ^append_begin(%append_original, %append_digit_start, %append_digits : i64, i64, i64)\n"
    "  ^append_begin(%append_begin_value: i64, %append_begin_start: i64, %append_begin_digits: i64):\n"
    "    %append_end = llvm.add %append_begin_start, %append_begin_digits : i64\n"
    "    llvm.br ^append_write(%append_begin_value, %append_end, %append_end : i64, i64, i64)\n"
    "  ^append_write(%append_remaining: i64, %append_position: i64, %append_result: i64):\n"
    "    %append_remainder = llvm.urem %append_remaining, %append_ten : i64\n"
    "    %append_digit_value = llvm.add %append_remainder, %append_ascii_zero : i64\n"
    "    %append_digit = llvm.trunc %append_digit_value : i64 to i8\n"
    "    %append_write_position = llvm.sub %append_position, %append_one : i64\n"
    "    %append_address = llvm.getelementptr %buffer[%append_write_position] : (!llvm.ptr, i64) -> !llvm.ptr, i8\n"
    "    llvm.store %append_digit, %append_address : i8, !llvm.ptr\n"
    "    %append_quotient = llvm.udiv %append_remaining, %append_ten : i64\n"
    "    %append_more = llvm.icmp \"ne\" %append_quotient, %append_zero : i64\n"
    "    llvm.cond_br %append_more, ^append_write(%append_quotient, %append_write_position, %append_result : i64, i64, i64), ^append_done(%append_result : i64)\n"
    "  ^append_done(%append_final: i64):\n"
    "    llvm.return %append_final : i64\n"
    "  }\n";

static const char MLIR0_BOOL_HELPER[] =
    "  llvm.func internal @w_seed_append_bool(%buffer: !llvm.ptr, %offset: i64, %value: i1) -> i64 {\n"
    "    %bool_one = llvm.mlir.constant(1 : i64) : i64\n"
    "    %bool_four = llvm.mlir.constant(4 : i64) : i64\n"
    "    %bool_five = llvm.mlir.constant(5 : i64) : i64\n"
    "    llvm.cond_br %value, ^bool_true, ^bool_false\n"
    "  ^bool_true:\n"
    "    %bool_true_0 = llvm.getelementptr %buffer[%offset] : (!llvm.ptr, i64) -> !llvm.ptr, i8\n"
    "    %bool_t = llvm.mlir.constant(116 : i8) : i8\n"
    "    llvm.store %bool_t, %bool_true_0 : i8, !llvm.ptr\n"
    "    %bool_true_1_offset = llvm.add %offset, %bool_one : i64\n"
    "    %bool_true_1 = llvm.getelementptr %buffer[%bool_true_1_offset] : (!llvm.ptr, i64) -> !llvm.ptr, i8\n"
    "    %bool_r = llvm.mlir.constant(114 : i8) : i8\n"
    "    llvm.store %bool_r, %bool_true_1 : i8, !llvm.ptr\n"
    "    %bool_true_2_offset = llvm.add %bool_true_1_offset, %bool_one : i64\n"
    "    %bool_true_2 = llvm.getelementptr %buffer[%bool_true_2_offset] : (!llvm.ptr, i64) -> !llvm.ptr, i8\n"
    "    %bool_u = llvm.mlir.constant(117 : i8) : i8\n"
    "    llvm.store %bool_u, %bool_true_2 : i8, !llvm.ptr\n"
    "    %bool_true_3_offset = llvm.add %bool_true_2_offset, %bool_one : i64\n"
    "    %bool_true_3 = llvm.getelementptr %buffer[%bool_true_3_offset] : (!llvm.ptr, i64) -> !llvm.ptr, i8\n"
    "    %bool_e = llvm.mlir.constant(101 : i8) : i8\n"
    "    llvm.store %bool_e, %bool_true_3 : i8, !llvm.ptr\n"
    "    %bool_true_end = llvm.add %offset, %bool_four : i64\n"
    "    llvm.return %bool_true_end : i64\n"
    "  ^bool_false:\n"
    "    %bool_false_0 = llvm.getelementptr %buffer[%offset] : (!llvm.ptr, i64) -> !llvm.ptr, i8\n"
    "    %bool_f = llvm.mlir.constant(102 : i8) : i8\n"
    "    llvm.store %bool_f, %bool_false_0 : i8, !llvm.ptr\n"
    "    %bool_false_1_offset = llvm.add %offset, %bool_one : i64\n"
    "    %bool_false_1 = llvm.getelementptr %buffer[%bool_false_1_offset] : (!llvm.ptr, i64) -> !llvm.ptr, i8\n"
    "    %bool_a = llvm.mlir.constant(97 : i8) : i8\n"
    "    llvm.store %bool_a, %bool_false_1 : i8, !llvm.ptr\n"
    "    %bool_false_2_offset = llvm.add %bool_false_1_offset, %bool_one : i64\n"
    "    %bool_false_2 = llvm.getelementptr %buffer[%bool_false_2_offset] : (!llvm.ptr, i64) -> !llvm.ptr, i8\n"
    "    %bool_l = llvm.mlir.constant(108 : i8) : i8\n"
    "    llvm.store %bool_l, %bool_false_2 : i8, !llvm.ptr\n"
    "    %bool_false_3_offset = llvm.add %bool_false_2_offset, %bool_one : i64\n"
    "    %bool_false_3 = llvm.getelementptr %buffer[%bool_false_3_offset] : (!llvm.ptr, i64) -> !llvm.ptr, i8\n"
    "    %bool_s = llvm.mlir.constant(115 : i8) : i8\n"
    "    llvm.store %bool_s, %bool_false_3 : i8, !llvm.ptr\n"
    "    %bool_false_4_offset = llvm.add %bool_false_3_offset, %bool_one : i64\n"
    "    %bool_false_4 = llvm.getelementptr %buffer[%bool_false_4_offset] : (!llvm.ptr, i64) -> !llvm.ptr, i8\n"
    "    %bool_false_e = llvm.mlir.constant(101 : i8) : i8\n"
    "    llvm.store %bool_false_e, %bool_false_4 : i8, !llvm.ptr\n"
    "    %bool_false_end = llvm.add %offset, %bool_five : i64\n"
    "    llvm.return %bool_false_end : i64\n"
    "  }\n";

#define MLIR0_FIXED_LITERAL_BYTES                                            \
  ((sizeof(MLIR0_SCHEMA_COMMENT) - 1u) + (sizeof(MLIR0_PREFIX) - 1u) +       \
   (sizeof(MLIR0_GLOBAL_MIDDLE) - 1u) + (sizeof(MLIR0_GLOBAL_SUFFIX) - 1u) + \
   (sizeof(MLIR0_LENGTH_MIDDLE) - 1u) + (sizeof(MLIR0_GEP_SUFFIX) - 1u) +    \
   (sizeof(MLIR0_RETURN_SUFFIX) - 1u))
#define MLIR0_VARIABLE_ESCAPED_BYTES                                         \
  ((size_t)MLIR0_MAX_STDOUT_BYTES * MLIR0_ESCAPE_BYTES_PER_INPUT)
#define MLIR0_DECIMAL_BYTES                                                   \
  ((size_t)MLIR0_DECIMAL_FIELDS * MLIR0_DECIMAL_MAX_BYTES)
#define MLIR0_REQUIRED_MAX_BYTES                                              \
  (MLIR0_FIXED_LITERAL_BYTES + MLIR0_VARIABLE_ESCAPED_BYTES +                \
   MLIR0_DECIMAL_BYTES)
#define MLIR0_DYNAMIC_SKELETON_MAX_BYTES 8192u
#define MLIR0_DYNAMIC_ACTION_MAX_BYTES 448u
#define MLIR0_DYNAMIC_VALUE_MAX_BYTES 160u
#define MLIR0_DYNAMIC_REQUIRED_MAX_BYTES                                      \
  ((sizeof(MLIR0_SCHEMA_COMMENT) - 1u) +                                     \
   (sizeof(MLIR0_RUNTIME_HELPERS) - 1u) +                              \
   (sizeof(MLIR0_BOOL_HELPER) - 1u) + MLIR0_DYNAMIC_SKELETON_MAX_BYTES + \
   ((size_t)MLIR0_MAX_STDOUT_BYTES * MLIR0_ESCAPE_BYTES_PER_INPUT) +         \
   ((size_t)MLIR0_DYNAMIC_MAX_ACTIONS * MLIR0_DYNAMIC_ACTION_MAX_BYTES) +    \
   ((size_t)W_SEED_NATIVE_SUBSET0_MAX_VALUES *                               \
    MLIR0_DYNAMIC_VALUE_MAX_BYTES))

_Static_assert(CHAR_BIT == 8, "w-seed MLIR0 requires 8-bit bytes");
_Static_assert(MLIR0_MAX_STDOUT_BYTES <= 9999u,
               "w-seed MLIR0 decimal fields must cover the bounded stdout size");
_Static_assert(MLIR0_FIXED_LITERAL_BYTES == 886u,
               "w-seed MLIR0 fixed artifact literals changed");
_Static_assert(MLIR0_REQUIRED_MAX_BYTES <= W_SEED_MLIR0_MAX_BYTES,
               "w-seed MLIR0 must retain the static artifact bound");
_Static_assert(MLIR0_DYNAMIC_REQUIRED_MAX_BYTES <= W_SEED_MLIR0_MAX_BYTES,
               "w-seed MLIR0 must retain the dynamic artifact budget");

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

static bool append_i64(uint8_t *buffer, size_t capacity, size_t *offset,
                       int64_t value) {
  char digits[32];
  size_t length = 0u;
  uint64_t magnitude = 0u;
  if (value < 0) {
    if (!append_bytes(buffer, capacity, offset, "-", 1u)) return false;
    magnitude = (uint64_t)(-(value + 1)) + 1u;
  } else {
    magnitude = (uint64_t)value;
  }
  do {
    digits[length] = (char)('0' + magnitude % 10u);
    magnitude /= 10u;
    length += 1u;
  } while (magnitude != 0u);
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

static bool build_static_artifact(
    const w_seed_native_subset0_sequence *sequence,
    const w_seed_mlir0_target *target, uint8_t *artifact, size_t capacity,
    size_t *written, uint8_t digest[MLIR0_DIGEST_BYTES]) {
  if (sequence == NULL || !target_is_supported(target) || artifact == NULL ||
      written == NULL || digest == NULL ||
      sequence->instruction_count == 0u ||
      sequence->instruction_count > W_SEED_NATIVE_SUBSET0_MAX_INSTRUCTIONS ||
      sequence->call_count == 0u ||
      sequence->call_count > W_SEED_NATIVE_SUBSET0_MAX_CALLS ||
      sequence->binding_count > W_SEED_NATIVE_SUBSET0_MAX_BINDINGS ||
      sequence->binding_count > sequence->instruction_count ||
      sequence->call_count > sequence->instruction_count ||
      sequence->instruction_count - sequence->call_count !=
          sequence->binding_count ||
      sequence->has_interpolation ||
      sequence->stdout_bytes > W_SEED_NATIVE_SUBSET0_MAX_STDOUT_BYTES)
    return false;
  size_t stdout_bytes = 0u;
  size_t offset = 0u;
  if (!append_literal(artifact, capacity, &offset, MLIR0_SCHEMA_COMMENT) ||
      !append_literal(artifact, capacity, &offset, MLIR0_PREFIX))
    return false;
  for (size_t call = 0u; call < sequence->call_count; call += 1u) {
    const w_seed_native_subset0_call_selection *item =
        &sequence->calls[call];
    if (item->payload_bytes > W_SEED_NATIVE_SUBSET0_MAX_PAYLOAD ||
        (item->payload_bytes != 0u && item->payload == NULL) ||
        item->payload_bytes > SIZE_MAX - MLIR0_NEWLINE_BYTES)
      return false;
    const size_t line_bytes = item->payload_bytes + MLIR0_NEWLINE_BYTES;
    if (stdout_bytes > W_SEED_NATIVE_SUBSET0_MAX_STDOUT_BYTES - line_bytes ||
        !append_escaped_bytes(artifact, capacity, &offset, item->payload,
                               item->payload_bytes) ||
        !append_hex_byte(artifact, capacity, &offset, 0x0au))
      return false;
    stdout_bytes += line_bytes;
  }
  if (stdout_bytes != sequence->stdout_bytes ||
      !append_literal(artifact, capacity, &offset, MLIR0_GLOBAL_MIDDLE) ||
      !append_size(artifact, capacity, &offset, stdout_bytes) ||
      !append_literal(artifact, capacity, &offset, MLIR0_GLOBAL_SUFFIX) ||
      !append_size(artifact, capacity, &offset, stdout_bytes) ||
      !append_literal(artifact, capacity, &offset, MLIR0_LENGTH_MIDDLE) ||
      !append_size(artifact, capacity, &offset, stdout_bytes) ||
      !append_literal(artifact, capacity, &offset, MLIR0_GEP_SUFFIX) ||
      !append_size(artifact, capacity, &offset, stdout_bytes) ||
      !append_literal(artifact, capacity, &offset, MLIR0_RETURN_SUFFIX))
    return false;
  *written = offset;
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  w_seed_sha256_update(&state, artifact, offset);
  w_seed_sha256_final(&state, digest);
  return true;
}

typedef enum {
  MLIR0_DYNAMIC_TEXT = 0,
  MLIR0_DYNAMIC_I64,
  MLIR0_DYNAMIC_BOOL,
} mlir0_dynamic_action_kind;

typedef struct {
  mlir0_dynamic_action_kind kind;
  size_t byte_offset;
  size_t byte_count;
  uint32_t value_index;
} mlir0_dynamic_action;

typedef struct {
  uint8_t text[MLIR0_MAX_STDOUT_BYTES];
  size_t text_bytes;
  mlir0_dynamic_action actions[MLIR0_DYNAMIC_MAX_ACTIONS];
  size_t action_count;
  bool has_bool;
} mlir0_dynamic_plan;

static bool dynamic_plan_append_text(mlir0_dynamic_plan *plan,
                                     const uint8_t *bytes, size_t length) {
  if (plan == NULL || (length != 0u && bytes == NULL)) return false;
  if (length == 0u) return true;
  if (plan->action_count >= MLIR0_DYNAMIC_MAX_ACTIONS ||
      plan->text_bytes > sizeof(plan->text) ||
      length > sizeof(plan->text) - plan->text_bytes)
    return false;
  (void)memcpy(plan->text + plan->text_bytes, bytes, length);
  plan->actions[plan->action_count] = (mlir0_dynamic_action){
      MLIR0_DYNAMIC_TEXT, plan->text_bytes, length, 0u};
  plan->text_bytes += length;
  plan->action_count += 1u;
  return true;
}

static bool dynamic_plan_append_i64(mlir0_dynamic_plan *plan,
                                    uint32_t value_index) {
  if (plan == NULL || plan->action_count >= MLIR0_DYNAMIC_MAX_ACTIONS)
    return false;
  plan->actions[plan->action_count] =
      (mlir0_dynamic_action){MLIR0_DYNAMIC_I64, 0u, 0u, value_index};
  plan->action_count += 1u;
  return true;
}

static bool dynamic_plan_append_bool(mlir0_dynamic_plan *plan,
                                     uint32_t value_index) {
  if (plan == NULL || plan->action_count >= MLIR0_DYNAMIC_MAX_ACTIONS)
    return false;
  plan->actions[plan->action_count] =
      (mlir0_dynamic_action){MLIR0_DYNAMIC_BOOL, 0u, 0u, value_index};
  plan->action_count += 1u;
  plan->has_bool = true;
  return true;
}

static bool value_string_bytes(const w_seed_hir0_program *program,
                               const w_seed_hir0_value *value,
                               const uint8_t **bytes, size_t *length) {
  if (program == NULL || value == NULL || bytes == NULL || length == NULL)
    return false;
  if (value->kind == W_SEED_HIR0_VALUE_CONST_STRING) {
    *length = value->byte_count;
    *bytes = *length == 0u ? NULL : program->value_bytes + value->byte_offset;
    return true;
  }
  if (value->kind == W_SEED_HIR0_VALUE_BINDING_READ &&
      value->binding_index < program->binding_count) {
    const w_seed_hir0_binding *binding =
        &program->bindings[value->binding_index];
    if (binding->initializer_value >= program->value_count) return false;
    const w_seed_hir0_value *initializer =
        &program->values[binding->initializer_value];
    if (initializer->kind != W_SEED_HIR0_VALUE_CONST_STRING ||
        initializer->type_index != binding->type_index)
      return false;
    *length = initializer->byte_count;
    *bytes = *length == 0u ? NULL
                          : program->value_bytes + initializer->byte_offset;
    return true;
  }
  return false;
}

static bool build_dynamic_plan(
    const w_seed_hir0_program *program,
    const w_seed_native_subset0_sequence *sequence,
    mlir0_dynamic_plan *plan) {
  if (program == NULL || sequence == NULL || plan == NULL ||
      !sequence->has_interpolation)
    return false;
  mlir0_dynamic_plan candidate;
  (void)memset(&candidate, 0, sizeof(candidate));
  for (size_t call_index = 0u; call_index < sequence->call_count;
       call_index += 1u) {
    const w_seed_hir0_value *root = sequence->calls[call_index].value;
    if (root == NULL) return false;
    if (root->kind != W_SEED_HIR0_VALUE_INTERPOLATED_STRING) {
      const uint8_t *bytes = NULL;
      size_t length = 0u;
      if (!value_string_bytes(program, root, &bytes, &length) ||
          !dynamic_plan_append_text(&candidate, bytes, length))
        return false;
    } else {
      for (size_t ordinal = 0u; ordinal < root->interpolation_segment_count;
           ordinal += 1u) {
        const w_seed_hir0_interpolation_segment *segment =
            &program->interpolation_segments[root->first_interpolation_segment +
                                             ordinal];
        if (segment->kind == W_SEED_HIR0_INTERPOLATION_TEXT) {
          const uint8_t *bytes = segment->byte_count == 0u
                                     ? NULL
                                     : program->value_bytes +
                                           segment->byte_offset;
          if (!dynamic_plan_append_text(&candidate, bytes,
                                        segment->byte_count))
            return false;
          continue;
        }
        if (segment->kind != W_SEED_HIR0_INTERPOLATION_VALUE ||
            segment->value_index >= program->value_count)
          return false;
        const w_seed_hir0_value *embedded =
            &program->values[segment->value_index];
        if (embedded->type_index >= program->type_count) return false;
        const w_seed_hir0_value *effective = embedded;
        uint32_t effective_index = segment->value_index;
        if (embedded->kind == W_SEED_HIR0_VALUE_BINDING_READ) {
          if (embedded->binding_index >= program->binding_count)
            return false;
          const w_seed_hir0_binding *binding =
              &program->bindings[embedded->binding_index];
          if (binding->initializer_value >= program->value_count ||
              binding->type_index != embedded->type_index)
            return false;
          effective_index = binding->initializer_value;
          effective = &program->values[effective_index];
        }
        const w_seed_hir0_type_kind type =
            program->types[embedded->type_index].kind;
        if (type == W_SEED_HIR0_TYPE_I64) {
          if (!dynamic_plan_append_i64(&candidate, effective_index))
            return false;
        } else if (type == W_SEED_HIR0_TYPE_BOOL) {
          if (effective->kind != W_SEED_HIR0_VALUE_CONST_BOOL ||
              !dynamic_plan_append_bool(&candidate, effective_index))
            return false;
        } else if (type == W_SEED_HIR0_TYPE_STRING) {
          const uint8_t *bytes = NULL;
          size_t length = 0u;
          if (!value_string_bytes(program, embedded, &bytes, &length) ||
              !dynamic_plan_append_text(&candidate, bytes, length))
            return false;
        } else {
          return false;
        }
      }
    }
    const uint8_t newline = 0x0au;
    if (!dynamic_plan_append_text(&candidate, &newline, 1u))
      return false;
  }
  if (candidate.action_count == 0u || candidate.text_bytes == 0u)
    return false;
  *plan = candidate;
  return true;
}

static const char *binary_operation(w_seed_hir0_binary_operator operation) {
  switch (operation) {
    case W_SEED_HIR0_BINARY_ADD:
      return "llvm.add";
    case W_SEED_HIR0_BINARY_SUBTRACT:
      return "llvm.sub";
    case W_SEED_HIR0_BINARY_MULTIPLY:
      return "llvm.mul";
    case W_SEED_HIR0_BINARY_DIVIDE:
      return "llvm.sdiv";
    case W_SEED_HIR0_BINARY_REMAINDER:
      return "llvm.srem";
  }
  return NULL;
}

static bool append_value_operations(const w_seed_hir0_program *program,
                                    uint8_t *artifact, size_t capacity,
                                    size_t *offset) {
  if (program == NULL || artifact == NULL || offset == NULL) return false;
  for (size_t index = 0u; index < program->value_count; index += 1u) {
    const w_seed_hir0_value *value = &program->values[index];
    if (value->kind == W_SEED_HIR0_VALUE_CONST_I64) {
      if (!append_literal(artifact, capacity, offset, "    %v") ||
          !append_size(artifact, capacity, offset, index) ||
          !append_literal(artifact, capacity, offset,
                          " = llvm.mlir.constant(") ||
          !append_i64(artifact, capacity, offset, value->integer_value) ||
          !append_literal(artifact, capacity, offset, " : i64) : i64\n"))
        return false;
    } else if (value->kind == W_SEED_HIR0_VALUE_CONST_BOOL) {
      if (!append_literal(artifact, capacity, offset, "    %v") ||
          !append_size(artifact, capacity, offset, index) ||
          !append_literal(artifact, capacity, offset,
                          value->bool_value
                              ? " = llvm.mlir.constant(true) : i1\n"
                              : " = llvm.mlir.constant(false) : i1\n"))
        return false;
    } else if (value->kind == W_SEED_HIR0_VALUE_BINARY_I64) {
      const char *operation = binary_operation(value->binary_operator);
      if (operation == NULL ||
          !append_literal(artifact, capacity, offset, "    %v") ||
          !append_size(artifact, capacity, offset, index) ||
          !append_literal(artifact, capacity, offset, " = ") ||
          !append_literal(artifact, capacity, offset, operation) ||
          !append_literal(artifact, capacity, offset, " %v") ||
          !append_size(artifact, capacity, offset, value->left_value) ||
          !append_literal(artifact, capacity, offset, ", %v") ||
          !append_size(artifact, capacity, offset, value->right_value) ||
          !append_literal(artifact, capacity, offset, " : i64\n"))
        return false;
    }
  }
  return true;
}

static bool append_dynamic_actions(const mlir0_dynamic_plan *plan,
                                   uint8_t *artifact, size_t capacity,
                                   size_t *offset) {
  if (plan == NULL || artifact == NULL || offset == NULL ||
      plan->action_count == 0u || plan->text_bytes == 0u)
    return false;
  for (size_t index = 0u; index < plan->action_count; index += 1u) {
    const mlir0_dynamic_action *action = &plan->actions[index];
    if (action->kind == MLIR0_DYNAMIC_TEXT) {
      if (!append_literal(artifact, capacity, offset, "    %text") ||
          !append_size(artifact, capacity, offset, index) ||
          !append_literal(artifact, capacity, offset,
                          " = llvm.getelementptr %text_base[0, ") ||
          !append_size(artifact, capacity, offset, action->byte_offset) ||
          !append_literal(artifact, capacity, offset,
                          "] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<") ||
          !append_size(artifact, capacity, offset, plan->text_bytes) ||
          !append_literal(artifact, capacity, offset, " x i8>\n") ||
          !append_literal(artifact, capacity, offset, "    %text_length") ||
          !append_size(artifact, capacity, offset, index) ||
          !append_literal(artifact, capacity, offset,
                          " = llvm.mlir.constant(") ||
          !append_size(artifact, capacity, offset, action->byte_count) ||
          !append_literal(artifact, capacity, offset, " : i64) : i64\n") ||
          !append_literal(artifact, capacity, offset, "    %cursor") ||
          !append_size(artifact, capacity, offset, index + 1u) ||
          !append_literal(artifact, capacity, offset,
                          " = llvm.call @w_seed_copy(%buffer, %cursor") ||
          !append_size(artifact, capacity, offset, index) ||
          !append_literal(artifact, capacity, offset, ", %text") ||
          !append_size(artifact, capacity, offset, index) ||
          !append_literal(artifact, capacity, offset, ", %text_length") ||
          !append_size(artifact, capacity, offset, index) ||
          !append_literal(
              artifact, capacity, offset,
              ") : (!llvm.ptr, i64, !llvm.ptr, i64) -> i64\n"))
        return false;
    } else if (action->kind == MLIR0_DYNAMIC_I64) {
      if (!append_literal(artifact, capacity, offset, "    %cursor") ||
          !append_size(artifact, capacity, offset, index + 1u) ||
          !append_literal(
              artifact, capacity, offset,
              " = llvm.call @w_seed_append_i64(%buffer, %cursor") ||
          !append_size(artifact, capacity, offset, index) ||
          !append_literal(artifact, capacity, offset, ", %v") ||
          !append_size(artifact, capacity, offset, action->value_index) ||
          !append_literal(artifact, capacity, offset,
                          ") : (!llvm.ptr, i64, i64) -> i64\n"))
        return false;
    } else if (action->kind == MLIR0_DYNAMIC_BOOL) {
      if (!append_literal(artifact, capacity, offset, "    %cursor") ||
          !append_size(artifact, capacity, offset, index + 1u) ||
          !append_literal(
              artifact, capacity, offset,
              " = llvm.call @w_seed_append_bool(%buffer, %cursor") ||
          !append_size(artifact, capacity, offset, index) ||
          !append_literal(artifact, capacity, offset, ", %v") ||
          !append_size(artifact, capacity, offset, action->value_index) ||
          !append_literal(artifact, capacity, offset,
                          ") : (!llvm.ptr, i64, i1) -> i64\n"))
        return false;
    } else {
      return false;
    }
  }
  return true;
}

static bool build_dynamic_artifact(
    const w_seed_hir0_program *program,
    const w_seed_native_subset0_sequence *sequence,
    const w_seed_mlir0_target *target, uint8_t *artifact, size_t capacity,
    size_t *written, uint8_t digest[MLIR0_DIGEST_BYTES]) {
  if (program == NULL || sequence == NULL || !sequence->has_interpolation ||
      !target_is_supported(target) || artifact == NULL || written == NULL ||
      digest == NULL ||
      sequence->maximum_stdout_bytes > MLIR0_MAX_STDOUT_BYTES)
    return false;
  mlir0_dynamic_plan plan;
  if (!build_dynamic_plan(program, sequence, &plan)) return false;

  size_t offset = 0u;
  if (!append_literal(artifact, capacity, &offset, MLIR0_SCHEMA_COMMENT) ||
      !append_literal(artifact, capacity, &offset,
                      "module attributes {llvm.target_triple = \"" W_SEED_MLIR0_TARGET_TRIPLE
                      "\"} {\n"
                      "  llvm.mlir.global private constant @w_seed_mlir0_text(\"") ||
      !append_escaped_bytes(artifact, capacity, &offset, plan.text,
                            plan.text_bytes) ||
      !append_literal(artifact, capacity, &offset, "\") : !llvm.array<") ||
      !append_size(artifact, capacity, &offset, plan.text_bytes) ||
      !append_literal(
          artifact, capacity, &offset,
          " x i8>\n"
          ) ||
      !append_literal(artifact, capacity, &offset, MLIR0_RUNTIME_HELPERS) ||
      (plan.has_bool &&
       !append_literal(artifact, capacity, &offset, MLIR0_BOOL_HELPER)) ||
      !append_literal(
          artifact, capacity, &offset,
          "  llvm.func @write(%fd: i32, %buffer: !llvm.ptr, %count: i64) -> i64\n"
          "  llvm.func @main() -> i32 {\n"
          "    %capacity = llvm.mlir.constant(4097 : i64) : i64\n"
          "    %buffer = llvm.alloca %capacity x i8 : (i64) -> !llvm.ptr\n"
          "    %text_base = llvm.mlir.addressof @w_seed_mlir0_text : !llvm.ptr\n") ||
      !append_value_operations(program, artifact, capacity, &offset) ||
      !append_literal(artifact, capacity, &offset,
                      "    %cursor0 = llvm.mlir.constant(0 : i64) : i64\n") ||
      !append_dynamic_actions(&plan, artifact, capacity, &offset) ||
      !append_literal(
          artifact, capacity, &offset,
          "    %fd = llvm.mlir.constant(1 : i32) : i32\n") ||
      !append_literal(artifact, capacity, &offset,
                      "    %written = llvm.call @write(%fd, %buffer, %cursor") ||
      !append_size(artifact, capacity, &offset, plan.action_count) ||
      !append_literal(
          artifact, capacity, &offset,
          ") : (i32, !llvm.ptr, i64) -> i64\n"
          "    %equal = llvm.icmp \"eq\" %written, %cursor") ||
      !append_size(artifact, capacity, &offset, plan.action_count) ||
      !append_literal(
          artifact, capacity, &offset,
          " : i64\n"
          "    %success = llvm.mlir.constant(0 : i32) : i32\n"
          "    %failure = llvm.mlir.constant(1 : i32) : i32\n"
          "    %status = llvm.select %equal, %success, %failure : i1, i32\n"
          "    llvm.return %status : i32\n"
          "  }\n"
          "}\n"))
    return false;
  *written = offset;
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  w_seed_sha256_update(&state, artifact, offset);
  w_seed_sha256_final(&state, digest);
  return true;
}

static bool build_artifact(
    const w_seed_hir0_program *program,
    const w_seed_native_subset0_sequence *sequence,
    const w_seed_mlir0_target *target, uint8_t *artifact, size_t capacity,
    size_t *written, uint8_t digest[MLIR0_DIGEST_BYTES]) {
  return sequence != NULL && sequence->has_interpolation
             ? build_dynamic_artifact(program, sequence, target, artifact,
                                      capacity, written, digest)
             : build_static_artifact(sequence, target, artifact, capacity,
                                     written, digest);
}

typedef struct {
  uint8_t text[MLIR0_MAX_STDOUT_BYTES];
  size_t text_bytes;
  mlir0_dynamic_action actions[MLIR0_DYNAMIC_MAX_ACTIONS];
  size_t action_count;
  size_t call_first_action[W_SEED_NATIVE_SUBSET0_MAX_CALLS];
  size_t call_action_count[W_SEED_NATIVE_SUBSET0_MAX_CALLS];
  bool has_bool;
} mlir0_program_plan;

static bool program_plan_append_text(mlir0_program_plan *plan,
                                     const uint8_t *bytes, size_t length) {
  if (plan == NULL || (length != 0u && bytes == NULL)) return false;
  if (length == 0u) return true;
  if (plan->action_count >= MLIR0_DYNAMIC_MAX_ACTIONS ||
      length > sizeof(plan->text) - plan->text_bytes)
    return false;
  (void)memcpy(plan->text + plan->text_bytes, bytes, length);
  plan->actions[plan->action_count] = (mlir0_dynamic_action){
      MLIR0_DYNAMIC_TEXT, plan->text_bytes, length, 0u};
  plan->text_bytes += length;
  plan->action_count += 1u;
  return true;
}

static bool program_plan_append_value(mlir0_program_plan *plan,
                                      const w_seed_hir0_program *program,
                                      uint32_t value_index) {
  if (plan == NULL || program == NULL ||
      value_index >= program->value_count ||
      plan->action_count >= MLIR0_DYNAMIC_MAX_ACTIONS)
    return false;
  const w_seed_hir0_value *value = &program->values[value_index];
  if (value->type_index >= program->type_count) return false;
  const w_seed_hir0_type_kind type = program->types[value->type_index].kind;
  if (type == W_SEED_HIR0_TYPE_I64) {
    plan->actions[plan->action_count] =
        (mlir0_dynamic_action){MLIR0_DYNAMIC_I64, 0u, 0u, value_index};
  } else if (type == W_SEED_HIR0_TYPE_BOOL) {
    plan->actions[plan->action_count] =
        (mlir0_dynamic_action){MLIR0_DYNAMIC_BOOL, 0u, 0u, value_index};
    plan->has_bool = true;
  } else if (type == W_SEED_HIR0_TYPE_STRING) {
    const uint8_t *bytes = NULL;
    size_t length = 0u;
    if (!value_string_bytes(program, value, &bytes, &length)) return false;
    return program_plan_append_text(plan, bytes, length);
  } else {
    return false;
  }
  plan->action_count += 1u;
  return true;
}

static bool program_plan_append_print(mlir0_program_plan *plan,
                                      const w_seed_hir0_program *program,
                                      uint32_t value_index) {
  if (plan == NULL || program == NULL ||
      value_index >= program->value_count)
    return false;
  const w_seed_hir0_value *root = &program->values[value_index];
  if (root->kind != W_SEED_HIR0_VALUE_INTERPOLATED_STRING) {
    const uint8_t *bytes = NULL;
    size_t length = 0u;
    if (!value_string_bytes(program, root, &bytes, &length) ||
        !program_plan_append_text(plan, bytes, length))
      return false;
  } else {
    for (size_t ordinal = 0u; ordinal < root->interpolation_segment_count;
         ordinal += 1u) {
      const size_t segment_index =
          (size_t)root->first_interpolation_segment + ordinal;
      if (segment_index >= program->interpolation_segment_count) return false;
      const w_seed_hir0_interpolation_segment *segment =
          &program->interpolation_segments[segment_index];
      if (segment->kind == W_SEED_HIR0_INTERPOLATION_TEXT) {
        const uint8_t *bytes = segment->byte_count == 0u
                                   ? NULL
                                   : program->value_bytes +
                                         segment->byte_offset;
        if (!program_plan_append_text(plan, bytes, segment->byte_count))
          return false;
      } else if (segment->kind == W_SEED_HIR0_INTERPOLATION_VALUE) {
        if (!program_plan_append_value(plan, program, segment->value_index))
          return false;
      } else {
        return false;
      }
    }
  }
  const uint8_t newline = 0x0au;
  return program_plan_append_text(plan, &newline, 1u);
}

static bool build_program_plan(const w_seed_hir0_program *program,
                               mlir0_program_plan *plan) {
  if (program == NULL || plan == NULL ||
      program->call_count > W_SEED_NATIVE_SUBSET0_MAX_CALLS)
    return false;
  mlir0_program_plan candidate;
  (void)memset(&candidate, 0, sizeof(candidate));
  for (size_t call_index = 0u; call_index < program->call_count;
       call_index += 1u) {
    const w_seed_hir0_call *call = &program->calls[call_index];
    if (call->callee_identity >= program->identity_count) return false;
    const w_seed_hir0_identity *callee =
        &program->identities[call->callee_identity];
    candidate.call_first_action[call_index] = candidate.action_count;
    if (callee->kind == W_SEED_HIR0_IDENTITY_HOST_PRELUDE) {
      if (call->argument_count != 1u ||
          call->first_argument >= program->argument_count ||
          !program_plan_append_print(
              &candidate, program,
              program->arguments[call->first_argument].value_index))
        return false;
    } else if (callee->kind != W_SEED_HIR0_IDENTITY_FUNCTION) {
      return false;
    }
    candidate.call_action_count[call_index] =
        candidate.action_count - candidate.call_first_action[call_index];
  }
  if (candidate.action_count == 0u || candidate.text_bytes == 0u)
    return false;
  *plan = candidate;
  return true;
}

static bool append_program_value_operand(
    const w_seed_hir0_program *program, uint32_t value_index,
    uint32_t function_index, uint8_t *artifact, size_t capacity,
    size_t *offset) {
  if (program == NULL || value_index >= program->value_count ||
      artifact == NULL || offset == NULL)
    return false;
  const w_seed_hir0_value *value = &program->values[value_index];
  if (value->kind == W_SEED_HIR0_VALUE_BINDING_READ) {
    if (value->binding_index >= program->binding_count) return false;
    return append_program_value_operand(
        program, program->bindings[value->binding_index].initializer_value,
        function_index, artifact, capacity, offset);
  }
  if (value->kind == W_SEED_HIR0_VALUE_PARAMETER_READ) {
    if (value->parameter_index >= program->parameter_count) return false;
    const w_seed_hir0_parameter *parameter =
        &program->parameters[value->parameter_index];
    return parameter->owner_function == function_index &&
           append_literal(artifact, capacity, offset, "%p") &&
           append_size(artifact, capacity, offset, parameter->ordinal);
  }
  if (value->kind == W_SEED_HIR0_VALUE_CALL_RESULT)
    return value->call_index < program->call_count &&
           append_literal(artifact, capacity, offset, "%call") &&
           append_size(artifact, capacity, offset, value->call_index);
  return (value->kind == W_SEED_HIR0_VALUE_CONST_I64 ||
          value->kind == W_SEED_HIR0_VALUE_CONST_BOOL ||
          value->kind == W_SEED_HIR0_VALUE_BINARY_I64) &&
         append_literal(artifact, capacity, offset, "%v") &&
         append_size(artifact, capacity, offset, value_index);
}

static bool append_program_value_tree(
    const w_seed_hir0_program *program, uint32_t value_index,
    uint32_t function_index, bool emitted[W_SEED_NATIVE_SUBSET0_MAX_VALUES],
    uint8_t *artifact, size_t capacity, size_t *offset, size_t depth) {
  if (program == NULL || emitted == NULL || artifact == NULL ||
      offset == NULL || depth > 256u || value_index >= program->value_count ||
      value_index >= W_SEED_NATIVE_SUBSET0_MAX_VALUES)
    return false;
  if (emitted[value_index]) return true;
  const w_seed_hir0_value *value = &program->values[value_index];
  if (value->kind == W_SEED_HIR0_VALUE_BINDING_READ ||
      value->kind == W_SEED_HIR0_VALUE_PARAMETER_READ ||
      value->kind == W_SEED_HIR0_VALUE_CALL_RESULT) {
    emitted[value_index] = true;
    return true;
  }
  if (value->kind == W_SEED_HIR0_VALUE_INTERPOLATED_STRING) {
    for (size_t ordinal = 0u; ordinal < value->interpolation_segment_count;
         ordinal += 1u) {
      const w_seed_hir0_interpolation_segment *segment =
          &program->interpolation_segments[value->first_interpolation_segment +
                                           ordinal];
      if (segment->kind == W_SEED_HIR0_INTERPOLATION_VALUE &&
          !append_program_value_tree(program, segment->value_index,
                                     function_index, emitted, artifact,
                                     capacity, offset, depth + 1u))
        return false;
    }
    emitted[value_index] = true;
    return true;
  }
  if (value->kind == W_SEED_HIR0_VALUE_CONST_STRING) {
    emitted[value_index] = true;
    return true;
  }
  if (value->kind == W_SEED_HIR0_VALUE_BINARY_I64) {
    const char *operation = binary_operation(value->binary_operator);
    if (operation == NULL ||
        !append_program_value_tree(program, value->left_value, function_index,
                                   emitted, artifact, capacity, offset,
                                   depth + 1u) ||
        !append_program_value_tree(program, value->right_value, function_index,
                                   emitted, artifact, capacity, offset,
                                   depth + 1u) ||
        !append_literal(artifact, capacity, offset, "    %v") ||
        !append_size(artifact, capacity, offset, value_index) ||
        !append_literal(artifact, capacity, offset, " = ") ||
        !append_literal(artifact, capacity, offset, operation) ||
        !append_literal(artifact, capacity, offset, " ") ||
        !append_program_value_operand(program, value->left_value,
                                      function_index, artifact, capacity,
                                      offset) ||
        !append_literal(artifact, capacity, offset, ", ") ||
        !append_program_value_operand(program, value->right_value,
                                      function_index, artifact, capacity,
                                      offset) ||
        !append_literal(artifact, capacity, offset, " : i64\n"))
      return false;
    emitted[value_index] = true;
    return true;
  }
  if (value->kind == W_SEED_HIR0_VALUE_CONST_I64) {
    if (!append_literal(artifact, capacity, offset, "    %v") ||
        !append_size(artifact, capacity, offset, value_index) ||
        !append_literal(artifact, capacity, offset,
                        " = llvm.mlir.constant(") ||
        !append_i64(artifact, capacity, offset, value->integer_value) ||
        !append_literal(artifact, capacity, offset, " : i64) : i64\n"))
      return false;
    emitted[value_index] = true;
    return true;
  }
  if (value->kind == W_SEED_HIR0_VALUE_CONST_BOOL) {
    if (!append_literal(artifact, capacity, offset, "    %v") ||
        !append_size(artifact, capacity, offset, value_index) ||
        !append_literal(artifact, capacity, offset,
                        value->bool_value
                            ? " = llvm.mlir.constant(true) : i1\n"
                            : " = llvm.mlir.constant(false) : i1\n"))
      return false;
    emitted[value_index] = true;
    return true;
  }
  return false;
}

static const char *program_type_name(const w_seed_hir0_program *program,
                                     uint32_t type_index) {
  if (program == NULL || type_index >= program->type_count) return NULL;
  if (program->types[type_index].kind == W_SEED_HIR0_TYPE_I64) return "i64";
  if (program->types[type_index].kind == W_SEED_HIR0_TYPE_BOOL) return "i1";
  return NULL;
}

static bool append_program_print_actions(
    const w_seed_hir0_program *program, const mlir0_program_plan *plan,
    size_t call_index, uint32_t function_index, uint8_t *artifact,
    size_t capacity, size_t *offset) {
  if (program == NULL || plan == NULL || artifact == NULL || offset == NULL ||
      call_index >= program->call_count ||
      call_index >= W_SEED_NATIVE_SUBSET0_MAX_CALLS)
    return false;
  const size_t first = plan->call_first_action[call_index];
  const size_t count = plan->call_action_count[call_index];
  if (count == 0u || first > plan->action_count ||
      count > plan->action_count - first ||
      !append_literal(artifact, capacity, offset, "    %cursor") ||
      !append_size(artifact, capacity, offset, call_index) ||
      !append_literal(artifact, capacity, offset,
                      "_0 = llvm.load %cursor_address : !llvm.ptr -> i64\n"))
    return false;
  for (size_t ordinal = 0u; ordinal < count; ordinal += 1u) {
    const size_t action_index = first + ordinal;
    const mlir0_dynamic_action *action = &plan->actions[action_index];
    if (action->kind == MLIR0_DYNAMIC_TEXT) {
      if (!append_literal(artifact, capacity, offset, "    %text") ||
          !append_size(artifact, capacity, offset, action_index) ||
          !append_literal(artifact, capacity, offset,
                          " = llvm.getelementptr %text_base[0, ") ||
          !append_size(artifact, capacity, offset, action->byte_offset) ||
          !append_literal(artifact, capacity, offset,
                          "] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<") ||
          !append_size(artifact, capacity, offset, plan->text_bytes) ||
          !append_literal(artifact, capacity, offset, " x i8>\n") ||
          !append_literal(artifact, capacity, offset, "    %text_length") ||
          !append_size(artifact, capacity, offset, action_index) ||
          !append_literal(artifact, capacity, offset,
                          " = llvm.mlir.constant(") ||
          !append_size(artifact, capacity, offset, action->byte_count) ||
          !append_literal(artifact, capacity, offset, " : i64) : i64\n") ||
          !append_literal(artifact, capacity, offset, "    %cursor") ||
          !append_size(artifact, capacity, offset, call_index) ||
          !append_literal(artifact, capacity, offset, "_") ||
          !append_size(artifact, capacity, offset, ordinal + 1u) ||
          !append_literal(artifact, capacity, offset,
                          " = llvm.call @w_seed_copy(%buffer, %cursor") ||
          !append_size(artifact, capacity, offset, call_index) ||
          !append_literal(artifact, capacity, offset, "_") ||
          !append_size(artifact, capacity, offset, ordinal) ||
          !append_literal(artifact, capacity, offset, ", %text") ||
          !append_size(artifact, capacity, offset, action_index) ||
          !append_literal(artifact, capacity, offset, ", %text_length") ||
          !append_size(artifact, capacity, offset, action_index) ||
          !append_literal(
              artifact, capacity, offset,
              ") : (!llvm.ptr, i64, !llvm.ptr, i64) -> i64\n"))
        return false;
    } else if (action->kind == MLIR0_DYNAMIC_I64 ||
               action->kind == MLIR0_DYNAMIC_BOOL) {
      if (!append_literal(artifact, capacity, offset, "    %cursor") ||
          !append_size(artifact, capacity, offset, call_index) ||
          !append_literal(artifact, capacity, offset, "_") ||
          !append_size(artifact, capacity, offset, ordinal + 1u) ||
          !append_literal(artifact, capacity, offset,
                          action->kind == MLIR0_DYNAMIC_I64
                              ? " = llvm.call @w_seed_append_i64(%buffer, %cursor"
                              : " = llvm.call @w_seed_append_bool(%buffer, %cursor") ||
          !append_size(artifact, capacity, offset, call_index) ||
          !append_literal(artifact, capacity, offset, "_") ||
          !append_size(artifact, capacity, offset, ordinal) ||
          !append_literal(artifact, capacity, offset, ", ") ||
          !append_program_value_operand(program, action->value_index,
                                        function_index, artifact, capacity,
                                        offset) ||
          !append_literal(artifact, capacity, offset,
                          action->kind == MLIR0_DYNAMIC_I64
                              ? ") : (!llvm.ptr, i64, i64) -> i64\n"
                              : ") : (!llvm.ptr, i64, i1) -> i64\n"))
        return false;
    } else {
      return false;
    }
  }
  return append_literal(artifact, capacity, offset, "    llvm.store %cursor") &&
         append_size(artifact, capacity, offset, call_index) &&
         append_literal(artifact, capacity, offset, "_") &&
         append_size(artifact, capacity, offset, count) &&
         append_literal(artifact, capacity, offset,
                        ", %cursor_address : i64, !llvm.ptr\n");
}

static bool append_program_local_call(
    const w_seed_hir0_program *program, const w_seed_hir0_call *call,
    uint32_t function_index, uint8_t *artifact, size_t capacity,
    size_t *offset) {
  if (program == NULL || call == NULL || artifact == NULL || offset == NULL ||
      call->callee_identity >= program->identity_count)
    return false;
  const w_seed_hir0_identity *callee =
      &program->identities[call->callee_identity];
  if (callee->kind != W_SEED_HIR0_IDENTITY_FUNCTION ||
      callee->target_index >= program->function_count ||
      call->argument_count > W_SEED_NATIVE_SUBSET0_MAX_PARAMETERS)
    return false;
  uint32_t values[W_SEED_NATIVE_SUBSET0_MAX_PARAMETERS];
  for (size_t index = 0u; index < W_SEED_NATIVE_SUBSET0_MAX_PARAMETERS;
       index += 1u)
    values[index] = W_SEED_HIR0_NONE;
  for (size_t source_ordinal = 0u; source_ordinal < call->argument_count;
       source_ordinal += 1u) {
    const w_seed_hir0_argument *argument =
        &program->arguments[(size_t)call->first_argument + source_ordinal];
    if (argument->parameter_ordinal >= call->argument_count ||
        values[argument->parameter_ordinal] != W_SEED_HIR0_NONE)
      return false;
    values[argument->parameter_ordinal] = argument->value_index;
  }
  if (call->result_type != 0u &&
      (!append_literal(artifact, capacity, offset, "    %call") ||
       !append_size(artifact, capacity, offset,
                    (size_t)(call - program->calls)) ||
       !append_literal(artifact, capacity, offset, " = ")))
    return false;
  if (!append_literal(artifact, capacity, offset,
                      call->result_type == 0u ? "    llvm.call @w_fn_"
                                              : "llvm.call @w_fn_") ||
      !append_size(artifact, capacity, offset, callee->target_index) ||
      !append_literal(artifact, capacity, offset,
                      "(%buffer, %cursor_address"))
    return false;
  for (size_t parameter = 0u; parameter < call->argument_count;
       parameter += 1u)
    if (values[parameter] == W_SEED_HIR0_NONE ||
        !append_literal(artifact, capacity, offset, ", ") ||
        !append_program_value_operand(program, values[parameter],
                                      function_index, artifact, capacity,
                                      offset))
      return false;
  if (!append_literal(artifact, capacity, offset,
                      ") : (!llvm.ptr, !llvm.ptr"))
    return false;
  const w_seed_hir0_function *target =
      &program->functions[callee->target_index];
  for (size_t parameter = 0u; parameter < target->parameter_count;
       parameter += 1u) {
    const w_seed_hir0_parameter *item =
        &program->parameters[(size_t)target->first_parameter + parameter];
    const char *type = program_type_name(program, item->type_index);
    if (type == NULL || !append_literal(artifact, capacity, offset, ", ") ||
        !append_literal(artifact, capacity, offset, type))
      return false;
  }
  if (call->result_type == 0u)
    return append_literal(artifact, capacity, offset, ") -> ()\n");
  const char *return_type = program_type_name(program, call->result_type);
  return return_type != NULL &&
         append_literal(artifact, capacity, offset, ") -> ") &&
         append_literal(artifact, capacity, offset, return_type) &&
         append_literal(artifact, capacity, offset, "\n");
}

static bool append_program_function(
    const w_seed_hir0_program *program, const mlir0_program_plan *plan,
    size_t function_index, uint8_t *artifact, size_t capacity,
    size_t *offset) {
  if (program == NULL || plan == NULL || artifact == NULL || offset == NULL ||
      function_index >= program->function_count)
    return false;
  const w_seed_hir0_function *function = &program->functions[function_index];
  if (!append_literal(artifact, capacity, offset,
                      "  llvm.func internal @w_fn_") ||
      !append_size(artifact, capacity, offset, function_index) ||
      !append_literal(artifact, capacity, offset,
                      "(%buffer: !llvm.ptr, %cursor_address: !llvm.ptr"))
    return false;
  for (size_t ordinal = 0u; ordinal < function->parameter_count;
       ordinal += 1u) {
    const w_seed_hir0_parameter *parameter =
        &program->parameters[(size_t)function->first_parameter + ordinal];
    const char *type = program_type_name(program, parameter->type_index);
    if (type == NULL ||
        !append_literal(artifact, capacity, offset, ", %p") ||
        !append_size(artifact, capacity, offset, ordinal) ||
        !append_literal(artifact, capacity, offset, ": ") ||
        !append_literal(artifact, capacity, offset, type))
      return false;
  }
  if (!append_literal(artifact, capacity, offset, ")"))
    return false;
  if (function->return_type != 0u) {
    const char *return_type =
        program_type_name(program, function->return_type);
    if (return_type == NULL ||
        !append_literal(artifact, capacity, offset, " -> ") ||
        !append_literal(artifact, capacity, offset, return_type))
      return false;
  }
  if (!append_literal(artifact, capacity, offset,
                      " {\n    %text_base = llvm.mlir.addressof "
                      "@w_seed_mlir0_text : !llvm.ptr\n"))
    return false;
  bool emitted[W_SEED_NATIVE_SUBSET0_MAX_VALUES] = {false};
  const w_seed_hir0_block *block = &program->blocks[function->first_block];
  for (size_t ordinal = 0u; ordinal < block->instruction_count;
       ordinal += 1u) {
    const w_seed_hir0_instruction *instruction =
        &program->instructions[(size_t)block->first_instruction + ordinal];
    if (instruction->kind == W_SEED_HIR0_INSTRUCTION_BINDING) {
      if (instruction->binding_index >= program->binding_count ||
          !append_program_value_tree(
              program,
              program->bindings[instruction->binding_index].initializer_value,
              (uint32_t)function_index, emitted, artifact, capacity, offset,
              0u))
        return false;
      continue;
    }
    if (instruction->kind != W_SEED_HIR0_INSTRUCTION_CALL ||
        instruction->call_index >= program->call_count)
      return false;
    const w_seed_hir0_call *call = &program->calls[instruction->call_index];
    for (size_t argument = 0u; argument < call->argument_count;
         argument += 1u)
      if (!append_program_value_tree(
              program,
              program->arguments[(size_t)call->first_argument + argument]
                  .value_index,
              (uint32_t)function_index, emitted, artifact, capacity, offset,
              0u))
        return false;
    const w_seed_hir0_identity *callee =
        &program->identities[call->callee_identity];
    if (callee->kind == W_SEED_HIR0_IDENTITY_HOST_PRELUDE) {
      if (!append_program_print_actions(program, plan,
                                        instruction->call_index,
                                        (uint32_t)function_index, artifact,
                                        capacity, offset))
        return false;
    } else if (!append_program_local_call(program, call,
                                          (uint32_t)function_index, artifact,
                                          capacity, offset)) {
      return false;
    }
  }
  if (block->terminator_index >= program->terminator_count)
    return false;
  const w_seed_hir0_terminator *terminator =
      &program->terminators[block->terminator_index];
  if (function->return_type == 0u)
    return terminator->kind == W_SEED_HIR0_TERMINATOR_RETURN_UNIT &&
           append_literal(artifact, capacity, offset,
                          "    llvm.return\n  }\n");
  const char *return_type = program_type_name(program, function->return_type);
  return return_type != NULL &&
         terminator->kind == W_SEED_HIR0_TERMINATOR_RETURN_VALUE &&
         append_program_value_tree(program, terminator->value_index,
                                   (uint32_t)function_index, emitted, artifact,
                                   capacity, offset, 0u) &&
         append_literal(artifact, capacity, offset, "    llvm.return ") &&
         append_program_value_operand(program, terminator->value_index,
                                      (uint32_t)function_index, artifact,
                                      capacity, offset) &&
         append_literal(artifact, capacity, offset, " : ") &&
         append_literal(artifact, capacity, offset, return_type) &&
         append_literal(artifact, capacity, offset, "\n  }\n");
}

static bool build_program_artifact(
    const w_seed_hir0_program *program,
    const w_seed_native_subset0_program *selection,
    const w_seed_mlir0_target *target, uint8_t *artifact, size_t capacity,
    size_t *written, uint8_t digest[MLIR0_DIGEST_BYTES]) {
  if (program == NULL || selection == NULL || !selection->has_local_calls ||
      !target_is_supported(target) || artifact == NULL || written == NULL ||
      digest == NULL || selection->maximum_stdout_bytes > MLIR0_MAX_STDOUT_BYTES)
    return false;
  mlir0_program_plan plan;
  if (!build_program_plan(program, &plan)) return false;
  size_t offset = 0u;
  if (!append_literal(artifact, capacity, &offset, MLIR0_SCHEMA_COMMENT) ||
      !append_literal(
          artifact, capacity, &offset,
          "module attributes {llvm.target_triple = \"" W_SEED_MLIR0_TARGET_TRIPLE
          "\"} {\n"
          "  llvm.mlir.global private constant @w_seed_mlir0_text(\"") ||
      !append_escaped_bytes(artifact, capacity, &offset, plan.text,
                            plan.text_bytes) ||
      !append_literal(artifact, capacity, &offset, "\") : !llvm.array<") ||
      !append_size(artifact, capacity, &offset, plan.text_bytes) ||
      !append_literal(artifact, capacity, &offset, " x i8>\n") ||
      !append_literal(artifact, capacity, &offset, MLIR0_RUNTIME_HELPERS) ||
      (plan.has_bool &&
       !append_literal(artifact, capacity, &offset, MLIR0_BOOL_HELPER)) ||
      !append_literal(
          artifact, capacity, &offset,
          "  llvm.func @write(%fd: i32, %buffer: !llvm.ptr, %count: i64) -> i64\n"))
    return false;
  for (size_t function = 0u; function < program->function_count;
       function += 1u)
    if (!append_program_function(program, &plan, function, artifact, capacity,
                                 &offset))
      return false;
  if (!append_literal(
          artifact, capacity, &offset,
          "  llvm.func @main() -> i32 {\n"
          "    %capacity = llvm.mlir.constant(4097 : i64) : i64\n"
          "    %buffer = llvm.alloca %capacity x i8 : (i64) -> !llvm.ptr\n"
          "    %cursor_count = llvm.mlir.constant(1 : i64) : i64\n"
          "    %cursor_address = llvm.alloca %cursor_count x i64 : (i64) -> !llvm.ptr\n"
          "    %cursor_zero = llvm.mlir.constant(0 : i64) : i64\n"
          "    llvm.store %cursor_zero, %cursor_address : i64, !llvm.ptr\n"
          "    llvm.call @w_fn_") ||
      !append_size(artifact, capacity, &offset,
                   selection->entry->target_function) ||
      !append_literal(
          artifact, capacity, &offset,
          "(%buffer, %cursor_address) : (!llvm.ptr, !llvm.ptr) -> ()\n"
          "    %length = llvm.load %cursor_address : !llvm.ptr -> i64\n"
          "    %fd = llvm.mlir.constant(1 : i32) : i32\n"
          "    %written = llvm.call @write(%fd, %buffer, %length) : (i32, !llvm.ptr, i64) -> i64\n"
          "    %equal = llvm.icmp \"eq\" %written, %length : i64\n"
          "    %success = llvm.mlir.constant(0 : i32) : i32\n"
          "    %failure = llvm.mlir.constant(1 : i32) : i32\n"
          "    %status = llvm.select %equal, %success, %failure : i1, i32\n"
          "    llvm.return %status : i32\n"
          "  }\n"
          "}\n"))
    return false;
  *written = offset;
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  w_seed_sha256_update(&state, artifact, offset);
  w_seed_sha256_final(&state, digest);
  return true;
}

typedef struct {
  const void *address;
  size_t count;
  size_t element_size;
} mlir0_range;

static bool range_add(mlir0_range *ranges, size_t capacity,
                      size_t *range_count, const void *address, size_t count,
                      size_t element_size) {
  if (ranges == NULL || range_count == NULL || *range_count >= capacity ||
      element_size == 0u || count > SIZE_MAX / element_size)
    return false;
  ranges[*range_count] = (mlir0_range){address, count, element_size};
  *range_count += 1u;
  return true;
}

static bool range_pair_overlaps(const mlir0_range *left,
                                const mlir0_range *right) {
  if (left == NULL || right == NULL || left->element_size == 0u ||
      right->element_size == 0u ||
      left->count > SIZE_MAX / left->element_size ||
      right->count > SIZE_MAX / right->element_size)
    return true;
  return ranges_overlap(left->address, left->count * left->element_size,
                        right->address, right->count * right->element_size);
}

static bool range_add_or_alias(mlir0_range *ranges, size_t capacity,
                               size_t *range_count, const void *address,
                               size_t count, size_t element_size) {
  return !range_add(ranges, capacity, range_count, address, count,
                    element_size);
}

static bool input_aliases_outputs(const w_seed_mlir0_input *input,
                                  const w_seed_mlir0_target *target,
                                  const w_seed_mlir0_output *output,
                                  const w_seed_mlir0_counts *counts,
                                  const w_seed_mlir0_result *result) {
  if (input == NULL || input->program == NULL || input->hir_result == NULL)
    return true;
  mlir0_range ranges[32];
  size_t range_count = 0u;
  const size_t range_capacity = sizeof(ranges) / sizeof(ranges[0]);
  if (range_add_or_alias(ranges, range_capacity, &range_count, input, 1u,
                         sizeof(*input)) ||
      range_add_or_alias(ranges, range_capacity, &range_count, target, 1u,
                         sizeof(*target)) ||
      range_add_or_alias(ranges, range_capacity, &range_count, counts, 1u,
                         sizeof(*counts)) ||
      range_add_or_alias(ranges, range_capacity, &range_count, result, 1u,
                         sizeof(*result)))
    return true;
  if (output != NULL &&
      (range_add_or_alias(ranges, range_capacity, &range_count, output, 1u,
                          sizeof(*output)) ||
       range_add_or_alias(ranges, range_capacity, &range_count, output->bytes,
                          output->capacity, sizeof(uint8_t))))
    return true;
  const w_seed_hir0_program *program = input->program;
  const mlir0_range input_ranges[] = {
      {program, 1u, sizeof(*program)},
      {input->hir_result, 1u, sizeof(*input->hir_result)},
      {program->modules, program->module_capacity,
       sizeof(*program->modules)},
      {program->identities, program->identity_capacity,
       sizeof(*program->identities)},
      {program->types, program->type_capacity, sizeof(*program->types)},
      {program->functions, program->function_capacity,
       sizeof(*program->functions)},
      {program->parameters, program->parameter_capacity,
       sizeof(*program->parameters)},
      {program->blocks, program->block_capacity, sizeof(*program->blocks)},
      {program->instructions, program->instruction_capacity,
       sizeof(*program->instructions)},
      {program->bindings, program->binding_capacity,
       sizeof(*program->bindings)},
      {program->calls, program->call_capacity, sizeof(*program->calls)},
      {program->host_parameters, program->host_parameter_capacity,
       sizeof(*program->host_parameters)},
      {program->arguments, program->argument_capacity,
       sizeof(*program->arguments)},
      {program->requirements, program->requirement_capacity,
       sizeof(*program->requirements)},
      {program->values, program->value_capacity, sizeof(*program->values)},
      {program->interpolation_segments,
       program->interpolation_segment_capacity,
       sizeof(*program->interpolation_segments)},
      {program->terminators, program->terminator_capacity,
       sizeof(*program->terminators)},
      {program->entries, program->entry_capacity, sizeof(*program->entries)},
      {program->text_bytes, program->text_byte_capacity, sizeof(uint8_t)},
      {program->value_bytes, program->value_byte_capacity, sizeof(uint8_t)},
      {program->receipt, program->receipt_capacity, sizeof(uint8_t)},
  };
  for (size_t index = 0u;
       index < sizeof(input_ranges) / sizeof(input_ranges[0]); index += 1u)
    if (!range_add(ranges, range_capacity, &range_count,
                   input_ranges[index].address, input_ranges[index].count,
                   input_ranges[index].element_size))
      return true;
  for (size_t first = 0u; first < range_count; first += 1u)
    for (size_t second = first + 1u; second < range_count; second += 1u)
      if (range_pair_overlaps(&ranges[first], &ranges[second])) return true;
  return false;
}

static bool output_buffer_aliases(const w_seed_mlir0_input *input,
                                  const w_seed_mlir0_target *target,
                                  const w_seed_mlir0_output *output,
                                  const w_seed_mlir0_result *result,
                                  size_t bytes) {
  if (output == NULL) return false;
  return ranges_overlap(input, sizeof(*input), output->bytes, bytes) ||
         ranges_overlap(target, sizeof(*target), output->bytes, bytes) ||
         ranges_overlap(result, sizeof(*result), output->bytes, bytes) ||
         ranges_overlap(output, sizeof(*output), output->bytes, bytes);
}

w_seed_mlir0_status w_seed_mlir0_measure(
    const w_seed_mlir0_input *input, const w_seed_mlir0_target *target,
    w_seed_mlir0_counts *counts, w_seed_mlir0_result *result) {
  if (input == NULL || input->program == NULL || input->hir_result == NULL ||
      counts == NULL || result == NULL)
    return W_SEED_MLIR0_INVALID_HIR;
  w_seed_native_subset0_program program_selection;
  const w_seed_native_subset0_status selected =
      w_seed_native_subset0_select_program(input->program, input->hir_result,
                                           &program_selection);
  if (selected == W_SEED_NATIVE_SUBSET0_INVALID)
    return W_SEED_MLIR0_INVALID_HIR;
  if (selected == W_SEED_NATIVE_SUBSET0_UNSUPPORTED)
    return W_SEED_MLIR0_UNSUPPORTED;
  if (input_aliases_outputs(input, target, NULL, counts, result))
    return W_SEED_MLIR0_ALIAS;
  if (!target_is_supported(target)) return W_SEED_MLIR0_UNSUPPORTED;
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  uint8_t digest[MLIR0_DIGEST_BYTES];
  size_t written = 0u;
  if (program_selection.has_local_calls) {
    if (!build_program_artifact(input->program, &program_selection, target,
                                artifact, sizeof(artifact), &written, digest))
      return W_SEED_MLIR0_INVALID_HIR;
  } else {
    w_seed_native_subset0_sequence sequence;
    const w_seed_native_subset0_status sequence_selected =
        w_seed_native_subset0_select_sequence(input->program,
                                              input->hir_result, &sequence);
    if (sequence_selected == W_SEED_NATIVE_SUBSET0_UNSUPPORTED)
      return W_SEED_MLIR0_UNSUPPORTED;
    if (sequence_selected != W_SEED_NATIVE_SUBSET0_OK ||
        !build_artifact(input->program, &sequence, target, artifact,
                        sizeof(artifact), &written, digest))
      return W_SEED_MLIR0_INVALID_HIR;
  }
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
    const w_seed_mlir0_input *input, const w_seed_mlir0_target *target,
    const w_seed_mlir0_output *output, w_seed_mlir0_result *result) {
  if (input == NULL || input->program == NULL || input->hir_result == NULL ||
      result == NULL)
    return W_SEED_MLIR0_INVALID_HIR;
  w_seed_native_subset0_program program_selection;
  const w_seed_native_subset0_status selected =
      w_seed_native_subset0_select_program(input->program, input->hir_result,
                                           &program_selection);
  if (selected == W_SEED_NATIVE_SUBSET0_INVALID)
    return W_SEED_MLIR0_INVALID_HIR;
  if (selected == W_SEED_NATIVE_SUBSET0_UNSUPPORTED)
    return W_SEED_MLIR0_UNSUPPORTED;
  if (input_aliases_outputs(input, target, output, NULL, result))
    return W_SEED_MLIR0_ALIAS;
  if (!target_is_supported(target)) return W_SEED_MLIR0_UNSUPPORTED;
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  uint8_t digest[MLIR0_DIGEST_BYTES];
  size_t written = 0u;
  if (program_selection.has_local_calls) {
    if (!build_program_artifact(input->program, &program_selection, target,
                                artifact, sizeof(artifact), &written, digest))
      return W_SEED_MLIR0_INVALID_HIR;
  } else {
    w_seed_native_subset0_sequence sequence;
    const w_seed_native_subset0_status sequence_selected =
        w_seed_native_subset0_select_sequence(input->program,
                                              input->hir_result, &sequence);
    if (sequence_selected == W_SEED_NATIVE_SUBSET0_UNSUPPORTED)
      return W_SEED_MLIR0_UNSUPPORTED;
    if (sequence_selected != W_SEED_NATIVE_SUBSET0_OK ||
        !build_artifact(input->program, &sequence, target, artifact,
                        sizeof(artifact), &written, digest))
      return W_SEED_MLIR0_INVALID_HIR;
  }
  if (output_buffer_aliases(input, target, output, result, written))
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
