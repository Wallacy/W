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
  MLIR0_FORMAT_BYTES = (2 * W_SEED_NATIVE_SUBSET0_MAX_STDOUT_BYTES) + 1,
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
  ((size_t)MLIR0_MAX_STDOUT_BYTES * MLIR0_ESCAPE_BYTES_PER_INPUT)
#define MLIR0_DECIMAL_BYTES                                                   \
  ((size_t)MLIR0_DECIMAL_FIELDS * MLIR0_DECIMAL_MAX_BYTES)
#define MLIR0_REQUIRED_MAX_BYTES                                              \
  (MLIR0_FIXED_LITERAL_BYTES + MLIR0_VARIABLE_ESCAPED_BYTES +                \
   MLIR0_DECIMAL_BYTES)

_Static_assert(CHAR_BIT == 8, "w-seed MLIR0 requires 8-bit bytes");
_Static_assert(MLIR0_MAX_STDOUT_BYTES <= 9999u,
               "w-seed MLIR0 decimal fields must cover the bounded stdout size");
_Static_assert(MLIR0_FIXED_LITERAL_BYTES == 886u,
               "w-seed MLIR0 fixed artifact literals changed");
_Static_assert(MLIR0_REQUIRED_MAX_BYTES <= W_SEED_MLIR0_MAX_BYTES,
               "w-seed MLIR0 must retain the static artifact bound");

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
  MLIR0_FORMAT_I64 = 0,
} mlir0_format_argument_kind;

typedef struct {
  mlir0_format_argument_kind kind;
  uint32_t value_index;
} mlir0_format_argument;

static bool append_format_bytes(uint8_t *format, size_t capacity,
                                size_t *offset, const uint8_t *bytes,
                                size_t length) {
  if (format == NULL || offset == NULL || (length != 0u && bytes == NULL))
    return false;
  for (size_t index = 0u; index < length; index += 1u) {
    if (bytes[index] == 0u) return false;
    if (bytes[index] == (uint8_t)'%' &&
        !append_bytes(format, capacity, offset, "%", 1u))
      return false;
    if (!append_bytes(format, capacity, offset, &bytes[index], 1u))
      return false;
  }
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
    *length = binding->byte_count;
    *bytes = *length == 0u ? NULL
                          : program->value_bytes + binding->byte_offset;
    return true;
  }
  return false;
}

static bool build_dynamic_format(
    const w_seed_hir0_program *program,
    const w_seed_native_subset0_sequence *sequence, uint8_t *format,
    size_t format_capacity, size_t *format_bytes,
    mlir0_format_argument *arguments, size_t argument_capacity,
    size_t *argument_count) {
  if (program == NULL || sequence == NULL || format == NULL ||
      format_bytes == NULL || arguments == NULL || argument_count == NULL ||
      !sequence->has_interpolation)
    return false;
  size_t offset = 0u;
  size_t count = 0u;
  for (size_t call_index = 0u; call_index < sequence->call_count;
       call_index += 1u) {
    const w_seed_hir0_value *root = sequence->calls[call_index].value;
    if (root == NULL) return false;
    if (root->kind != W_SEED_HIR0_VALUE_INTERPOLATED_STRING) {
      const uint8_t *bytes = NULL;
      size_t length = 0u;
      if (!value_string_bytes(program, root, &bytes, &length) ||
          !append_format_bytes(format, format_capacity, &offset, bytes,
                               length))
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
          if (!append_format_bytes(format, format_capacity, &offset, bytes,
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
        const w_seed_hir0_type_kind type =
            program->types[embedded->type_index].kind;
        if (count >= argument_capacity) return false;
        if (type == W_SEED_HIR0_TYPE_I64) {
          if (!append_bytes(format, format_capacity, &offset, "%ld", 3u))
            return false;
          arguments[count] =
              (mlir0_format_argument){MLIR0_FORMAT_I64, segment->value_index};
        } else {
          return false;
        }
        count += 1u;
      }
    }
    const uint8_t newline = 0x0au;
    if (!append_bytes(format, format_capacity, &offset, &newline, 1u))
      return false;
  }
  const uint8_t nul = 0u;
  if (!append_bytes(format, format_capacity, &offset, &nul, 1u)) return false;
  *format_bytes = offset;
  *argument_count = count;
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

static bool append_snprintf_call(const mlir0_format_argument *arguments,
                                 size_t argument_count, uint8_t *artifact,
                                 size_t capacity, size_t *offset) {
  if (arguments == NULL || artifact == NULL || offset == NULL) return false;
  if (!append_literal(artifact, capacity, offset,
                      "    %length32 = llvm.call @snprintf(%buffer, %capacity, %format"))
    return false;
  for (size_t index = 0u; index < argument_count; index += 1u) {
    if (!append_literal(artifact, capacity, offset, ", %v") ||
        !append_size(artifact, capacity, offset,
                     arguments[index].value_index))
      return false;
  }
  if (!append_literal(
          artifact, capacity, offset,
          ") vararg(!llvm.func<i32 (ptr, i64, ptr, ...)>) : (!llvm.ptr, i64, !llvm.ptr"))
    return false;
  for (size_t index = 0u; index < argument_count; index += 1u)
    if (!append_literal(artifact, capacity, offset, ", i64"))
      return false;
  return append_literal(artifact, capacity, offset, ") -> i32\n");
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
  uint8_t format[MLIR0_FORMAT_BYTES];
  mlir0_format_argument arguments[
      W_SEED_NATIVE_SUBSET0_MAX_INTERPOLATION_SEGMENTS];
  size_t format_bytes = 0u;
  size_t argument_count = 0u;
  if (!build_dynamic_format(
          program, sequence, format, sizeof(format), &format_bytes, arguments,
          sizeof(arguments) / sizeof(arguments[0]), &argument_count))
    return false;

  size_t offset = 0u;
  if (!append_literal(artifact, capacity, &offset, MLIR0_SCHEMA_COMMENT) ||
      !append_literal(artifact, capacity, &offset,
                      "module attributes {llvm.target_triple = \"" W_SEED_MLIR0_TARGET_TRIPLE
                      "\"} {\n"
                      "  llvm.mlir.global private constant @w_seed_mlir0_format(\"") ||
      !append_escaped_bytes(artifact, capacity, &offset, format,
                            format_bytes) ||
      !append_literal(artifact, capacity, &offset, "\") : !llvm.array<") ||
      !append_size(artifact, capacity, &offset, format_bytes) ||
      !append_literal(
          artifact, capacity, &offset,
          " x i8>\n"
          "  llvm.func @snprintf(!llvm.ptr, i64, !llvm.ptr, ...) -> i32\n"
          "  llvm.func @write(%fd: i32, %buffer: !llvm.ptr, %count: i64) -> i64\n"
          "  llvm.func @main() -> i32 {\n"
          "    %capacity = llvm.mlir.constant(4097 : i64) : i64\n"
          "    %buffer = llvm.alloca %capacity x i8 : (i64) -> !llvm.ptr\n"
          "    %format_base = llvm.mlir.addressof @w_seed_mlir0_format : !llvm.ptr\n"
          "    %format = llvm.getelementptr %format_base[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<") ||
      !append_size(artifact, capacity, &offset, format_bytes) ||
      !append_literal(artifact, capacity, &offset, " x i8>\n") ||
      !append_value_operations(program, artifact, capacity, &offset) ||
      !append_snprintf_call(arguments, argument_count, artifact, capacity,
                            &offset) ||
      !append_literal(
          artifact, capacity, &offset,
          "    %zero32 = llvm.mlir.constant(0 : i32) : i32\n"
          "    %limit32 = llvm.mlir.constant(4096 : i32) : i32\n"
          "    %nonnegative = llvm.icmp \"sge\" %length32, %zero32 : i32\n"
          "    %within = llvm.icmp \"sle\" %length32, %limit32 : i32\n"
          "    %valid = llvm.and %nonnegative, %within : i1\n"
          "    llvm.cond_br %valid, ^write, ^failed\n"
          "  ^write:\n"
          "    %length = llvm.sext %length32 : i32 to i64\n"
          "    %fd = llvm.mlir.constant(1 : i32) : i32\n"
          "    %written = llvm.call @write(%fd, %buffer, %length) : (i32, !llvm.ptr, i64) -> i64\n"
          "    %equal = llvm.icmp \"eq\" %written, %length : i64\n"
          "    %success = llvm.mlir.constant(0 : i32) : i32\n"
          "    %failure = llvm.mlir.constant(1 : i32) : i32\n"
          "    %status = llvm.select %equal, %success, %failure : i1, i32\n"
          "    llvm.return %status : i32\n"
          "  ^failed:\n"
          "    %failed_status = llvm.mlir.constant(1 : i32) : i32\n"
          "    llvm.return %failed_status : i32\n"
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
  w_seed_native_subset0_sequence sequence;
  const w_seed_native_subset0_status selected =
      w_seed_native_subset0_select_sequence(input->program, input->hir_result,
                                            &sequence);
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
  if (!build_artifact(input->program, &sequence, target, artifact,
                      sizeof(artifact), &written, digest))
    return W_SEED_MLIR0_INVALID_HIR;
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
  w_seed_native_subset0_sequence sequence;
  const w_seed_native_subset0_status selected =
      w_seed_native_subset0_select_sequence(input->program, input->hir_result,
                                            &sequence);
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
  if (!build_artifact(input->program, &sequence, target, artifact,
                      sizeof(artifact), &written, digest))
    return W_SEED_MLIR0_INVALID_HIR;
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
