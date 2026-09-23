#include "w_seed_native0.h"
#include "w_seed_parallel_selection0.h"
#include "w_seed_scalar_evaluator0.h"
#include "../src/w_seed_native_subset0.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition)) {                                                        \
      (void)fprintf(stderr, "native0 check failed: %s (%s:%d)\n", #condition, \
                    __FILE__, __LINE__);                                      \
      (void)remove(TEST_PATH);                                                 \
      return false;                                                            \
    }                                                                          \
  } while (0)

static const char TEST_PATH[] = "w_seed_native0_test.w";
static w_seed_native0_storage storage;

static uint32_t edge_value_at(const w_seed_hir0_program *program,
                              size_t terminator_index) {
  const w_seed_hir0_terminator *terminator =
      &program->terminators[terminator_index];
  if (terminator->edge_argument_count == 0u ||
      terminator->first_edge_argument == W_SEED_HIR0_NONE)
    return W_SEED_HIR0_NONE;
  return program->edge_arguments[terminator->first_edge_argument].value_index;
}
static const w_seed_mlir0_target TARGET = {
    W_SEED_MLIR0_TARGET_X86_64_UNKNOWN_LINUX_GNU};
static const w_seed_mlir0_target WINDOWS_TARGET = {
    W_SEED_MLIR0_TARGET_X86_64_PC_WINDOWS_MSVC};

static bool write_source(const uint8_t *bytes, size_t length) {
  FILE *file = fopen(TEST_PATH, "wb");
  if (file == NULL) return false;
  const size_t written =
      length == 0u ? 0u : fwrite(bytes, sizeof(uint8_t), length, file);
  const int close_result = fclose(file);
  return written == length && close_result == 0;
}

static bool append_test_source(char *source, size_t capacity, size_t *length,
                               const char *format, ...) {
  if (source == NULL || length == NULL || format == NULL || *length > capacity)
    return false;
  va_list arguments;
  va_start(arguments, format);
  const int written = vsnprintf(source + *length, capacity - *length, format,
                                arguments);
  va_end(arguments);
  if (written < 0 || (size_t)written >= capacity - *length) return false;
  *length += (size_t)written;
  return true;
}

static w_seed_native0_status run_source(
    const uint8_t *bytes, size_t length, const char *identity,
    size_t identity_length, uint8_t *output_bytes, size_t output_capacity,
    w_seed_native0_result *result) {
  const w_seed_native0_input input = {
      .path = TEST_PATH,
      .path_length = sizeof(TEST_PATH) - 1u,
      .logical_source_id = {identity, identity_length},
      .target = TARGET,
      .artifact_kind = W_SEED_MLIR0_ARTIFACT_EXECUTABLE};
  if (!write_source(bytes, length)) return W_SEED_NATIVE0_SOURCE;
  const w_seed_native0_output output = {output_bytes, output_capacity};
  return w_seed_native0_run(&input, &storage, &output, result);
}

static w_seed_native0_status run_source_mode(
    const uint8_t *bytes, size_t length, const char *identity,
    size_t identity_length, const w_seed_mlir0_target *target,
    w_seed_mlir0_artifact_kind artifact_kind, uint8_t *output_bytes,
    size_t output_capacity, w_seed_native0_result *result) {
  if (target == NULL) return W_SEED_NATIVE0_INVALID;
  const w_seed_native0_input input = {
      .path = TEST_PATH,
      .path_length = sizeof(TEST_PATH) - 1u,
      .logical_source_id = {identity, identity_length},
      .target = *target,
      .artifact_kind = artifact_kind};
  if (!write_source(bytes, length)) return W_SEED_NATIVE0_SOURCE;
  const w_seed_native0_output output = {output_bytes, output_capacity};
  return w_seed_native0_run(&input, &storage, &output, result);
}

static bool contains_bytes(const uint8_t *bytes, size_t length,
                           const char *needle) {
  if (bytes == NULL || needle == NULL) return false;
  const size_t needle_length = strlen(needle);
  if (needle_length == 0u || needle_length > length) return false;
  for (size_t offset = 0u; offset + needle_length <= length; offset += 1u)
    if (memcmp(bytes + offset, needle, needle_length) == 0) return true;
  return false;
}

static bool hir_text_equals(const w_seed_hir0_program *program,
                            w_seed_hir0_text text, const char *literal) {
  if (program == NULL || literal == NULL || program->text_bytes == NULL)
    return false;
  const size_t length = strlen(literal);
  return text.count == length && text.offset <= program->text_byte_count &&
         length <= program->text_byte_count - text.offset &&
         memcmp(program->text_bytes + text.offset, literal, length) == 0;
}

static size_t count_bytes(const uint8_t *bytes, size_t length,
                          const char *needle) {
  if (bytes == NULL || needle == NULL) return 0u;
  const size_t needle_length = strlen(needle);
  if (needle_length == 0u || needle_length > length) return 0u;
  size_t count = 0u;
  for (size_t offset = 0u; offset + needle_length <= length; offset += 1u)
    if (memcmp(bytes + offset, needle, needle_length) == 0) count += 1u;
  return count;
}

static size_t find_bytes(const uint8_t *bytes, size_t length,
                         const char *needle, size_t start) {
  if (bytes == NULL || needle == NULL || start > length) return SIZE_MAX;
  const size_t needle_length = strlen(needle);
  if (needle_length == 0u || needle_length > length - start) return SIZE_MAX;
  for (size_t offset = start; offset + needle_length <= length; offset += 1u)
    if (memcmp(bytes + offset, needle, needle_length) == 0) return offset;
  return SIZE_MAX;
}

static size_t count_mlir_lines_with_fragment_and_type(
    const uint8_t *bytes, size_t length, const char *fragment,
    const char *type_suffix) {
  if (bytes == NULL || fragment == NULL || type_suffix == NULL) return 0u;
  size_t count = 0u;
  size_t cursor = 0u;
  while (cursor < length) {
    size_t line_end = find_bytes(bytes, length, "\n", cursor);
    if (line_end == SIZE_MAX) line_end = length;
    const size_t line_length = line_end - cursor;
    if (contains_bytes(bytes + cursor, line_length, fragment) &&
        contains_bytes(bytes + cursor, line_length, type_suffix))
      count += 1u;
    cursor = line_end == length ? length : line_end + 1u;
  }
  return count;
}

static bool append_source_text(char *buffer, size_t capacity, size_t *offset,
                               const char *text) {
  if (buffer == NULL || offset == NULL || text == NULL || *offset > capacity)
    return false;
  const size_t count = strlen(text);
  if (count > capacity - *offset) return false;
  (void)memcpy(buffer + *offset, text, count);
  *offset += count;
  buffer[*offset] = '\0';
  return true;
}

static bool make_process_stdout_bound_source(char *buffer, size_t capacity,
                                             size_t helper_calls,
                                             bool extra_empty_print) {
  if (buffer == NULL || helper_calls == 0u || helper_calls > 2u) return false;
  /* Reuse one 255-byte literal so this source stays below Native0's 4096-byte
   * source bound; each of the sixteen print calls still emits 256 bytes. */
  char line[256];
  for (size_t index = 0u; index < 255u; index += 1u) line[index] = 'x';
  line[255] = '\0';
  size_t offset = 0u;
  if (!append_source_text(
          buffer, capacity, &offset,
          "import { Arguments as InputArgs, Context as InputContext, "
          "ExitCode as InputExit } from std.process\n"
          "fn emitOutput() { let line = \""))
    return false;
  if (!append_source_text(buffer, capacity, &offset, line) ||
      !append_source_text(buffer, capacity, &offset, "\" "))
    return false;
  for (size_t index = 0u; index < 16u; index += 1u)
    if (!append_source_text(buffer, capacity, &offset, "print(line) "))
      return false;
  if (!append_source_text(
          buffer, capacity, &offset,
          "}\nasync fn dispatch(input: InputArgs, context: InputContext): "
          "InputExit { "))
    return false;
  for (size_t index = 0u; index < helper_calls; index += 1u)
    if (!append_source_text(buffer, capacity, &offset, "emitOutput() "))
      return false;
  if (extra_empty_print &&
      !append_source_text(buffer, capacity, &offset, "print(\"\") "))
    return false;
  return append_source_text(buffer, capacity, &offset,
                            "return .success }\nentry(dispatch)\n");
}

static bool append_nested_chain(char *buffer, size_t capacity, size_t *offset,
                                size_t depth) {
  if (depth == 0u)
    return append_source_text(buffer, capacity, offset,
                              "print(\"x\") ");
  return append_source_text(buffer, capacity, offset, "if true { ") &&
         append_nested_chain(buffer, capacity, offset, depth - 1u) &&
         append_source_text(buffer, capacity, offset, "} ");
}

static bool make_nested_chain_source(char *buffer, size_t capacity,
                                     size_t depth) {
  size_t offset = 0u;
  if (!append_source_text(buffer, capacity, &offset, "fn main() { ") ||
      !append_nested_chain(buffer, capacity, &offset, depth))
    return false;
  return append_source_text(buffer, capacity, &offset, "}\nentry(main)\n");
}

static bool append_nested_tree(char *buffer, size_t capacity, size_t *offset,
                               size_t depth) {
  if (depth == 0u) return true;
  return append_source_text(buffer, capacity, offset, "if true { ") &&
         append_nested_tree(buffer, capacity, offset, depth - 1u) &&
         append_source_text(buffer, capacity, offset, "} else { ") &&
         append_nested_tree(buffer, capacity, offset, depth - 1u) &&
         append_source_text(buffer, capacity, offset, "} ");
}

static bool make_nested_tree_source(char *buffer, size_t capacity,
                                    size_t depth) {
  size_t offset = 0u;
  if (!append_source_text(buffer, capacity, &offset, "fn main() { ") ||
      !append_nested_tree(buffer, capacity, &offset, depth) ||
      !append_source_text(buffer, capacity, &offset, "print(\"x\") }\n"))
    return false;
  return append_source_text(buffer, capacity, &offset, "entry(main)\n");
}

static bool test_products(void) {
  CHECK(strcmp(W_SEED_NATIVE0_SCHEMA_VERSION, "w-seed-native0-10") == 0);
  CHECK(strcmp(W_SEED_MLIR0_SCHEMA_VERSION, "w-seed-mlir0-61") == 0);
  CHECK(strcmp(W_SEED_MLIR0_WINDOWS_SCHEMA_VERSION,
               "w-seed-mlir0-windows-47") == 0);
  static const uint8_t literal[] =
      "fn serve() { print(\"Table 42 remains open\") }\n"
      "entry(serve)\n";
  static const uint8_t binding[] =
      "fn serve() { let message = \"Table 42 remains open\" "
      "print(message) }\nentry(serve)\n";
  static const uint8_t trivia[] =
      "// leading trivia\nfn serve() { print(\"Table 42 remains open\") } "
      "// trailing trivia\nentry(serve)\n";
  static uint8_t literal_bytes[W_SEED_MLIR0_MAX_BYTES];
  static uint8_t binding_bytes[W_SEED_MLIR0_MAX_BYTES];
  static uint8_t trivia_bytes[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result literal_result;
  w_seed_native0_result binding_result;
  w_seed_native0_result trivia_result;
  CHECK(run_source(literal, sizeof(literal) - 1u, "literal-id", 10u,
                   literal_bytes, sizeof(literal_bytes), &literal_result) ==
        W_SEED_NATIVE0_OK);
  CHECK(storage.runtime_requirements == W_SEED_RUNTIME_REQUIREMENTS_NONE);
  CHECK(storage.document.logical_source_id.length == 10u &&
        memcmp(storage.document.logical_source_id.data, "literal-id", 10u) ==
            0);
  CHECK(storage.hir_program.module_count != 0u &&
        storage.hir_modules[0].source_id.count == 10u &&
        storage.hir_modules[0].source_id.offset <=
            storage.hir_program.text_byte_count &&
        storage.hir_modules[0].source_id.count <=
            storage.hir_program.text_byte_count -
                storage.hir_modules[0].source_id.offset &&
        memcmp(storage.hir_text + storage.hir_modules[0].source_id.offset,
               "literal-id", 10u) == 0);
  CHECK(run_source(binding, sizeof(binding) - 1u, "binding-id", 10u,
                   binding_bytes, sizeof(binding_bytes), &binding_result) ==
        W_SEED_NATIVE0_OK);
  CHECK(run_source(trivia, sizeof(trivia) - 1u, "trivia-id", 9u, trivia_bytes,
                   sizeof(trivia_bytes), &trivia_result) == W_SEED_NATIVE0_OK);
  CHECK(literal_result.mlir.written.mlir_bytes ==
        binding_result.mlir.written.mlir_bytes);
  CHECK(memcmp(literal_bytes, binding_bytes,
               literal_result.mlir.written.mlir_bytes) == 0);
  CHECK(literal_result.mlir.written.mlir_bytes ==
        trivia_result.mlir.written.mlir_bytes);
  CHECK(memcmp(literal_bytes, trivia_bytes,
               literal_result.mlir.written.mlir_bytes) == 0);
  CHECK(memcmp(literal_result.mlir.mlir_sha256, binding_result.mlir.mlir_sha256,
               sizeof(literal_result.mlir.mlir_sha256)) == 0);
  CHECK(memcmp(literal_result.mlir.mlir_sha256, trivia_result.mlir.mlir_sha256,
               sizeof(literal_result.mlir.mlir_sha256)) == 0);

  static const uint8_t linear[] =
      "fn serve() {\n"
      "  let message = \"Table 42 remains open\"\n"
      "  print(message)\n"
      "  print(\"Kitchen is ready\")\n"
      "}\nentry(serve)\n";
  static uint8_t linear_bytes[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result linear_result;
  CHECK(run_source(linear, sizeof(linear) - 1u, "linear-id", 9u,
                   linear_bytes, sizeof(linear_bytes), &linear_result) ==
        W_SEED_NATIVE0_OK);
  CHECK(storage.hir_program.instruction_count == 3u &&
        storage.hir_program.binding_count == 1u &&
        storage.hir_program.call_count == 2u &&
        linear_result.mlir.written.mlir_bytes ==
            linear_result.mlir.required.mlir_bytes);
  CHECK(contains_bytes(
      linear_bytes, linear_result.mlir.written.mlir_bytes,
      "\\54\\61\\62\\6c\\65\\20\\34\\32\\20\\72\\65\\6d\\61\\69\\6e\\73\\20\\6f\\70\\65\\6e\\0a"
      "\\4b\\69\\74\\63\\68\\65\\6e\\20\\69\\73\\20\\72\\65\\61\\64\\79\\0a"));
  CHECK(contains_bytes(linear_bytes, linear_result.mlir.written.mlir_bytes,
                       "!llvm.array<39 x i8>"));

  static const uint8_t cfg[] =
      "fn serve(isOpen: Bool) {\n"
      "  if isOpen { print(\"Kitchen open\") } else { "
      "print(\"Kitchen closed\") }\n"
      "  print(\"After service\")\n"
      "}\n"
      "fn main() { serve(isOpen: true) serve(isOpen: false) }\n"
      "entry(main)\n";
  static uint8_t cfg_bytes[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result cfg_result;
  CHECK(run_source(cfg, sizeof(cfg) - 1u, "cfg-id", 6u, cfg_bytes,
                   sizeof(cfg_bytes), &cfg_result) == W_SEED_NATIVE0_OK);
  CHECK(storage.hir_program.functions[0].block_count == 4u &&
        storage.hir_program.functions[1].block_count == 1u &&
        storage.hir_program.terminators[0].kind ==
            W_SEED_HIR0_TERMINATOR_BRANCH);
  CHECK(contains_bytes(cfg_bytes, cfg_result.mlir.written.mlir_bytes,
                       "llvm.cond_br %p0, ^w_fn_0_b_1, ^w_fn_0_b_2") &&
        count_bytes(cfg_bytes, cfg_result.mlir.written.mlir_bytes,
                    "llvm.br ^w_fn_0_b_3\n") == 2u &&
        contains_bytes(cfg_bytes, cfg_result.mlir.written.mlir_bytes,
                       "\\4b\\69\\74\\63\\68\\65\\6e\\20\\6f\\70\\65\\6e\\0a") &&
        contains_bytes(cfg_bytes, cfg_result.mlir.written.mlir_bytes,
                       "\\4b\\69\\74\\63\\68\\65\\6e\\20\\63\\6c\\6f\\73\\65\\64\\0a") &&
        count_bytes(cfg_bytes, cfg_result.mlir.written.mlir_bytes,
                    "\\41\\66\\74\\65\\72\\20\\73\\65\\72\\76\\69\\63\\65\\0a") == 1u);
  return true;
}

static bool test_strict_float_native_admission(void) {
  static const uint8_t SOURCE[] =
      "fn f32Value(): f32 { return -1.5_f32 }\n"
      "fn f64Value(): f64 { return -1.5_f64 }\n"
      "fn main() { if (-1.5_f32) < 0.0_f32 && (-1.5_f64) < 0.0_f64 && "
      "(1.5_f32 + 2.25_f32) > 0.0_f32 && "
      "(1.5_f64 + 2.25_f64) > 0.0_f64 { print(\"float\") } "
      "else { print(\"float\") } }\n"
      "entry(main)\n";
  static uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  CHECK(run_source(SOURCE, sizeof(SOURCE) - 1u, "strict-float-native",
                   sizeof("strict-float-native") - 1u, artifact,
                   sizeof(artifact), &result) == W_SEED_NATIVE0_OK);
  CHECK(w_seed_hir0_verify(&storage.hir_program, &storage.hir_result));
  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(
            &storage.hir_program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  CHECK(contains_bytes(artifact, result.mlir.written.mlir_bytes,
                       "llvm.mlir.constant(0x3fc00000 : f32) : f32"));
  CHECK(contains_bytes(artifact, result.mlir.written.mlir_bytes,
                       "llvm.mlir.constant(0x3ff8000000000000 : f64) : f64"));
  CHECK(contains_bytes(artifact, result.mlir.written.mlir_bytes,
                       "llvm.fadd "));
  CHECK(contains_bytes(artifact, result.mlir.written.mlir_bytes,
                       "llvm.fneg "));
  CHECK(!contains_bytes(artifact, result.mlir.written.mlir_bytes, "fastmath"));
  return true;
}

static bool test_float_bits_native_admission(void) {
  static const uint8_t SOURCE[] =
      "fn main() { let f32Value: f32 = f32.fromBits(0x80000000_u32) "
      "let f64Value: f64 = f64.fromBits(0x7ff8123456789abc_u64) "
      "let f32Bits: u32 = f32Value.toBits() "
      "let f64Bits: u64 = f64Value.toBits() print(\"bit bridge\") }\n"
      "entry(main)\n";
  static uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  const w_seed_native0_status status =
      run_source(SOURCE, sizeof(SOURCE) - 1u, "float-bits-native",
                 sizeof("float-bits-native") - 1u, artifact, sizeof(artifact),
                 &result);
  CHECK(status == W_SEED_NATIVE0_OK);
  CHECK(w_seed_hir0_verify(&storage.hir_program, &storage.hir_result));

  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(
            &storage.hir_program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  CHECK(contains_bytes(artifact, result.mlir.written.mlir_bytes,
                       "llvm.trunc %v") &&
        count_bytes(artifact, result.mlir.written.mlir_bytes,
                    "llvm.bitcast ") == 4u &&
        contains_bytes(artifact, result.mlir.written.mlir_bytes,
                       "llvm.zext ") &&
        !contains_bytes(artifact, result.mlir.written.mlir_bytes,
                        "uitofp") &&
        !contains_bytes(artifact, result.mlir.written.mlir_bytes,
                        "sitofp") &&
        !contains_bytes(artifact, result.mlir.written.mlir_bytes,
                        "fastmath"));

  uint32_t u32_type = W_SEED_HIR0_NONE;
  uint32_t f64_type = W_SEED_HIR0_NONE;
  uint32_t from32 = W_SEED_HIR0_NONE;
  uint32_t to32 = W_SEED_HIR0_NONE;
  uint32_t from64 = W_SEED_HIR0_NONE;
  uint32_t to64 = W_SEED_HIR0_NONE;
  for (size_t index = 0u; index < storage.hir_program.type_count; index += 1u) {
    const w_seed_hir0_type *type = &storage.hir_types[index];
    if (type->kind == W_SEED_HIR0_TYPE_INTEGER &&
        !type->integer_is_signed && type->integer_bit_width == 32u)
      u32_type = (uint32_t)index;
    if (type->kind == W_SEED_HIR0_TYPE_F64) f64_type = (uint32_t)index;
  }
  size_t from_count = 0u;
  size_t to_count = 0u;
  for (size_t index = 0u; index < storage.hir_program.value_count; index += 1u) {
    const w_seed_hir0_value *value = &storage.hir_values[index];
    if (value->kind == W_SEED_HIR0_VALUE_FLOAT_FROM_BITS) {
      CHECK(value->left_value < storage.hir_program.value_count &&
            storage.hir_values[value->left_value].owner_kind ==
                W_SEED_HIR0_VALUE_OWNER_FLOAT_BITS_CONVERSION &&
            storage.hir_values[value->left_value].owner_index == index &&
            storage.hir_values[value->left_value].owner_ordinal == 0u);
      from_count += 1u;
      if (value->source_type == u32_type) from32 = (uint32_t)index;
      else from64 = (uint32_t)index;
    } else if (value->kind == W_SEED_HIR0_VALUE_FLOAT_TO_BITS) {
      CHECK(value->left_value < storage.hir_program.value_count &&
            storage.hir_values[value->left_value].owner_kind ==
                W_SEED_HIR0_VALUE_OWNER_FLOAT_BITS_CONVERSION &&
            storage.hir_values[value->left_value].owner_index == index &&
            storage.hir_values[value->left_value].owner_ordinal == 0u);
      to_count += 1u;
      if (value->type_index == u32_type) to32 = (uint32_t)index;
      else to64 = (uint32_t)index;
    }
  }
  CHECK(u32_type != W_SEED_HIR0_NONE && f64_type != W_SEED_HIR0_NONE &&
        from_count == 2u && to_count == 2u && from32 != W_SEED_HIR0_NONE &&
        to32 != W_SEED_HIR0_NONE && from64 != W_SEED_HIR0_NONE &&
        to64 != W_SEED_HIR0_NONE);

  const w_seed_hir0_value saved_from32 = storage.hir_values[from32];
  const w_seed_hir0_value saved_from32_child =
      storage.hir_values[saved_from32.left_value];
  const w_seed_hir0_type saved_u32 = storage.hir_types[u32_type];
  storage.hir_values[from32].source_type = f64_type;
  CHECK(w_seed_native_subset0_select_program(
            &storage.hir_program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_INVALID);
  storage.hir_values[from32] = saved_from32;
  CHECK(w_seed_hir0_verify(&storage.hir_program, &storage.hir_result));

  storage.hir_types[u32_type].integer_bit_width = 64u;
  CHECK(w_seed_native_subset0_select_program(
            &storage.hir_program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_INVALID);
  storage.hir_types[u32_type] = saved_u32;
  CHECK(w_seed_hir0_verify(&storage.hir_program, &storage.hir_result));

  storage.hir_values[saved_from32.left_value].owner_ordinal = 1u;
  CHECK(w_seed_native_subset0_select_program(
            &storage.hir_program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_INVALID);
  storage.hir_values[saved_from32.left_value] = saved_from32_child;
  CHECK(w_seed_hir0_verify(&storage.hir_program, &storage.hir_result));
  return true;
}

static bool test_enum_frontend_storage(void) {
  static const uint8_t source[] =
      "enum Stage { accepted reserving preparing serving }\n"
      "enum Marker { flagged(code: i64) }\n"
      "alias WorkStage = Stage<[.preparing, .serving]>\n"
      "fn label(stage: Stage): String { return switch stage { "
      "case .accepted: \"A\" case .reserving: \"R\" "
      "case .preparing: \"P\" case .serving: \"S\" } }\n"
      "fn isWork(stage: Stage): Bool { return stage in "
      "(.preparing, .serving) }\n"
      "entry { print(\"enum storage\") }\n";
  static uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  (void)memset(output, 0xa5u, sizeof(output));
  (void)memset(&result, 0x5au, sizeof(result));
  const w_seed_native0_result result_snapshot = result;

  const w_seed_native0_status status =
      run_source(source, sizeof(source) - 1u, "enum-storage", 12u, output,
                 sizeof(output), &result);
  CHECK(status == W_SEED_NATIVE0_UNSUPPORTED);
  /* Frontend normalization succeeds; HIR0 keeps membership outside this
   * backend slice's closed family. */
  CHECK(storage.frontend_result.status == W_SEED_FRONTEND_OK &&
        storage.frontend_result.written.enums == 2u &&
        storage.frontend_result.written.enum_cases == 5u &&
        storage.frontend_result.written.enum_case_parameters == 1u &&
        storage.frontend_result.written.switch_arms == 4u &&
        storage.frontend_result.written.enum_subset_members == 2u &&
        storage.frontend_result.written.enum_membership_cases == 2u &&
        storage.output.enums == storage.enums &&
        storage.output.enum_capacity == W_SEED_NATIVE0_ENUMS &&
        storage.output.enum_cases == storage.enum_cases &&
        storage.output.enum_case_capacity == W_SEED_NATIVE0_ENUM_CASES &&
        storage.output.enum_case_parameters == storage.enum_case_parameters &&
        storage.output.enum_case_parameter_capacity ==
            W_SEED_NATIVE0_ENUM_CASE_PARAMETERS &&
        storage.output.switch_arms == storage.switch_arms &&
        storage.output.switch_arm_capacity == W_SEED_NATIVE0_SWITCH_ARMS &&
        storage.output.enum_subset_members == storage.enum_subset_members &&
        storage.output.enum_subset_member_capacity ==
            W_SEED_NATIVE0_ENUM_SUBSET_MEMBERS &&
        storage.output.enum_membership_cases == storage.enum_membership_cases &&
        storage.output.enum_membership_case_capacity ==
            W_SEED_NATIVE0_ENUM_MEMBERSHIP_CASES);
  for (size_t index = 0u; index < sizeof(output); index += 1u)
    CHECK(output[index] == 0xa5u);
  CHECK(memcmp(&result, &result_snapshot, sizeof(result)) == 0);

  static const uint8_t hir_source[] =
      "enum Stage { ready queued }\n"
      "fn current(): Stage { return .ready }\n"
      "entry { print(\"enum hir\") }\n";
  (void)memset(output, 0xa6u, sizeof(output));
  (void)memset(&result, 0x6au, sizeof(result));
  const w_seed_native0_status hir_status =
      run_source(hir_source, sizeof(hir_source) - 1u, "enum-hir", 8u,
                 output, sizeof(output), &result);
  bool has_enum_value = false;
  for (size_t value = 0u; value < storage.hir_program.value_count; value += 1u)
    if (storage.hir_program.values[value].kind == W_SEED_HIR0_VALUE_ENUM_CASE)
      has_enum_value = true;
  CHECK(hir_status == W_SEED_NATIVE0_OK);
  CHECK(storage.hir_result.status == W_SEED_HIR0_OK &&
        storage.hir_program.enum_count == 1u &&
        storage.hir_program.enum_case_count == 2u &&
        storage.hir_program.types[4].kind == W_SEED_HIR0_TYPE_ENUM &&
        has_enum_value &&
        w_seed_hir0_verify(&storage.hir_program, &storage.hir_result));
  CHECK(contains_bytes(output, result.mlir.written.mlir_bytes, "llvm.func"));

  static const uint8_t payload_source[] =
      "enum Marker { clear flagged(code: i64) }\n"
      "fn mark(code: i64): Marker { return .flagged(code: code) }\n"
      "entry { }\n";
  (void)memset(output, 0xa7u, sizeof(output));
  (void)memset(&result, 0x7au, sizeof(result));
  const w_seed_native0_result payload_result_snapshot = result;
  const w_seed_native0_status payload_status = run_source(
      payload_source, sizeof(payload_source) - 1u, "enum-payload", 12u,
      output, sizeof(output), &result);
  CHECK(payload_status == W_SEED_NATIVE0_UNSUPPORTED &&
        storage.hir_result.status == W_SEED_HIR0_OK &&
        storage.hir_program.enum_count == 1u &&
        storage.hir_program.enum_case_count == 2u &&
        storage.hir_program.enum_case_parameter_count == 1u &&
        storage.hir_program.enum_payload_count == 1u &&
        storage.hir_program.enum_cases[0].payload_count == 0u &&
        storage.hir_program.enum_cases[1].first_payload == 0u &&
        storage.hir_program.enum_cases[1].payload_count == 1u &&
        storage.hir_program.enum_case_parameters[0].owner_case == 1u &&
        storage.hir_program.enum_case_parameters[0].type_index == 2u &&
        storage.hir_program.enum_payloads[0].owner_value == 1u &&
        storage.hir_program.enum_payloads[0].ordinal == 0u &&
        storage.hir_program.enum_payloads[0].parameter_ordinal == 0u &&
        storage.hir_program.values[1].kind == W_SEED_HIR0_VALUE_ENUM_CASE &&
        storage.hir_program.values[1].first_enum_payload == 0u &&
        storage.hir_program.values[1].enum_payload_count == 1u &&
        w_seed_hir0_verify(&storage.hir_program, &storage.hir_result));
  /* An empty entry still has no supported output plan. */
  for (size_t index = 0u; index < sizeof(output); index += 1u)
    CHECK(output[index] == 0xa7u);
  CHECK(memcmp(&result, &payload_result_snapshot, sizeof(result)) == 0);

  static const uint8_t capture_source[] =
      "enum Course { starter main(price: i64, tax: i64) }\n"
      "fn bill(course: Course): i64 { return switch course { "
      "case .main(tax: let fee, price: let amount): amount + fee "
      "case .starter: 10 } }\n"
      "entry { let order: Course = .main(tax: 2, price: 30) "
      "let total = bill(course: order) print(\"Bill ${total}\") }\n";
  CHECK(run_source(capture_source, sizeof(capture_source) - 1u,
                   "enum-capture", 12u, output, sizeof(output), &result) ==
        W_SEED_NATIVE0_OK);
  CHECK(storage.hir_result.status == W_SEED_HIR0_OK &&
        storage.hir_program.switch_capture_count == 2u &&
        storage.hir_switch_captures[0].parameter_ordinal == 1u &&
        storage.hir_switch_captures[1].parameter_ordinal == 0u &&
        w_seed_hir0_verify(&storage.hir_program, &storage.hir_result));
  CHECK(contains_bytes(output, result.mlir.written.mlir_bytes, "llvm.insertvalue"));
  CHECK(contains_bytes(output, result.mlir.written.mlir_bytes, "llvm.extractvalue"));
  return true;
}

static bool test_enum_payload_native_lowering(void) {
  static const uint8_t source[] =
      "enum Course { starter main(price: i64, tax: i64) dessert(price: i64) }\n"
      "fn order(price: i64, tax: i64): Course { "
      "return .main(tax: tax + 1, price: price) }\n"
      "fn bill(course: Course): i64 { return switch course { "
      "case .dessert(price: let amount): amount "
      "case .starter: 10 "
      "case .main(tax: let fee, price: let amount): amount + fee } }\n"
      "entry { let first = order(price: 30, tax: 2) "
      "let starter: Course = .starter "
      "let dessert: Course = .dessert(price: 7) "
      "let firstBill = bill(course: first) "
      "let starterBill = bill(course: starter) "
      "let dessertBill = bill(course: dessert) "
      "print(\"Bills ${firstBill}/${starterBill}/${dessertBill}\") }\n";
  static uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  const w_seed_mlir0_target targets[] = {TARGET, WINDOWS_TARGET};
  for (size_t target = 0u; target < 2u; target += 1u) {
    CHECK(run_source_mode(source, sizeof(source) - 1u, "enum-payload-run", 16u,
                          &targets[target], W_SEED_MLIR0_ARTIFACT_EXECUTABLE,
                          output, sizeof(output), &result) == W_SEED_NATIVE0_OK);
    const size_t length = result.mlir.written.mlir_bytes;
    CHECK(length == result.mlir.required.mlir_bytes);
    CHECK(storage.hir_program.switch_capture_count == 3u &&
          w_seed_hir0_verify(&storage.hir_program, &storage.hir_result));
    CHECK(contains_bytes(output, length, "!llvm.struct<(i2, array<2 x i64>)>"));
    CHECK(contains_bytes(output, length, "llvm.insertvalue"));
    CHECK(contains_bytes(output, length, "llvm.extractvalue"));
    CHECK(contains_bytes(output, length, "llvm.mlir.zero : !llvm.struct"));
    CHECK(contains_bytes(output, length, "llvm.intr.sadd.with.overflow"));
    CHECK(!contains_bytes(output, length, "malloc"));
    (void)memset(output, 0xacu, sizeof(output));
    const w_seed_native0_result snapshot = result;
    CHECK(run_source_mode(source, sizeof(source) - 1u, "enum-payload-run", 16u,
                          &targets[target], W_SEED_MLIR0_ARTIFACT_EXECUTABLE,
                          output, length - 1u, &result) == W_SEED_NATIVE0_CAPACITY);
    CHECK(memcmp(&snapshot, &result, sizeof(result)) == 0);
    for (size_t byte = 0u; byte < sizeof(output); byte += 1u)
      CHECK(output[byte] == 0xacu);
  }
  return true;
}

static bool test_bool_payload_native_lowering(void) {
  static const uint8_t source[] =
      "enum Flags { none bits(open: Bool, staffed: Bool, stocked: Bool, licensed: Bool) }\n"
      "fn opened(state: Flags): Bool { return switch state { "
      "case .bits(open: let value, ...): value "
      "case .none: false } }\n"
      "entry { let state: Flags = .bits(licensed: true, open: true, staffed: false, stocked: true) "
      "let value = opened(state: state) print(\"Open ${value}\") }\n";
  static const uint8_t mixed_source[] =
      "enum Mixed { "
      "none "
      "packed(b0: Bool, b1: Bool, b2: Bool, b3: Bool, b4: Bool, "
      "b5: Bool, b6: Bool, b7: Bool, b8: Bool) "
      "amount(value: i64) "
      "checked(flag: Bool, amount: i64) }\n"
      "fn boundary(state: Mixed): Bool { return switch state { "
      "case .checked(flag: let value, amount: _): value "
      "case .amount(value: _): false "
      "case .none: false "
      "case .packed(b7: let value, ...): value } }\n"
      "fn nextLane(state: Mixed): Bool { return switch state { "
      "case .checked(flag: let value, amount: _): value "
      "case .amount(value: _): false "
      "case .none: false "
      "case .packed(b8: let value, ...): value } }\n"
      "fn charge(state: Mixed): i64 { return switch state { "
      "case .checked(flag: _, amount: let value): value "
      "case .amount(value: let value): value "
      "case .none: 0 "
      "case .packed(b0: _, b1: _, b2: _, b3: _, b4: _, b5: _, b6: _, "
      "b7: _, b8: _): 0 } }\n"
      "entry { "
      "let packed: Mixed = .packed(b8: true, b0: false, b7: true, b1: false, "
      "b2: true, b3: false, b4: true, b5: false, b6: true) "
      "let checked: Mixed = .checked(amount: 31, flag: true) "
      "let high = nextLane(state: packed) "
      "let edge = boundary(state: packed) "
      "let total = charge(state: checked) "
      "print(\"Mixed ${high}/${edge}/${total}\") }\n";
  static uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  const w_seed_mlir0_target targets[] = {TARGET, WINDOWS_TARGET};
  for (size_t target = 0u; target < 2u; target += 1u) {
    const w_seed_native0_status pure_status = run_source_mode(
        source, sizeof(source) - 1u, "enum-bool-pure", 14u, &targets[target],
        W_SEED_MLIR0_ARTIFACT_EXECUTABLE, output, sizeof(output), &result);
    CHECK(pure_status == W_SEED_NATIVE0_OK);
    CHECK(contains_bytes(output, result.mlir.written.mlir_bytes,
                         "!llvm.struct<(i1, array<4 x i8>)>") &&
          contains_bytes(output, result.mlir.written.mlir_bytes,
                         "llvm.zext") &&
          contains_bytes(output, result.mlir.written.mlir_bytes,
                         "llvm.trunc") &&
          !contains_bytes(output, result.mlir.written.mlir_bytes, "malloc"));
    CHECK(run_source_mode(
              mixed_source, sizeof(mixed_source) - 1u, "enum-bool-mixed", 15u,
              &targets[target], W_SEED_MLIR0_ARTIFACT_EXECUTABLE, output,
              sizeof(output), &result) == W_SEED_NATIVE0_OK);
    const size_t length = result.mlir.written.mlir_bytes;
    CHECK(contains_bytes(output, length, "!llvm.struct<(i2, array<2 x i64>)>") &&
          contains_bytes(output, length, "llvm.shl") &&
          contains_bytes(output, length, "llvm.lshr") &&
          contains_bytes(output, length, "llvm.and") &&
          contains_bytes(output, length, "llvm.trunc") &&
          !contains_bytes(output, length, "array<9 x i64>") &&
          !contains_bytes(output, length, "malloc"));
  }
  return true;
}

static bool test_enum_switch_native_lowering(void) {
  /* Source arms are deliberately out of declaration order.  HIR and MLIR
   * must still dispatch in the enum's canonical starter/main/dessert order. */
  static const uint8_t source[] =
      "enum Course {\n"
      "  starter\n"
      "  main\n"
      "  dessert\n"
      "}\n"
      "\n"
      "fn price(course: Course): i64 {\n"
      "  return switch course {\n"
      "    case .dessert: 20\n"
      "    case .starter: 10\n"
      "    case .main: 30\n"
      "  }\n"
      "}\n"
      "\n"
      "entry {\n"
      "  let starter = price(course: .starter)\n"
      "  print(\"Courses ${starter}\")\n"
      "}\n";
  static uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  const w_seed_native0_status switch_status =
      run_source(source, sizeof(source) - 1u, "enum-switch", 11u, output,
                 sizeof(output), &result);
  CHECK(switch_status == W_SEED_NATIVE0_OK);
  CHECK(result.status == W_SEED_NATIVE0_OK &&
        result.mlir.written.mlir_bytes == result.mlir.required.mlir_bytes);

  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(
            &storage.hir_program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  CHECK(selection.has_enum_switch && selection.has_cfg &&
        storage.hir_program.function_count == 2u);
  const w_seed_hir0_function *price = &storage.hir_program.functions[0];
  CHECK(price->parameter_count == 1u && price->first_block <
                                            storage.hir_program.block_count);
  const uint32_t parameter_index = price->first_parameter;
  CHECK(parameter_index < storage.hir_program.parameter_count);
  const uint32_t enum_type =
      storage.hir_program.parameters[parameter_index].type_index;
  CHECK(enum_type < storage.hir_program.type_count &&
        storage.hir_program.types[enum_type].kind == W_SEED_HIR0_TYPE_ENUM);
  const w_seed_hir0_terminator *dispatch =
      &storage.hir_program.terminators[
          storage.hir_program.blocks[price->first_block].terminator_index];
  CHECK(dispatch->kind == W_SEED_HIR0_TERMINATOR_SWITCH_ENUM &&
        dispatch->value_index < storage.hir_program.value_count &&
        storage.hir_program.values[dispatch->value_index].type_index ==
            enum_type &&
        dispatch->switch_carrier_width == 2u &&
        dispatch->switch_edge_count == 3u);
  for (size_t ordinal = 0u; ordinal < dispatch->switch_edge_count; ordinal += 1u) {
    const w_seed_hir0_switch_edge *edge =
        &storage.hir_program.switch_edges[dispatch->first_switch_edge + ordinal];
    CHECK(edge->ordinal == ordinal && edge->enum_index == dispatch->switch_enum_index &&
          edge->enum_case_index ==
              storage.hir_program.enums[dispatch->switch_enum_index].first_case +
                  ordinal &&
           edge->target_block == price->first_block + 1u + ordinal);
  }
  CHECK(contains_bytes(output, result.mlir.written.mlir_bytes,
                       "cf.switch %p0 : i2, [") &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "default: ^w_fn_0_switch_default,") &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "      0: ^w_fn_0_b_1,") &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "      1: ^w_fn_0_b_2,") &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "      -2: ^w_fn_0_b_3\n") &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "^w_fn_0_switch_default:\n    llvm.unreachable") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes, "llvm.switch"));
  const size_t switch_offset =
      find_bytes(output, result.mlir.written.mlir_bytes, "cf.switch", 0u);
  const size_t default_offset = find_bytes(
      output, result.mlir.written.mlir_bytes, "^w_fn_0_switch_default:",
      switch_offset == SIZE_MAX ? 0u : switch_offset);
  const size_t comparison_offset =
      find_bytes(output, result.mlir.written.mlir_bytes, "llvm.icmp",
                 switch_offset == SIZE_MAX ? 0u : switch_offset);
  CHECK(switch_offset != SIZE_MAX && default_offset > switch_offset &&
        (comparison_offset == SIZE_MAX || comparison_offset > default_offset));

  /* A forged HIR carrier proof is rejected before any caller-owned bytes or
   * result fields are published. */
  const uint32_t dispatch_index = storage.hir_program.blocks[price->first_block]
                                      .terminator_index;
  storage.hir_terminators[dispatch_index].switch_carrier_width = 1u;
  uint8_t forged_output[W_SEED_MLIR0_MAX_BYTES];
  (void)memset(forged_output, 0xa5u, sizeof(forged_output));
  w_seed_mlir0_result forged_result;
  (void)memset(&forged_result, 0x5au, sizeof(forged_result));
  const w_seed_mlir0_result forged_snapshot = forged_result;
  const w_seed_mlir0_input forged_input = {
      &storage.hir_program, &storage.hir_result,
      W_SEED_MLIR0_ARTIFACT_EXECUTABLE};
  CHECK(w_seed_mlir0_emit(
            &forged_input, &TARGET,
            &(w_seed_mlir0_output){forged_output, sizeof(forged_output)},
            &forged_result) == W_SEED_MLIR0_INVALID_HIR);
  for (size_t index = 0u; index < sizeof(forged_output); index += 1u)
    CHECK(forged_output[index] == 0xa5u);
  CHECK(memcmp(&forged_result, &forged_snapshot, sizeof(forged_result)) == 0);

  /* The checked-in Restaurant fixture exercises the complete carrier path:
   * three enum arguments, three local calls, and full interpolation. */
  static const uint8_t restaurant_source[] =
      "enum Course {\n"
      "  starter\n"
      "  main\n"
      "  dessert\n"
      "}\n"
      "\n"
      "fn price(course: Course): i64 {\n"
      "  return switch course {\n"
      "    case .starter: 10\n"
      "    case .main: 30\n"
      "    case .dessert: 20\n"
      "  }\n"
      "}\n"
      "\n"
      "entry {\n"
      "  let starter = price(course: .starter)\n"
      "  let main = price(course: .main)\n"
      "  let dessert = price(course: .dessert)\n"
      "  print(\"Courses ${starter}/${main}/${dessert}\")\n"
      "}\n";
  static uint8_t restaurant_output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result restaurant_result;
  const w_seed_native0_status restaurant_status =
      run_source(restaurant_source, sizeof(restaurant_source) - 1u,
                 "enum", 15u, restaurant_output,
                 sizeof(restaurant_output), &restaurant_result);
  CHECK(restaurant_status == W_SEED_NATIVE0_OK);
  CHECK(restaurant_result.status == W_SEED_NATIVE0_OK &&
        contains_bytes(restaurant_output, restaurant_result.mlir.written.mlir_bytes,
                       "cf.switch %p0 : i2, [") &&
        contains_bytes(restaurant_output, restaurant_result.mlir.written.mlir_bytes,
                       "\\43\\6f\\75\\72\\73\\65\\73\\20"));
  w_seed_native0_result restaurant_windows_result;
  const w_seed_native0_status restaurant_windows_status = run_source_mode(
      restaurant_source, sizeof(restaurant_source) - 1u, "enum.w",
      17u, &WINDOWS_TARGET, W_SEED_MLIR0_ARTIFACT_EXECUTABLE,
      restaurant_output, sizeof(restaurant_output), &restaurant_windows_result);
  CHECK(restaurant_windows_status == W_SEED_NATIVE0_OK &&
        restaurant_windows_result.status == W_SEED_NATIVE0_OK &&
        contains_bytes(restaurant_output,
                       restaurant_windows_result.mlir.written.mlir_bytes,
                       "cf.switch %p0 : i2, [") &&
        contains_bytes(restaurant_output,
                       restaurant_windows_result.mlir.written.mlir_bytes,
                       "      -2: ^w_fn_0_b_3\n"));
  return true;
}

static bool test_enum_subset_switch_native_lowering(void) {
  static const uint8_t source[] =
      "enum Stage {\n"
      "  accepted\n"
      "  reserving\n"
      "  preparing\n"
      "  serving\n"
      "  completed\n"
      "}\n"
      "\n"
      "alias WorkStage = Stage<[.serving, .preparing]>\n"
      "\n"
      "fn label(stage: WorkStage): i64 {\n"
      "  return switch stage {\n"
      "    case .serving: 2\n"
      "    case .preparing: 1\n"
      "  }\n"
      "}\n"
      "\n"
      "entry {\n"
      "  let preparing = label(stage: .preparing)\n"
      "  let serving = label(stage: .serving)\n"
      "  print(\"Work ${preparing}/${serving}\")\n"
      "}\n";
  static uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  CHECK(run_source(source, sizeof(source) - 1u, "enum-subset-switch", 18u,
                   output, sizeof(output), &result) == W_SEED_NATIVE0_OK);
  CHECK(result.status == W_SEED_NATIVE0_OK &&
        storage.hir_program.function_count == 2u);

  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(
            &storage.hir_program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  CHECK(selection.has_enum_switch && selection.has_cfg);
  const w_seed_hir0_function *label = &storage.hir_program.functions[0];
  CHECK(label->parameter_count == 1u && label->first_parameter <
                                             storage.hir_program.parameter_count);
  const uint32_t subset_type =
      storage.hir_program.parameters[label->first_parameter].type_index;
  CHECK(subset_type < storage.hir_program.type_count &&
        storage.hir_program.types[subset_type].kind ==
            W_SEED_HIR0_TYPE_ENUM_SUBSET);
  const w_seed_hir0_type *subset = &storage.hir_program.types[subset_type];
  CHECK(subset->enum_index == 0u && subset->subset_member_count == 2u &&
        subset->first_subset_member < storage.hir_program.enum_subset_member_count);
  const w_seed_hir0_enum *base = &storage.hir_program.enums[subset->enum_index];
  CHECK(base->case_count == 5u);
  CHECK(storage.hir_program.enum_subset_members[subset->first_subset_member]
                .enum_case_index == base->first_case + 2u &&
        storage.hir_program.enum_subset_members[subset->first_subset_member + 1u]
                .enum_case_index == base->first_case + 3u);
  const w_seed_hir0_terminator *dispatch =
      &storage.hir_program.terminators[
          storage.hir_program.blocks[label->first_block].terminator_index];
  CHECK(dispatch->kind == W_SEED_HIR0_TERMINATOR_SWITCH_ENUM &&
        dispatch->switch_carrier_width == 3u &&
        dispatch->switch_edge_count == 2u &&
        storage.hir_program.values[dispatch->value_index].type_index ==
            subset_type);
  for (size_t ordinal = 0u; ordinal < dispatch->switch_edge_count; ordinal += 1u) {
    const w_seed_hir0_switch_edge *edge =
        &storage.hir_program.switch_edges[dispatch->first_switch_edge + ordinal];
    CHECK(edge->ordinal == ordinal && edge->enum_index == subset->enum_index &&
          edge->enum_case_index == base->first_case + 2u + ordinal &&
          edge->target_block == label->first_block + 1u + ordinal);
  }
  CHECK(contains_bytes(output, result.mlir.written.mlir_bytes,
                       "cf.switch %p0 : i3, [") &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "      2: ^w_fn_0_b_1,") &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "      3: ^w_fn_0_b_2\n") &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "^w_fn_0_switch_default:\n    llvm.unreachable\n") &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "^w_fn_0_switch_default:") == 1u &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "llvm.mlir.constant(2 : i3) : i3") &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "llvm.mlir.constant(3 : i3) : i3") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "llvm.mlir.constant(0 : i3) : i3") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "llvm.mlir.constant(1 : i3) : i3") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "llvm.mlir.constant(4 : i3) : i3") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes, "llvm.switch"));

  /* A subset edge must retain the base case identity; forged identity is
   * rejected before Native0 publishes the caller-owned artifact. */
  const uint32_t subset_member_index = subset->first_subset_member;
  const uint32_t saved_case =
      storage.hir_enum_subset_members[subset_member_index].enum_case_index;
  storage.hir_enum_subset_members[subset_member_index].enum_case_index =
      base->first_case;
  uint8_t forged_output[W_SEED_MLIR0_MAX_BYTES];
  (void)memset(forged_output, 0xa5u, sizeof(forged_output));
  w_seed_mlir0_result forged_result;
  (void)memset(&forged_result, 0x5au, sizeof(forged_result));
  const w_seed_mlir0_result forged_snapshot = forged_result;
  const w_seed_mlir0_input forged_input = {
      &storage.hir_program, &storage.hir_result,
      W_SEED_MLIR0_ARTIFACT_EXECUTABLE};
  CHECK(w_seed_mlir0_emit(
            &forged_input, &TARGET,
            &(w_seed_mlir0_output){forged_output, sizeof(forged_output)},
            &forged_result) == W_SEED_MLIR0_INVALID_HIR);
  for (size_t index = 0u; index < sizeof(forged_output); index += 1u)
    CHECK(forged_output[index] == 0xa5u);
  CHECK(memcmp(&forged_result, &forged_snapshot, sizeof(forged_result)) == 0);
  storage.hir_enum_subset_members[subset_member_index].enum_case_index =
      saved_case;
  CHECK(w_seed_hir0_verify(&storage.hir_program, &storage.hir_result));

  w_seed_native0_result windows_result;
  CHECK(run_source_mode(source, sizeof(source) - 1u, "enum-subset-switch", 18u,
                        &WINDOWS_TARGET, W_SEED_MLIR0_ARTIFACT_EXECUTABLE,
                        output, sizeof(output), &windows_result) ==
            W_SEED_NATIVE0_OK);
  CHECK(windows_result.status == W_SEED_NATIVE0_OK &&
        contains_bytes(output, windows_result.mlir.written.mlir_bytes,
                       "cf.switch %p0 : i3, [") &&
        contains_bytes(output, windows_result.mlir.written.mlir_bytes,
                       "      3: ^w_fn_0_b_2\n"));
  return true;
}

static bool test_process_handler_catalog_and_artifact(void) {
  static const uint8_t canonical[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn launch(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { return .success }\n"
      "entry(launch)\n";
  static const uint8_t variant[] =
      "// aliases and trivia must not change the handler artifact\n"
      "import { Arguments as A, Context as C, ExitCode as E } from std.process\n"
      "async fn renamed(args: A, ctx: C): E { return .success }\n"
      "// the entry function name is not an ABI selector\n"
      "entry(renamed)\n";
  static const uint8_t unknown_module[] =
      "import { Arguments, Context, ExitCode } from std.other\n"
      "async fn launch(args: Arguments, ctx: Context): ExitCode { "
      "return .success }\nentry(launch)\n";
  static uint8_t canonical_bytes[W_SEED_MLIR0_MAX_BYTES];
  static uint8_t variant_bytes[W_SEED_MLIR0_MAX_BYTES];
  static uint8_t windows_bytes[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result canonical_result;
  w_seed_native0_result variant_result;
  w_seed_native0_result windows_result;

  CHECK(run_source_mode(
            canonical, sizeof(canonical) - 1u, "process-canonical", 17u,
            &TARGET, W_SEED_MLIR0_ARTIFACT_PROCESS_HANDLER, canonical_bytes,
            sizeof(canonical_bytes), &canonical_result) == W_SEED_NATIVE0_OK);
  CHECK(storage.input.external_module_count == 1u &&
        storage.input.resolved_import_count == 1u &&
        storage.hir_program.external_module_count == 1u &&
        storage.hir_program.external_symbol_count == 4u &&
        storage.hir_program.functions[0].direct_entry ==
            W_SEED_HIR0_DIRECT_ENTRY_AVAILABLE &&
        storage.hir_program.entries[0].adapter_kind ==
            W_SEED_HIR0_ENTRY_ADAPTER_NATIVE_PROCESS &&
        storage.hir_program.entries[0].cleanup_obligation ==
            W_SEED_HIR0_ENTRY_CLEANUP_RELEASE_HANDLER_OWNERS);
  CHECK(contains_bytes(canonical_bytes,
                       canonical_result.mlir.written.mlir_bytes,
                       "// " W_SEED_MLIR0_PROCESS_SCHEMA_VERSION "\n") &&
        contains_bytes(canonical_bytes,
                       canonical_result.mlir.written.mlir_bytes,
                       "llvm.target_triple = \"" W_SEED_MLIR0_TARGET_TRIPLE
                       "\"") &&
        contains_bytes(canonical_bytes,
                       canonical_result.mlir.written.mlir_bytes,
                       "llvm.func @w_seed_process_entry0_handler") &&
        contains_bytes(canonical_bytes,
                       canonical_result.mlir.written.mlir_bytes,
                       "llvm.call @w_seed_process_entry0_context_drop(%context)") &&
        contains_bytes(canonical_bytes,
                       canonical_result.mlir.written.mlir_bytes,
                       "llvm.call @w_seed_process_entry0_arguments_drop(%arguments)") &&
        !contains_bytes(canonical_bytes,
                        canonical_result.mlir.written.mlir_bytes,
                        "llvm.func @main") &&
        !contains_bytes(canonical_bytes,
                        canonical_result.mlir.written.mlir_bytes,
                        "GetStdHandle"));
  const size_t context_call = find_bytes(
      canonical_bytes, canonical_result.mlir.written.mlir_bytes,
      "llvm.call @w_seed_process_entry0_context_drop", 0u);
  const size_t arguments_call = find_bytes(
      canonical_bytes, canonical_result.mlir.written.mlir_bytes,
      "llvm.call @w_seed_process_entry0_arguments_drop", 0u);
  CHECK(context_call != SIZE_MAX && arguments_call != SIZE_MAX &&
        context_call < arguments_call);

  CHECK(run_source_mode(
            variant, sizeof(variant) - 1u, "process-variant", 15u, &TARGET,
            W_SEED_MLIR0_ARTIFACT_PROCESS_HANDLER, variant_bytes,
            sizeof(variant_bytes), &variant_result) == W_SEED_NATIVE0_OK);
  CHECK(variant_result.mlir.written.mlir_bytes ==
            canonical_result.mlir.written.mlir_bytes &&
        memcmp(variant_bytes, canonical_bytes,
               canonical_result.mlir.written.mlir_bytes) == 0 &&
        memcmp(variant_result.mlir.mlir_sha256,
               canonical_result.mlir.mlir_sha256,
               sizeof(canonical_result.mlir.mlir_sha256)) == 0);

  CHECK(run_source_mode(
            canonical, sizeof(canonical) - 1u, "process-windows", 15u,
            &WINDOWS_TARGET, W_SEED_MLIR0_ARTIFACT_PROCESS_HANDLER,
            windows_bytes, sizeof(windows_bytes), &windows_result) ==
        W_SEED_NATIVE0_OK);
  CHECK(contains_bytes(windows_bytes, windows_result.mlir.written.mlir_bytes,
                       "llvm.target_triple = \"" W_SEED_MLIR0_TARGET_TRIPLE_WINDOWS
                       "\"") &&
        !contains_bytes(windows_bytes, windows_result.mlir.written.mlir_bytes,
                        "mainCRTStartup"));

  CHECK(run_source_mode(
            canonical, sizeof(canonical) - 1u, "process-default", 15u,
            &TARGET, W_SEED_MLIR0_ARTIFACT_EXECUTABLE, canonical_bytes,
            sizeof(canonical_bytes), &canonical_result) ==
        W_SEED_NATIVE0_OK);
  CHECK(contains_bytes(canonical_bytes,
                       canonical_result.mlir.written.mlir_bytes,
                       "llvm.func @main"));
  CHECK(contains_bytes(canonical_bytes,
                       canonical_result.mlir.written.mlir_bytes,
                       "llvm.func internal @w_seed_process_arguments_drop"));

  (void)memset(canonical_bytes, 0xb6u, sizeof(canonical_bytes));
  (void)memset(&canonical_result, 0x6bu, sizeof(canonical_result));
  uint8_t unknown_snapshot[sizeof(canonical_result)];
  (void)memcpy(unknown_snapshot, &canonical_result, sizeof(unknown_snapshot));
  CHECK(run_source_mode(
            unknown_module, sizeof(unknown_module) - 1u, "process-unknown",
            15u, &TARGET, W_SEED_MLIR0_ARTIFACT_PROCESS_HANDLER,
            canonical_bytes, sizeof(canonical_bytes), &canonical_result) ==
        W_SEED_NATIVE0_UNSUPPORTED);
  for (size_t index = 0u; index < sizeof(canonical_bytes); index += 1u)
    CHECK(canonical_bytes[index] == 0xb6u);
  CHECK(memcmp(&canonical_result, unknown_snapshot,
               sizeof(unknown_snapshot)) == 0);

  (void)memset(canonical_bytes, 0xc7u, sizeof(canonical_bytes));
  (void)memset(&canonical_result, 0x7cu, sizeof(canonical_result));
  uint8_t invalid_kind_snapshot[sizeof(canonical_result)];
  (void)memcpy(invalid_kind_snapshot, &canonical_result,
               sizeof(invalid_kind_snapshot));
  CHECK(run_source_mode(
            canonical, sizeof(canonical) - 1u, "process-kind", 12u, &TARGET,
            (w_seed_mlir0_artifact_kind)-1, canonical_bytes,
            sizeof(canonical_bytes), &canonical_result) ==
        W_SEED_NATIVE0_UNSUPPORTED);
  for (size_t index = 0u; index < sizeof(canonical_bytes); index += 1u)
    CHECK(canonical_bytes[index] == 0xc7u);
  CHECK(memcmp(&canonical_result, invalid_kind_snapshot,
               sizeof(invalid_kind_snapshot)) == 0);
  return true;
}

static bool test_process_input0_public_artifact(void) {
  static const uint8_t source[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { if args.isEmpty { print(\"missing\") "
      "return .failure(2) } else { print(\"received\") return .success } }\n"
      "entry(run)\n";
  static uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  static uint8_t explicit_output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  w_seed_native0_result explicit_result;
  CHECK(run_source_mode(
            source, sizeof(source) - 1u, "process-input0", 14u,
            &WINDOWS_TARGET, W_SEED_MLIR0_ARTIFACT_EXECUTABLE, output,
            sizeof(output), &result) == W_SEED_NATIVE0_OK);
  CHECK(storage.hir_program.external_symbol_count == 7u &&
        storage.hir_program.functions[0].direct_entry ==
            W_SEED_HIR0_DIRECT_ENTRY_AVAILABLE &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "// " W_SEED_MLIR0_PROCESS_EXECUTABLE_SCHEMA_VERSION
                       "\n") &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "llvm.func @mainCRTStartup") &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "@w_seed_process_arguments_is_empty") &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "@w_seed_process_context_drop") &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "@w_seed_process_arguments_drop") &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "@w_seed_process_root_finalize"));

  CHECK(run_source_mode(
            source, sizeof(source) - 1u, "process-input0", 14u,
            &WINDOWS_TARGET, W_SEED_MLIR0_ARTIFACT_PROCESS_EXECUTABLE,
            explicit_output, sizeof(explicit_output), &explicit_result) ==
        W_SEED_NATIVE0_OK);
  CHECK(explicit_result.mlir.written.mlir_bytes ==
            result.mlir.written.mlir_bytes &&
        memcmp(explicit_output, output,
               result.mlir.written.mlir_bytes) == 0 &&
        memcmp(explicit_result.mlir.mlir_sha256, result.mlir.mlir_sha256,
               sizeof(result.mlir.mlir_sha256)) == 0);

  (void)memset(explicit_output, 0xa5, sizeof(explicit_output));
  (void)memset(&explicit_result, 0x5a, sizeof(explicit_result));
  const w_seed_native0_result short_snapshot = explicit_result;
  CHECK(result.mlir.written.mlir_bytes > 0u &&
        run_source_mode(
            source, sizeof(source) - 1u, "process-input0", 14u,
            &WINDOWS_TARGET, W_SEED_MLIR0_ARTIFACT_PROCESS_EXECUTABLE,
            explicit_output, result.mlir.written.mlir_bytes - 1u,
            &explicit_result) == W_SEED_NATIVE0_CAPACITY);
  for (size_t index = 0u; index < sizeof(explicit_output); index += 1u)
    CHECK(explicit_output[index] == 0xa5u);
  CHECK(memcmp(&explicit_result, &short_snapshot,
               sizeof(explicit_result)) == 0);

  CHECK(run_source_mode(
            source, sizeof(source) - 1u, "process-input0", 14u, &TARGET,
            W_SEED_MLIR0_ARTIFACT_PROCESS_EXECUTABLE, explicit_output,
            sizeof(explicit_output), &explicit_result) ==
        W_SEED_NATIVE0_OK);
  CHECK(contains_bytes(explicit_output,
                       explicit_result.mlir.written.mlir_bytes,
                       "llvm.target_triple = \"" W_SEED_MLIR0_TARGET_TRIPLE
                       "\"") &&
        contains_bytes(explicit_output,
                       explicit_result.mlir.written.mlir_bytes,
                       "llvm.func @main"));
  return true;
}

static bool test_process_integer_exactly_adapter(void) {
  static const uint8_t success_source[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode throws NumericConversionError { "
      "let narrowed = try i8(exactly: 1) return .success }\n"
      "entry(run)\n";
  static const uint8_t error_source[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode throws NumericConversionError { "
      "let narrowed = try i8(exactly: 128) return .success }\n"
      "entry(run)\n";
  static const uint8_t runtime_source[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode throws NumericConversionError { "
      "let narrowed = try i8(exactly: args.count) "
      "print(\"Exact ${narrowed}\") return .success }\n"
      "entry(run)\n";
  static const uint8_t arithmetic_fault_source[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode throws NumericConversionError { "
      "let narrowed = try i8(exactly: args.count) "
      "print(\"Exact ${narrowed + 1_i8}\") return .success }\n"
      "entry(run)\n";
  static const uint8_t direct_throw_source[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "enum Failure: Error { denied }\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode throws Failure { throw .denied }\n"
      "entry(run)\n";
  static uint8_t success_output[W_SEED_MLIR0_MAX_BYTES];
  static uint8_t error_output[W_SEED_MLIR0_MAX_BYTES];
  static uint8_t runtime_output[W_SEED_MLIR0_MAX_BYTES];
  static uint8_t arithmetic_fault_output[W_SEED_MLIR0_MAX_BYTES];
  static uint8_t rejected_output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result success_result;
  w_seed_native0_result error_result;
  w_seed_native0_result runtime_result;
  w_seed_native0_result arithmetic_fault_result;
  w_seed_native0_result rejected_result;
  const w_seed_native0_status success_status = run_source_mode(
      success_source, sizeof(success_source) - 1u, "process-exact-success",
      21u, &WINDOWS_TARGET, W_SEED_MLIR0_ARTIFACT_PROCESS_EXECUTABLE,
      success_output, sizeof(success_output), &success_result);
  CHECK(success_status == W_SEED_NATIVE0_OK);
  CHECK(storage.hir_program.call_count == 0u &&
        storage.hir_program.binding_count == 1u &&
        storage.hir_program.functions[0].is_throws &&
        storage.hir_program.entries[0].cleanup_obligation ==
            W_SEED_HIR0_ENTRY_CLEANUP_RELEASE_HANDLER_OWNERS_REVERSE_ON_ALL_OUTCOMES);
  w_seed_native_subset0_process selection;
  CHECK(w_seed_native_subset0_select_process_executable(
            &storage.hir_program, &storage.hir_result, &selection) ==
            W_SEED_NATIVE_SUBSET0_OK &&
        selection.has_integer_exactly && selection.maximum_stdout_bytes == 0u &&
        selection.exact_source_bit_width == 64u &&
        selection.exact_destination_bit_width == 8u &&
        selection.exact_source_is_signed && selection.exact_destination_is_signed);
  const w_seed_native0_status runtime_status = run_source_mode(
      runtime_source, sizeof(runtime_source) - 1u, "process-exact-runtime",
      21u, &WINDOWS_TARGET, W_SEED_MLIR0_ARTIFACT_PROCESS_EXECUTABLE,
      runtime_output, sizeof(runtime_output), &runtime_result);
  CHECK(runtime_status == W_SEED_NATIVE0_OK);
  CHECK(storage.runtime_requirements ==
        W_SEED_RUNTIME_REQUIREMENTS_PROCESS_ARGUMENTS);
  CHECK(storage.hir_program.call_count == 1u &&
        storage.hir_program.binding_count == 1u);
  CHECK(w_seed_native_subset0_select_process_executable(
            &storage.hir_program, &storage.hir_result, &selection) ==
            W_SEED_NATIVE_SUBSET0_OK &&
        selection.has_integer_exactly &&
        selection.exact_source_is_target_usize &&
        selection.exact_source_bit_width == 0u &&
        !selection.exact_source_is_signed &&
        selection.exact_destination_bit_width == 8u &&
        selection.exact_destination_is_signed &&
        selection.maximum_stdout_bytes > 0u);
  CHECK(contains_bytes(runtime_output, runtime_result.mlir.written.mlir_bytes,
                       "@w_seed_process_arguments_count"));
  CHECK(contains_bytes(runtime_output, runtime_result.mlir.written.mlir_bytes,
                       "llvm.icmp \"ule\""));
  CHECK(contains_bytes(runtime_output, runtime_result.mlir.written.mlir_bytes,
                       "llvm.call @w_seed_append_i64"));
  CHECK(contains_bytes(runtime_output, runtime_result.mlir.written.mlir_bytes,
                       "llvm.call @w_seed_write"));
  const size_t runtime_bytes = runtime_result.mlir.written.mlir_bytes;
  const size_t runtime_finalize = find_bytes(
      runtime_output, runtime_bytes,
      "llvm.call @w_seed_process_root_finalize", 0u);
  const size_t runtime_map = find_bytes(
      runtime_output, runtime_bytes, "^process_map_outcome", runtime_finalize);
  const size_t runtime_typed_error = find_bytes(
      runtime_output, runtime_bytes, "^process_abnormal(", runtime_map);
  const size_t runtime_success = find_bytes(
      runtime_output, runtime_bytes, "^process_exact_success", runtime_map);
  const size_t runtime_write = find_bytes(
      runtime_output, runtime_bytes, "llvm.call @w_seed_write", runtime_success);
  CHECK(runtime_finalize != SIZE_MAX && runtime_map > runtime_finalize &&
        runtime_typed_error > runtime_map && runtime_success > runtime_map &&
        runtime_write > runtime_success && runtime_write > runtime_typed_error);
  CHECK(run_source_mode(
            arithmetic_fault_source, sizeof(arithmetic_fault_source) - 1u,
            "process-arithmetic-fault", 24u, &WINDOWS_TARGET,
            W_SEED_MLIR0_ARTIFACT_PROCESS_EXECUTABLE,
            arithmetic_fault_output, sizeof(arithmetic_fault_output),
            &arithmetic_fault_result) == W_SEED_NATIVE0_OK);
  const size_t arithmetic_fault_bytes =
      arithmetic_fault_result.mlir.written.mlir_bytes;
  const size_t arithmetic_fault_drop = find_bytes(
      arithmetic_fault_output, arithmetic_fault_bytes,
      "llvm.call @w_seed_process_context_drop", 0u);
  const size_t arithmetic_fault_map = find_bytes(
      arithmetic_fault_output, arithmetic_fault_bytes,
      "^process_map_outcome", arithmetic_fault_drop);
  const size_t arithmetic_fault_flush = find_bytes(
      arithmetic_fault_output, arithmetic_fault_bytes,
      "llvm.call @w_seed_write", arithmetic_fault_map);
  CHECK(contains_bytes(arithmetic_fault_output, arithmetic_fault_bytes,
                       "@w_seed_process_checked_add_i64") &&
        contains_bytes(arithmetic_fault_output, arithmetic_fault_bytes,
                       "llvm.store %one, %fault : i64, !llvm.ptr") &&
        contains_bytes(arithmetic_fault_output, arithmetic_fault_bytes,
                       "llvm.mlir.constant(8589934592 : i64)") &&
        contains_bytes(arithmetic_fault_output, arithmetic_fault_bytes,
                       "llvm.mlir.constant(2 : i32)") &&
        !contains_bytes(arithmetic_fault_output, arithmetic_fault_bytes,
                        "@w_seed_checked_add_i64") &&
        arithmetic_fault_drop != SIZE_MAX &&
        arithmetic_fault_map > arithmetic_fault_drop &&
        arithmetic_fault_flush > arithmetic_fault_map);
  const size_t runtime_exact_source_index =
      (size_t)(selection.exact_source_value - storage.hir_program.values);
  CHECK(runtime_exact_source_index < storage.hir_program.value_count);
  const w_seed_hir0_value saved_runtime_exact_source =
      storage.hir_values[runtime_exact_source_index];
  storage.hir_values[runtime_exact_source_index].external_symbol_index = 5u;
  CHECK(w_seed_native_subset0_select_process_executable(
            &storage.hir_program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_INVALID);
  storage.hir_values[runtime_exact_source_index] = saved_runtime_exact_source;
  const w_seed_hir0_entry_cleanup_kind exact_cleanup =
      storage.hir_program.entries[0].cleanup_obligation;
  w_seed_hir0_program mutated_program = storage.hir_program;
  w_seed_hir0_entry mutated_entry = storage.hir_program.entries[0];
  mutated_program.entries = &mutated_entry;
  mutated_entry.cleanup_obligation =
      W_SEED_HIR0_ENTRY_CLEANUP_RELEASE_HANDLER_OWNERS_REVERSE_ON_TYPED_ERROR;
  CHECK(w_seed_native_subset0_select_process_executable(
            &mutated_program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_INVALID);
  mutated_entry.cleanup_obligation = exact_cleanup;
  w_seed_hir0_terminator mutated_terminators[3];
  CHECK(storage.hir_program.terminator_count ==
        sizeof(mutated_terminators) / sizeof(mutated_terminators[0]));
  (void)memcpy(mutated_terminators, storage.hir_program.terminators,
               sizeof(mutated_terminators));
  mutated_program.terminators = mutated_terminators;
  mutated_terminators[0].target_block = mutated_terminators[0].else_block;
  CHECK(w_seed_native_subset0_select_process_executable(
            &mutated_program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_INVALID);
  const size_t success_bytes = success_result.mlir.written.mlir_bytes;
  CHECK(success_bytes != 0u &&
        contains_bytes(success_output, success_bytes,
                       "llvm.func @mainCRTStartup") &&
        contains_bytes(success_output, success_bytes,
                       "llvm.cond_br %process_exact_fits") &&
        contains_bytes(success_output, success_bytes,
                       "llvm.icmp \"sle\" %v") &&
        contains_bytes(success_output, success_bytes,
                       "llvm.mlir.constant(4294967297 : i64) : i64") &&
        contains_bytes(success_output, success_bytes,
                       "llvm.return %process_exact_error_carrier : i64") &&
        contains_bytes(success_output, success_bytes,
                       "%process_outcome_kind = llvm.lshr") &&
        contains_bytes(success_output, success_bytes,
                       "%process_typed_error_kind = llvm.mlir.constant(1 : i64)") &&
        contains_bytes(success_output, success_bytes,
                       "%process_checked_fault_kind = llvm.mlir.constant(2 : i64)") &&
        contains_bytes(success_output, success_bytes,
                       "llvm.cond_br %process_has_abnormal_outcome") &&
        contains_bytes(success_output, success_bytes,
                       "^w_fn_0_b_1(%process_exact_destination : i64), ^w_fn_0_b_2(%process_exact_error_payload : i64)") &&
        !contains_bytes(success_output, success_bytes,
                        "llvm.call @w_seed_process_arguments_is_empty") &&
        !contains_bytes(success_output, success_bytes,
                        "llvm.call @w_seed_process_arguments_count") &&
        !contains_bytes(success_output, success_bytes,
                        "llvm.call @w_seed_write"));
  const size_t exact_cast = find_bytes(
      success_output, success_bytes, "%process_exact_destination =", 0u);
  const size_t exact_branch = find_bytes(
      success_output, success_bytes, "llvm.cond_br %process_exact_fits", 0u);
  CHECK(exact_cast != SIZE_MAX && exact_branch != SIZE_MAX &&
        exact_cast < exact_branch);
  const size_t context_call = find_bytes(
      success_output, success_bytes,
      "llvm.call @w_seed_process_context_drop", 0u);
  const size_t arguments_call = find_bytes(
      success_output, success_bytes,
      "llvm.call @w_seed_process_arguments_drop", 0u);
  const size_t root_finalize_call = find_bytes(
      success_output, success_bytes,
      "llvm.call @w_seed_process_root_finalize", 0u);
  const size_t outcome_map = find_bytes(
      success_output, success_bytes, "^process_map_outcome", 0u);
  CHECK(context_call != SIZE_MAX && arguments_call != SIZE_MAX &&
        root_finalize_call != SIZE_MAX && outcome_map != SIZE_MAX &&
        count_bytes(success_output, success_bytes,
                    "llvm.call @w_seed_process_context_drop") == 1u &&
        count_bytes(success_output, success_bytes,
                    "llvm.call @w_seed_process_arguments_drop") == 1u &&
        count_bytes(success_output, success_bytes,
                    "llvm.call @w_seed_process_root_finalize") == 1u &&
        context_call < arguments_call && arguments_call < root_finalize_call &&
        root_finalize_call < outcome_map);

  CHECK(run_source_mode(
            error_source, sizeof(error_source) - 1u, "process-exact-error",
            19u, &WINDOWS_TARGET,
            W_SEED_MLIR0_ARTIFACT_PROCESS_EXECUTABLE, error_output,
            sizeof(error_output), &error_result) == W_SEED_NATIVE0_OK);
  const size_t error_bytes = error_result.mlir.written.mlir_bytes;
  CHECK(error_bytes != 0u &&
        contains_bytes(error_output, error_bytes,
                       "%v1 = llvm.mlir.constant(128 : i64) : i64") &&
        contains_bytes(success_output, success_bytes,
                       "%v1 = llvm.mlir.constant(1 : i64) : i64") &&
        (error_bytes != success_bytes ||
         memcmp(error_output, success_output, error_bytes) != 0) &&
        memcmp(error_result.mlir.mlir_sha256, success_result.mlir.mlir_sha256,
               sizeof(error_result.mlir.mlir_sha256)) != 0);

  (void)memset(rejected_output, 0xd4u, sizeof(rejected_output));
  (void)memset(&rejected_result, 0x4du, sizeof(rejected_result));
  const w_seed_native0_result rejected_snapshot = rejected_result;
  CHECK(run_source_mode(
            direct_throw_source, sizeof(direct_throw_source) - 1u,
            "process-direct-throw", 20u, &WINDOWS_TARGET,
            W_SEED_MLIR0_ARTIFACT_PROCESS_EXECUTABLE, rejected_output,
            sizeof(rejected_output), &rejected_result) != W_SEED_NATIVE0_OK);
  for (size_t index = 0u; index < sizeof(rejected_output); index += 1u)
    CHECK(rejected_output[index] == 0xd4u);
  CHECK(memcmp(&rejected_result, &rejected_snapshot,
               sizeof(rejected_snapshot)) == 0);
  return true;
}

static bool test_process_runtime_float_rounding_subset(void) {
  static const uint8_t source[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode throws NumericConversionError { "
      "let rounded = try i8(rounding: if args.count == 0 { 2.5_f64 } "
      "else { 3.5_f64 }, mode: .nearestEven) "
      "print(\"Rounded ${rounded}\") return .success }\n"
      "entry(run)\n";
  static uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  const w_seed_native0_status native_status = run_source_mode(
      source, sizeof(source) - 1u, "process-runtime-float-rounding",
      sizeof("process-runtime-float-rounding") - 1u, &WINDOWS_TARGET,
      W_SEED_MLIR0_ARTIFACT_PROCESS_EXECUTABLE, output, sizeof(output),
      &result);
  CHECK(native_status == W_SEED_NATIVE0_OK);
  CHECK(w_seed_hir0_verify(&storage.hir_program, &storage.hir_result));
  w_seed_native_subset0_process selection;
  CHECK(w_seed_native_subset0_select_process_executable(
            &storage.hir_program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  const uint32_t function_index = storage.hir_program.entries[0].target_function;
  const w_seed_hir0_function *function =
      &storage.hir_program.functions[function_index];
  const uint32_t split_block = function->first_block + 3u;
  CHECK(selection.has_float_to_integer_rounding &&
        selection.rounding_split_block_index == split_block &&
        selection.rounding_source_value != NULL &&
        selection.rounding_source_value->kind ==
            W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ &&
        selection.rounding_source_type_index < storage.hir_program.type_count &&
        storage.hir_program.types[selection.rounding_source_type_index].kind ==
            W_SEED_HIR0_TYPE_F64 &&
        selection.rounding_normal_block_index == function->first_block + 4u &&
        selection.rounding_non_finite_block_index ==
            function->first_block + 5u &&
        selection.rounding_out_of_range_block_index ==
            function->first_block + 6u &&
        selection.rounding_mode ==
            W_SEED_HIR0_ROUNDING_MODE_NEAREST_EVEN);

  const uint32_t branch_terminator =
      storage.hir_program.blocks[function->first_block].terminator_index;
  const w_seed_hir0_terminator saved_branch =
      storage.hir_terminators[branch_terminator];
  storage.hir_terminators[branch_terminator].else_block =
      storage.hir_terminators[branch_terminator].target_block;
  CHECK(w_seed_native_subset0_select_process_executable(
            &storage.hir_program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_INVALID);
  storage.hir_terminators[branch_terminator] = saved_branch;
  CHECK(w_seed_hir0_verify(&storage.hir_program, &storage.hir_result));
  return true;
}

static bool test_panic_native_routes(void) {
  static const uint8_t ordinary[] =
      "entry { panic(\"native ordinary panic\") }\n";
  static uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  CHECK(run_source(ordinary, sizeof(ordinary) - 1u, "native-panic", 12u,
                   output, sizeof(output), &result) == W_SEED_NATIVE0_OK);
  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(
            &storage.hir_program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK &&
        selection.has_reachable_panic && selection.maximum_stdout_bytes == 0u);
  CHECK(contains_bytes(output, result.mlir.written.mlir_bytes,
                       "    \"llvm.intr.trap\"() : () -> ()\n"
                       "    llvm.unreachable\n") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "native ordinary panic"));

  CHECK(run_source_mode(
            ordinary, sizeof(ordinary) - 1u, "native-panic", 12u,
            &WINDOWS_TARGET, W_SEED_MLIR0_ARTIFACT_EXECUTABLE, output,
            sizeof(output), &result) == W_SEED_NATIVE0_OK);
  CHECK(contains_bytes(output, result.mlir.written.mlir_bytes,
                       "llvm.target_triple = \""
                       W_SEED_MLIR0_TARGET_TRIPLE_WINDOWS "\"") &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "    \"llvm.intr.trap\"() : () -> ()\n"
                       "    llvm.unreachable\n") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "native ordinary panic"));

  static const uint8_t process[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { panic(\"native process panic\") }\n"
      "entry(run)\n";
  CHECK(run_source_mode(
            process, sizeof(process) - 1u, "native-process-panic", 20u,
            &TARGET, W_SEED_MLIR0_ARTIFACT_PROCESS_EXECUTABLE, output,
            sizeof(output), &result) == W_SEED_NATIVE0_OK);
  w_seed_native_subset0_process process_selection;
  CHECK(w_seed_native_subset0_select_process_executable(
            &storage.hir_program, &storage.hir_result, &process_selection) ==
        W_SEED_NATIVE_SUBSET0_OK && process_selection.has_reachable_panic &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "    \"llvm.intr.trap\"() : () -> ()\n"
                       "    llvm.unreachable\n") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "native process panic"));
  CHECK(run_source_mode(
            process, sizeof(process) - 1u, "native-process-panic", 20u,
            &WINDOWS_TARGET, W_SEED_MLIR0_ARTIFACT_PROCESS_EXECUTABLE, output,
            sizeof(output), &result) == W_SEED_NATIVE0_OK);
  CHECK(contains_bytes(output, result.mlir.written.mlir_bytes,
                       "llvm.target_triple = \""
                       W_SEED_MLIR0_TARGET_TRIPLE_WINDOWS "\"") &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "    \"llvm.intr.trap\"() : () -> ()\n"
                       "    llvm.unreachable\n") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "native process panic"));

  static const uint8_t dead[] =
      "fn dead() { panic(\"native dead panic\") }\nentry {}\n";
  (void)memset(output, 0x6bu, sizeof(output));
  uint8_t dead_output_snapshot[sizeof(output)];
  (void)memcpy(dead_output_snapshot, output, sizeof(dead_output_snapshot));
  const w_seed_native0_result dead_snapshot = result;
  CHECK(run_source(dead, sizeof(dead) - 1u, "native-dead-panic", 17u, output,
                   sizeof(output), &result) == W_SEED_NATIVE0_UNSUPPORTED);
  CHECK(memcmp(output, dead_output_snapshot, sizeof(output)) == 0 &&
        memcmp(&result, &dead_snapshot, sizeof(result)) == 0);
  return true;
}

static bool test_process_arguments_count_public_artifact(void) {
  static const uint8_t source[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { if args.count != 0 { print(\"has arguments\") "
      "return .success } else { print(\"no arguments\") "
      "return .success } }\n"
      "entry(run)\n";
  static uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  CHECK(run_source_mode(
            source, sizeof(source) - 1u, "process-arguments-count",
            sizeof("process-arguments-count") - 1u, &WINDOWS_TARGET,
            W_SEED_MLIR0_ARTIFACT_PROCESS_EXECUTABLE, output, sizeof(output),
            &result) == W_SEED_NATIVE0_OK);
  const w_seed_hir0_program *program = &storage.hir_program;
  size_t count_reads = 0u;
  size_t count_comparisons = 0u;
  size_t usize_literals = 0u;
  for (size_t value_index = 0u; value_index < program->value_count;
       value_index += 1u) {
    const w_seed_hir0_value *value = &program->values[value_index];
    if (value->kind == W_SEED_HIR0_VALUE_EXTERNAL_MEMBER &&
        value->external_module_index == 0u &&
        value->external_symbol_index == 6u &&
        value->type_index < program->type_count &&
        program->types[value->type_index].kind == W_SEED_HIR0_TYPE_USIZE &&
        hir_text_equals(program, value->member_name, "count"))
      count_reads += 1u;
    if (value->kind == W_SEED_HIR0_VALUE_USIZE_COUNT_COMPARISON)
      count_comparisons += 1u;
    if (value->kind == W_SEED_HIR0_VALUE_CONST_USIZE &&
        value->unsigned_integer_value == UINT64_C(0))
      usize_literals += 1u;
  }
  CHECK(program->external_symbol_count == 7u && count_reads == 1u &&
        count_comparisons == 1u && usize_literals == 1u &&
        program->binding_count == 0u);
  CHECK(contains_bytes(output, result.mlir.written.mlir_bytes,
                       "@w_seed_process_arguments_count") &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "@w_seed_process_arguments_count") >= 2u &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "llvm.icmp \"ne\"") &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "llvm.mlir.constant(0 : i64) : i64") &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       ") : (!llvm.ptr) -> i64"));

  w_seed_native_subset0_process process_selection;
  CHECK(w_seed_native_subset0_select_process_executable(
            program, &storage.hir_result, &process_selection) ==
            W_SEED_NATIVE_SUBSET0_OK &&
        process_selection.count_symbol_index == 6u);

  /* Raw process owners and non-existent/optional selectors must not reach the
   * executable adapter. The direct selector check is a second fail-closed
   * boundary after each source rejection. */
  static const char *const REJECTED[] = {
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { let saved = args return .success }\n"
      "entry(run)\n",
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { let saved = ctx return .success }\n"
      "entry(run)\n",
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { let copied = copy args return .success }\n"
      "entry(run)\n",
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { let copied = copy ctx return .success }\n"
      "entry(run)\n",
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "fn takeArgs(value: ProcessArguments) { }\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { takeArgs(value: args) return .success }\n"
      "entry(run)\n",
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "fn takeContext(value: ProcessContext) { }\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { takeContext(value: ctx) return .success }\n"
      "entry(run)\n",
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { return args }\n"
      "entry(run)\n",
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { return ctx }\n"
      "entry(run)\n",
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { return args.length }\n"
      "entry(run)\n",
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { return args[0] }\n"
      "entry(run)\n",
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { return ctx.count }\n"
      "entry(run)\n",
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { print(\"count ${args?.count}\") "
      "return .success }\n"
      "entry(run)\n"};
  static uint8_t rejected_output[W_SEED_MLIR0_MAX_BYTES];
  for (size_t index = 0u; index < sizeof(REJECTED) / sizeof(REJECTED[0]);
       index += 1u) {
    w_seed_native0_result rejected_result;
    CHECK(run_source_mode(
              (const uint8_t *)REJECTED[index], strlen(REJECTED[index]),
              "process-arguments-count-negative", 35u, &WINDOWS_TARGET,
              W_SEED_MLIR0_ARTIFACT_PROCESS_EXECUTABLE, rejected_output,
              sizeof(rejected_output), &rejected_result) !=
          W_SEED_NATIVE0_OK);
    w_seed_native_subset0_process rejected_selection;
    CHECK(w_seed_native_subset0_select_process_executable(
              &storage.hir_program, &storage.hir_result,
              &rejected_selection) != W_SEED_NATIVE_SUBSET0_OK);
  }
  return true;
}

static bool expect_process_count_native_unsigned_predicate(
    const uint8_t *source, size_t source_length, const char *predicate) {
  static uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  CHECK(run_source_mode(
            source, source_length, "process-arguments-ordered",
            sizeof("process-arguments-ordered") - 1u, &WINDOWS_TARGET,
            W_SEED_MLIR0_ARTIFACT_PROCESS_EXECUTABLE, output, sizeof(output),
            &result) == W_SEED_NATIVE0_OK);
  uint32_t comparison_value = UINT32_MAX;
  for (uint32_t index = 0u; index < storage.hir_program.value_count; ++index) {
    if (storage.hir_program.values[index].kind ==
        W_SEED_HIR0_VALUE_USIZE_COUNT_COMPARISON) {
      comparison_value = index;
      break;
    }
  }
  CHECK(comparison_value != UINT32_MAX);
  const w_seed_hir0_value *comparison =
      &storage.hir_program.values[comparison_value];
  char expected[160];
  const int expected_length = snprintf(expected, sizeof(expected),
                                       "%%v%u = %s %%v%u, %%v%u : i64",
                                       comparison_value, predicate,
                                       comparison->left_value,
                                       comparison->right_value);
  CHECK(expected_length > 0 && (size_t)expected_length < sizeof(expected));
  CHECK(contains_bytes(output, result.mlir.written.mlir_bytes, expected));
  return true;
}

static bool test_process_arguments_count_ordered_native(void) {
  static const uint8_t LESS_SOURCE[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { print(\"matches ${args.count < 2}\") "
      "return .success }\n"
      "entry(run)\n";
  static const uint8_t LESS_EQUAL_SOURCE[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { print(\"matches ${args.count <= 2}\") "
      "return .success }\n"
      "entry(run)\n";
  static const uint8_t GREATER_SOURCE[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { print(\"matches ${args.count > 2}\") "
      "return .success }\n"
      "entry(run)\n";
  static const uint8_t GREATER_EQUAL_SOURCE[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { print(\"matches ${args.count >= 2}\") "
      "return .success }\n"
      "entry(run)\n";
  static const uint8_t REVERSED_LESS_SOURCE[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { print(\"matches ${2 < args.count}\") "
      "return .success }\n"
      "entry(run)\n";
  static const uint8_t REVERSED_LESS_EQUAL_SOURCE[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { print(\"matches ${2 <= args.count}\") "
      "return .success }\n"
      "entry(run)\n";
  static const uint8_t REVERSED_GREATER_SOURCE[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { print(\"matches ${2 > args.count}\") "
      "return .success }\n"
      "entry(run)\n";
  static const uint8_t REVERSED_GREATER_EQUAL_SOURCE[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { print(\"matches ${2 >= args.count}\") "
      "return .success }\n"
      "entry(run)\n";
  CHECK(expect_process_count_native_unsigned_predicate(
      LESS_SOURCE, sizeof(LESS_SOURCE) - 1u, "llvm.icmp \"ult\""));
  CHECK(expect_process_count_native_unsigned_predicate(
      LESS_EQUAL_SOURCE, sizeof(LESS_EQUAL_SOURCE) - 1u,
      "llvm.icmp \"ule\""));
  CHECK(expect_process_count_native_unsigned_predicate(
      GREATER_SOURCE, sizeof(GREATER_SOURCE) - 1u, "llvm.icmp \"ugt\""));
  CHECK(expect_process_count_native_unsigned_predicate(
      GREATER_EQUAL_SOURCE, sizeof(GREATER_EQUAL_SOURCE) - 1u,
      "llvm.icmp \"uge\""));
  CHECK(expect_process_count_native_unsigned_predicate(
      REVERSED_LESS_SOURCE, sizeof(REVERSED_LESS_SOURCE) - 1u,
      "llvm.icmp \"ult\""));
  CHECK(expect_process_count_native_unsigned_predicate(
      REVERSED_LESS_EQUAL_SOURCE, sizeof(REVERSED_LESS_EQUAL_SOURCE) - 1u,
      "llvm.icmp \"ule\""));
  CHECK(expect_process_count_native_unsigned_predicate(
      REVERSED_GREATER_SOURCE, sizeof(REVERSED_GREATER_SOURCE) - 1u,
      "llvm.icmp \"ugt\""));
  CHECK(expect_process_count_native_unsigned_predicate(
      REVERSED_GREATER_EQUAL_SOURCE,
      sizeof(REVERSED_GREATER_EQUAL_SOURCE) - 1u,
      "llvm.icmp \"uge\""));
  return true;
}

static bool test_process_enum_payload_public_artifact(void) {
  static const uint8_t source[] =
      "import {\n"
      "  Arguments as InputArgs,\n"
      "  Context as InputContext,\n"
      "  ExitCode as InputExit,\n"
      "} from std.process\n"
      "\n"
      "enum AdmissionState {\n"
      "  unavailable\n"
      "  observed(missing: Bool, amount: i64)\n"
      "}\n"
      "\n"
      "fn buildAdmission(missing: Bool): AdmissionState {\n"
      "  return .observed(amount: 17, missing: missing)\n"
      "}\n"
      "\n"
      "fn admissionIsMissing(state: AdmissionState): Bool {\n"
      "  return switch state {\n"
      "    case .unavailable: false\n"
      "    case .observed(missing: let missing, amount: _): missing\n"
      "  }\n"
      "}\n"
      "\n"
      "async fn dispatch(input: InputArgs, environment: InputContext): InputExit {\n"
      "  let state = buildAdmission(missing: input.isEmpty)\n"
      "  let missing = admissionIsMissing(state: state)\n"
      "  let repeated = input.isEmpty\n"
      "  if missing {\n"
      "    print(\"enum-missing ${repeated}\")\n"
      "    return .failure(7)\n"
      "  } else {\n"
      "    print(\"enum-received ${repeated}\")\n"
      "    return .success\n"
      "  }\n"
      "}\n"
      "\n"
      "entry(dispatch)\n";
  static uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  CHECK(run_source_mode(
            source, sizeof(source) - 1u, "process-enum-payload",
            sizeof("process-enum-payload") - 1u, &WINDOWS_TARGET,
            W_SEED_MLIR0_ARTIFACT_PROCESS_EXECUTABLE, output,
            sizeof(output), &result) == W_SEED_NATIVE0_OK);
  const w_seed_hir0_program *program = &storage.hir_program;
  CHECK(storage.hir_result.status == W_SEED_HIR0_OK &&
        w_seed_hir0_verify(program, &storage.hir_result) &&
        program->entry_count == 1u &&
        program->entries[0].target_function != 0u &&
        program->entries[0].target_function < program->function_count);

  bool has_failure_seven = false;
  size_t is_empty_reads = 0u;
  for (size_t value_index = 0u; value_index < program->value_count; value_index += 1u) {
    const w_seed_hir0_value *value = &program->values[value_index];
    if (value->kind == W_SEED_HIR0_VALUE_EXTERNAL_MEMBER &&
        hir_text_equals(program, value->member_name, "isEmpty"))
      is_empty_reads += 1u;
    if (value->kind == W_SEED_HIR0_VALUE_EXTERNAL_ENUM_CASE &&
        hir_text_equals(program, value->member_name, "failure") &&
        value->left_value != W_SEED_HIR0_NONE &&
        value->left_value < program->value_count &&
        program->values[value->left_value].kind ==
            W_SEED_HIR0_VALUE_CONST_I64 &&
        program->values[value->left_value].integer_value == 7)
      has_failure_seven = true;
  }
  CHECK(has_failure_seven && is_empty_reads == 2u);

  const size_t length = result.mlir.written.mlir_bytes;
  CHECK(contains_bytes(output, length, "llvm.func @mainCRTStartup") &&
        contains_bytes(output, length, "@w_seed_append_bool") &&
        contains_bytes(output, length, "llvm.insertvalue") &&
        contains_bytes(output, length, "llvm.extractvalue") &&
        contains_bytes(output, length, "!llvm.struct<(i1, array<2 x i64>)>") &&
        contains_bytes(output, length, "cf.switch") &&
        contains_bytes(output, length, "llvm.cond_br") &&
        contains_bytes(output, length,
                       "\\65\\6e\\75\\6d\\2d\\6d\\69\\73\\73\\69\\6e\\67\\20") &&
        contains_bytes(output, length,
                       "\\65\\6e\\75\\6d\\2d\\72\\65\\63\\65\\69\\76\\65\\64\\20") &&
        !contains_bytes(output, length, "@w_seed_process_missing") &&
        !contains_bytes(output, length, "@w_seed_process_received") &&
        !contains_bytes(output, length, "missing\\0A") &&
        !contains_bytes(output, length, "received\\0A"));
  return true;
}

static bool test_process_stdout_bounds(void) {
  static char source[W_SEED_NATIVE0_MAX_SOURCE_BYTES];
  static uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  const size_t source_capacity = sizeof(source) - 1u;
  const size_t identity_length = sizeof("process-stdout-bound") - 1u;
  CHECK(make_process_stdout_bound_source(source, source_capacity, 1u, false) &&
        strlen(source) <= source_capacity &&
        run_source_mode((const uint8_t *)source, strlen(source),
                        "process-stdout-bound", identity_length,
                        &WINDOWS_TARGET,
                        W_SEED_MLIR0_ARTIFACT_PROCESS_EXECUTABLE, output,
                        sizeof(output), &result) == W_SEED_NATIVE0_OK);
  CHECK(result.status == W_SEED_NATIVE0_OK &&
        result.mlir.written.mlir_bytes == result.mlir.required.mlir_bytes &&
        result.mlir.written.mlir_bytes != 0u &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "llvm.func @mainCRTStartup"));

  CHECK(make_process_stdout_bound_source(source, source_capacity, 2u, false));
  (void)memset(output, 0xa5u, sizeof(output));
  (void)memset(&result, 0x5au, sizeof(result));
  const w_seed_native0_result twice_snapshot = result;
  CHECK(run_source_mode((const uint8_t *)source, strlen(source),
                        "process-stdout-bound", identity_length,
                        &WINDOWS_TARGET,
                        W_SEED_MLIR0_ARTIFACT_PROCESS_EXECUTABLE, output,
                        sizeof(output), &result) == W_SEED_NATIVE0_UNSUPPORTED);
  for (size_t index = 0u; index < sizeof(output); index += 1u)
    CHECK(output[index] == 0xa5u);
  CHECK(memcmp(&result, &twice_snapshot, sizeof(result)) == 0);

  CHECK(make_process_stdout_bound_source(source, source_capacity, 1u, true));
  (void)memset(output, 0xb6u, sizeof(output));
  (void)memset(&result, 0x6bu, sizeof(result));
  const w_seed_native0_result extra_snapshot = result;
  CHECK(run_source_mode((const uint8_t *)source, strlen(source),
                        "process-stdout-bound", identity_length,
                        &WINDOWS_TARGET,
                        W_SEED_MLIR0_ARTIFACT_PROCESS_EXECUTABLE, output,
                        sizeof(output), &result) == W_SEED_NATIVE0_UNSUPPORTED);
  for (size_t index = 0u; index < sizeof(output); index += 1u)
    CHECK(output[index] == 0xb6u);
  CHECK(memcmp(&result, &extra_snapshot, sizeof(result)) == 0);
  return true;
}

static bool test_logical_native_selector(void) {
  static const uint8_t source[] =
      "fn rhs(flag: Bool): Bool { return !flag }\n"
      "fn both(left: Bool): Bool { return left && rhs(flag: false) }\n"
      "fn either(left: Bool): Bool { return left || rhs(flag: true) }\n"
      "fn main() { print(\"logical\") }\n"
      "entry(main)\n";
  static uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  (void)memset(output, 0, sizeof(output));
  (void)memset(&result, 0, sizeof(result));
  const w_seed_native0_status status = run_source(
      source, sizeof(source) - 1u, "logical-id", 10u, output, sizeof(output),
      &result);
  CHECK(status == W_SEED_NATIVE0_OK && result.status == W_SEED_NATIVE0_OK &&
        result.source_bytes == sizeof(source) - 1u &&
        result.mlir.written.mlir_bytes == result.mlir.required.mlir_bytes &&
        result.mlir.written.mlir_bytes != 0u &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "llvm.cond_br"));
  CHECK(storage.hir_output.block_arguments == storage.hir_block_arguments &&
        storage.hir_output.block_argument_capacity ==
            W_SEED_NATIVE0_HIR_BLOCK_ARGUMENTS &&
        storage.hir_program.block_arguments == storage.hir_block_arguments &&
        storage.hir_program.block_argument_capacity ==
            W_SEED_NATIVE0_HIR_BLOCK_ARGUMENTS);

  const w_seed_hir0_program *program = &storage.hir_program;
  CHECK(program->function_count == 4u && program->call_count == 3u &&
        program->block_arguments == storage.hir_block_arguments &&
        program->block_argument_count == 2u &&
        program->functions[0].block_count == 1u &&
        program->functions[1].block_count == 4u &&
        program->functions[2].block_count == 4u);
  const size_t both_start = program->functions[1].first_block;
  const size_t either_start = program->functions[2].first_block;
  CHECK(program->terminators[both_start].logical_operator ==
            W_SEED_HIR0_LOGICAL_AND &&
        program->terminators[either_start].logical_operator ==
            W_SEED_HIR0_LOGICAL_OR &&
        program->blocks[both_start + 3u].block_argument_count == 1u &&
        program->blocks[either_start + 3u].block_argument_count == 1u &&
        program->block_arguments[0].owner_block == both_start + 3u &&
        program->block_arguments[1].owner_block == either_start + 3u);
  CHECK(edge_value_at(program, both_start + 1u) != W_SEED_HIR0_NONE &&
        edge_value_at(program, both_start + 2u) != W_SEED_HIR0_NONE &&
        edge_value_at(program, either_start + 1u) != W_SEED_HIR0_NONE &&
        edge_value_at(program, either_start + 2u) != W_SEED_HIR0_NONE);

  size_t unary_count = 0u;
  size_t bool_call_count = 0u;
  for (size_t value = 0u; value < program->value_count; value += 1u) {
    if (program->values[value].kind == W_SEED_HIR0_VALUE_UNARY_BOOL &&
        program->values[value].unary_operator == W_SEED_HIR0_UNARY_NOT)
      unary_count += 1u;
    if (program->values[value].kind == W_SEED_HIR0_VALUE_CALL_RESULT &&
        program->values[value].type_index == W_SEED_HIR0_TYPE_BOOL)
      bool_call_count += 1u;
  }
  CHECK(unary_count == 1u && bool_call_count == 2u);
  CHECK(program->calls[0].argument_count == 1u &&
        program->calls[1].argument_count == 1u &&
        program->arguments[0].type_index == W_SEED_HIR0_TYPE_BOOL &&
        program->arguments[1].type_index == W_SEED_HIR0_TYPE_BOOL);

  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(program, &storage.hir_result,
                                             &selection) ==
            W_SEED_NATIVE_SUBSET0_OK);
  CHECK(selection.has_cfg && selection.has_local_calls && selection.has_bool &&
        selection.maximum_stdout_bytes != 0u);
  return true;
}

static bool test_multi_carrier_native_subset_selector(void) {
  static const uint8_t source[] =
      "fn serve(limit: i64): i64 {\n"
      "  var served = 0\n"
      "  var total = 0\n"
      "  while served < limit {\n"
      "    total = total + 2\n"
      "    served = served + 1\n"
      "  }\n"
      "  return served + total\n"
      "}\n"
      "fn main() { let result = serve(limit: 3) print(\"${result}\") }\n"
      "entry(main)\n";
  static uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  const w_seed_native0_status native_status = run_source(
      source, sizeof(source) - 1u, "multi-carrier-native-subset", 23u,
      output, sizeof(output), &result);
  CHECK(native_status == W_SEED_NATIVE0_OK);
  const w_seed_hir0_program *program = &storage.hir_program;
  CHECK(program->function_count == 2u &&
        program->functions[0].block_count == 4u &&
        program->blocks[1].block_argument_count == 2u &&
        program->blocks[0].instruction_count == 2u &&
        program->blocks[2].instruction_count == 2u &&
        program->terminators[0].edge_argument_count == 2u &&
        program->terminators[2].edge_argument_count == 2u);
  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(
            program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  CHECK(selection.natural_loop_functions[0] && selection.has_cfg &&
        selection.has_local_calls && selection.maximum_stdout_bytes != 0u);

  const w_seed_hir0_block_argument saved_argument =
      storage.hir_block_arguments[1];
  storage.hir_block_arguments[1].ordinal = 0u;
  CHECK(w_seed_native_subset0_select_program(
            program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_INVALID);
  storage.hir_block_arguments[1] = saved_argument;
  CHECK(w_seed_native_subset0_select_program(
            program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);

  const w_seed_hir0_edge_argument saved_edge = storage.hir_edge_arguments[2];
  storage.hir_edge_arguments[2].ordinal = 1u;
  CHECK(w_seed_native_subset0_select_program(
            program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_INVALID);
  storage.hir_edge_arguments[2] = saved_edge;
  CHECK(w_seed_native_subset0_select_program(
            program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);

  const w_seed_hir0_edge_argument saved_edge_value =
      storage.hir_edge_arguments[2];
  storage.hir_edge_arguments[2].value_index =
      storage.hir_edge_arguments[3].value_index;
  CHECK(w_seed_native_subset0_select_program(
            program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_INVALID);
  storage.hir_edge_arguments[2] = saved_edge_value;
  CHECK(w_seed_native_subset0_select_program(
            program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);

  const w_seed_hir0_binding saved_binding = storage.hir_bindings[3];
  storage.hir_bindings[3].type_index = W_SEED_HIR0_TYPE_BOOL;
  CHECK(w_seed_native_subset0_select_program(
            program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_INVALID);
  storage.hir_bindings[3] = saved_binding;
  CHECK(w_seed_native_subset0_select_program(
            program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);

  const w_seed_hir0_terminator saved_body = storage.hir_terminators[2];
  storage.hir_terminators[2].edge_argument_count = 1u;
  CHECK(w_seed_native_subset0_select_program(
            program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_INVALID);
  storage.hir_terminators[2] = saved_body;
  CHECK(w_seed_native_subset0_select_program(
            program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);

  const w_seed_hir0_binding saved_source = storage.hir_bindings[2];
  storage.hir_bindings[2].source_binding = 0u;
  CHECK(w_seed_native_subset0_select_program(
            program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_INVALID);
  storage.hir_bindings[2] = saved_source;
  CHECK(w_seed_native_subset0_select_program(
            program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  return true;
}

static bool test_break_continue_multi_carrier_native_subset_selector(void) {
  /* This mirrors fixtures/while-break-continue.w so the native source path
   * exercises the same verified HIR contract as the standalone witness. */
  static const uint8_t source[] =
      "fn scan(limit: i64): i64 {\n"
      "  var index = 0\n"
      "  var total = 0\n"
      "  while index < limit {\n"
      "    index = index + 1\n"
      "    if index == 2 { continue }\n"
      "    if index == 5 { break }\n"
      "    total = total + index\n"
      "  }\n"
      "  return total\n"
      "}\n"
      "\n"
      "entry {\n"
      "  let result = scan(limit: 9)\n"
      "  print(\"${result}\")\n"
      "}\n";
  static uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  const w_seed_native0_status source_status = run_source(
      source, sizeof(source) - 1u, "while-break-continue-native",
      sizeof("while-break-continue-native") - 1u, output, sizeof(output),
      &result);
  CHECK(source_status == W_SEED_NATIVE0_OK);
  CHECK(result.status == W_SEED_NATIVE0_OK &&
        result.source_bytes == sizeof(source) - 1u &&
        result.mlir.written.mlir_bytes != 0u &&
        result.mlir.written.mlir_bytes == result.mlir.required.mlir_bytes &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "llvm.cond_br") &&
        contains_bytes(output, result.mlir.written.mlir_bytes, "llvm.br"));

  const w_seed_hir0_program *program = &storage.hir_program;
  CHECK(w_seed_hir0_verify(program, &storage.hir_result));
  size_t scan_index = SIZE_MAX;
  for (size_t function = 0u; function < program->function_count;
       function += 1u)
    if (hir_text_equals(program, program->functions[function].name, "scan"))
      scan_index = function;
  CHECK(scan_index < program->function_count);

  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(
            program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  CHECK(selection.has_cfg && selection.has_local_calls &&
        selection.verified_i64_loop_cfg_functions[scan_index] &&
        !selection.natural_loop_functions[scan_index] &&
        selection.maximum_stdout_bytes != 0u);

  const w_seed_hir0_function *scan = &program->functions[scan_index];
  uint32_t tuple_jump_index = W_SEED_HIR0_NONE;
  for (size_t block_index = scan->first_block;
       block_index < (size_t)scan->first_block + scan->block_count;
       block_index += 1u) {
    const uint32_t terminator_index =
        program->blocks[block_index].terminator_index;
    const w_seed_hir0_terminator *terminator =
        &program->terminators[terminator_index];
    if (terminator->kind == W_SEED_HIR0_TERMINATOR_JUMP &&
        terminator->edge_argument_count == 2u) {
      tuple_jump_index = terminator_index;
      break;
    }
  }
  CHECK(tuple_jump_index != W_SEED_HIR0_NONE);

  const w_seed_hir0_terminator tuple_jump =
      storage.hir_terminators[tuple_jump_index];
  const size_t edge_index = tuple_jump.first_edge_argument;
  CHECK(edge_index < program->edge_argument_count);
  const w_seed_hir0_edge_argument tuple_edge =
      storage.hir_edge_arguments[edge_index];
  storage.hir_edge_arguments[edge_index].type_index = W_SEED_HIR0_TYPE_BOOL;
  CHECK(w_seed_native_subset0_select_program(
            program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_INVALID);
  storage.hir_edge_arguments[edge_index] = tuple_edge;
  CHECK(w_seed_native_subset0_select_program(
            program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);

  storage.hir_edge_arguments[edge_index].owner_terminator =
      W_SEED_HIR0_NONE;
  CHECK(w_seed_native_subset0_select_program(
            program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_INVALID);
  storage.hir_edge_arguments[edge_index] = tuple_edge;
  CHECK(w_seed_native_subset0_select_program(
            program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);

  storage.hir_edge_arguments[edge_index].owner_block = W_SEED_HIR0_NONE;
  CHECK(w_seed_native_subset0_select_program(
            program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_INVALID);
  storage.hir_edge_arguments[edge_index] = tuple_edge;
  CHECK(w_seed_native_subset0_select_program(
            program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);

  const uint32_t entry_function = program->entries[0].target_function;
  CHECK(entry_function < program->function_count &&
        entry_function != scan_index);
  storage.hir_terminators[tuple_jump_index].target_block =
      program->functions[entry_function].first_block;
  CHECK(w_seed_native_subset0_select_program(
            program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_INVALID);
  storage.hir_terminators[tuple_jump_index] = tuple_jump;
  CHECK(w_seed_native_subset0_select_program(
            program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);

  uint32_t branch_index = W_SEED_HIR0_NONE;
  uint32_t carrier_block_index = W_SEED_HIR0_NONE;
  for (size_t block_index = scan->first_block;
       block_index < (size_t)scan->first_block + scan->block_count;
       block_index += 1u) {
    const w_seed_hir0_block *block = &program->blocks[block_index];
    if (carrier_block_index == W_SEED_HIR0_NONE &&
        block->block_argument_count != 0u)
      carrier_block_index = (uint32_t)block_index;
    const w_seed_hir0_terminator *terminator =
        &program->terminators[block->terminator_index];
    if (branch_index == W_SEED_HIR0_NONE &&
        terminator->kind == W_SEED_HIR0_TERMINATOR_BRANCH)
      branch_index = block->terminator_index;
  }
  CHECK(branch_index != W_SEED_HIR0_NONE &&
        carrier_block_index != W_SEED_HIR0_NONE);
  const w_seed_hir0_terminator loop_branch =
      storage.hir_terminators[branch_index];
  CHECK(loop_branch.target_block != carrier_block_index &&
        loop_branch.else_block != carrier_block_index);
  storage.hir_terminators[branch_index].target_block = carrier_block_index;
  CHECK(w_seed_native_subset0_select_program(
            program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_INVALID);
  storage.hir_terminators[branch_index] = loop_branch;
  CHECK(w_seed_native_subset0_select_program(
            program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);

  static const uint8_t unsupported[] =
      "fn scan(limit: i64): i64 {\n"
      "  var index = 0\n"
      "  var total = 0\n"
      "  while index < limit {\n"
      "    index = index + 1\n"
      "    if index == 2 { continue }\n"
      "    if index == 5 { break }\n"
      "    total = total + (index + (1 / 0))\n"
      "  }\n"
      "  return total\n"
      "}\n"
      "entry {\n"
      "  let result = scan(limit: 9)\n"
      "  print(\"${result}\")\n"
      "}\n";
  (void)memset(output, 0xa9u, sizeof(output));
  (void)memset(&result, 0xb0u, sizeof(result));
  const w_seed_native0_result result_snapshot = result;
  const w_seed_native0_status unsupported_status = run_source(
      unsupported, sizeof(unsupported) - 1u,
      "while-break-continue-unsupported",
      sizeof("while-break-continue-unsupported") - 1u, output,
      sizeof(output), &result);
  CHECK(unsupported_status == W_SEED_NATIVE0_UNSUPPORTED &&
        storage.frontend_result.status == W_SEED_FRONTEND_OK &&
        storage.hir_result.status == W_SEED_HIR0_OK &&
        w_seed_hir0_verify(&storage.hir_program, &storage.hir_result) &&
        w_seed_native_subset0_select_program(
            &storage.hir_program, &storage.hir_result, &selection) ==
            W_SEED_NATIVE_SUBSET0_UNSUPPORTED &&
        memcmp(&result, &result_snapshot, sizeof(result)) == 0);
  for (size_t byte = 0u; byte < sizeof(output); byte += 1u)
    CHECK(output[byte] == 0xa9u);
  return true;
}

static bool test_post_loop_continuation_native_subset(void) {
  static const uint8_t source[] =
      "fn settle(limit: i64): i64 {\n"
      "  var served = 0\n"
      "  var total = 0\n"
      "  while served < limit {\n"
      "    total = total + 2\n"
      "    served = served + 1\n"
      "  }\n"
      "  total = total + served\n"
      "  return total\n"
      "}\n"
      "fn main() { let result = settle(limit: 3) print(\"Final ${result}\") }\n"
      "entry(main)\n";
  static uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  CHECK(run_source(source, sizeof(source) - 1u, "post-loop-native", 16u,
                   output, sizeof(output), &result) == W_SEED_NATIVE0_OK);
  const w_seed_hir0_program *program = &storage.hir_program;
  CHECK(program->functions[0].block_count == 4u &&
        program->blocks[3].instruction_count == 1u &&
        program->blocks[3].first_instruction == 4u &&
        program->bindings[4].source_binding == 1u &&
        program->bindings[4].previous_version == 2u &&
        program->bindings[2].next_version == 4u);
  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(
            program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  const size_t function_start =
      find_bytes(output, result.mlir.written.mlir_bytes,
                 "llvm.func internal @w_fn_0", 0u);
  const size_t entry_start =
      find_bytes(output, result.mlir.written.mlir_bytes,
                 "llvm.func internal @w_fn_1", function_start);
  CHECK(function_start != SIZE_MAX && entry_start > function_start);
  const size_t function_bytes = entry_start - function_start;
  CHECK(selection.natural_loop_functions[0] && selection.has_cfg &&
        contains_bytes(output + function_start, function_bytes,
                       "%loop0_0, %loop0_1 = scf.while (") &&
        contains_bytes(output + function_start, function_bytes,
                       "@w_seed_checked_add_i64(%loop0_1, %loop0_0, %v") &&
        contains_bytes(output + function_start, function_bytes,
                       "llvm.return %v") &&
        !contains_bytes(output + function_start, function_bytes,
                        "llvm.alloca"));

  const w_seed_hir0_instruction saved_instruction = storage.hir_instructions[4];
  storage.hir_instructions[4].owner_block = 2u;
  CHECK(w_seed_native_subset0_select_program(
            program, &storage.hir_result, &selection) !=
        W_SEED_NATIVE_SUBSET0_OK);
  storage.hir_instructions[4] = saved_instruction;

  const w_seed_hir0_block saved_exit = storage.hir_blocks[3];
  storage.hir_blocks[3].instruction_count = 2u;
  CHECK(w_seed_native_subset0_select_program(
            program, &storage.hir_result, &selection) !=
        W_SEED_NATIVE_SUBSET0_OK);
  storage.hir_blocks[3] = saved_exit;

  const w_seed_hir0_binding saved_continuation = storage.hir_bindings[4];
  storage.hir_bindings[4].type_index = W_SEED_HIR0_TYPE_BOOL;
  CHECK(w_seed_native_subset0_select_program(
            program, &storage.hir_result, &selection) !=
        W_SEED_NATIVE_SUBSET0_OK);
  storage.hir_bindings[4] = saved_continuation;

  const w_seed_hir0_binding saved_update = storage.hir_bindings[2];
  storage.hir_bindings[2].next_version = W_SEED_HIR0_NONE;
  CHECK(w_seed_native_subset0_select_program(
            program, &storage.hir_result, &selection) !=
        W_SEED_NATIVE_SUBSET0_OK);
  storage.hir_bindings[2] = saved_update;

  static const char *const rejected[] = {
      "fn settle(limit: i64): i64 { var served = 0 var total = 0 "
      "while served < limit { total = total + 2 served = served + 1 } "
      "let after = total + served return after } entry(settle)\n",
      "fn settle(limit: i64): i64 { var served = 0 var total = 0 "
      "while served < limit { total = total + 2 served = served + 1 } "
      "total = total + served total = total + 1 return total } entry(settle)\n",
      "fn adjust(value: i64): i64 { return value + 1 }\n"
      "fn settle(limit: i64): i64 { var served = 0 var total = 0 "
      "while served < limit { total = total + 2 served = served + 1 } "
      "total = adjust(value: total) return total } entry(settle)\n",
      "fn settle(limit: i64): i64 { var served = 0 var total = 0 "
      "while served < limit { total = total + 2 served = served + 1 } "
      "limit = total + served return total } entry(settle)\n",
      "fn settle(limit: i64): i64 { var served = 0 var total = 0 "
      "while served < limit { total = total + 2 served = served + 1 } "
      "if limit > 0 { total = total + served } return total } entry(settle)\n",
      "fn settle(limit: i64): i64 { var served = 0 var total = 0 "
      "while served < limit { total = total + 2 served = served + 1 } "
      "print(\"after\") return total } entry(settle)\n",
  };
  for (size_t index = 0u; index < sizeof(rejected) / sizeof(rejected[0]);
       index += 1u) {
    w_seed_native0_result rejected_result;
    (void)memset(&rejected_result, 0x71, sizeof(rejected_result));
    const w_seed_native0_result snapshot = rejected_result;
    CHECK(run_source((const uint8_t *)rejected[index], strlen(rejected[index]),
                     "post-loop-reject", 17u, output, sizeof(output),
                     &rejected_result) != W_SEED_NATIVE0_OK);
    CHECK(memcmp(&rejected_result, &snapshot, sizeof(snapshot)) == 0);
  }
  return true;
}

static bool test_unary_i64_native_selector(void) {
  static const uint8_t constant_source[] =
      "fn negative(): i64 { return -7 }\n"
      "entry { let value = negative() print(\"${value}\") }\n";
  static uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  const w_seed_native0_status status = run_source(
      constant_source, sizeof(constant_source) - 1u, "unary-negate", 12u,
      output, sizeof(output), &result);
  CHECK(status == W_SEED_NATIVE0_OK);
  size_t unary_index = SIZE_MAX;
  for (size_t index = 0u; index < storage.hir_program.value_count; index += 1u)
    if (storage.hir_values[index].kind == W_SEED_HIR0_VALUE_UNARY_I64)
      unary_index = index;
  CHECK(unary_index != SIZE_MAX &&
        storage.hir_values[unary_index].unary_operator ==
            W_SEED_HIR0_UNARY_NEGATE &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       " = llvm.sub ") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "@w_seed_checked_subtract_i64"));
  return true;
}

static bool test_integer_prefix_native_matrix(void) {
  typedef struct {
    const char *type_name;
    const char *literal_suffix;
    bool is_signed;
    uint16_t bit_width;
  } integer_case;
  static const integer_case CASES[] = {
      {"i8", "i8", true, 8u},     {"i16", "i16", true, 16u},
      {"i32", "i32", true, 32u},  {"i64", "i64", true, 64u},
      {"Int", "i64", true, 64u},  {"u8", "u8", false, 8u},
      {"u16", "u16", false, 16u}, {"u32", "u32", false, 32u},
      {"u64", "u64", false, 64u}, {"UInt", "u64", false, 64u},
  };
  char source[W_SEED_NATIVE0_MAX_SOURCE_BYTES + 1u];
  size_t source_length = 0u;
  source[0] = '\0';
  for (size_t index = 0u; index < sizeof(CASES) / sizeof(CASES[0]);
       index += 1u) {
    const integer_case *integer = &CASES[index];
    if (integer->is_signed)
      CHECK(append_test_source(
          source, sizeof(source), &source_length,
          "fn negate_%s(value: %s): %s { return -value }\n",
          integer->type_name, integer->type_name, integer->type_name));
    CHECK(append_test_source(
        source, sizeof(source), &source_length,
        "fn invert_%s(value: %s): %s { return ~value }\n",
        integer->type_name, integer->type_name, integer->type_name));
  }
  CHECK(append_test_source(source, sizeof(source), &source_length,
                           "fn main() { "));
  for (size_t index = 0u; index < sizeof(CASES) / sizeof(CASES[0]);
       index += 1u) {
    const integer_case *integer = &CASES[index];
    if (integer->is_signed)
      CHECK(append_test_source(
          source, sizeof(source), &source_length,
          "let neg_%s = negate_%s(value: 7_%s) ", integer->type_name,
          integer->type_name, integer->literal_suffix));
    CHECK(append_test_source(
        source, sizeof(source), &source_length,
        "let inv_%s = invert_%s(value: %s_%s) ", integer->type_name,
        integer->type_name, integer->is_signed ? "42" : "85",
        integer->literal_suffix));
  }
  CHECK(append_test_source(source, sizeof(source), &source_length,
                           "print(\""));
  bool first = true;
  for (size_t index = 0u; index < sizeof(CASES) / sizeof(CASES[0]);
       index += 1u) {
    const integer_case *integer = &CASES[index];
    if (integer->is_signed) {
      CHECK(append_test_source(
          source, sizeof(source), &source_length,
          "%s%s ${neg_%s}/${inv_%s}", first ? "" : "; ",
          integer->type_name, integer->type_name, integer->type_name));
      first = false;
    } else {
      CHECK(append_test_source(
          source, sizeof(source), &source_length, "%s%s ${inv_%s}",
          first ? "" : "; ", integer->type_name, integer->type_name));
      first = false;
    }
  }
  CHECK(append_test_source(source, sizeof(source), &source_length,
                           "\") }\nentry(main)\n"));

  static uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  const w_seed_native0_status status = run_source(
      (const uint8_t *)source, source_length, "integer-prefix-family",
      sizeof("integer-prefix-family") - 1u, artifact, sizeof(artifact),
      &result);
  CHECK(status == W_SEED_NATIVE0_OK &&
        result.status == W_SEED_NATIVE0_OK &&
        result.mlir.written.mlir_bytes == result.mlir.required.mlir_bytes);
  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(
            &storage.hir_program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  CHECK(selection.has_local_calls && storage.hir_program.function_count == 16u);

  size_t negate_count = 0u;
  size_t signed_bit_not_count = 0u;
  size_t unsigned_bit_not_count = 0u;
  size_t negate_by_width[4] = {0u, 0u, 0u, 0u};
  size_t signed_bit_not_by_width[4] = {0u, 0u, 0u, 0u};
  size_t unsigned_bit_not_by_width[4] = {0u, 0u, 0u, 0u};
  for (size_t value_index = 0u;
       value_index < storage.hir_program.value_count; value_index += 1u) {
    const w_seed_hir0_value *value = &storage.hir_program.values[value_index];
    if (value->kind != W_SEED_HIR0_VALUE_UNARY_I64 &&
        value->kind != W_SEED_HIR0_VALUE_UNARY_U64)
      continue;
    CHECK(value->type_index < storage.hir_program.type_count);
    const w_seed_hir0_type *type = &storage.hir_program.types[value->type_index];
    CHECK(type->integer_bit_width == 8u || type->integer_bit_width == 16u ||
          type->integer_bit_width == 32u || type->integer_bit_width == 64u);
    const size_t width_slot = type->integer_bit_width == 8u
                                  ? 0u
                                  : (type->integer_bit_width == 16u
                                         ? 1u
                                         : (type->integer_bit_width == 32u
                                                ? 2u
                                                : 3u));
    CHECK(value->left_value < storage.hir_program.value_count);
    const w_seed_hir0_value *operand =
        &storage.hir_program.values[value->left_value];
    CHECK(operand->kind == W_SEED_HIR0_VALUE_PARAMETER_READ &&
          operand->parameter_index < storage.hir_program.parameter_count);
    const w_seed_hir0_parameter *parameter =
        &storage.hir_program.parameters[operand->parameter_index];
    char expected_mlir[256];
    int expected_length = 0;
    if (value->unary_operator == W_SEED_HIR0_UNARY_NEGATE) {
      CHECK(value->kind == W_SEED_HIR0_VALUE_UNARY_I64 &&
            type->integer_is_signed);
      negate_count += 1u;
      negate_by_width[width_slot] += 1u;
      expected_length = snprintf(
          expected_mlir, sizeof(expected_mlir),
          "%%v%u_checked_width = llvm.mlir.constant(%u : i64) : i64\n",
          (unsigned int)value_index,
          (unsigned int)type->integer_bit_width);
      CHECK(expected_length > 0 && (size_t)expected_length <
                                      sizeof(expected_mlir) &&
            contains_bytes(artifact, result.mlir.written.mlir_bytes,
                           expected_mlir));
      expected_length = snprintf(
          expected_mlir, sizeof(expected_mlir),
          "%%v%u = llvm.call @w_seed_checked_subtract_i64(%%v%u_neg_zero, "
          "%%p%u, %%v%u_checked_width) : (i64, i64, i64) -> i64\n",
          (unsigned int)value_index, (unsigned int)value_index,
          (unsigned int)parameter->ordinal, (unsigned int)value_index);
      CHECK(expected_length > 0 && (size_t)expected_length <
                                      sizeof(expected_mlir) &&
            contains_bytes(artifact, result.mlir.written.mlir_bytes,
                           expected_mlir));
    } else {
      CHECK(value->unary_operator == W_SEED_HIR0_UNARY_BIT_NOT);
      if (type->integer_is_signed) {
        CHECK(value->kind == W_SEED_HIR0_VALUE_UNARY_I64);
        signed_bit_not_count += 1u;
        signed_bit_not_by_width[width_slot] += 1u;
      } else {
        CHECK(value->kind == W_SEED_HIR0_VALUE_UNARY_U64);
        unsigned_bit_not_count += 1u;
        unsigned_bit_not_by_width[width_slot] += 1u;
      }
      if (type->integer_bit_width == 64u) {
        expected_length = snprintf(
            expected_mlir, sizeof(expected_mlir),
            "%%v%u_bit_not_mask = llvm.mlir.constant(-1 : i64) : i64\n",
            (unsigned int)value_index);
        CHECK(expected_length > 0 && (size_t)expected_length <
                                        sizeof(expected_mlir) &&
              contains_bytes(artifact, result.mlir.written.mlir_bytes,
                             expected_mlir));
        expected_length = snprintf(
            expected_mlir, sizeof(expected_mlir),
            "%%v%u = llvm.xor %%p%u, %%v%u_bit_not_mask : i64\n",
            (unsigned int)value_index, (unsigned int)parameter->ordinal,
            (unsigned int)value_index);
        CHECK(expected_length > 0 && (size_t)expected_length <
                                        sizeof(expected_mlir) &&
              contains_bytes(artifact, result.mlir.written.mlir_bytes,
                             expected_mlir));
      } else {
        expected_length = snprintf(
            expected_mlir, sizeof(expected_mlir),
            "%%v%u_bit_not_narrow = llvm.trunc %%p%u : i64 to i%u\n",
            (unsigned int)value_index, (unsigned int)parameter->ordinal,
            (unsigned int)type->integer_bit_width);
        CHECK(expected_length > 0 && (size_t)expected_length <
                                        sizeof(expected_mlir) &&
              contains_bytes(artifact, result.mlir.written.mlir_bytes,
                             expected_mlir));
        expected_length = snprintf(
            expected_mlir, sizeof(expected_mlir),
            "%%v%u_bit_not_mask = llvm.mlir.constant(-1 : i%u) : i%u\n",
            (unsigned int)value_index,
            (unsigned int)type->integer_bit_width,
            (unsigned int)type->integer_bit_width);
        CHECK(expected_length > 0 && (size_t)expected_length <
                                        sizeof(expected_mlir) &&
              contains_bytes(artifact, result.mlir.written.mlir_bytes,
                             expected_mlir));
        expected_length = snprintf(
            expected_mlir, sizeof(expected_mlir),
            "%%v%u_bit_not_raw = llvm.xor %%v%u_bit_not_narrow, "
            "%%v%u_bit_not_mask : i%u\n",
            (unsigned int)value_index, (unsigned int)value_index,
            (unsigned int)value_index,
            (unsigned int)type->integer_bit_width);
        CHECK(expected_length > 0 && (size_t)expected_length <
                                        sizeof(expected_mlir) &&
              contains_bytes(artifact, result.mlir.written.mlir_bytes,
                             expected_mlir));
        expected_length = snprintf(
            expected_mlir, sizeof(expected_mlir),
            "%%v%u = llvm.%s %%v%u_bit_not_raw : i%u to i64\n",
            (unsigned int)value_index,
            type->integer_is_signed ? "sext" : "zext",
            (unsigned int)value_index,
            (unsigned int)type->integer_bit_width);
        CHECK(expected_length > 0 && (size_t)expected_length <
                                        sizeof(expected_mlir) &&
              contains_bytes(artifact, result.mlir.written.mlir_bytes,
                             expected_mlir));
      }
    }
  }
  const size_t mlir_bytes = result.mlir.written.mlir_bytes;
  CHECK(negate_count == 5u && signed_bit_not_count == 5u &&
        unsigned_bit_not_count == 5u &&
        negate_by_width[0] == 1u && negate_by_width[1] == 1u &&
        negate_by_width[2] == 1u && negate_by_width[3] == 2u &&
        signed_bit_not_by_width[0] == 1u &&
        signed_bit_not_by_width[1] == 1u &&
        signed_bit_not_by_width[2] == 1u &&
        signed_bit_not_by_width[3] == 2u &&
        unsigned_bit_not_by_width[0] == 1u &&
        unsigned_bit_not_by_width[1] == 1u &&
        unsigned_bit_not_by_width[2] == 1u &&
        unsigned_bit_not_by_width[3] == 2u &&
        count_bytes(artifact, mlir_bytes, "llvm.call @w_seed_checked_subtract_i64(") ==
            5u &&
        count_bytes(artifact, mlir_bytes, "llvm.xor ") == 10u &&
        count_bytes(artifact, mlir_bytes, "llvm.trunc ") >= 6u &&
        count_bytes(artifact, mlir_bytes, "llvm.sext ") >= 3u &&
        count_bytes(artifact, mlir_bytes, "llvm.zext ") >= 3u &&
        contains_bytes(artifact, mlir_bytes, "llvm.intr.trap"));
  return true;
}

static bool test_implicit_integer_widening_native(void) {
  static const uint8_t source[] =
      "fn signed(value: i8): i32 { return value }\n"
      "fn unsigned(value: u8): u32 { return value }\n"
      "fn cross(value: i16): i16 { return value }\n"
      "fn main() { let binding: i32 = 3_i8 "
      "let mixed: i32 = 4_i32 "
      "let signedValue = signed(value: 1_i8) "
      "let unsignedValue = unsigned(value: 2_u8) "
      "let crossValue = cross(value: 3_u8) "
      "print(\"${binding}/${mixed}/${signedValue}/${unsignedValue}/${crossValue}\") }\n"
      "entry(main)\n";
  static uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  const w_seed_native0_status status = run_source(
      source, sizeof(source) - 1u, "integer-widening-native", 22u, output,
      sizeof(output), &result);
  CHECK(status == W_SEED_NATIVE0_OK);
  const w_seed_hir0_program *program = &storage.hir_program;
  size_t widening_count = 0u;
  bool saw_i8_i32 = false;
  bool saw_u8_u32 = false;
  bool saw_u8_i16 = false;
  for (size_t index = 0u; index < program->value_count; index += 1u) {
    const w_seed_hir0_value *value = &program->values[index];
    if (value->kind != W_SEED_HIR0_VALUE_INTEGER_WIDEN) continue;
    CHECK(value->source_type < program->type_count &&
          value->type_index < program->type_count);
    const w_seed_hir0_type *source_type = &program->types[value->source_type];
    const w_seed_hir0_type *destination_type = &program->types[value->type_index];
    CHECK(source_type->integer_bit_width != 0u &&
          destination_type->integer_bit_width != 0u &&
          destination_type->integer_bit_width > source_type->integer_bit_width);
    widening_count += 1u;
    saw_i8_i32 |= source_type->integer_is_signed &&
                  source_type->integer_bit_width == 8u &&
                  destination_type->integer_is_signed &&
                  destination_type->integer_bit_width == 32u;
    saw_u8_u32 |= !source_type->integer_is_signed &&
                  source_type->integer_bit_width == 8u &&
                  !destination_type->integer_is_signed &&
                  destination_type->integer_bit_width == 32u;
    saw_u8_i16 |= !source_type->integer_is_signed &&
                  source_type->integer_bit_width == 8u &&
                  destination_type->integer_is_signed &&
                  destination_type->integer_bit_width == 16u;
  }
  CHECK(widening_count >= 4u && saw_i8_i32 && saw_u8_u32 && saw_u8_i16);
  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(
            program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  CHECK(count_bytes(output, result.mlir.written.mlir_bytes,
                    "llvm.trunc") >= widening_count &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "llvm.sext") >= 1u &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "llvm.zext") >= 2u);
  return true;
}

static bool test_numeric_widen_native(void) {
  static const uint8_t source[] =
      "fn signedF32(value: i16): f32 { return value }\n"
      "fn unsignedF64(value: u32): f64 { return value }\n"
      "fn widenF32(value: f32): f64 { return value }\n"
      "entry { let negative = signedF32(value: -32767_i16 - 1_i16) "
      "let positive = unsignedF64(value: 4294967295_u32) "
      "let directNegative: f32 = -32767_i16 - 1_i16 "
      "let directPositive: f64 = 4294967295_u32 "
      "let directUnsignedF32: f32 = 65535_u16 "
      "let directSignedF64: f64 = -32767_i16 - 1_i16 "
      "let directFloat: f64 = 1.5_f32 "
      "let parameterFloat = widenF32(value: 2.25_f32) "
      "if negative == -32768.0_f32 && positive == 4294967295.0_f64 && "
      "directNegative == -32768.0_f32 && "
      "directPositive == 4294967295.0_f64 && "
      "directUnsignedF32 == 65535.0_f32 && "
      "directSignedF64 == -32768.0_f64 && "
      "directFloat == 1.5_f64 && parameterFloat == 2.25_f64 { "
      "print(\"Numeric widen ok\") } else { "
      "print(\"Numeric widen bad\") } }\n";
  static uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  static const uint8_t sequence_source[] =
      "entry { let value: f64 = 1.5_f32 print(\"Sequence widen ok\") }\n";
  CHECK(run_source(sequence_source, sizeof(sequence_source) - 1u,
                   "numeric-widen-sequence",
                   sizeof("numeric-widen-sequence") - 1u, output,
                   sizeof(output), &result) == W_SEED_NATIVE0_OK);
  CHECK(result.mlir.written.mlir_bytes != 0u &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "llvm.fpext %v") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes, "fastmath"));
  const w_seed_native0_status status =
      run_source(source, sizeof(source) - 1u, "numeric-widen-native",
                 sizeof("numeric-widen-native") - 1u, output, sizeof(output),
                 &result);
  CHECK(status == W_SEED_NATIVE0_OK);
  CHECK(result.mlir.written.mlir_bytes == result.mlir.required.mlir_bytes &&
        result.mlir.written.mlir_bytes != 0u);
  CHECK(contains_bytes(output, result.mlir.written.mlir_bytes,
                       "llvm.sitofp") &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "llvm.uitofp") &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "llvm.fpext") &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "_numeric_widen_bits = llvm.trunc") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes, "fastmath") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "w_seed_numeric_widen"));

  const w_seed_hir0_program *program = &storage.hir_program;
  uint32_t wrapper_index = W_SEED_HIR0_NONE;
  size_t wrapper_count = 0u;
  for (size_t index = 0u; index < program->value_count; index += 1u) {
    const w_seed_hir0_value *value = &program->values[index];
    if (value->kind != W_SEED_HIR0_VALUE_NUMERIC_WIDEN) continue;
    CHECK(value->source_type < program->type_count &&
          value->type_index < program->type_count &&
          value->left_value < program->value_count &&
          value->right_value == W_SEED_HIR0_NONE);
    const w_seed_hir0_value *child = &program->values[value->left_value];
    CHECK(child->type_index == value->source_type &&
          child->owner_kind == W_SEED_HIR0_VALUE_OWNER_NUMERIC_WIDEN &&
          child->owner_index == index && child->owner_ordinal == 0u);
    if (wrapper_index == W_SEED_HIR0_NONE) wrapper_index = (uint32_t)index;
    wrapper_count += 1u;
  }
  CHECK(wrapper_count >= 8u && wrapper_index != W_SEED_HIR0_NONE);
  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(
            program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);

  const w_seed_hir0_value saved_wrapper = storage.hir_values[wrapper_index];
  const w_seed_hir0_value saved_child =
      storage.hir_values[saved_wrapper.left_value];
  storage.hir_values[wrapper_index].source_type = saved_wrapper.type_index;
  CHECK(!w_seed_hir0_verify(program, &storage.hir_result) &&
        w_seed_native_subset0_select_program(
            program, &storage.hir_result, &selection) ==
            W_SEED_NATIVE_SUBSET0_INVALID);
  storage.hir_values[wrapper_index] = saved_wrapper;
  CHECK(w_seed_hir0_verify(program, &storage.hir_result));

  storage.hir_values[wrapper_index].type_index = saved_child.type_index;
  CHECK(!w_seed_hir0_verify(program, &storage.hir_result) &&
        w_seed_native_subset0_select_program(
            program, &storage.hir_result, &selection) ==
            W_SEED_NATIVE_SUBSET0_INVALID);
  storage.hir_values[wrapper_index] = saved_wrapper;
  CHECK(w_seed_hir0_verify(program, &storage.hir_result));

  storage.hir_values[saved_wrapper.left_value].type_index =
      saved_wrapper.type_index;
  CHECK(!w_seed_hir0_verify(program, &storage.hir_result) &&
        w_seed_native_subset0_select_program(
            program, &storage.hir_result, &selection) ==
            W_SEED_NATIVE_SUBSET0_INVALID);
  storage.hir_values[saved_wrapper.left_value] = saved_child;
  CHECK(w_seed_hir0_verify(program, &storage.hir_result));

  storage.hir_values[saved_wrapper.left_value].owner_kind =
      W_SEED_HIR0_VALUE_OWNER_ARGUMENT;
  CHECK(!w_seed_hir0_verify(program, &storage.hir_result) &&
        w_seed_native_subset0_select_program(
            program, &storage.hir_result, &selection) ==
            W_SEED_NATIVE_SUBSET0_INVALID);
  storage.hir_values[saved_wrapper.left_value] = saved_child;
  CHECK(w_seed_hir0_verify(program, &storage.hir_result));
  return true;
}

static bool test_explicit_integer_truncating_bits_native(void) {
  typedef struct {
    bool source_signed;
    uint16_t source_width;
    bool destination_signed;
    uint16_t destination_width;
  } integer_route;
  static const integer_route ROUTES[] = {
      {true, 16u, true, 8u}, {true, 8u, true, 16u},
      {false, 8u, true, 8u}, {true, 64u, false, 64u},
      {false, 64u, true, 64u}};
  static const uint8_t source[] =
      "fn narrow(value: i16): i8 { return i8(truncatingBits: value) }\n"
      "fn widen(value: i8): i16 { return i16(truncatingBits: value) }\n"
      "fn reinterpret(value: u8): i8 { return i8(truncatingBits: value) }\n"
      "fn intToUInt(value: Int): UInt { return UInt(truncatingBits: value) }\n"
      "fn uintToInt(value: UInt): Int { return Int(truncatingBits: value) }\n"
      "fn main() { let narrowResult = narrow(value: 258_i16) "
      "let widenResult = widen(value: -7_i8) "
      "let reinterpretResult = reinterpret(value: 250_u8) "
      "let intToUIntResult = intToUInt(value: -7) "
      "let uintToIntResult = uintToInt(value: 18446744073709551615_u64) "
      "print(\"Trunc ${narrowResult}/${widenResult}/${reinterpretResult}/"
      "${intToUIntResult}/${uintToIntResult}\") }\n"
      "entry(main)\n";
  static uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  CHECK(run_source(source, sizeof(source) - 1u, "integer-truncating-bits",
                   sizeof("integer-truncating-bits") - 1u, output,
                   sizeof(output), &result) == W_SEED_NATIVE0_OK);
  CHECK(result.mlir.written.mlir_bytes == result.mlir.required.mlir_bytes &&
        result.mlir.written.mlir_bytes != 0u);

  const w_seed_hir0_program *program = &storage.hir_program;
  bool route_seen[sizeof(ROUTES) / sizeof(ROUTES[0])] = {false};
  size_t wrapper_count = 0u;
  uint32_t forged_wrapper = W_SEED_HIR0_NONE;
  for (size_t value_index = 0u; value_index < program->value_count;
       value_index += 1u) {
    const w_seed_hir0_value *value = &program->values[value_index];
    if (value->kind != W_SEED_HIR0_VALUE_INTEGER_TRUNCATING_BITS) continue;
    CHECK(value->source_type < program->type_count &&
          value->type_index < program->type_count &&
          value->left_value < program->value_count &&
          value->right_value == W_SEED_HIR0_NONE);
    const w_seed_hir0_type *source_type = &program->types[value->source_type];
    const w_seed_hir0_type *destination_type =
        &program->types[value->type_index];
    const w_seed_hir0_value *child = &program->values[value->left_value];
    CHECK(source_type->kind == W_SEED_HIR0_TYPE_INTEGER ||
          source_type->kind == W_SEED_HIR0_TYPE_I64 ||
          source_type->kind == W_SEED_HIR0_TYPE_U64);
    CHECK(destination_type->kind == W_SEED_HIR0_TYPE_INTEGER ||
          destination_type->kind == W_SEED_HIR0_TYPE_I64 ||
          destination_type->kind == W_SEED_HIR0_TYPE_U64);
    CHECK(child->kind == W_SEED_HIR0_VALUE_PARAMETER_READ &&
          child->type_index == value->source_type &&
          child->parameter_index < program->parameter_count &&
          child->owner_kind ==
              W_SEED_HIR0_VALUE_OWNER_INTEGER_TRUNCATING_BITS &&
          child->owner_index == value_index && child->owner_ordinal == 0u);
    const w_seed_hir0_parameter *parameter =
        &program->parameters[child->parameter_index];
    CHECK(parameter->type_index == value->source_type &&
          parameter->ordinal == 0u);
    size_t route_index = 0u;
    while (route_index < sizeof(ROUTES) / sizeof(ROUTES[0]) &&
           (ROUTES[route_index].source_signed !=
                source_type->integer_is_signed ||
            ROUTES[route_index].source_width !=
                source_type->integer_bit_width ||
            ROUTES[route_index].destination_signed !=
                destination_type->integer_is_signed ||
            ROUTES[route_index].destination_width !=
                destination_type->integer_bit_width))
      route_index += 1u;
    CHECK(route_index < sizeof(ROUTES) / sizeof(ROUTES[0]) &&
          !route_seen[route_index]);
    route_seen[route_index] = true;
    wrapper_count += 1u;
    if (forged_wrapper == W_SEED_HIR0_NONE &&
        value->source_type != value->type_index)
      forged_wrapper = (uint32_t)value_index;

    char expected[192];
    int written = 0;
    if (source_type->integer_bit_width < 64u) {
      written = snprintf(
          expected, sizeof(expected),
          "%%v%u_truncating_source_bits = llvm.trunc %%p%u : i64 to i%u\n",
          (unsigned)value_index, (unsigned)parameter->ordinal,
          (unsigned)source_type->integer_bit_width);
      CHECK(written > 0 && (size_t)written < sizeof(expected) &&
            contains_bytes(output, result.mlir.written.mlir_bytes, expected));
      written = snprintf(
          expected, sizeof(expected),
          "%%v%u_truncating_source = llvm.%s %%v%u_truncating_source_bits : "
          "i%u to i64\n",
          (unsigned)value_index,
          source_type->integer_is_signed ? "sext" : "zext",
          (unsigned)value_index, (unsigned)source_type->integer_bit_width);
      CHECK(written > 0 && (size_t)written < sizeof(expected) &&
            contains_bytes(output, result.mlir.written.mlir_bytes, expected));
    }
    if (destination_type->integer_bit_width < 64u) {
      if (source_type->integer_bit_width < 64u) {
        written = snprintf(
            expected, sizeof(expected),
            "%%v%u_truncating_destination_bits = llvm.trunc %%v%u_"
            "truncating_source : i64 to i%u\n",
            (unsigned)value_index, (unsigned)value_index,
            (unsigned)destination_type->integer_bit_width);
      } else {
        written = snprintf(
            expected, sizeof(expected),
            "%%v%u_truncating_destination_bits = llvm.trunc %%p%u : i64 to "
            "i%u\n",
            (unsigned)value_index, (unsigned)parameter->ordinal,
            (unsigned)destination_type->integer_bit_width);
      }
      CHECK(written > 0 && (size_t)written < sizeof(expected) &&
            contains_bytes(output, result.mlir.written.mlir_bytes, expected));
      written = snprintf(
          expected, sizeof(expected),
          "%%v%u = llvm.%s %%v%u_truncating_destination_bits : i%u to i64\n",
          (unsigned)value_index,
          destination_type->integer_is_signed ? "sext" : "zext",
          (unsigned)value_index,
          (unsigned)destination_type->integer_bit_width);
    } else {
      if (source_type->integer_bit_width < 64u) {
        written = snprintf(expected, sizeof(expected),
                           "%%v%u_truncating_zero = llvm.mlir.constant(0 : "
                           "i64) : i64\n    %%v%u = llvm.or %%v%u_"
                           "truncating_source, %%v%u_truncating_zero : i64\n",
                           (unsigned)value_index,
                           (unsigned)value_index, (unsigned)value_index,
                           (unsigned)value_index);
      } else {
        written = snprintf(expected, sizeof(expected),
                           "%%v%u_truncating_zero = llvm.mlir.constant(0 : "
                           "i64) : i64\n    %%v%u = llvm.or %%p%u, %%v%u_"
                           "truncating_zero : i64\n",
                           (unsigned)value_index, (unsigned)value_index,
                           (unsigned)parameter->ordinal,
                           (unsigned)value_index);
      }
    }
    CHECK(written > 0 && (size_t)written < sizeof(expected) &&
          contains_bytes(output, result.mlir.written.mlir_bytes, expected));
  }
  CHECK(wrapper_count == sizeof(ROUTES) / sizeof(ROUTES[0]) &&
        route_seen[0] && route_seen[1] && route_seen[2] && route_seen[3] &&
        route_seen[4] && forged_wrapper != W_SEED_HIR0_NONE &&
        program->call_count == sizeof(ROUTES) / sizeof(ROUTES[0]) + 1u);
  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(program, &storage.hir_result,
                                             &selection) ==
            W_SEED_NATIVE_SUBSET0_OK &&
        selection.has_local_calls);

  const w_seed_mlir0_input mlir_input = {
      .program = program,
      .hir_result = &storage.hir_result,
      .artifact_kind = W_SEED_MLIR0_ARTIFACT_EXECUTABLE};
  const w_seed_hir0_value saved_wrapper = storage.hir_values[forged_wrapper];
  w_seed_mlir0_counts forged_counts;
  w_seed_mlir0_result forged_result;
  (void)memset(&forged_counts, 0x31, sizeof(forged_counts));
  (void)memset(&forged_result, 0x42, sizeof(forged_result));
  const w_seed_mlir0_counts counts_snapshot = forged_counts;
  const w_seed_mlir0_result result_snapshot = forged_result;
  storage.hir_values[forged_wrapper].source_type =
      saved_wrapper.type_index;
  CHECK(w_seed_mlir0_measure(&mlir_input, &TARGET, &forged_counts,
                             &forged_result) == W_SEED_MLIR0_INVALID_HIR &&
        memcmp(&forged_counts, &counts_snapshot, sizeof(forged_counts)) == 0 &&
        memcmp(&forged_result, &result_snapshot, sizeof(forged_result)) == 0);
  uint8_t rejected_output[64];
  (void)memset(rejected_output, 0xa5, sizeof(rejected_output));
  uint8_t output_snapshot[sizeof(rejected_output)];
  (void)memcpy(output_snapshot, rejected_output, sizeof(output_snapshot));
  const w_seed_mlir0_output forged_output = {
      rejected_output, sizeof(rejected_output)};
  CHECK(w_seed_mlir0_emit(&mlir_input, &TARGET, &forged_output,
                          &forged_result) == W_SEED_MLIR0_INVALID_HIR &&
        memcmp(rejected_output, output_snapshot, sizeof(rejected_output)) == 0 &&
        memcmp(&forged_result, &result_snapshot, sizeof(forged_result)) == 0);
  storage.hir_values[forged_wrapper] = saved_wrapper;
  CHECK(w_seed_hir0_verify(program, &storage.hir_result));

  storage.hir_values[forged_wrapper].type_index =
      saved_wrapper.source_type;
  CHECK(w_seed_mlir0_measure(&mlir_input, &TARGET, &forged_counts,
                             &forged_result) == W_SEED_MLIR0_INVALID_HIR);
  storage.hir_values[forged_wrapper] = saved_wrapper;
  CHECK(w_seed_hir0_verify(program, &storage.hir_result));
  return true;
}

static bool test_explicit_integer_saturating_native(void) {
  static const uint8_t source[] =
      "fn signedToSigned(value: i16): i8 { "
      "return i8(saturating: value) }\n"
      "fn unsignedToSigned(value: u16): i8 { "
      "return i8(saturating: value) }\n"
      "fn signedToUnsigned(value: i16): u8 { "
      "return u8(saturating: value) }\n"
      "fn unsignedToUnsigned(value: u16): u8 { "
      "return u8(saturating: value) }\n"
      "fn aliasToAlias(value: UInt): Int { "
      "return Int(saturating: value) }\n"
      "fn main() { let a = signedToSigned(value: -200_i16) "
      "let b = unsignedToSigned(value: 300_u16) "
      "let c = signedToUnsigned(value: -1_i16) "
      "let d = unsignedToUnsigned(value: 300_u16) "
      "let e = aliasToAlias(value: 18446744073709551615_u64) "
      "print(\"${a}/${b}/${c}/${d}/${e}\") }\n"
      "entry(main)\n";
  static uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  const w_seed_native0_status status = run_source(
      source, sizeof(source) - 1u, "integer-saturating",
      sizeof("integer-saturating") - 1u, output, sizeof(output), &result);
  if (status != W_SEED_NATIVE0_OK)
    (void)fprintf(stderr,
                   "saturating native status=%d frontend=%d hir=%d mlir=%d\n",
                   (int)status, (int)storage.frontend_result.status,
                   (int)storage.hir_result.status, (int)result.mlir.status);
  CHECK(status == W_SEED_NATIVE0_OK);
  CHECK(result.mlir.written.mlir_bytes == result.mlir.required.mlir_bytes &&
        result.mlir.written.mlir_bytes != 0u);

  const w_seed_hir0_program *program = &storage.hir_program;
  size_t wrapper_count = 0u;
  for (size_t value_index = 0u; value_index < program->value_count;
       value_index += 1u) {
    const w_seed_hir0_value *value = &program->values[value_index];
    if (value->kind != W_SEED_HIR0_VALUE_INTEGER_SATURATING) continue;
    CHECK(value->source_type < program->type_count &&
          value->type_index < program->type_count &&
          value->left_value < program->value_count &&
          value->right_value == W_SEED_HIR0_NONE);
    const w_seed_hir0_type *source_type = &program->types[value->source_type];
    const w_seed_hir0_type *destination_type =
        &program->types[value->type_index];
    const w_seed_hir0_value *child = &program->values[value->left_value];
    CHECK(child->type_index == value->source_type &&
          child->owner_kind == W_SEED_HIR0_VALUE_OWNER_INTEGER_SATURATING &&
          child->owner_index == value_index && child->owner_ordinal == 0u);
    CHECK((source_type->integer_bit_width == 16u ||
           source_type->integer_bit_width == 64u) &&
          (destination_type->integer_bit_width == 8u ||
           destination_type->integer_bit_width == 64u));
    wrapper_count += 1u;
  }
  CHECK(wrapper_count == 5u);
  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(program, &storage.hir_result,
                                             &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  CHECK(selection.has_local_calls);
  CHECK(count_bytes(output, result.mlir.written.mlir_bytes,
                    "_saturating_low_test = llvm.icmp \"slt\"") == 2u &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "_saturating_high_test = llvm.icmp \"sgt\"") == 1u &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "_saturating_high_test = llvm.icmp \"ugt\"") == 4u &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "_saturating_low_clamp = llvm.select") == 2u &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "_saturating_high_clamp = llvm.select") == 5u &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "_saturating_source_bits = llvm.trunc") == 4u &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "_saturating_min = llvm.mlir.constant") == 2u &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "_saturating_max = llvm.mlir.constant") == 5u &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "w_seed_saturating"));

  const w_seed_mlir0_input mlir_input = {
      .program = program,
      .hir_result = &storage.hir_result,
      .artifact_kind = W_SEED_MLIR0_ARTIFACT_EXECUTABLE};
  uint32_t forged_wrapper = W_SEED_HIR0_NONE;
  for (size_t value_index = 0u; value_index < program->value_count;
       value_index += 1u)
    if (program->values[value_index].kind ==
        W_SEED_HIR0_VALUE_INTEGER_SATURATING) {
      forged_wrapper = (uint32_t)value_index;
      break;
    }
  CHECK(forged_wrapper != W_SEED_HIR0_NONE);
  const w_seed_hir0_value saved_wrapper = storage.hir_values[forged_wrapper];
  w_seed_mlir0_counts forged_counts;
  w_seed_mlir0_result forged_result;
  storage.hir_values[forged_wrapper].source_type = saved_wrapper.type_index;
  CHECK(w_seed_mlir0_measure(&mlir_input, &TARGET, &forged_counts,
                             &forged_result) == W_SEED_MLIR0_INVALID_HIR);
  storage.hir_values[forged_wrapper] = saved_wrapper;
  CHECK(w_seed_hir0_verify(program, &storage.hir_result));
  return true;
}

static bool test_scalar_if_value_native(void) {
  static const uint8_t source[] =
      "fn serve(isOpen: Bool, openCount: i64, closedCount: i64): i64 { "
      "return if isOpen { openCount } else { closedCount } }\n"
      "fn main() { let open = serve(isOpen: true, openCount: 5, "
      "closedCount: 2) let closed = serve(isOpen: false, openCount: 5, "
      "closedCount: 2) print(\"Open ${open}; closed ${closed}\") }\n"
      "entry(main)\n";
  static uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  CHECK(run_source(source, sizeof(source) - 1u, "scalar-if", 20u,
                   output, sizeof(output), &result) == W_SEED_NATIVE0_OK);
  CHECK(result.status == W_SEED_NATIVE0_OK &&
        result.mlir.written.mlir_bytes == result.mlir.required.mlir_bytes &&
        result.mlir.written.mlir_bytes != 0u);
  const w_seed_hir0_program *program = &storage.hir_program;
  CHECK(program->function_count == 2u && program->block_count == 5u &&
        program->block_argument_count == 1u &&
        program->functions[0].block_count == 4u &&
        program->functions[1].block_count == 1u &&
        program->terminators[0].result_type == W_SEED_HIR0_TYPE_I64 &&
        program->blocks[3].block_argument_count == 1u &&
        program->block_arguments[0].type_index == W_SEED_HIR0_TYPE_I64);
  CHECK(contains_bytes(output, result.mlir.written.mlir_bytes,
                       "llvm.func internal @w_fn_0(%buffer: !llvm.ptr, "
                       "%cursor_address: !llvm.ptr, %p0: i1, %p1: i64, "
                       "%p2: i64) -> i64") &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "llvm.cond_br %p0, ^w_fn_0_b_1, ^w_fn_0_b_2") &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "llvm.br ^w_fn_0_b_3(%p1 : i64)") == 1u &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "llvm.br ^w_fn_0_b_3(%p2 : i64)") == 1u &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "@w_seed_append_i64(%buffer, %cursor"));
  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(program, &storage.hir_result,
                                             &selection) ==
            W_SEED_NATIVE_SUBSET0_OK);
  /* Each runtime i64 interpolation reserves 20 bytes (the signed decimal
   * spelling bound); the fixed text is 5 + 9 bytes and print appends one LF.
   * The concrete calls below produce 17 bytes, so the selector's 55-byte
   * maximum is an upper bound rather than a prediction of those constants. */
  const size_t interpolation_upper_bound = 5u + 20u + 9u + 20u;
  const size_t print_line_upper_bound = interpolation_upper_bound + 1u;
  const size_t concrete_stdout_bytes = sizeof("Open 5; closed 2\n") - 1u;
  CHECK(selection.has_cfg && selection.has_local_calls &&
        selection.has_interpolation &&
        selection.maximum_stdout_bytes == print_line_upper_bound &&
        selection.maximum_stdout_bytes >= concrete_stdout_bytes);
  return true;
}

static bool test_nested_scalar_if_value_native(void) {
  static const uint8_t source[] =
      "fn choose(outer: Bool, inner: Bool, open: i64, middle: i64, "
      "closed: i64): i64 { return if outer { if inner { open } else { "
      "middle } } else { closed } }\n"
      "fn main() { let first = choose(outer: true, inner: true, open: 1, "
      "middle: 2, closed: 3) let second = choose(outer: true, inner: false, "
      "open: 1, middle: 2, closed: 3) let third = choose(outer: false, "
      "inner: false, open: 1, middle: 2, closed: 3) "
      "print(\"${first},${second},${third}\") }\n"
      "entry(main)\n";
  static uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  CHECK(run_source(source, sizeof(source) - 1u, "nested-scalar-if",
                   28u, output, sizeof(output), &result) == W_SEED_NATIVE0_OK);
  CHECK(result.status == W_SEED_NATIVE0_OK &&
        result.mlir.written.mlir_bytes == result.mlir.required.mlir_bytes &&
        result.mlir.written.mlir_bytes != 0u &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "llvm.cond_br") >= 2u &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "llvm.call @w_fn_0") == 3u &&
        !contains_bytes(output, result.mlir.written.mlir_bytes, "1,2,3"));
  const w_seed_hir0_program *program = &storage.hir_program;
  CHECK(program->function_count == 2u && program->block_count == 8u &&
        program->block_argument_count == 2u &&
        program->functions[0].block_count == 7u &&
        program->functions[1].block_count == 1u &&
        program->terminators[0].kind == W_SEED_HIR0_TERMINATOR_BRANCH &&
        program->terminators[1].kind == W_SEED_HIR0_TERMINATOR_BRANCH &&
        program->terminators[0].result_type == W_SEED_HIR0_TYPE_I64 &&
        program->terminators[1].result_type == W_SEED_HIR0_TYPE_I64 &&
        program->blocks[4].block_argument_count == 1u &&
        program->blocks[6].block_argument_count == 1u &&
        program->block_arguments[0].type_index == W_SEED_HIR0_TYPE_I64 &&
        program->block_arguments[1].type_index == W_SEED_HIR0_TYPE_I64);
  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(program, &storage.hir_result,
                                             &selection) ==
            W_SEED_NATIVE_SUBSET0_OK);
  CHECK(selection.has_cfg && selection.has_local_calls &&
        selection.has_interpolation && selection.has_bool &&
        selection.maximum_stdout_bytes == 63u);

  const w_seed_hir0_terminator saved_outer = storage.hir_terminators[0];
  storage.hir_terminators[0].target_block = 2u;
  CHECK(w_seed_native_subset0_select_program(
            program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_INVALID);
  storage.hir_terminators[0] = saved_outer;
  CHECK(w_seed_native_subset0_select_program(program, &storage.hir_result,
                                             &selection) ==
            W_SEED_NATIVE_SUBSET0_OK);
  return true;
}

static bool test_terminal_branch_returns_native(void) {
  static const uint8_t source[] =
      "fn sign(value: i64): i64 { if value < 0 { return -1 } "
      "if value == 0 { return 0 } return 1 }\n"
      "fn main() { let negative = sign(value: -5) "
      "let zero = sign(value: 0) let positive = sign(value: 7) "
      "print(\"${negative},${zero},${positive}\") }\n"
      "entry(main)\n";
  static uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  CHECK(run_source(source, sizeof(source) - 1u, "terminal-returns", 16u,
                   output, sizeof(output), &result) == W_SEED_NATIVE0_OK);
  const w_seed_hir0_program *program = &storage.hir_program;
  CHECK(program->function_count == 2u && program->block_count == 6u &&
        program->functions[0].block_count == 5u &&
        program->functions[1].block_count == 1u &&
        program->terminators[0].kind == W_SEED_HIR0_TERMINATOR_BRANCH &&
        program->terminators[1].kind == W_SEED_HIR0_TERMINATOR_RETURN_VALUE &&
        program->terminators[2].kind == W_SEED_HIR0_TERMINATOR_BRANCH &&
        program->terminators[3].kind == W_SEED_HIR0_TERMINATOR_RETURN_VALUE &&
        program->terminators[4].kind == W_SEED_HIR0_TERMINATOR_RETURN_VALUE);
  CHECK(count_bytes(output, result.mlir.written.mlir_bytes,
                    "llvm.cond_br %v") == 2u &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "^w_fn_0_b_1, ^w_fn_0_b_2") &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "^w_fn_0_b_3, ^w_fn_0_b_4") &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "llvm.return %v") == 3u &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "llvm.call @w_fn_0") == 3u &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "-1,0,1"));
  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(program, &storage.hir_result,
                                             &selection) ==
            W_SEED_NATIVE_SUBSET0_OK);
  CHECK(selection.has_cfg && selection.has_local_calls &&
        selection.has_interpolation && selection.maximum_stdout_bytes == 63u);

  const w_seed_hir0_terminator saved = storage.hir_terminators[0];
  const w_seed_native_subset0_program selection_snapshot = selection;
  storage.hir_terminators[0].else_block =
      storage.hir_terminators[0].target_block;
  CHECK(w_seed_native_subset0_select_program(program, &storage.hir_result,
                                             &selection) ==
        W_SEED_NATIVE_SUBSET0_INVALID);
  CHECK(memcmp(&selection, &selection_snapshot, sizeof(selection)) == 0);
  storage.hir_terminators[0] = saved;
  CHECK(w_seed_native_subset0_select_program(program, &storage.hir_result,
                                             &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  return true;
}

static bool test_missing_terminal_return_remains_unsupported(void) {
  static const uint8_t source[] =
      "fn invalid(left: Bool): Bool { if left { return true } }\n"
      "fn main() { print(\"invalid\") }\n"
      "entry(main)\n";
  static uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  (void)memset(output, 0x6au, sizeof(output));
  (void)memset(&result, 0x7bu, sizeof(result));
  const w_seed_native0_result snapshot = result;
  CHECK(run_source(source, sizeof(source) - 1u, "scalar-if", 9u, output,
                   sizeof(output), &result) != W_SEED_NATIVE0_OK);
  CHECK(memcmp(&result, &snapshot, sizeof(result)) == 0);
  for (size_t byte = 0u; byte < sizeof(output); byte += 1u)
    CHECK(output[byte] == 0x6au);
  return true;
}

static bool test_direct_scalar_call_return_remains_unsupported(void) {
  static const uint8_t source[] =
      "fn value(): i64 { return 42 }\n"
      "fn relay(): i64 { return value() }\n"
      "fn main() { print(\"relay\") }\n"
      "entry(main)\n";
  static uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  (void)memset(output, 0x4cu, sizeof(output));
  (void)memset(&result, 0x5du, sizeof(result));
  const w_seed_native0_result snapshot = result;
  CHECK(run_source(source, sizeof(source) - 1u, "direct-call-return", 18u,
                   output, sizeof(output), &result) != W_SEED_NATIVE0_OK);
  CHECK(memcmp(&result, &snapshot, sizeof(result)) == 0);
  for (size_t byte = 0u; byte < sizeof(output); byte += 1u)
    CHECK(output[byte] == 0x4cu);
  return true;
}

static bool test_nested_depth_and_linear_analysis(void) {
  static char depth_source[W_SEED_NATIVE0_MAX_SOURCE_BYTES];
  static uint8_t depth_output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result depth_result;
  CHECK(make_nested_chain_source(depth_source, sizeof(depth_source) - 1u,
                                 W_SEED_HIR0_MAX_NESTING));
  const w_seed_native0_status depth64_status = run_source(
      (const uint8_t *)depth_source, strlen(depth_source), "depth64-id", 10u,
      depth_output, sizeof(depth_output), &depth_result);
  CHECK(depth64_status == W_SEED_NATIVE0_OK);
  CHECK(storage.hir_program.functions[0].block_count ==
            W_SEED_NATIVE0_HIR_BLOCKS_PER_FUNCTION &&
        storage.hir_program.block_count ==
            W_SEED_NATIVE0_HIR_BLOCKS_PER_FUNCTION &&
        storage.hir_program.value_count ==
            W_SEED_HIR0_MAX_NESTING + 1u &&
        contains_bytes(depth_output, depth_result.mlir.written.mlir_bytes,
                       "^w_fn_0_b_192:") &&
        contains_bytes(depth_output, depth_result.mlir.written.mlir_bytes,
                       "\\78\\0a"));

  CHECK(make_nested_tree_source(depth_source, sizeof(depth_source) - 1u, 7u));
  CHECK(run_source((const uint8_t *)depth_source, strlen(depth_source),
                   "tree-id", 7u, depth_output, sizeof(depth_output),
                   &depth_result) == W_SEED_NATIVE0_OK);
  /* 2^7 - 1 nested diamonds are a structural stress witness. */
  CHECK(storage.hir_program.block_count == 382u &&
        storage.hir_program.value_count == 128u &&
        contains_bytes(depth_output, depth_result.mlir.written.mlir_bytes,
                       "^w_fn_0_b_381:") &&
        count_bytes(depth_output, depth_result.mlir.written.mlir_bytes,
                    "llvm.cond_br") >= 127u);

  CHECK(make_nested_chain_source(depth_source, sizeof(depth_source) - 1u,
                                 W_SEED_HIR0_MAX_NESTING + 1u));
  (void)memset(depth_output, 0xa5u, sizeof(depth_output));
  (void)memset(&depth_result, 0x5au, sizeof(depth_result));
  const w_seed_native0_result snapshot = depth_result;
  CHECK(run_source((const uint8_t *)depth_source, strlen(depth_source),
                   "depth65-id", 10u, depth_output, sizeof(depth_output),
                   &depth_result) == W_SEED_NATIVE0_UNSUPPORTED);
  CHECK(memcmp(&depth_result, &snapshot, sizeof(snapshot)) == 0);
  for (size_t index = 0u; index < sizeof(depth_output); index += 1u)
    CHECK(depth_output[index] == 0xa5u);
  return true;
}

static bool test_failures_and_capacity(void) {
  static const uint8_t hello[] =
      "fn main() { print(\"Hello, world!\") }\nentry(main)\n";
  static uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  CHECK(run_source(hello, sizeof(hello) - 1u, "capacity-id", 11u, output,
                   sizeof(output), &result) == W_SEED_NATIVE0_OK);
  const size_t required = result.mlir.required.mlir_bytes;
  CHECK(required > 1u);

  (void)memset(output, 0xa5u, sizeof(output));
  (void)memset(&result, 0x5au, sizeof(result));
  const w_seed_native0_result result_snapshot = result;
  CHECK(run_source(hello, sizeof(hello) - 1u, "capacity-id", 11u, output,
                   required - 1u, &result) == W_SEED_NATIVE0_CAPACITY);
  for (size_t index = 0u; index < sizeof(output); index += 1u)
    CHECK(output[index] == 0xa5u);
  CHECK(memcmp(&result, &result_snapshot, sizeof(result)) == 0);

  (void)memset(output, 0x4au, sizeof(output));
  (void)memset(&result, 0x4bu, sizeof(result));
  CHECK(run_source(hello, sizeof(hello) - 1u, "capacity-id", 11u, output,
                   required, &result) == W_SEED_NATIVE0_OK);
  CHECK(result.mlir.required.mlir_bytes == required &&
        result.mlir.written.mlir_bytes == required);
  for (size_t index = required; index < sizeof(output); index += 1u)
    CHECK(output[index] == 0x4au);

  (void)memset(output, 0x3cu, sizeof(output));
  (void)memset(&result, 0x6du, sizeof(result));
  const w_seed_native0_result null_result_snapshot = result;
  CHECK(run_source(hello, sizeof(hello) - 1u, "capacity-id", 11u, NULL, 0u,
                   &result) == W_SEED_NATIVE0_CAPACITY);
  for (size_t index = 0u; index < sizeof(output); index += 1u)
    CHECK(output[index] == 0x3cu);
  CHECK(memcmp(&result, &null_result_snapshot, sizeof(result)) == 0);

  static const uint8_t empty[] = {0u};
  (void)memset(output, 0x44u, sizeof(output));
  (void)memset(&result, 0x55u, sizeof(result));
  const w_seed_native0_result empty_snapshot = result;
  CHECK(run_source(empty, 0u, "empty-id", 8u, output, sizeof(output),
                   &result) == W_SEED_NATIVE0_SOURCE);
  for (size_t index = 0u; index < sizeof(output); index += 1u)
    CHECK(output[index] == 0x44u);
  CHECK(memcmp(&result, &empty_snapshot, sizeof(result)) == 0);

  uint8_t oversized[W_SEED_NATIVE0_MAX_SOURCE_BYTES + 1u];
  (void)memset(oversized, (int)'x', sizeof(oversized));
  (void)memset(output, 0x66u, sizeof(output));
  (void)memset(&result, 0x77u, sizeof(result));
  const w_seed_native0_result oversized_snapshot = result;
  CHECK(run_source(oversized, sizeof(oversized), "oversized-id", 12u, output,
                   sizeof(output), &result) == W_SEED_NATIVE0_SOURCE);
  for (size_t index = 0u; index < sizeof(output); index += 1u)
    CHECK(output[index] == 0x66u);
  CHECK(memcmp(&result, &oversized_snapshot, sizeof(result)) == 0);

  static const uint8_t invalid_utf8[] = {0xc3u};
  (void)memset(output, 0x88u, sizeof(output));
  (void)memset(&result, 0x99u, sizeof(result));
  const w_seed_native0_result invalid_snapshot = result;
  CHECK(run_source(invalid_utf8, sizeof(invalid_utf8), "invalid-id", 10u,
                   output, sizeof(output), &result) == W_SEED_NATIVE0_SOURCE);
  for (size_t index = 0u; index < sizeof(output); index += 1u)
    CHECK(output[index] == 0x88u);
  CHECK(memcmp(&result, &invalid_snapshot, sizeof(result)) == 0);

  static const char missing_path[] = "w_seed_native0_missing.w";
  (void)remove(missing_path);
  const w_seed_native0_input missing_input = {
      missing_path,
      sizeof(missing_path) - 1u,
      {"missing-id", 10u},
      TARGET,
      W_SEED_MLIR0_ARTIFACT_EXECUTABLE};
  (void)memset(output, 0x8au, sizeof(output));
  (void)memset(&result, 0x8bu, sizeof(result));
  const w_seed_native0_result missing_snapshot = result;
  CHECK(w_seed_native0_run(
            &missing_input, &storage,
            &(w_seed_native0_output){output, sizeof(output)}, &result) ==
        W_SEED_NATIVE0_SOURCE);
  for (size_t index = 0u; index < sizeof(output); index += 1u)
    CHECK(output[index] == 0x8au);
  CHECK(memcmp(&result, &missing_snapshot, sizeof(result)) == 0);

  static const uint8_t incomplete[] = "fn main( {";
  (void)memset(output, 0xb1u, sizeof(output));
  (void)memset(&result, 0xb2u, sizeof(result));
  const w_seed_native0_result incomplete_snapshot = result;
  CHECK(run_source(incomplete, sizeof(incomplete) - 1u, "parse-id", 8u,
                   output, sizeof(output), &result) == W_SEED_NATIVE0_PARSE);
  for (size_t index = 0u; index < sizeof(output); index += 1u)
    CHECK(output[index] == 0xb1u);
  CHECK(memcmp(&result, &incomplete_snapshot, sizeof(result)) == 0);

  static const uint8_t unsupported[] =
      "fn main() { noop() }\nentry(main)\n";
  (void)memset(output, 0xc1u, sizeof(output));
  (void)memset(&result, 0xc2u, sizeof(result));
  const w_seed_native0_result unsupported_snapshot = result;
  CHECK(run_source(unsupported, sizeof(unsupported) - 1u, "unsupported-id",
                   14u, output, sizeof(output), &result) ==
        W_SEED_NATIVE0_UNSUPPORTED);
  for (size_t index = 0u; index < sizeof(output); index += 1u)
    CHECK(output[index] == 0xc1u);
  CHECK(memcmp(&result, &unsupported_snapshot, sizeof(result)) == 0);

  static const uint8_t interpolation[] =
      "fn main() { print(\"The answer is ${6 * 7}\") }\nentry(main)\n";
  (void)memset(output, 0xd1u, sizeof(output));
  (void)memset(&result, 0xd2u, sizeof(result));
  CHECK(run_source(interpolation, sizeof(interpolation) - 1u,
                   "interpolation-id", 16u, output, sizeof(output), &result) ==
        W_SEED_NATIVE0_OK);
  CHECK(result.mlir.written.mlir_bytes == result.mlir.required.mlir_bytes);
  CHECK(storage.hir_program.value_count == 4u &&
        storage.hir_program.interpolation_segment_count == 2u);
  CHECK(contains_bytes(output, result.mlir.written.mlir_bytes,
                       "llvm.call @w_seed_checked_multiply_i64(%v0, %v1, "
                       "%v2_checked_width) : (i64, i64, i64) -> i64"));
  CHECK(contains_bytes(output, result.mlir.written.mlir_bytes,
                       "llvm.call @w_seed_append_i64"));
  CHECK(!contains_bytes(output, result.mlir.written.mlir_bytes,
                        "snprintf"));

  return true;
}

static bool test_aliases(void) {
  static const uint8_t hello[] =
      "fn main() { print(\"Hello\") }\nentry(main)\n";
  uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  CHECK(run_source(hello, sizeof(hello) - 1u, "alias-id", 8u, output,
                   sizeof(output), &result) == W_SEED_NATIVE0_OK);
  const w_seed_native0_result snapshot = result;
  const w_seed_native0_input input = {
      TEST_PATH,
      sizeof(TEST_PATH) - 1u,
      {"alias-id", 8u},
      TARGET,
      W_SEED_MLIR0_ARTIFACT_EXECUTABLE};
  union {
    w_seed_native0_output output;
    w_seed_native0_result result;
  } descriptor_and_result = {{output, sizeof(output)}};
  const w_seed_native0_output descriptor_snapshot =
      descriptor_and_result.output;
  CHECK(w_seed_native0_run(&input, &storage, &descriptor_and_result.output,
                           &descriptor_and_result.result) ==
        W_SEED_NATIVE0_MLIR);
  CHECK(memcmp(&descriptor_and_result.output, &descriptor_snapshot,
               sizeof(descriptor_snapshot)) == 0);
  CHECK(memcmp(&result, &snapshot, sizeof(result)) == 0);

  w_seed_native0_result alias_snapshot;
  (void)memset(&alias_snapshot, 0xd2u, sizeof(alias_snapshot));
  const w_seed_native0_result alias_result_snapshot = alias_snapshot;
  static uint8_t output_snapshot[W_SEED_MLIR0_MAX_BYTES];
  (void)memcpy(output_snapshot, output, sizeof(output_snapshot));
  CHECK(w_seed_native0_run(
            &input, &storage,
            &(w_seed_native0_output){(uint8_t *)&storage, 1u},
            &alias_snapshot) == W_SEED_NATIVE0_MLIR);
  CHECK(memcmp(&alias_snapshot, &alias_result_snapshot,
               sizeof(alias_snapshot)) == 0);
  CHECK(memcmp(output, output_snapshot, sizeof(output_snapshot)) == 0);

  (void)memset(&alias_snapshot, 0xc3u, sizeof(alias_snapshot));
  const w_seed_native0_result bytes_result_snapshot = alias_snapshot;
  CHECK(w_seed_native0_run(
            &input, &storage,
            &(w_seed_native0_output){(uint8_t *)&alias_snapshot,
                                     sizeof(alias_snapshot)},
            &alias_snapshot) == W_SEED_NATIVE0_MLIR);
  CHECK(memcmp(&alias_snapshot, &bytes_result_snapshot,
               sizeof(alias_snapshot)) == 0);

  char path_alias[] = "w_seed_native0_path_alias.w";
  const w_seed_native0_input path_input = {
      path_alias,
      sizeof(path_alias) - 1u,
      {"path-alias", 10u},
      TARGET,
      W_SEED_MLIR0_ARTIFACT_EXECUTABLE};
  (void)memset(&alias_snapshot, 0xe1u, sizeof(alias_snapshot));
  const w_seed_native0_result path_snapshot = alias_snapshot;
  CHECK(w_seed_native0_run(
            &path_input, &storage,
            &(w_seed_native0_output){(uint8_t *)path_alias, 1u},
            &alias_snapshot) == W_SEED_NATIVE0_MLIR);
  CHECK(memcmp(path_alias, "w_seed_native0_path_alias.w",
               sizeof(path_alias) - 1u) == 0);
  CHECK(memcmp(&alias_snapshot, &path_snapshot, sizeof(alias_snapshot)) == 0);

  char identity_alias[] = "identity-alias";
  const w_seed_native0_input identity_input = {
      TEST_PATH,
      sizeof(TEST_PATH) - 1u,
      {identity_alias, sizeof(identity_alias) - 1u},
      TARGET,
      W_SEED_MLIR0_ARTIFACT_EXECUTABLE};
  (void)memset(&alias_snapshot, 0xf1u, sizeof(alias_snapshot));
  const w_seed_native0_result identity_snapshot = alias_snapshot;
  CHECK(w_seed_native0_run(
            &identity_input, &storage,
            &(w_seed_native0_output){(uint8_t *)identity_alias, 1u},
            &alias_snapshot) == W_SEED_NATIVE0_MLIR);
  CHECK(memcmp(identity_alias, "identity-alias",
               sizeof(identity_alias) - 1u) == 0);
  CHECK(memcmp(&alias_snapshot, &identity_snapshot,
               sizeof(alias_snapshot)) == 0);

  static union {
    w_seed_native0_input input;
    w_seed_native0_storage storage;
  } input_storage_alias;
  input_storage_alias.input = (w_seed_native0_input){
      TEST_PATH,
      sizeof(TEST_PATH) - 1u,
      {"input-storage", 13u},
      TARGET,
      W_SEED_MLIR0_ARTIFACT_EXECUTABLE};
  (void)memset(output, 0x31u, sizeof(output));
  (void)memset(&alias_snapshot, 0x32u, sizeof(alias_snapshot));
  const w_seed_native0_result input_storage_result_snapshot = alias_snapshot;
  const w_seed_native0_output input_storage_output = {output, sizeof(output)};
  static uint8_t input_storage_output_snapshot[W_SEED_MLIR0_MAX_BYTES];
  (void)memcpy(input_storage_output_snapshot, output,
               sizeof(input_storage_output_snapshot));
  CHECK(w_seed_native0_run(
            &input_storage_alias.input,
            (w_seed_native0_storage *)&input_storage_alias,
            &input_storage_output, &alias_snapshot) == W_SEED_NATIVE0_MLIR);
  CHECK(memcmp(output, input_storage_output_snapshot,
               sizeof(input_storage_output_snapshot)) == 0);
  CHECK(memcmp(&alias_snapshot, &input_storage_result_snapshot,
               sizeof(alias_snapshot)) == 0);

  union {
    w_seed_native0_input input;
    w_seed_native0_result result;
  } input_result_alias = {{
      TEST_PATH,
      sizeof(TEST_PATH) - 1u,
      {"input-result", 12u},
      TARGET,
      W_SEED_MLIR0_ARTIFACT_EXECUTABLE}};
  (void)memset(output, 0x41u, sizeof(output));
  (void)memset(&alias_snapshot, 0x42u, sizeof(alias_snapshot));
  const w_seed_native0_result input_result_snapshot = alias_snapshot;
  uint8_t input_result_output_snapshot[W_SEED_MLIR0_MAX_BYTES];
  (void)memcpy(input_result_output_snapshot, output,
               sizeof(input_result_output_snapshot));
  const w_seed_native0_output input_result_output = {output, sizeof(output)};
  CHECK(w_seed_native0_run(&input_result_alias.input, &storage,
                           &input_result_output,
                           &input_result_alias.result) ==
        W_SEED_NATIVE0_MLIR);
  CHECK(memcmp(output, input_result_output_snapshot,
               sizeof(input_result_output_snapshot)) == 0);
  CHECK(memcmp(&alias_snapshot, &input_result_snapshot,
               sizeof(alias_snapshot)) == 0);

  static union {
    w_seed_native0_output output;
    w_seed_native0_storage storage;
  } output_storage_alias;
  output_storage_alias.output =
      (w_seed_native0_output){output, sizeof(output)};
  const w_seed_native0_input output_storage_input = {
      TEST_PATH,
      sizeof(TEST_PATH) - 1u,
      {"output-storage", 15u},
      TARGET,
      W_SEED_MLIR0_ARTIFACT_EXECUTABLE};
  const w_seed_native0_output output_storage_snapshot =
      output_storage_alias.output;
  (void)memset(&alias_snapshot, 0x52u, sizeof(alias_snapshot));
  const w_seed_native0_result output_storage_result_snapshot = alias_snapshot;
  CHECK(w_seed_native0_run(
            &output_storage_input,
            (w_seed_native0_storage *)&output_storage_alias,
            &output_storage_alias.output, &alias_snapshot) ==
        W_SEED_NATIVE0_MLIR);
  CHECK(memcmp(&output_storage_alias.output, &output_storage_snapshot,
               sizeof(output_storage_snapshot)) == 0);
  CHECK(memcmp(&alias_snapshot, &output_storage_result_snapshot,
               sizeof(alias_snapshot)) == 0);

  (void)memcpy(storage.source_bytes, "alias.w", 8u);
  (void)memcpy(storage.const_bytes, "alias-id", 8u);
  const uint8_t storage_path_snapshot[8] = {0};
  const uint8_t storage_id_snapshot[8] = {0};
  (void)memcpy((void *)storage_path_snapshot, storage.source_bytes, 8u);
  (void)memcpy((void *)storage_id_snapshot, storage.const_bytes, 8u);
  const w_seed_native0_input path_storage_input = {
      (const char *)storage.source_bytes,
      7u,
      {(const char *)storage.const_bytes, 8u},
      TARGET,
      W_SEED_MLIR0_ARTIFACT_EXECUTABLE};
  (void)memset(&alias_snapshot, 0x62u, sizeof(alias_snapshot));
  const w_seed_native0_result path_storage_result_snapshot = alias_snapshot;
  CHECK(w_seed_native0_run(
            &path_storage_input, &storage,
            &(w_seed_native0_output){output, sizeof(output)},
            &alias_snapshot) == W_SEED_NATIVE0_MLIR);
  CHECK(memcmp(storage.source_bytes, storage_path_snapshot, 8u) == 0 &&
        memcmp(storage.const_bytes, storage_id_snapshot, 8u) == 0);
  CHECK(memcmp(&alias_snapshot, &path_storage_result_snapshot,
               sizeof(alias_snapshot)) == 0);
  return true;
}

static bool test_signed_comparison_products(void) {
  static const uint8_t source[] =
      "fn admit(guests: i64, seats: i64) { "
      "if guests <= seats { print(\"Seat party\") } "
      "else { print(\"Waitlist\") } }\n"
      "fn main() { admit(guests: 3, seats: 4) admit(guests: 4, seats: 4) "
      "admit(guests: 5, seats: 4) }\nentry(main)\n";
  uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  const w_seed_native0_status comparison_status = run_source(
      source, sizeof(source) - 1u, "comparison", 10u,
      output, sizeof(output), &result);
  CHECK(comparison_status == W_SEED_NATIVE0_OK);
  CHECK(contains_bytes(output, result.mlir.written.mlir_bytes,
                        "llvm.icmp \"sle\" %p0, %p1 : i64"));
  CHECK(contains_bytes(output, result.mlir.written.mlir_bytes, "llvm.cond_br %v"));
  static const char *const rejected[] = {
      "fn main() { print(\"${true == false}\") }\nentry(main)\n",
      "fn main() { let same = \"a\" == \"b\" print(\"${same}\") }\nentry(main)\n",
      "fn main() { let same = 3 == true print(\"${same}\") }\nentry(main)\n",
      ("fn main() { let x = 9223372036854775807 + 1 "
       "print(\"${x > 0}\") }\nentry(main)\n"),
      "fn main() { let x = 1 / 0 print(\"${x > 0}\") }\nentry(main)\n",
  };
  for (size_t index = 0u; index < sizeof(rejected) / sizeof(rejected[0]); index += 1u) {
    (void)memset(output, 0x51, sizeof(output));
    (void)memset(&result, 0x52, sizeof(result));
    const w_seed_native0_result snapshot = result;
    const w_seed_native0_status status = run_source(
        (const uint8_t *)rejected[index], strlen(rejected[index]), "comparison",
        10u, output, sizeof(output), &result);
    CHECK(status != W_SEED_NATIVE0_OK);
    CHECK(memcmp(&result, &snapshot, sizeof(result)) == 0);
    for (size_t byte = 0u; byte < sizeof(output); byte += 1u)
      CHECK(output[byte] == 0x51u);
  }
  return true;
}

static bool test_integer_comparison_family_capacity(void) {
  FILE *file = fopen(W_SEED_INTEGER_COMPARISON_FIXTURE_PATH, "rb");
  CHECK(file != NULL);
  uint8_t source[W_SEED_NATIVE0_MAX_SOURCE_BYTES + 1u] = {0u};
  const size_t source_length =
      fread(source, sizeof(uint8_t), sizeof(source), file);
  const bool read_ok = !ferror(file) && fclose(file) == 0 &&
                       source_length > 0u &&
                       source_length <= W_SEED_NATIVE0_MAX_SOURCE_BYTES;
  CHECK(read_ok);

  static uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result native_result = {0};
  CHECK(run_source(source, source_length, "integer-comparison-capacity", 27u,
                   artifact, sizeof(artifact), &native_result) ==
        W_SEED_NATIVE0_OK);
  CHECK(native_result.source_bytes == source_length &&
        native_result.mlir.required.mlir_bytes != 0u &&
        native_result.mlir.written.mlir_bytes ==
            native_result.mlir.required.mlir_bytes);
  CHECK(storage.frontend_result.written.expressions > 256u &&
        storage.frontend_result.written.expressions <
            W_SEED_NATIVE0_EXPRESSIONS &&
        storage.frontend_result.written.symbols > 64u &&
        storage.frontend_result.written.symbols < W_SEED_NATIVE0_SYMBOLS &&
        storage.hir_program.value_count > 256u &&
        storage.hir_program.value_count <= W_SEED_NATIVE0_HIR_VALUE_RECORDS &&
        storage.hir_program.binding_count > 32u &&
        storage.hir_program.instruction_count <=
            W_SEED_NATIVE_SUBSET0_MAX_INSTRUCTIONS &&
        storage.hir_program.interpolation_segment_count <=
            W_SEED_NATIVE_SUBSET0_MAX_INTERPOLATION_SEGMENTS);

  w_seed_frontend_counts frontend_counts = {0};
  w_seed_frontend_result frontend_measure = {0};
  CHECK(w_seed_frontend_measure(&storage.input, &frontend_counts,
                                &frontend_measure) == W_SEED_FRONTEND_OK);
  CHECK(frontend_counts.expressions ==
            storage.frontend_result.written.expressions &&
        frontend_counts.symbols == storage.frontend_result.written.symbols);
  const size_t full_expression_capacity = storage.output.expression_capacity;
  const size_t full_symbol_capacity = storage.output.symbol_capacity;

  storage.output.expression_capacity = frontend_counts.expressions;
  storage.output.symbol_capacity = frontend_counts.symbols;
  CHECK(w_seed_frontend_run(&storage.input, &storage.output,
                            &storage.frontend_result) == W_SEED_FRONTEND_OK);
  storage.output.expression_capacity = frontend_counts.expressions - 1u;
  CHECK(w_seed_frontend_run(&storage.input, &storage.output,
                            &storage.frontend_result) ==
        W_SEED_FRONTEND_CAPACITY);
  CHECK(storage.frontend_result.required.expressions ==
        frontend_counts.expressions);
  storage.output.expression_capacity = frontend_counts.expressions;
  storage.output.symbol_capacity = frontend_counts.symbols - 1u;
  CHECK(w_seed_frontend_run(&storage.input, &storage.output,
                            &storage.frontend_result) ==
        W_SEED_FRONTEND_CAPACITY);
  CHECK(storage.frontend_result.required.symbols == frontend_counts.symbols);
  storage.output.expression_capacity = full_expression_capacity;
  storage.output.symbol_capacity = full_symbol_capacity;
  CHECK(w_seed_frontend_run(&storage.input, &storage.output,
                            &storage.frontend_result) == W_SEED_FRONTEND_OK);

  const w_seed_hir0_input hir_input = {
      .frontend_input = &storage.input,
      .frontend_output = &storage.output,
      .frontend_result = &storage.frontend_result,
      .execution_profile = W_SEED_HIR0_EXECUTION_PROFILE_NORMAL};
  w_seed_hir0_counts hir_counts = {0};
  w_seed_hir0_result hir_measure = {0};
  CHECK(w_seed_hir0_measure(&hir_input, &hir_counts, &hir_measure) ==
        W_SEED_HIR0_OK);
  CHECK(hir_counts.values == storage.hir_program.value_count &&
        hir_counts.values > 256u && hir_counts.values <
            W_SEED_NATIVE0_HIR_VALUE_RECORDS);
  const size_t full_value_capacity = storage.hir_output.value_capacity;
  storage.hir_output.value_capacity = hir_counts.values;
  CHECK(w_seed_hir0_run(&hir_input, &storage.hir_output,
                        &storage.hir_result) == W_SEED_HIR0_OK);
  storage.hir_output.value_capacity = hir_counts.values - 1u;
  CHECK(w_seed_hir0_run(&hir_input, &storage.hir_output,
                        &storage.hir_result) == W_SEED_HIR0_CAPACITY);
  CHECK(storage.hir_result.required.values == hir_counts.values);
  storage.hir_output.value_capacity = full_value_capacity;
  return true;
}

static bool test_unsigned_binary_u64_slice(void) {
  static const uint8_t source[] =
      "fn add(left: UInt, right: UInt): UInt { return left + right }\n"
      "fn subtract(left: UInt, right: UInt): UInt { return left - right }\n"
      "fn multiply(left: UInt, right: UInt): UInt { return left * right }\n"
      "fn divide(left: UInt, right: UInt): UInt { return left / right }\n"
      "fn remainder(left: UInt, right: UInt): UInt { return left % right }\n"
      "fn equal(left: UInt, right: UInt): Bool { return left == right }\n"
      "fn notEqual(left: UInt, right: UInt): Bool { return left != right }\n"
      "fn less(left: UInt, right: UInt): Bool { return left < right }\n"
      "fn lessEqual(left: UInt, right: UInt): Bool { return left <= right }\n"
      "fn greater(left: UInt, right: UInt): Bool { return left > right }\n"
      "fn greaterEqual(left: UInt, right: UInt): Bool { return left >= right }\n"
      "fn main() { let high = 9223372036854775808_u64 "
      "let maximum = 18446744073709551615_u64 "
      "let sum = add(left: high, right: 2_u64) "
      "let difference = subtract(left: sum, right: 1_u64) "
      "let product = multiply(left: 3_u64, right: 7_u64) "
      "let quotient = divide(left: 23_u64, right: 3_u64) "
      "let remaining = remainder(left: 23_u64, right: 3_u64) "
      "let eq = equal(left: high, right: high) "
      "let ne = notEqual(left: high, right: 9223372036854775807_u64) "
      "let lt = less(left: high, right: maximum) "
      "let le = lessEqual(left: high, right: high) "
      "let gt = greater(left: maximum, right: high) "
      "let ge = greaterEqual(left: high, right: high) "
      "print(\"${sum} ${difference} ${product} ${quotient} ${remaining} "
      "${eq} ${ne} ${lt} ${le} ${gt} ${ge}\") }\n"
      "entry(main)\n";
  uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  CHECK(run_source(source, sizeof(source) - 1u, "uint-binary", 11u, output,
                   sizeof(output), &result) == W_SEED_NATIVE0_OK);
  size_t binary_u64_count = 0u;
  size_t integer_comparison_count = 0u;
  for (size_t index = 0u; index < storage.hir_program.value_count;
       index += 1u) {
    if (storage.hir_program.values[index].kind ==
        W_SEED_HIR0_VALUE_BINARY_U64)
      binary_u64_count += 1u;
    if (storage.hir_program.values[index].kind ==
        W_SEED_HIR0_VALUE_BINARY_INTEGER_COMPARISON)
      integer_comparison_count += 1u;
  }
  CHECK(binary_u64_count == 5u && integer_comparison_count == 6u);
  CHECK(contains_bytes(output, result.mlir.written.mlir_bytes,
                       "llvm.mlir.constant(-9223372036854775808 : i64) : i64") &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "llvm.mlir.constant(-1 : i64) : i64"));
  CHECK(count_bytes(output, result.mlir.written.mlir_bytes,
                    "llvm.call @w_seed_checked_add_u64") == 1u &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "llvm.call @w_seed_checked_subtract_u64") == 1u &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "llvm.call @w_seed_checked_multiply_u64") == 1u &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "llvm.call @w_seed_checked_divide_u64") == 1u &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "llvm.call @w_seed_checked_remainder_u64") == 1u);
  CHECK(contains_bytes(output, result.mlir.written.mlir_bytes,
                       "llvm.icmp \"eq\"") &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "llvm.icmp \"ne\"") &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "llvm.icmp \"ult\"") &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "llvm.icmp \"ule\"") &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "llvm.icmp \"ugt\"") &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "llvm.icmp \"uge\""));
  CHECK(!contains_bytes(output, result.mlir.written.mlir_bytes,
                        "@w_seed_checked_add_i64") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "@w_seed_checked_subtract_i64") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "@w_seed_checked_multiply_i64") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "@w_seed_checked_divide_i64") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "@w_seed_checked_remainder_i64") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "llvm.intr.sadd.with.overflow") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "llvm.intr.ssub.with.overflow") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "llvm.intr.smul.with.overflow") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "llvm.icmp \"slt\"") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "llvm.icmp \"sle\"") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "llvm.icmp \"sgt\"") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "llvm.icmp \"sge\"") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "@w_seed_append_i64"));

  static const uint8_t rejected_constants[][192] = {
      "fn main() { let x = 18446744073709551615_u64 + 1_u64 "
      "print(\"${x}\") }\nentry(main)\n",
      "fn main() { let x = 0_u64 - 1_u64 print(\"${x}\") }\n"
      "entry(main)\n",
      "fn main() { let x = 18446744073709551615_u64 * 2_u64 "
      "print(\"${x}\") }\nentry(main)\n",
      "fn main() { let x = 1_u64 / 0_u64 print(\"${x}\") }\n"
      "entry(main)\n",
      "fn main() { let x = 1_u64 % 0_u64 print(\"${x}\") }\n"
      "entry(main)\n"};
  for (size_t index = 0u;
       index < sizeof(rejected_constants) / sizeof(rejected_constants[0]);
       index += 1u) {
    (void)memset(output, 0x71u, sizeof(output));
    (void)memset(&result, 0x72u, sizeof(result));
    const w_seed_native0_result snapshot = result;
    CHECK(run_source(rejected_constants[index],
                     strlen((const char *)rejected_constants[index]),
                     "uint-rejected", 13u, output, sizeof(output), &result) !=
          W_SEED_NATIVE0_OK);
    CHECK(memcmp(&result, &snapshot, sizeof(result)) == 0);
    for (size_t byte = 0u; byte < sizeof(output); byte += 1u)
      CHECK(output[byte] == 0x71u);
  }

  static const uint8_t cfg_source[] =
      "fn choose(flag: Bool): UInt { return if flag { 1_u64 + 2_u64 } "
      "else { 3_u64 } }\n"
      "fn main() { let value = choose(flag: true) print(\"${value}\") }\n"
      "entry(main)\n";
  (void)memset(output, 0x81u, sizeof(output));
  (void)memset(&result, 0x82u, sizeof(result));
  const w_seed_native0_result cfg_snapshot = result;
  CHECK(run_source(cfg_source, sizeof(cfg_source) - 1u, "uint-cfg", 8u,
                   output, sizeof(output), &result) != W_SEED_NATIVE0_OK);
  CHECK(memcmp(&result, &cfg_snapshot, sizeof(result)) == 0);
  for (size_t byte = 0u; byte < sizeof(output); byte += 1u)
    CHECK(output[byte] == 0x81u);
  return true;
}

static bool test_u64_wrapping_add_slice(void) {
  static const uint8_t source[] =
      "fn wrap(value: UInt): UInt { return u64.wrappingAdd(value, 1_u64) }\n"
      "fn main() { let result = wrap(value: 18446744073709551615_u64) "
      "print(\"${result}\") }\n"
      "entry(main)\n";
  uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  CHECK(run_source(source, sizeof(source) - 1u, "uint-wrapping", 13u,
                   output, sizeof(output), &result) == W_SEED_NATIVE0_OK);
  size_t wrapping_count = 0u;
  for (size_t index = 0u; index < storage.hir_program.value_count;
       index += 1u)
    if (storage.hir_program.values[index].kind ==
            W_SEED_HIR0_VALUE_BINARY_U64 &&
        storage.hir_program.values[index].binary_operator ==
            W_SEED_HIR0_BINARY_WRAPPING_ADD)
      wrapping_count += 1u;
  CHECK(wrapping_count == 1u);
  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(
            &storage.hir_program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  const size_t wrapper_start =
      find_bytes(output, result.mlir.written.mlir_bytes,
                 "llvm.func internal @w_fn_0", 0u);
  const size_t entry_start =
      find_bytes(output, result.mlir.written.mlir_bytes,
                 "llvm.func internal @w_fn_1", wrapper_start);
  CHECK(selection.has_local_calls && !selection.has_cfg &&
        wrapper_start != SIZE_MAX && entry_start != SIZE_MAX &&
        count_bytes(output + wrapper_start, entry_start - wrapper_start,
                    "llvm.add ") == wrapping_count &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "llvm.call @w_seed_checked_add_u64") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "llvm.intr.uadd.with.overflow"));

  static const uint8_t rejected[][192] = {
      "fn bad(value: UInt): UInt { return UInt.wrappingAdd(value, 1_u64) }\n"
      "entry(bad)\n",
      "fn bad(value: UInt): UInt { return u64.wrappingAdd(value, 1_i64) }\n"
      "entry(bad)\n",
      "fn bad(value: UInt): UInt { return u64.wrappingAdd(value) }\n"
      "entry(bad)\n",
      "fn bad(value: UInt): UInt { return u64.wrappingAdd(value, 1_u64, "
      "2_u64) }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return u64.wrappingAdd(left: value, "
      "1_u64) }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return u64?.wrappingAdd(value, 1_u64) }\n"
      "entry(bad)\n",
      "fn bad(value: UInt): UInt { return u64.wrappingAdd("
      "u64.wrappingAdd(value, 1_u64), 1_u64) }\nentry(bad)\n",
      "const u64: UInt = 1_u64\n"
      "fn bad(value: UInt): UInt { return u64.wrappingAdd(value, 1_u64) }\n"
      "entry(bad)\n"};
  for (size_t index = 0u; index < sizeof(rejected) / sizeof(rejected[0]);
       index += 1u) {
    (void)memset(output, 0x91u, sizeof(output));
    (void)memset(&result, 0x92u, sizeof(result));
    const w_seed_native0_result snapshot = result;
    CHECK(run_source(rejected[index], strlen((const char *)rejected[index]),
                     "uint-wrapping-bad", 16u, output, sizeof(output),
                     &result) != W_SEED_NATIVE0_OK);
    CHECK(memcmp(&result, &snapshot, sizeof(result)) == 0);
    for (size_t byte = 0u; byte < sizeof(output); byte += 1u)
      CHECK(output[byte] == 0x91u);
  }
  return true;
}

static bool test_u64_saturating_add_slice(void) {
  static const uint8_t source[] =
      "fn clamp(left: UInt, right: UInt): UInt { return "
      "u64.saturatingAdd(left, right) }\n"
      "fn main() { let maximum = clamp(left: 18446744073709551615_u64, "
      "right: 1_u64) let ordinary = clamp(left: 7_u64, right: 5_u64) "
      "print(\"${maximum}/${ordinary}\") }\nentry(main)\n";
  uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  CHECK(run_source(source, sizeof(source) - 1u, "uint-saturating-add", 19u,
                   output, sizeof(output), &result) == W_SEED_NATIVE0_OK);
  size_t saturating_count = 0u;
  for (size_t index = 0u; index < storage.hir_program.value_count;
       index += 1u)
    if (storage.hir_program.values[index].kind ==
            W_SEED_HIR0_VALUE_BINARY_U64 &&
        storage.hir_program.values[index].binary_operator ==
            W_SEED_HIR0_BINARY_SATURATING_ADD)
      saturating_count += 1u;
  CHECK(saturating_count == 1u && storage.hir_program.call_count == 3u);
  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(
            &storage.hir_program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  CHECK(selection.has_local_calls && !selection.has_cfg &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "llvm.intr.uadd.sat") == 1u &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "@w_seed_checked_add_u64") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "\"llvm.intr.trap\"() : () -> ()"));

  static const uint8_t rejected[][208] = {
      "fn bad(value: UInt): UInt { return "
      "UInt.saturatingAdd(value, 1_u64) }\nentry(bad)\n",
      "fn bad(value: Int): UInt { return "
      "u64.saturatingAdd(value, 1_u64) }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return "
      "u64.saturatingAdd(value, 1_i64) }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return "
      "u64.saturatingAdd(value) }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return "
      "u64.saturatingAdd(value, 1_u64, 2_u64) }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return "
      "u64.saturatingAdd(left: value, 1_u64) }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return "
      "u64?.saturatingAdd(value, 1_u64) }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return u64.saturatingAdd("
      "u64.saturatingAdd(value, 1_u64), 1_u64) }\nentry(bad)\n",
      "const u64: UInt = 1_u64\n"
      "fn bad(value: UInt): UInt { return "
      "u64.saturatingAdd(value, 1_u64) }\nentry(bad)\n",
      "fn bad(value: UInt): Int { return "
      "u64.saturatingAdd(value, 1_u64) }\nentry(bad)\n"};
  for (size_t index = 0u; index < sizeof(rejected) / sizeof(rejected[0]);
       index += 1u) {
    (void)memset(output, 0x93u, sizeof(output));
    (void)memset(&result, 0x94u, sizeof(result));
    const w_seed_native0_result snapshot = result;
    CHECK(run_source(rejected[index], strlen((const char *)rejected[index]),
                     "uint-saturating-add-bad", 23u, output, sizeof(output),
                     &result) != W_SEED_NATIVE0_OK);
    CHECK(memcmp(&result, &snapshot, sizeof(result)) == 0);
    for (size_t byte = 0u; byte < sizeof(output); byte += 1u)
      CHECK(output[byte] == 0x93u);
  }
  return true;
}

static bool test_u64_saturating_subtract_slice(void) {
  static const uint8_t source[] =
      "fn clamp(left: UInt, right: UInt): UInt { return "
      "u64.saturatingSubtract(left, right) }\n"
      "fn main() { let zero = clamp(left: 0_u64, right: 1_u64) "
      "let ordinary = clamp(left: 12_u64, right: 5_u64) "
      "print(\"${zero}/${ordinary}\") }\nentry(main)\n";
  uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  CHECK(run_source(source, sizeof(source) - 1u, "uint-saturating-subtract",
                   24u, output, sizeof(output), &result) == W_SEED_NATIVE0_OK);
  size_t saturating_count = 0u;
  for (size_t index = 0u; index < storage.hir_program.value_count;
       index += 1u)
    if (storage.hir_program.values[index].kind ==
            W_SEED_HIR0_VALUE_BINARY_U64 &&
        storage.hir_program.values[index].binary_operator ==
            W_SEED_HIR0_BINARY_SATURATING_SUBTRACT)
      saturating_count += 1u;
  CHECK(saturating_count == 1u && storage.hir_program.call_count == 3u);
  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(
            &storage.hir_program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  CHECK(selection.has_local_calls && !selection.has_cfg &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "llvm.intr.usub.sat") == 1u &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "@w_seed_checked_subtract_u64") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "\"llvm.intr.trap\"() : () -> ()"));

  static const uint8_t rejected[][224] = {
      "fn bad(value: UInt): UInt { return "
      "UInt.saturatingSubtract(value, 1_u64) }\nentry(bad)\n",
      "fn bad(value: Int): UInt { return "
      "u64.saturatingSubtract(value, 1_u64) }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return "
      "u64.saturatingSubtract(value, 1_i64) }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return "
      "u64.saturatingSubtract(value) }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return "
      "u64.saturatingSubtract(value, 1_u64, 2_u64) }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return "
      "u64.saturatingSubtract(left: value, 1_u64) }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return "
      "u64?.saturatingSubtract(value, 1_u64) }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return u64.saturatingSubtract("
      "u64.saturatingSubtract(value, 1_u64), 1_u64) }\nentry(bad)\n",
      "const u64: UInt = 1_u64\n"
      "fn bad(value: UInt): UInt { return "
      "u64.saturatingSubtract(value, 1_u64) }\nentry(bad)\n",
      "fn bad(value: UInt): Int { return "
      "u64.saturatingSubtract(value, 1_u64) }\nentry(bad)\n"};
  for (size_t index = 0u; index < sizeof(rejected) / sizeof(rejected[0]);
       index += 1u) {
    (void)memset(output, 0x95u, sizeof(output));
    (void)memset(&result, 0x96u, sizeof(result));
    const w_seed_native0_result snapshot = result;
    CHECK(run_source(rejected[index], strlen((const char *)rejected[index]),
                     "uint-saturating-subtract-bad", 28u, output,
                     sizeof(output), &result) != W_SEED_NATIVE0_OK);
    CHECK(memcmp(&result, &snapshot, sizeof(result)) == 0);
    for (size_t byte = 0u; byte < sizeof(output); byte += 1u)
      CHECK(output[byte] == 0x95u);
  }
  return true;
}

static bool test_u64_saturating_multiply_slice(void) {
  static const uint8_t source[] =
      "fn clamp(left: UInt, right: UInt): UInt { return "
      "u64.saturatingMultiply(left, right) }\n"
      "fn main() { let maximum = clamp(left: 18446744073709551615_u64, "
      "right: 2_u64) let ordinary = clamp(left: 6_u64, right: 7_u64) "
      "print(\"${maximum}/${ordinary}\") }\nentry(main)\n";
  uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  CHECK(run_source(source, sizeof(source) - 1u, "uint-saturating-multiply",
                   24u, output, sizeof(output), &result) == W_SEED_NATIVE0_OK);
  size_t saturating_count = 0u;
  for (size_t index = 0u; index < storage.hir_program.value_count;
       index += 1u)
    if (storage.hir_program.values[index].kind ==
            W_SEED_HIR0_VALUE_BINARY_U64 &&
        storage.hir_program.values[index].binary_operator ==
            W_SEED_HIR0_BINARY_SATURATING_MULTIPLY)
      saturating_count += 1u;
  CHECK(saturating_count == 1u && storage.hir_program.call_count == 3u);
  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(
            &storage.hir_program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  CHECK(selection.has_local_calls && !selection.has_cfg &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "llvm.intr.umul.with.overflow") == 1u &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "llvm.extractvalue") == 2u &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "llvm.select") &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "llvm.mlir.constant(-1 : i64)") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "@w_seed_checked_multiply_u64") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "\"llvm.intr.umul.sat\"") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "\"llvm.intr.trap\"() : () -> ()"));

  static const uint8_t rejected[][224] = {
      "fn bad(value: UInt): UInt { return "
      "UInt.saturatingMultiply(value, 2_u64) }\nentry(bad)\n",
      "fn bad(value: Int): UInt { return "
      "u64.saturatingMultiply(value, 2_u64) }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return "
      "u64.saturatingMultiply(value, 2_i64) }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return "
      "u64.saturatingMultiply(value) }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return "
      "u64.saturatingMultiply(value, 2_u64, 3_u64) }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return "
      "u64.saturatingMultiply(left: value, 2_u64) }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return "
      "u64?.saturatingMultiply(value, 2_u64) }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return u64.saturatingMultiply("
      "u64.saturatingMultiply(value, 2_u64), 3_u64) }\nentry(bad)\n",
      "const u64: UInt = 1_u64\n"
      "fn bad(value: UInt): UInt { return "
      "u64.saturatingMultiply(value, 2_u64) }\nentry(bad)\n",
      "fn bad(value: UInt): Int { return "
      "u64.saturatingMultiply(value, 2_u64) }\nentry(bad)\n"};
  for (size_t index = 0u; index < sizeof(rejected) / sizeof(rejected[0]);
       index += 1u) {
    (void)memset(output, 0x97u, sizeof(output));
    (void)memset(&result, 0x98u, sizeof(result));
    const w_seed_native0_result snapshot = result;
    CHECK(run_source(rejected[index], strlen((const char *)rejected[index]),
                     "uint-saturating-multiply-bad", 28u, output,
                     sizeof(output), &result) != W_SEED_NATIVE0_OK);
    CHECK(memcmp(&result, &snapshot, sizeof(result)) == 0);
    for (size_t byte = 0u; byte < sizeof(output); byte += 1u)
      CHECK(output[byte] == 0x97u);
  }
  return true;
}

static bool test_u64_saturating_policy_slice(void) {
  static const uint8_t source[] =
      "fn saturatingNegate(value: UInt): UInt { return "
      "u64.saturatingNegate(value) }\n"
      "fn saturatingPower(base: UInt, exponent: UInt): UInt { return "
      "u64.saturatingPower(base, exponent) }\n"
      "entry { let negZero = saturatingNegate(value: 0_u64) "
      "let negMaximum = saturatingNegate(value: 18446744073709551615_u64) "
      "let ordinary = saturatingPower(base: 2_u64, exponent: 3_u64) "
      "let clamped = saturatingPower(base: 2_u64, exponent: 64_u64) "
      "let zeroPowerZero = saturatingPower(base: 0_u64, exponent: 0_u64) "
      "print(\"${negZero}/${negMaximum}/${ordinary}/${clamped}/${zeroPowerZero}\") }\n";
  uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  CHECK(run_source(source, sizeof(source) - 1u, "uint-saturating-policy",
                   sizeof("uint-saturating-policy") - 1u, output,
                   sizeof(output), &result) == W_SEED_NATIVE0_OK);
  size_t negate_count = 0u;
  size_t power_count = 0u;
  for (size_t index = 0u; index < storage.hir_program.value_count;
       index += 1u) {
    const w_seed_hir0_value *value = &storage.hir_program.values[index];
    if (value->kind == W_SEED_HIR0_VALUE_UNARY_U64 &&
        value->unary_operator == W_SEED_HIR0_UNARY_SATURATING_NEGATE)
      negate_count += 1u;
    if (value->kind == W_SEED_HIR0_VALUE_BINARY_U64 &&
        value->binary_operator == W_SEED_HIR0_BINARY_SATURATING_POWER)
      power_count += 1u;
  }
  CHECK(negate_count == 1u && power_count == 1u &&
        storage.hir_program.call_count == 6u);
  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(
            &storage.hir_program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  CHECK(selection.has_local_calls && !selection.has_cfg &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "llvm.intr.usub.sat") == 1u &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "llvm.func internal @w_seed_saturating_power_u64") ==
            1u &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "llvm.call @w_seed_saturating_power_u64") == 1u &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "llvm.select %acc_overflow, %max") &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "llvm.cond_br %last, ^saturating_power_done") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "@w_seed_checked_power_u64"));

  static const char *const rejected[] = {
      "fn bad(value: UInt): UInt { return UInt.saturatingNegate(value) }\n"
      "entry(bad)\n",
      "fn bad(value: UInt): UInt { return u64.saturatingNegate(value, 1_u64) }\n"
      "entry(bad)\n",
      "fn bad(base: UInt, exponent: UInt): UInt { return "
      "u64.saturatingPower(base, 1_i64) }\nentry(bad)\n",
      "fn bad(base: UInt, exponent: UInt): UInt { return "
      "u64.saturatingPower(left: base, exponent) }\nentry(bad)\n",
      "fn bad(base: UInt, exponent: UInt): Int { return "
      "u64.saturatingPower(base, exponent) }\nentry(bad)\n",
  };
  for (size_t index = 0u; index < sizeof(rejected) / sizeof(rejected[0]);
       index += 1u) {
    (void)memset(output, 0x9bu, sizeof(output));
    (void)memset(&result, 0x9cu, sizeof(result));
    const w_seed_native0_result snapshot = result;
    CHECK(run_source((const uint8_t *)rejected[index], strlen(rejected[index]),
                     "uint-saturating-policy-bad",
                     sizeof("uint-saturating-policy-bad") - 1u, output,
                     sizeof(output), &result) != W_SEED_NATIVE0_OK);
    CHECK(memcmp(&result, &snapshot, sizeof(result)) == 0);
    for (size_t byte = 0u; byte < sizeof(output); byte += 1u)
      CHECK(output[byte] == 0x9bu);
  }
  return true;
}

static bool test_u64_wrapping_subtract_slice(void) {
  static const uint8_t source[] =
      "fn wrap(value: UInt): UInt { return u64.wrappingSubtract(value, 1_u64) }\n"
      "fn main() { let result = wrap(value: 0_u64) "
      "print(\"${result}\") }\n"
      "entry(main)\n";
  uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  CHECK(run_source(source, sizeof(source) - 1u, "uint-wrapping-subtract", 13u,
                   output, sizeof(output), &result) == W_SEED_NATIVE0_OK);
  size_t wrapping_count = 0u;
  for (size_t index = 0u; index < storage.hir_program.value_count;
       index += 1u)
    if (storage.hir_program.values[index].kind ==
            W_SEED_HIR0_VALUE_BINARY_U64 &&
        storage.hir_program.values[index].binary_operator ==
            W_SEED_HIR0_BINARY_WRAPPING_SUBTRACT)
      wrapping_count += 1u;
  CHECK(wrapping_count == 1u);
  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(
            &storage.hir_program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  const size_t wrapper_start =
      find_bytes(output, result.mlir.written.mlir_bytes,
                 "llvm.func internal @w_fn_0", 0u);
  const size_t entry_start =
      find_bytes(output, result.mlir.written.mlir_bytes,
                 "llvm.func internal @w_fn_1", wrapper_start);
  CHECK(selection.has_local_calls && !selection.has_cfg &&
        wrapper_start != SIZE_MAX && entry_start != SIZE_MAX &&
        count_bytes(output + wrapper_start, entry_start - wrapper_start,
                    "llvm.sub ") == wrapping_count &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "llvm.call @w_seed_checked_subtract_u64") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "llvm.intr.usub.with.overflow"));

  static const uint8_t rejected[][220] = {
      "fn bad(value: UInt): UInt { return UInt.wrappingSubtract(value, 1_u64) }\n"
      "entry(bad)\n",
      "fn bad(value: UInt): UInt { return u64.wrappingSubtract(value, 1_i64) }\n"
      "entry(bad)\n",
      "fn bad(value: UInt): UInt { return u64.wrappingSubtract(value) }\n"
      "entry(bad)\n",
      "fn bad(value: UInt): UInt { return u64.wrappingSubtract(value, 1_u64, "
      "2_u64) }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return u64.wrappingSubtract(left: value, "
      "1_u64) }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return u64?.wrappingSubtract(value, 1_u64) }\n"
      "entry(bad)\n",
      "fn bad(value: UInt): UInt { return u64.wrappingSubtract("
      "u64.wrappingSubtract(value, 1_u64), 1_u64) }\nentry(bad)\n",
      "const u64: UInt = 1_u64\n"
      "fn bad(value: UInt): UInt { return u64.wrappingSubtract(value, 1_u64) }\n"
      "entry(bad)\n"};
  for (size_t index = 0u; index < sizeof(rejected) / sizeof(rejected[0]);
       index += 1u) {
    (void)memset(output, 0x93u, sizeof(output));
    (void)memset(&result, 0x94u, sizeof(result));
    const w_seed_native0_result snapshot = result;
    CHECK(run_source(rejected[index], strlen((const char *)rejected[index]),
                     "uint-wrapping-subtract-bad", 16u, output,
                     sizeof(output), &result) != W_SEED_NATIVE0_OK);
    CHECK(memcmp(&result, &snapshot, sizeof(result)) == 0);
    for (size_t byte = 0u; byte < sizeof(output); byte += 1u)
      CHECK(output[byte] == 0x93u);
  }
  return true;
}

static bool test_u64_wrapping_multiply_slice(void) {
  static const uint8_t source[] =
      "fn wrap(value: UInt): UInt { return u64.wrappingMultiply(value, 2_u64) }\n"
      "fn main() { let result = wrap(value: 18446744073709551615_u64) "
      "print(\"${result}\") }\n"
      "entry(main)\n";
  uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  CHECK(run_source(source, sizeof(source) - 1u, "uint-wrapping-multiply", 21u,
                   output, sizeof(output), &result) == W_SEED_NATIVE0_OK);
  size_t wrapping_count = 0u;
  for (size_t index = 0u; index < storage.hir_program.value_count;
       index += 1u)
    if (storage.hir_program.values[index].kind ==
            W_SEED_HIR0_VALUE_BINARY_U64 &&
        storage.hir_program.values[index].binary_operator ==
            W_SEED_HIR0_BINARY_WRAPPING_MULTIPLY)
      wrapping_count += 1u;
  CHECK(wrapping_count == 1u);
  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(
            &storage.hir_program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  const size_t wrapper_start =
      find_bytes(output, result.mlir.written.mlir_bytes,
                 "llvm.func internal @w_fn_0", 0u);
  const size_t entry_start =
      find_bytes(output, result.mlir.written.mlir_bytes,
                 "llvm.func internal @w_fn_1", wrapper_start);
  CHECK(selection.has_local_calls && !selection.has_cfg &&
        wrapper_start != SIZE_MAX && entry_start != SIZE_MAX &&
        count_bytes(output + wrapper_start, entry_start - wrapper_start,
                    "llvm.mul ") == wrapping_count &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "llvm.call @w_seed_checked_multiply_u64") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "llvm.intr.umul.with.overflow"));

  static const uint8_t rejected[][240] = {
      "fn bad(value: UInt): UInt { return UInt.wrappingMultiply(value, 2_u64) }\n"
      "entry(bad)\n",
      "fn bad(value: UInt): UInt { return u64.wrappingMultiply(value, 2_i64) }\n"
      "entry(bad)\n",
      "fn bad(value: UInt): UInt { return u64.wrappingMultiply(value) }\n"
      "entry(bad)\n",
      "fn bad(value: UInt): UInt { return u64.wrappingMultiply(value, 2_u64, "
      "3_u64) }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return u64.wrappingMultiply(left: value, "
      "2_u64) }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return u64?.wrappingMultiply(value, 2_u64) }\n"
      "entry(bad)\n",
      "fn bad(value: UInt): UInt { return u64.wrappingMultiply("
      "u64.wrappingMultiply(value, 2_u64), 2_u64) }\nentry(bad)\n",
      "const u64: UInt = 1_u64\n"
      "fn bad(value: UInt): UInt { return u64.wrappingMultiply(value, 2_u64) }\n"
      "entry(bad)\n"};
  for (size_t index = 0u; index < sizeof(rejected) / sizeof(rejected[0]);
       index += 1u) {
    (void)memset(output, 0x95u, sizeof(output));
    (void)memset(&result, 0x96u, sizeof(result));
    const w_seed_native0_result snapshot = result;
    CHECK(run_source(rejected[index], strlen((const char *)rejected[index]),
                     "uint-wrapping-multiply-bad", 25u, output, sizeof(output),
                     &result) != W_SEED_NATIVE0_OK);
    CHECK(memcmp(&result, &snapshot, sizeof(result)) == 0);
    for (size_t byte = 0u; byte < sizeof(output); byte += 1u)
      CHECK(output[byte] == 0x95u);
  }
  return true;
}

static bool test_u64_wrapping_power_slice(void) {
  static const uint8_t source[] =
      "fn wrap(base: UInt, exponent: UInt): UInt { "
      "return u64.wrappingPower(base, exponent) }\n"
      "fn main() { let large = wrap(base: 3_u64, exponent: 40_u64) "
      "let zero = wrap(base: 999_u64, exponent: 0_u64) "
      "let maximum = wrap(base: 0_u64, "
      "exponent: 18446744073709551615_u64) "
      "print(\"${large}/${zero}/${maximum}\") }\n"
      "entry(main)\n";
  uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  CHECK(run_source(source, sizeof(source) - 1u, "uint-wrapping-power", 19u,
                   output, sizeof(output), &result) == W_SEED_NATIVE0_OK);
  size_t wrapping_count = 0u;
  for (size_t index = 0u; index < storage.hir_program.value_count;
       index += 1u)
    if (storage.hir_program.values[index].kind ==
            W_SEED_HIR0_VALUE_BINARY_U64 &&
        storage.hir_program.values[index].binary_operator ==
            W_SEED_HIR0_BINARY_WRAPPING_POWER)
      wrapping_count += 1u;
  CHECK(wrapping_count == 1u && storage.hir_program.call_count == 4u);
  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(
            &storage.hir_program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  const size_t wrapper_start =
      find_bytes(output, result.mlir.written.mlir_bytes,
                 "llvm.func internal @w_fn_0", 0u);
  const size_t entry_start =
      find_bytes(output, result.mlir.written.mlir_bytes,
                 "llvm.func internal @w_fn_1", wrapper_start);
  CHECK(selection.has_local_calls && !selection.has_cfg &&
        wrapper_start != SIZE_MAX && entry_start != SIZE_MAX &&
        contains_bytes(output + wrapper_start, entry_start - wrapper_start,
                       "llvm.call @w_seed_wrapping_power_u64(%p0, %p1)") &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "llvm.func internal @w_seed_wrapping_power_u64") == 1u &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "llvm.call @w_seed_wrapping_power_u64") == 1u &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "llvm.lshr %remaining, %one : i64") &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "llvm.mul ") == 2u &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "llvm.mlir.constant(-1 : i64) : i64") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "@w_seed_checked_power_u64") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "llvm.intr.umul.with.overflow"));

  static const char *const rejected[] = {
      "fn bad(base: UInt, exponent: UInt): UInt { return "
      "UInt.wrappingPower(base, exponent) }\nentry(bad)\n",
      "fn bad(base: UInt, exponent: UInt): UInt { return "
      "u64.wrappingPower(base, 1_i64) }\nentry(bad)\n",
      "fn bad(base: UInt, exponent: UInt): UInt { return "
      "u64.wrappingPower(1_i64, exponent) }\nentry(bad)\n",
      "fn bad(base: UInt, exponent: UInt): UInt { return "
      "u64.wrappingPower(base) }\nentry(bad)\n",
      "fn bad(base: UInt, exponent: UInt): UInt { return "
      "u64.wrappingPower(base, exponent, 1_u64) }\nentry(bad)\n",
      "fn bad(base: UInt, exponent: UInt): UInt { return "
      "u64.wrappingPower(left: base, exponent) }\nentry(bad)\n",
      "fn bad(base: UInt, exponent: UInt): UInt { return "
      "u64?.wrappingPower(base, exponent) }\nentry(bad)\n",
      "fn bad(base: UInt, exponent: UInt): UInt { return "
      "u64.wrappingPower(u64.wrappingPower(base, exponent), exponent) }\n"
      "entry(bad)\n",
      "const u64: UInt = 1_u64\n"
      "fn bad(base: UInt, exponent: UInt): UInt { return "
      "u64.wrappingPower(base, exponent) }\nentry(bad)\n",
      "fn bad(base: UInt, exponent: UInt): Int { return "
      "u64.wrappingPower(base, exponent) }\nentry(bad)\n"};
  for (size_t index = 0u; index < sizeof(rejected) / sizeof(rejected[0]);
       index += 1u) {
    (void)memset(output, 0x99u, sizeof(output));
    (void)memset(&result, 0x9au, sizeof(result));
    const w_seed_native0_result snapshot = result;
    CHECK(run_source((const uint8_t *)rejected[index], strlen(rejected[index]),
                     "uint-wrapping-power-bad", 23u, output, sizeof(output),
                     &result) != W_SEED_NATIVE0_OK);
    CHECK(memcmp(&result, &snapshot, sizeof(result)) == 0);
    for (size_t byte = 0u; byte < sizeof(output); byte += 1u)
      CHECK(output[byte] == 0x99u);
  }
  return true;
}

static bool test_u64_overflowing_power_slice(void) {
  static const uint8_t source[] =
      "entry { "
      "let ordinary = u64.overflowingPower(2_u64, 3_u64) "
      "let overflow = u64.overflowingPower(2_u64, 64_u64) "
      "let zero = u64.overflowingPower(0_u64, 0_u64) "
      "let ordinaryValue = ordinary.0 let ordinaryOverflow = ordinary.1 "
      "let overflowValue = overflow.0 let overflowFlag = overflow.1 "
      "let zeroValue = zero.0 let zeroFlag = zero.1 "
      "print(\"\x24{ordinaryValue}/\x24{ordinaryOverflow}/\x24{overflowValue}/"
      "\x24{overflowFlag}/\x24{zeroValue}/\x24{zeroFlag}\") }\n";
  uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  CHECK(run_source(source, sizeof(source) - 1u, "uint-overflowing-power",
                   sizeof("uint-overflowing-power") - 1u, output,
                   sizeof(output), &result) == W_SEED_NATIVE0_OK);
  size_t overflowing_power_count = 0u;
  for (size_t index = 0u; index < storage.hir_program.value_count;
       index += 1u)
    if (storage.hir_program.values[index].kind ==
            W_SEED_HIR0_VALUE_BINARY_U64 &&
        storage.hir_program.values[index].binary_operator ==
            W_SEED_HIR0_BINARY_OVERFLOWING_POWER)
      overflowing_power_count += 1u;
  CHECK(overflowing_power_count == 3u &&
        storage.hir_program.call_count == 1u);
  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(
            &storage.hir_program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  CHECK(!selection.has_local_calls && !selection.has_cfg &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "llvm.func internal @w_seed_overflowing_power_u64") ==
            1u &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "llvm.call @w_seed_overflowing_power_u64") == 3u &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "\"llvm.intr.umul.with.overflow\"") == 2u &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "llvm.extractvalue") >= 10u &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "llvm.or %overflowed, %acc_overflow : i1") &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "llvm.insertvalue %overflowed_result") &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "llvm.call @w_seed_append_u64") &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "llvm.call @w_seed_append_bool") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "@w_seed_checked_power_u64"));

  static const char *const rejected[] = {
      "entry { let pair = u64.overflowingPower(1_u64) }\n",
      "entry { let pair = u64.overflowingPower(1_u64, true) }\n",
      "entry { let pair = UInt.overflowingPower(1_u64, 2_u64) }\n",
      "entry { let value: UInt = u64.overflowingPower(1_u64, 2_u64) }\n",
  };
  for (size_t index = 0u; index < sizeof(rejected) / sizeof(rejected[0]);
       index += 1u) {
    (void)memset(output, 0x99u, sizeof(output));
    (void)memset(&result, 0x9au, sizeof(result));
    const w_seed_native0_result snapshot = result;
    CHECK(run_source((const uint8_t *)rejected[index], strlen(rejected[index]),
                     "uint-overflowing-power-bad",
                     sizeof("uint-overflowing-power-bad") - 1u, output,
                     sizeof(output), &result) != W_SEED_NATIVE0_OK);
    CHECK(memcmp(&result, &snapshot, sizeof(result)) == 0);
    for (size_t byte = 0u; byte < sizeof(output); byte += 1u)
      CHECK(output[byte] == 0x99u);
  }
  return true;
}

static bool test_u64_overflowing_add_slice(void) {
  static const uint8_t source[] =
      "entry { "
      "let maximum = u64.overflowingAdd(18446744073709551615_u64, 1_u64) "
      "let ordinary = u64.overflowingAdd(10_u64, 1_u64) "
      "print(\"Overflowing \x24{maximum.0}/\x24{maximum.1}/"
      "\x24{ordinary.0}/\x24{ordinary.1}\") }\n";
  uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  CHECK(run_source(source, sizeof(source) - 1u, "uint-overflowing-add",
                   sizeof("uint-overflowing-add") - 1u, output,
                   sizeof(output), &result) == W_SEED_NATIVE0_OK);
  size_t overflowing_add_count = 0u;
  size_t projection_count = 0u;
  for (size_t index = 0u; index < storage.hir_program.value_count;
       index += 1u) {
    const w_seed_hir0_value *value = &storage.hir_program.values[index];
    if (value->kind == W_SEED_HIR0_VALUE_BINARY_U64 &&
        value->binary_operator == W_SEED_HIR0_BINARY_OVERFLOWING_ADD)
      overflowing_add_count += 1u;
    else if (value->kind == W_SEED_HIR0_VALUE_TUPLE_ELEMENT)
      projection_count += 1u;
  }
  CHECK(overflowing_add_count == 2u && projection_count == 4u &&
        storage.hir_program.call_count == 1u);
  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(
            &storage.hir_program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  CHECK(!selection.has_local_calls && !selection.has_cfg &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "llvm.intr.uadd.with.overflow") == 2u &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "llvm.extractvalue") == projection_count &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "llvm.call @w_seed_append_u64") &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "llvm.call @w_seed_append_bool"));
  return true;
}

static bool test_u64_overflowing_family_slice(void) {
  static const uint8_t source[] =
      "entry { "
      "let ordinarySubtract = u64.overflowingSubtract(42_u64, 1_u64) "
      "let underflowSubtract = u64.overflowingSubtract(0_u64, 1_u64) "
      "let ordinaryMultiply = u64.overflowingMultiply(6_u64, 7_u64) "
      "let overflowMultiply = u64.overflowingMultiply(18446744073709551615_u64, 2_u64) "
      "let zeroNegate = u64.overflowingNegate(0_u64) "
      "let oneNegate = u64.overflowingNegate(1_u64) "
      "print(\"Overflowing family \x24{ordinarySubtract.0}/\x24{ordinarySubtract.1}/\x24{underflowSubtract.0}/\x24{underflowSubtract.1}/\x24{ordinaryMultiply.0}/\x24{ordinaryMultiply.1}/\x24{overflowMultiply.0}/\x24{overflowMultiply.1}/\x24{zeroNegate.0}/\x24{zeroNegate.1}/\x24{oneNegate.0}/\x24{oneNegate.1}\") }\n";
  uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  CHECK(run_source(source, sizeof(source) - 1u, "uint-overflowing-family",
                   sizeof("uint-overflowing-family") - 1u, output,
                   sizeof(output), &result) == W_SEED_NATIVE0_OK);
  size_t subtract_count = 0u;
  size_t multiply_count = 0u;
  size_t negate_count = 0u;
  size_t projection_count = 0u;
  for (size_t index = 0u; index < storage.hir_program.value_count;
       index += 1u) {
    const w_seed_hir0_value *value = &storage.hir_program.values[index];
    if (value->kind == W_SEED_HIR0_VALUE_BINARY_U64 &&
        value->binary_operator == W_SEED_HIR0_BINARY_OVERFLOWING_SUBTRACT)
      subtract_count += 1u;
    else if (value->kind == W_SEED_HIR0_VALUE_BINARY_U64 &&
             value->binary_operator == W_SEED_HIR0_BINARY_OVERFLOWING_MULTIPLY)
      multiply_count += 1u;
    else if (value->kind == W_SEED_HIR0_VALUE_UNARY_U64 &&
             value->unary_operator == W_SEED_HIR0_UNARY_OVERFLOWING_NEGATE)
      negate_count += 1u;
    else if (value->kind == W_SEED_HIR0_VALUE_TUPLE_ELEMENT)
      projection_count += 1u;
  }
  CHECK(subtract_count == 2u && multiply_count == 2u && negate_count == 2u &&
        projection_count == 12u && storage.hir_program.call_count == 1u);
  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(
            &storage.hir_program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  CHECK(!selection.has_local_calls && !selection.has_cfg &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "llvm.intr.usub.with.overflow") == 4u &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "llvm.intr.umul.with.overflow") == 2u &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "llvm.extractvalue") == projection_count &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "llvm.call @w_seed_append_u64") &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "llvm.call @w_seed_append_bool") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "@w_seed_checked_subtract_u64") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "@w_seed_checked_multiply_u64") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "@w_seed_checked_negate_u64"));

  static const char *const rejected[] = {
      "entry { let pair = UInt.overflowingSubtract(1_u64, 2_u64) }\n",
      "entry { let pair = u64.overflowingSubtract(1_u64) }\n",
      "entry { let pair = u64.overflowingSubtract(1_u64, 2_u64, 3_u64) }\n",
      "entry { let pair = u64.overflowingSubtract(1_i64, 2_u64) }\n",
      "entry { let pair = u64.overflowingSubtract(1_u64, true) }\n",
      "entry { let pair = u64.overflowingSubtract(left: 1_u64, 2_u64) }\n",
      "entry { let value: UInt = u64.overflowingSubtract(1_u64, 2_u64) }\n",
      "entry { let pair = UInt.overflowingMultiply(1_u64, 2_u64) }\n",
      "entry { let pair = u64.overflowingMultiply(1_u64) }\n",
      "entry { let pair = u64.overflowingMultiply(1_u64, 2_u64, 3_u64) }\n",
      "entry { let pair = u64.overflowingMultiply(1_u64, 2_i64) }\n",
      "entry { let pair = u64.overflowingMultiply(1_i64, 2_u64) }\n",
      "entry { let pair = u64.overflowingMultiply(left: 1_u64, 2_u64) }\n",
      "entry { let value: UInt = u64.overflowingMultiply(1_u64, 2_u64) }\n",
      "entry { let pair = UInt.overflowingNegate(1_u64) }\n",
      "entry { let pair = u64.overflowingNegate() }\n",
      "entry { let pair = u64.overflowingNegate(1_u64, 2_u64) }\n",
      "entry { let pair = u64.overflowingNegate(1_i64) }\n",
      "entry { let pair = u64.overflowingNegate(true) }\n",
      "entry { let value: UInt = u64.overflowingNegate(1_u64) }\n",
      "entry { let pair = u64.overflowingSubtract(1_u64, 2_u64) let invalid = pair.2 }\n"};
  for (size_t index = 0u; index < sizeof(rejected) / sizeof(rejected[0]);
       index += 1u) {
    (void)memset(output, 0x9fu, sizeof(output));
    (void)memset(&result, 0xa0u, sizeof(result));
    const w_seed_native0_result snapshot = result;
    CHECK(run_source((const uint8_t *)rejected[index], strlen(rejected[index]),
                     "uint-overflowing-family-bad", 26u, output,
                     sizeof(output), &result) != W_SEED_NATIVE0_OK);
    CHECK(memcmp(&result, &snapshot, sizeof(result)) == 0);
    for (size_t byte = 0u; byte < sizeof(output); byte += 1u)
      CHECK(output[byte] == 0x9fu);
  }
  return true;
}

static bool test_u64_wrapping_shift_left_slice(void) {
  static const uint8_t source[] =
      "fn shift(value: UInt, count: UInt): UInt { "
      "return u64.wrappingShiftLeft(value, count) }\n"
      "fn main() { "
      "let wrapped = shift(value: 18446744073709551615_u64, count: 1_u64) "
      "let zero = shift(value: 1_u64, count: 0_u64) "
      "let edge = shift(value: 1_u64, count: 63_u64) "
      "print(\"${wrapped}/${zero}/${edge}\") }\n"
      "entry(main)\n";
  uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  CHECK(run_source(source, sizeof(source) - 1u, "uint-wrapping-shift-left",
                   24u, output, sizeof(output), &result) == W_SEED_NATIVE0_OK);
  size_t wrapping_count = 0u;
  for (size_t index = 0u; index < storage.hir_program.value_count;
       index += 1u)
    if (storage.hir_program.values[index].kind ==
            W_SEED_HIR0_VALUE_BINARY_U64 &&
        storage.hir_program.values[index].binary_operator ==
            W_SEED_HIR0_BINARY_WRAPPING_SHIFT_LEFT)
      wrapping_count += 1u;
  CHECK(wrapping_count == 1u && storage.hir_program.call_count == 4u);
  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(
            &storage.hir_program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  CHECK(selection.has_local_calls && !selection.has_cfg &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "llvm.func internal @w_seed_wrapping_shift_left_u64") ==
            1u &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "llvm.call @w_seed_wrapping_shift_left_u64") == 1u &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "llvm.icmp \"uge\" %count, %width : i64") &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "llvm.shl %left, %count : i64") &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "\"llvm.intr.trap\"() : () -> ()") &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "llvm.unreachable") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "@w_seed_checked_shift_left") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "llvm.intr.ushl.with.overflow"));

  static const char *const rejected[] = {
      "fn bad(value: UInt, count: UInt): UInt { return "
      "UInt.wrappingShiftLeft(value, count) }\nentry(bad)\n",
      "fn bad(value: UInt, count: UInt): UInt { return "
      "u64.wrappingShiftLeft(value, 1_i64) }\nentry(bad)\n",
      "fn bad(value: UInt, count: UInt): UInt { return "
      "u64.wrappingShiftLeft(1_i64, count) }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return "
      "u64.wrappingShiftLeft(value) }\nentry(bad)\n",
      "fn bad(value: UInt, count: UInt): UInt { return "
      "u64.wrappingShiftLeft(value, count, 1_u64) }\nentry(bad)\n",
      "fn bad(value: UInt, count: UInt): UInt { return "
      "u64.wrappingShiftLeft(value: value, count) }\nentry(bad)\n",
      "fn bad(value: UInt, count: UInt): UInt { return "
      "u64?.wrappingShiftLeft(value, count) }\nentry(bad)\n",
      "fn bad(value: UInt, count: UInt): UInt { return "
      "u64.wrappingShiftLeft(u64.wrappingShiftLeft(value, count), count) }\n"
      "entry(bad)\n",
      "const u64: UInt = 1_u64\n"
      "fn bad(value: UInt, count: UInt): UInt { return "
      "u64.wrappingShiftLeft(value, count) }\nentry(bad)\n",
      "fn bad(value: UInt, count: UInt): Int { return "
      "u64.wrappingShiftLeft(value, count) }\nentry(bad)\n",
      "entry { let bad = u64.wrappingShiftLeft(1_u64, 64_u64) }\n"};
  for (size_t index = 0u; index < sizeof(rejected) / sizeof(rejected[0]);
       index += 1u) {
    (void)memset(output, 0x9bu, sizeof(output));
    (void)memset(&result, 0x9cu, sizeof(result));
    const w_seed_native0_result snapshot = result;
    CHECK(run_source((const uint8_t *)rejected[index], strlen(rejected[index]),
                     "uint-wrapping-shift-left-bad", 28u, output,
                     sizeof(output), &result) != W_SEED_NATIVE0_OK);
    CHECK(memcmp(&result, &snapshot, sizeof(result)) == 0);
    for (size_t byte = 0u; byte < sizeof(output); byte += 1u)
      CHECK(output[byte] == 0x9bu);
  }
  return true;
}

static bool test_u64_masked_shift_left_slice(void) {
  static const uint8_t source[] =
      "fn shift(value: UInt, count: UInt): UInt { "
      "return u64.maskedShiftLeft(value, count) }\n"
      "fn main() { "
      "let zero = shift(value: 1_u64, count: 0_u64) "
      "let edge = shift(value: 1_u64, count: 63_u64) "
      "let width = shift(value: 7_u64, count: 64_u64) "
      "let next = shift(value: 1_u64, count: 65_u64) "
      "print(\"${zero}/${edge}/${width}/${next}\") }\n"
      "entry(main)\n";
  uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  CHECK(run_source(source, sizeof(source) - 1u, "uint-masked-shift-left",
                   22u, output, sizeof(output), &result) == W_SEED_NATIVE0_OK);
  size_t masked_count = 0u;
  for (size_t index = 0u; index < storage.hir_program.value_count;
       index += 1u)
    if (storage.hir_program.values[index].kind ==
            W_SEED_HIR0_VALUE_BINARY_U64 &&
        storage.hir_program.values[index].binary_operator ==
            W_SEED_HIR0_BINARY_MASKED_SHIFT_LEFT)
      masked_count += 1u;
  CHECK(masked_count == 1u && storage.hir_program.call_count == 5u);
  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(
            &storage.hir_program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  CHECK(selection.has_local_calls && !selection.has_cfg &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "@w_seed_masked_shift_left_u64") == 0u &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "llvm.call @w_seed_masked_shift_left_u64") == 0u &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "_shift_mask = llvm.mlir.constant(63 : i64)") &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "_shift_count_mod = llvm.and ") &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "llvm.shl ") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "@w_seed_checked_shift_left") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "llvm.intr.ushl.with.overflow"));

  static const char *const rejected[] = {
      "fn bad(value: UInt, count: UInt): UInt { return "
      "UInt.maskedShiftLeft(value, count) }\nentry(bad)\n",
      "fn bad(value: UInt, count: UInt): UInt { return "
      "u64.maskedShiftLeft(value, 1_i64) }\nentry(bad)\n",
      "fn bad(value: UInt, count: UInt): UInt { return "
      "u64.maskedShiftLeft(1_i64, count) }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return "
      "u64.maskedShiftLeft(value) }\nentry(bad)\n",
      "fn bad(value: UInt, count: UInt): UInt { return "
      "u64.maskedShiftLeft(value, count, 1_u64) }\nentry(bad)\n",
      "fn bad(value: UInt, count: UInt): UInt { return "
      "u64.maskedShiftLeft(value: value, count) }\nentry(bad)\n",
      "fn bad(value: UInt, count: UInt): UInt { return "
      "u64?.maskedShiftLeft(value, count) }\nentry(bad)\n",
      "fn bad(value: UInt, count: UInt): UInt { return "
      "u64.maskedShiftLeft(u64.maskedShiftLeft(value, count), count) }\n"
      "entry(bad)\n",
      "const u64: UInt = 1_u64\n"
      "fn bad(value: UInt, count: UInt): UInt { return "
      "u64.maskedShiftLeft(value, count) }\nentry(bad)\n",
      "fn bad(value: UInt, count: UInt): Int { return "
      "u64.maskedShiftLeft(value, count) }\nentry(bad)\n"};
  for (size_t index = 0u; index < sizeof(rejected) / sizeof(rejected[0]);
       index += 1u) {
    (void)memset(output, 0x9du, sizeof(output));
    (void)memset(&result, 0x9eu, sizeof(result));
    const w_seed_native0_result snapshot = result;
    CHECK(run_source((const uint8_t *)rejected[index], strlen(rejected[index]),
                     "uint-masked-shift-left-bad", 26u, output,
                     sizeof(output), &result) != W_SEED_NATIVE0_OK);
    CHECK(memcmp(&result, &snapshot, sizeof(result)) == 0);
    for (size_t byte = 0u; byte < sizeof(output); byte += 1u)
      CHECK(output[byte] == 0x9du);
  }
  return true;
}

static bool test_u64_masked_shift_right_slice(void) {
  static const uint8_t source[] =
      "fn shift(value: UInt, count: UInt): UInt { "
      "return u64.maskedShiftRight(value, count) }\n"
      "fn main() { "
      "let zero = shift(value: 128_u64, count: 0_u64) "
      "let edge = shift(value: 9223372036854775808_u64, count: 63_u64) "
      "let width = shift(value: 7_u64, count: 64_u64) "
      "let next = shift(value: 128_u64, count: 65_u64) "
      "print(\"${zero}/${edge}/${width}/${next}\") }\n"
      "entry(main)\n";
  uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  CHECK(run_source(source, sizeof(source) - 1u, "uint-masked-shift-right",
                   23u, output, sizeof(output), &result) == W_SEED_NATIVE0_OK);
  size_t masked_count = 0u;
  for (size_t index = 0u; index < storage.hir_program.value_count;
       index += 1u)
    if (storage.hir_program.values[index].kind ==
            W_SEED_HIR0_VALUE_BINARY_U64 &&
        storage.hir_program.values[index].binary_operator ==
            W_SEED_HIR0_BINARY_MASKED_SHIFT_RIGHT)
      masked_count += 1u;
  CHECK(masked_count == 1u && storage.hir_program.call_count == 5u);
  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(
            &storage.hir_program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  CHECK(selection.has_local_calls && !selection.has_cfg &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "@w_seed_masked_shift_right_u64") == 0u &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "llvm.call @w_seed_masked_shift_right_u64") == 0u &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "_shift_mask = llvm.mlir.constant(63 : i64)") &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "_shift_count_mod = llvm.and ") &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "llvm.lshr ") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "@w_seed_checked_shift_right"));

  static const char *const rejected[] = {
      "fn bad(value: UInt, count: UInt): UInt { return "
      "UInt.maskedShiftRight(value, count) }\nentry(bad)\n",
      "fn bad(value: UInt, count: UInt): UInt { return "
      "u64.maskedShiftRight(value, 1_i64) }\nentry(bad)\n",
      "fn bad(value: UInt, count: UInt): UInt { return "
      "u64.maskedShiftRight(1_i64, count) }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return "
      "u64.maskedShiftRight(value) }\nentry(bad)\n",
      "fn bad(value: UInt, count: UInt): UInt { return "
      "u64.maskedShiftRight(value, count, 1_u64) }\nentry(bad)\n",
      "fn bad(value: UInt, count: UInt): UInt { return "
      "u64.maskedShiftRight(value: value, count) }\nentry(bad)\n",
      "fn bad(value: UInt, count: UInt): UInt { return "
      "u64?.maskedShiftRight(value, count) }\nentry(bad)\n",
      "fn bad(value: UInt, count: UInt): UInt { return "
      "u64.maskedShiftRight(u64.maskedShiftRight(value, count), count) }\n"
      "entry(bad)\n",
      "const u64: UInt = 1_u64\n"
      "fn bad(value: UInt, count: UInt): UInt { return "
      "u64.maskedShiftRight(value, count) }\nentry(bad)\n",
      "fn bad(value: UInt, count: UInt): Int { return "
      "u64.maskedShiftRight(value, count) }\nentry(bad)\n"};
  for (size_t index = 0u; index < sizeof(rejected) / sizeof(rejected[0]);
       index += 1u) {
    (void)memset(output, 0x9fu, sizeof(output));
    (void)memset(&result, 0xa0u, sizeof(result));
    const w_seed_native0_result snapshot = result;
    CHECK(run_source((const uint8_t *)rejected[index], strlen(rejected[index]),
                     "uint-masked-shift-right-bad", 27u, output,
                     sizeof(output), &result) != W_SEED_NATIVE0_OK);
    CHECK(memcmp(&result, &snapshot, sizeof(result)) == 0);
    for (size_t byte = 0u; byte < sizeof(output); byte += 1u)
      CHECK(output[byte] == 0x9fu);
  }
  return true;
}

static bool test_u64_logical_shift_right_slice(void) {
  static const uint8_t source[] =
      "fn shift(value: UInt, count: UInt): UInt { "
      "return u64.logicalShiftRight(value, count) }\n"
      "fn main() { "
      "let zero = shift(value: 128_u64, count: 0_u64) "
      "let edge = shift(value: 9223372036854775808_u64, count: 63_u64) "
      "let next = shift(value: 128_u64, count: 1_u64) "
      "print(\"${zero}/${edge}/${next}\") }\n"
      "entry(main)\n";
  uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  CHECK(run_source(source, sizeof(source) - 1u, "uint-logical-shift-right",
                   24u, output, sizeof(output), &result) == W_SEED_NATIVE0_OK);
  size_t logical_count = 0u;
  for (size_t index = 0u; index < storage.hir_program.value_count;
       index += 1u)
    if (storage.hir_program.values[index].kind ==
            W_SEED_HIR0_VALUE_BINARY_U64 &&
        storage.hir_program.values[index].binary_operator ==
            W_SEED_HIR0_BINARY_LOGICAL_SHIFT_RIGHT)
      logical_count += 1u;
  CHECK(logical_count == 1u && storage.hir_program.call_count == 4u);
  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(
            &storage.hir_program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  CHECK(selection.has_local_calls && !selection.has_cfg &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "llvm.func internal @w_seed_logical_shift_right_integer") ==
            1u &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "llvm.call @w_seed_logical_shift_right_integer") == 1u &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "llvm.icmp \"uge\" %count, %width : i64") &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "llvm.lshr %normalized, %count : i64") &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "\"llvm.intr.trap\"() : () -> ()") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "_shift_count_mod = llvm.and ") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "@w_seed_checked_shift_right"));

  static const char *const rejected[] = {
      "fn bad(value: UInt, count: UInt): UInt { return "
      "UInt.logicalShiftRight(value, count) }\nentry(bad)\n",
      "fn bad(value: UInt, count: UInt): UInt { return "
      "u64.logicalShiftRight(value, 1_i64) }\nentry(bad)\n",
      "fn bad(value: UInt, count: UInt): UInt { return "
      "u64.logicalShiftRight(1_i64, count) }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return "
      "u64.logicalShiftRight(value) }\nentry(bad)\n",
      "fn bad(value: UInt, count: UInt): UInt { return "
      "u64.logicalShiftRight(value, count, 1_u64) }\nentry(bad)\n",
      "fn bad(value: UInt, count: UInt): UInt { return "
      "u64.logicalShiftRight(value: value, count) }\nentry(bad)\n",
      "fn bad(value: UInt, count: UInt): UInt { return "
      "u64?.logicalShiftRight(value, count) }\nentry(bad)\n",
      "fn bad(value: UInt, count: UInt): UInt { return "
      "u64.logicalShiftRight(u64.logicalShiftRight(value, count), count) }\n"
      "entry(bad)\n",
      "const u64: UInt = 1_u64\n"
      "fn bad(value: UInt, count: UInt): UInt { return "
      "u64.logicalShiftRight(value, count) }\nentry(bad)\n",
      "fn bad(value: UInt, count: UInt): Int { return "
      "u64.logicalShiftRight(value, count) }\nentry(bad)\n",
      "entry { let bad = u64.logicalShiftRight(1_u64, 64_u64) }\n"};
  for (size_t index = 0u; index < sizeof(rejected) / sizeof(rejected[0]);
       index += 1u) {
    (void)memset(output, 0xa1u, sizeof(output));
    (void)memset(&result, 0xa2u, sizeof(result));
    const w_seed_native0_result snapshot = result;
    CHECK(run_source((const uint8_t *)rejected[index], strlen(rejected[index]),
                     "uint-logical-shift-right-bad", 28u, output,
                     sizeof(output), &result) != W_SEED_NATIVE0_OK);
    CHECK(memcmp(&result, &snapshot, sizeof(result)) == 0);
    for (size_t byte = 0u; byte < sizeof(output); byte += 1u)
      CHECK(output[byte] == 0xa1u);
  }
  return true;
}

static bool test_fixed_integer_shift_policy_native_matrix(void) {
  typedef struct {
    const char *spelling;
    const char *high_bit_literal;
    bool is_signed;
    uint16_t bit_width;
  } integer_case;
  static const integer_case INTEGERS[] = {
      {"i8", "-64_i8", true, 8u},
      {"u8", "128_u8", false, 8u},
      {"i16", "-16384_i16", true, 16u},
      {"u16", "32768_u16", false, 16u},
      {"i32", "-1073741824_i32", true, 32u},
      {"u32", "2147483648_u32", false, 32u},
      {"i64", "-4611686018427387904_i64", true, 64u},
      {"u64", "9223372036854775808_u64", false, 64u},
  };
  typedef struct {
    const char *name;
    const char *member;
    w_seed_hir0_binary_operator operation;
  } shift_case;
  static const shift_case SHIFTS[] = {
      {"masked_left", "maskedShiftLeft",
       W_SEED_HIR0_BINARY_MASKED_SHIFT_LEFT},
      {"masked_right", "maskedShiftRight",
       W_SEED_HIR0_BINARY_MASKED_SHIFT_RIGHT},
      {"logical_right", "logicalShiftRight",
       W_SEED_HIR0_BINARY_LOGICAL_SHIFT_RIGHT},
  };
  static char source[W_SEED_NATIVE0_MAX_SOURCE_BYTES];
  static char invalid_source[512];
  static uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;

  for (size_t integer_index = 0u;
       integer_index < sizeof(INTEGERS) / sizeof(INTEGERS[0]);
       integer_index += 1u) {
    const integer_case *integer = &INTEGERS[integer_index];
    size_t source_length = 0u;
    for (size_t shift_index = 0u;
         shift_index < sizeof(SHIFTS) / sizeof(SHIFTS[0]); shift_index += 1u)
      CHECK(append_test_source(
          source, sizeof(source), &source_length,
          "fn %s(value: %s, count: UInt): %s { return %s.%s(value, count) }\n",
          SHIFTS[shift_index].name, integer->spelling, integer->spelling,
          integer->spelling, SHIFTS[shift_index].member));
    CHECK(append_test_source(source, sizeof(source), &source_length,
                             "fn main() {\n"));
    for (size_t shift_index = 0u;
         shift_index < sizeof(SHIFTS) / sizeof(SHIFTS[0]); shift_index += 1u)
      for (uint16_t boundary = 0u; boundary < 4u; boundary += 1u) {
        const uint16_t count =
            boundary == 0u
                ? 0u
                : (boundary == 1u
                       ? (uint16_t)(integer->bit_width - 1u)
                       : (boundary == 2u ? integer->bit_width
                                         : (uint16_t)(integer->bit_width + 1u)));
        CHECK(append_test_source(
            source, sizeof(source), &source_length,
            "let %s_%u = %s(value: %s, count: %u_u64)\n",
            SHIFTS[shift_index].name, (unsigned)boundary,
            SHIFTS[shift_index].name, integer->high_bit_literal,
            (unsigned)count));
      }
    CHECK(append_test_source(source, sizeof(source), &source_length,
                             "print(\""));
    bool first_interpolation = true;
    for (size_t shift_index = 0u;
         shift_index < sizeof(SHIFTS) / sizeof(SHIFTS[0]); shift_index += 1u)
      for (uint16_t boundary = 0u; boundary < 4u; boundary += 1u) {
        const char *format =
            first_interpolation ? "${%s_%u}" : "/${%s_%u}";
        CHECK(append_test_source(source, sizeof(source), &source_length,
                                 format, SHIFTS[shift_index].name,
                                 (unsigned)boundary));
        first_interpolation = false;
      }
    CHECK(append_test_source(source, sizeof(source), &source_length,
                             "\")\n}\nentry(main)\n"));

    CHECK(run_source((const uint8_t *)source, source_length,
                     "fixed-integer-shift-policy-matrix",
                     sizeof("fixed-integer-shift-policy-matrix") - 1u,
                     output, sizeof(output), &result) == W_SEED_NATIVE0_OK);
    CHECK(storage.hir_program.function_count == 4u &&
          storage.hir_program.call_count == 13u);
    w_seed_native_subset0_program selection;
    CHECK(w_seed_native_subset0_select_program(
              &storage.hir_program, &storage.hir_result, &selection) ==
          W_SEED_NATIVE_SUBSET0_OK);
    CHECK(selection.has_local_calls && !selection.has_cfg &&
          selection.has_interpolation);

    size_t operation_counts[sizeof(SHIFTS) / sizeof(SHIFTS[0])] = {0u};
    uint32_t forged_shift = W_SEED_HIR0_NONE;
    uint32_t u64_type = W_SEED_HIR0_NONE;
    for (size_t type_index = 0u;
         type_index < storage.hir_program.type_count; type_index += 1u) {
      const w_seed_hir0_type *type = &storage.hir_program.types[type_index];
      if (type->kind == W_SEED_HIR0_TYPE_U64 &&
          !type->integer_is_signed && type->integer_bit_width == 64u)
        u64_type = (uint32_t)type_index;
    }
    for (size_t value_index = 0u;
         value_index < storage.hir_program.value_count; value_index += 1u) {
      const w_seed_hir0_value *value = &storage.hir_program.values[value_index];
      size_t shift_index = SIZE_MAX;
      for (size_t candidate = 0u;
           candidate < sizeof(SHIFTS) / sizeof(SHIFTS[0]); candidate += 1u)
        if (value->binary_operator == SHIFTS[candidate].operation) {
          shift_index = candidate;
          break;
        }
      if (shift_index == SIZE_MAX) continue;
      CHECK(value->type_index < storage.hir_program.type_count &&
            storage.hir_program.types[value->type_index].integer_is_signed ==
                integer->is_signed &&
            storage.hir_program.types[value->type_index].integer_bit_width ==
                integer->bit_width);
      operation_counts[shift_index] += 1u;
      if (shift_index == 0u && integer_index == 0u)
        forged_shift = (uint32_t)value_index;
    }
    CHECK(operation_counts[0] == 1u && operation_counts[1] == 1u &&
          operation_counts[2] == 1u &&
          contains_bytes(output, result.mlir.written.mlir_bytes,
                         "llvm.shl ") &&
          contains_bytes(output, result.mlir.written.mlir_bytes,
                         integer->is_signed ? "llvm.ashr " : "llvm.lshr ") &&
          count_bytes(output, result.mlir.written.mlir_bytes,
                      "llvm.func internal @w_seed_logical_shift_right_integer") ==
              1u &&
          count_bytes(output, result.mlir.written.mlir_bytes,
                      "llvm.call @w_seed_logical_shift_right_integer") == 1u &&
          contains_bytes(output, result.mlir.written.mlir_bytes,
                         "llvm.icmp \"uge\" %count, %width : i64") &&
          contains_bytes(output, result.mlir.written.mlir_bytes,
                         "llvm.lshr %normalized, %count : i64"));
    if (integer->bit_width < 64u)
      CHECK(contains_bytes(output, result.mlir.written.mlir_bytes,
                           "_shift_count = llvm.trunc") &&
            contains_bytes(output, result.mlir.written.mlir_bytes,
                           integer->bit_width == 8u
                               ? "to i8"
                               : (integer->bit_width == 16u ? "to i16"
                                                            : "to i32")));
    uint8_t mask_needle[64];
    const int mask_length = snprintf(
        (char *)mask_needle, sizeof(mask_needle),
        "_shift_mask = llvm.mlir.constant(%u : i64)",
        (unsigned)integer->bit_width - 1u);
    CHECK(mask_length > 0 && (size_t)mask_length < sizeof(mask_needle) &&
          contains_bytes(output, result.mlir.written.mlir_bytes,
                         (const char *)mask_needle));

    if (integer_index == 0u) {
      CHECK(forged_shift != W_SEED_HIR0_NONE && u64_type != W_SEED_HIR0_NONE);
      const uint32_t saved_type = storage.hir_values[forged_shift].type_index;
      storage.hir_values[forged_shift].type_index = u64_type;
      w_seed_native_subset0_program forged_selection;
      (void)memset(&forged_selection, 0x5au, sizeof(forged_selection));
      const w_seed_native_subset0_program forged_snapshot = forged_selection;
      CHECK(w_seed_native_subset0_select_program(
                &storage.hir_program, &storage.hir_result,
                &forged_selection) == W_SEED_NATIVE_SUBSET0_INVALID);
      CHECK(memcmp(&forged_selection, &forged_snapshot,
                   sizeof(forged_selection)) == 0);
      storage.hir_values[forged_shift].type_index = saved_type;
      CHECK(w_seed_hir0_verify(&storage.hir_program, &storage.hir_result));
    }
  }

  for (size_t integer_index = 0u;
       integer_index < sizeof(INTEGERS) / sizeof(INTEGERS[0]);
       integer_index += 1u) {
    const integer_case *integer = &INTEGERS[integer_index];
    for (uint16_t invalid = 0u; invalid < 2u; invalid += 1u) {
      const unsigned count = (unsigned)integer->bit_width + invalid;
      size_t invalid_length = 0u;
      CHECK(append_test_source(
          invalid_source, sizeof(invalid_source), &invalid_length,
          "entry { let invalid = %s.logicalShiftRight(%s, %u_u64) "
          "print(\"${invalid}\") }\n",
          integer->spelling, integer->high_bit_literal, count));
      (void)memset(output, 0xc7u, sizeof(output));
      (void)memset(&result, 0xd8u, sizeof(result));
      const w_seed_native0_result result_snapshot = result;
      CHECK(run_source((const uint8_t *)invalid_source, invalid_length,
                       "logical-shift-out-of-width",
                       sizeof("logical-shift-out-of-width") - 1u,
                       output, sizeof(output), &result) != W_SEED_NATIVE0_OK);
      CHECK(memcmp(&result, &result_snapshot, sizeof(result)) == 0);
      for (size_t byte = 0u; byte < sizeof(output); byte += 1u)
        CHECK(output[byte] == 0xc7u);
    }
  }
  return true;
}

static bool test_u64_rotated_left_slice(void) {
  static const uint8_t source[] =
      "fn rotate(value: UInt, count: UInt): UInt { "
      "return u64.rotatedLeft(value, count) }\n"
      "fn main() { "
      "let zero = rotate(value: 1_u64, count: 0_u64) "
      "let edge = rotate(value: 1_u64, count: 63_u64) "
      "let width = rotate(value: 7_u64, count: 64_u64) "
      "let next = rotate(value: 1_u64, count: 65_u64) "
      "print(\"${zero}/${edge}/${width}/${next}\") }\n"
      "entry(main)\n";
  uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  CHECK(run_source(source, sizeof(source) - 1u, "uint-rotated-left", 17u,
                   output, sizeof(output), &result) == W_SEED_NATIVE0_OK);
  size_t rotated_count = 0u;
  for (size_t index = 0u; index < storage.hir_program.value_count;
       index += 1u)
    if (storage.hir_program.values[index].kind ==
            W_SEED_HIR0_VALUE_BINARY_U64 &&
        storage.hir_program.values[index].binary_operator ==
            W_SEED_HIR0_BINARY_ROTATED_LEFT)
      rotated_count += 1u;
  CHECK(rotated_count == 1u && storage.hir_program.call_count == 5u);
  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(
            &storage.hir_program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  CHECK(selection.has_local_calls && !selection.has_cfg &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "@w_seed_rotated_left_u64") &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "llvm.intr.fshl") == 1u &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "_rotate_mask = llvm.mlir.constant(63 : i64)") &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "_rotate_mod = llvm.and ") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "\"llvm.intr.trap\"() : () -> ()"));

  static const char *const rejected[] = {
      "fn bad(value: UInt, count: UInt): UInt { return "
      "UInt.rotatedLeft(value, count) }\nentry(bad)\n",
      "fn bad(value: UInt, count: UInt): UInt { return "
      "u64.rotatedLeft(value, 1_i64) }\nentry(bad)\n",
      "fn bad(value: UInt, count: UInt): UInt { return "
      "u64.rotatedLeft(1_i64, count) }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return "
      "u64.rotatedLeft(value) }\nentry(bad)\n",
      "fn bad(value: UInt, count: UInt): UInt { return "
      "u64.rotatedLeft(value, count, 1_u64) }\nentry(bad)\n",
      "fn bad(value: UInt, count: UInt): UInt { return "
      "u64.rotatedLeft(value: value, count) }\nentry(bad)\n",
      "fn bad(value: UInt, count: UInt): UInt { return "
      "u64?.rotatedLeft(value, count) }\nentry(bad)\n",
      "fn bad(value: UInt, count: UInt): UInt { return "
      "u64.rotatedLeft(u64.rotatedLeft(value, count), count) }\nentry(bad)\n",
      "const u64: UInt = 1_u64\n"
      "fn bad(value: UInt, count: UInt): UInt { return "
      "u64.rotatedLeft(value, count) }\nentry(bad)\n",
      "fn bad(value: UInt, count: UInt): Int { return "
      "u64.rotatedLeft(value, count) }\nentry(bad)\n"};
  for (size_t index = 0u; index < sizeof(rejected) / sizeof(rejected[0]);
       index += 1u) {
    (void)memset(output, 0xa3u, sizeof(output));
    (void)memset(&result, 0xa4u, sizeof(result));
    const w_seed_native0_result snapshot = result;
    CHECK(run_source((const uint8_t *)rejected[index], strlen(rejected[index]),
                     "uint-rotated-left-bad", 21u, output, sizeof(output),
                     &result) != W_SEED_NATIVE0_OK);
    CHECK(memcmp(&result, &snapshot, sizeof(result)) == 0);
    for (size_t byte = 0u; byte < sizeof(output); byte += 1u)
      CHECK(output[byte] == 0xa3u);
  }
  return true;
}

static bool test_u64_rotated_right_slice(void) {
  static const uint8_t source[] =
      "fn rotate(value: UInt, count: UInt): UInt { "
      "return u64.rotatedRight(value, count) }\n"
      "fn main() { "
      "let zero = rotate(value: 3_u64, count: 0_u64) "
      "let edge = rotate(value: 1_u64, count: 63_u64) "
      "let width = rotate(value: 7_u64, count: 64_u64) "
      "let next = rotate(value: 3_u64, count: 65_u64) "
      "print(\"${zero}/${edge}/${width}/${next}\") }\n"
      "entry(main)\n";
  uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  CHECK(run_source(source, sizeof(source) - 1u, "uint-rotated-right", 18u,
                   output, sizeof(output), &result) == W_SEED_NATIVE0_OK);
  size_t rotated_count = 0u;
  for (size_t index = 0u; index < storage.hir_program.value_count;
       index += 1u)
    if (storage.hir_program.values[index].kind ==
            W_SEED_HIR0_VALUE_BINARY_U64 &&
        storage.hir_program.values[index].binary_operator ==
            W_SEED_HIR0_BINARY_ROTATED_RIGHT)
      rotated_count += 1u;
  CHECK(rotated_count == 1u && storage.hir_program.call_count == 5u);
  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(
            &storage.hir_program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  CHECK(selection.has_local_calls && !selection.has_cfg &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "@w_seed_rotated_right_u64") &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "llvm.intr.fshr") == 1u &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "_rotate_mask = llvm.mlir.constant(63 : i64)") &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "_rotate_mod = llvm.and ") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "\"llvm.intr.trap\"() : () -> ()"));

  static const char *const rejected[] = {
      "fn bad(value: UInt, count: UInt): UInt { return "
      "UInt.rotatedRight(value, count) }\nentry(bad)\n",
      "fn bad(value: UInt, count: UInt): UInt { return "
      "u64.rotatedRight(value, 1_i64) }\nentry(bad)\n",
      "fn bad(value: UInt, count: UInt): UInt { return "
      "u64.rotatedRight(1_i64, count) }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return "
      "u64.rotatedRight(value) }\nentry(bad)\n",
      "fn bad(value: UInt, count: UInt): UInt { return "
      "u64.rotatedRight(value, count, 1_u64) }\nentry(bad)\n",
      "fn bad(value: UInt, count: UInt): UInt { return "
      "u64.rotatedRight(value: value, count) }\nentry(bad)\n",
      "fn bad(value: UInt, count: UInt): UInt { return "
      "u64?.rotatedRight(value, count) }\nentry(bad)\n",
      "fn bad(value: UInt, count: UInt): UInt { return "
      "u64.rotatedRight(u64.rotatedRight(value, count), count) }\nentry(bad)\n",
      "const u64: UInt = 1_u64\n"
      "fn bad(value: UInt, count: UInt): UInt { return "
      "u64.rotatedRight(value, count) }\nentry(bad)\n",
      "fn bad(value: UInt, count: UInt): Int { return "
      "u64.rotatedRight(value, count) }\nentry(bad)\n"};
  for (size_t index = 0u; index < sizeof(rejected) / sizeof(rejected[0]);
       index += 1u) {
    (void)memset(output, 0xa5u, sizeof(output));
    (void)memset(&result, 0xa6u, sizeof(result));
    const w_seed_native0_result snapshot = result;
    CHECK(run_source((const uint8_t *)rejected[index], strlen(rejected[index]),
                     "uint-rotated-right-bad", 22u, output, sizeof(output),
                     &result) != W_SEED_NATIVE0_OK);
    CHECK(memcmp(&result, &snapshot, sizeof(result)) == 0);
    for (size_t byte = 0u; byte < sizeof(output); byte += 1u)
      CHECK(output[byte] == 0xa5u);
  }
  return true;
}

static bool test_u64_count_ones_slice(void) {
  static const uint8_t source[] =
      "fn count(value: UInt): UInt { return u64.countOnes(value) }\n"
      "fn main() { let zero = count(value: 0_u64) "
      "let full = count(value: 18446744073709551615_u64) "
      "let pattern = count(value: 0xf0f0f0f00f0f0f0f_u64) "
      "print(\"${zero}/${full}/${pattern}\") }\n"
      "entry(main)\n";
  uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  CHECK(run_source(source, sizeof(source) - 1u, "uint-count-ones", 15u,
                   output, sizeof(output), &result) == W_SEED_NATIVE0_OK);
  size_t count_ones_count = 0u;
  for (size_t index = 0u; index < storage.hir_program.value_count;
       index += 1u)
    if (storage.hir_program.values[index].kind ==
            W_SEED_HIR0_VALUE_UNARY_U64 &&
        storage.hir_program.values[index].unary_operator ==
            W_SEED_HIR0_UNARY_COUNT_ONES)
      count_ones_count += 1u;
  CHECK(count_ones_count == 1u && storage.hir_program.call_count == 4u);
  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(
            &storage.hir_program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  CHECK(selection.has_local_calls && !selection.has_cfg &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "llvm.intr.ctpop") == 1u &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "\"llvm.intr.trap\"() : () -> ()"));

  static const char *const rejected[] = {
      "fn bad(value: UInt): UInt { return "
      "UInt.countOnes(value) }\nentry(bad)\n",
      "fn bad(value: Int): UInt { return "
      "u64.countOnes(value) }\nentry(bad)\n",
      "fn bad(): UInt { return u64.countOnes() }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return "
      "u64.countOnes(value, value) }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return "
      "u64.countOnes(value: value) }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return "
      "u64?.countOnes(value) }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return "
      "u64.countOnes(u64.countOnes(value)) }\nentry(bad)\n",
      "const u64: UInt = 1_u64\n"
      "fn bad(value: UInt): UInt { return "
      "u64.countOnes(value) }\nentry(bad)\n",
      "fn bad(value: UInt): Int { return "
      "u64.countOnes(value) }\nentry(bad)\n"};
  for (size_t index = 0u; index < sizeof(rejected) / sizeof(rejected[0]);
       index += 1u) {
    (void)memset(output, 0xa7u, sizeof(output));
    (void)memset(&result, 0xa8u, sizeof(result));
    const w_seed_native0_result snapshot = result;
    CHECK(run_source((const uint8_t *)rejected[index], strlen(rejected[index]),
                     "uint-count-ones-bad", 19u, output, sizeof(output),
                     &result) != W_SEED_NATIVE0_OK);
    CHECK(memcmp(&result, &snapshot, sizeof(result)) == 0);
    for (size_t byte = 0u; byte < sizeof(output); byte += 1u)
      CHECK(output[byte] == 0xa7u);
  }
  return true;
}

static bool test_u64_count_zeros_slice(void) {
  static const uint8_t source[] =
      "fn count(value: UInt): UInt { return u64.countZeros(value) }\n"
      "fn main() { let zero = count(value: 0_u64) "
      "let full = count(value: 18446744073709551615_u64) "
      "let pattern = count(value: 0xf0f0f0f00f0f0f0f_u64) "
      "print(\"${zero}/${full}/${pattern}\") }\n"
      "entry(main)\n";
  uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  CHECK(run_source(source, sizeof(source) - 1u, "uint-count-zeros", 16u,
                   output, sizeof(output), &result) == W_SEED_NATIVE0_OK);
  size_t count_zeros_count = 0u;
  for (size_t index = 0u; index < storage.hir_program.value_count;
       index += 1u)
    if (storage.hir_program.values[index].kind ==
            W_SEED_HIR0_VALUE_UNARY_U64 &&
        storage.hir_program.values[index].unary_operator ==
            W_SEED_HIR0_UNARY_COUNT_ZEROS)
      count_zeros_count += 1u;
  CHECK(count_zeros_count == 1u && storage.hir_program.call_count == 4u);
  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(
            &storage.hir_program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  CHECK(selection.has_local_calls && !selection.has_cfg &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "llvm.intr.ctpop") == 1u &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "_bit_width = llvm.mlir.constant(64 : i64)") == 1u &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "_bit_ones : i64") == 1u &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "\"llvm.intr.trap\"() : () -> ()"));

  static const char *const rejected[] = {
      "fn bad(value: UInt): UInt { return "
      "UInt.countZeros(value) }\nentry(bad)\n",
      "fn bad(value: Int): UInt { return "
      "u64.countZeros(value) }\nentry(bad)\n",
      "fn bad(): UInt { return u64.countZeros() }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return "
      "u64.countZeros(value, value) }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return "
      "u64.countZeros(value: value) }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return "
      "u64?.countZeros(value) }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return "
      "u64.countZeros(u64.countZeros(value)) }\nentry(bad)\n",
      "const u64: UInt = 1_u64\n"
      "fn bad(value: UInt): UInt { return "
      "u64.countZeros(value) }\nentry(bad)\n",
      "fn bad(value: UInt): Int { return "
      "u64.countZeros(value) }\nentry(bad)\n"};
  for (size_t index = 0u; index < sizeof(rejected) / sizeof(rejected[0]);
       index += 1u) {
    (void)memset(output, 0xa9u, sizeof(output));
    (void)memset(&result, 0xaau, sizeof(result));
    const w_seed_native0_result snapshot = result;
    CHECK(run_source((const uint8_t *)rejected[index], strlen(rejected[index]),
                     "uint-count-zeros-bad", 20u, output, sizeof(output),
                     &result) != W_SEED_NATIVE0_OK);
    CHECK(memcmp(&result, &snapshot, sizeof(result)) == 0);
    for (size_t byte = 0u; byte < sizeof(output); byte += 1u)
      CHECK(output[byte] == 0xa9u);
  }
  return true;
}

static bool test_u64_count_leading_zeros_slice(void) {
  static const uint8_t source[] =
      "fn count(value: UInt): UInt { return u64.countLeadingZeros(value) }\n"
      "fn main() { let zero = count(value: 0_u64) "
      "let one = count(value: 1_u64) "
      "let pattern = count(value: 0xf0_u64) "
      "print(\"${zero}/${one}/${pattern}\") }\n"
      "entry(main)\n";
  uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  CHECK(run_source(source, sizeof(source) - 1u, "uint-leading-zeros", 18u,
                   output, sizeof(output), &result) == W_SEED_NATIVE0_OK);
  size_t count_leading_count = 0u;
  for (size_t index = 0u; index < storage.hir_program.value_count;
       index += 1u)
    if (storage.hir_program.values[index].kind ==
            W_SEED_HIR0_VALUE_UNARY_U64 &&
        storage.hir_program.values[index].unary_operator ==
            W_SEED_HIR0_UNARY_COUNT_LEADING_ZEROS)
      count_leading_count += 1u;
  CHECK(count_leading_count == 1u && storage.hir_program.call_count == 4u);
  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(
            &storage.hir_program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  CHECK(selection.has_local_calls && !selection.has_cfg &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "llvm.intr.ctlz") == 1u &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "is_zero_poison = false") == 1u &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "\"llvm.intr.trap\"() : () -> ()"));

  static const char *const rejected[] = {
      "fn bad(value: UInt): UInt { return "
      "UInt.countLeadingZeros(value) }\nentry(bad)\n",
      "fn bad(value: Int): UInt { return "
      "u64.countLeadingZeros(value) }\nentry(bad)\n",
      "fn bad(): UInt { return u64.countLeadingZeros() }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return "
      "u64.countLeadingZeros(value, value) }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return "
      "u64.countLeadingZeros(value: value) }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return "
      "u64?.countLeadingZeros(value) }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return "
      "u64.countLeadingZeros(u64.countLeadingZeros(value)) }\nentry(bad)\n",
      "const u64: UInt = 1_u64\n"
      "fn bad(value: UInt): UInt { return "
      "u64.countLeadingZeros(value) }\nentry(bad)\n",
      "fn bad(value: UInt): Int { return "
      "u64.countLeadingZeros(value) }\nentry(bad)\n"};
  for (size_t index = 0u; index < sizeof(rejected) / sizeof(rejected[0]);
       index += 1u) {
    (void)memset(output, 0xabu, sizeof(output));
    (void)memset(&result, 0xacu, sizeof(result));
    const w_seed_native0_result snapshot = result;
    CHECK(run_source((const uint8_t *)rejected[index], strlen(rejected[index]),
                     "uint-leading-zeros-bad", 22u, output, sizeof(output),
                     &result) != W_SEED_NATIVE0_OK);
    CHECK(memcmp(&result, &snapshot, sizeof(result)) == 0);
    for (size_t byte = 0u; byte < sizeof(output); byte += 1u)
      CHECK(output[byte] == 0xabu);
  }
  return true;
}

static bool test_u64_count_trailing_zeros_slice(void) {
  static const uint8_t source[] =
      "fn count(value: UInt): UInt { return u64.countTrailingZeros(value) }\n"
      "fn main() { let zero = count(value: 0_u64) "
      "let one = count(value: 1_u64) "
      "let pattern = count(value: 0xf000_u64) "
      "print(\"${zero}/${one}/${pattern}\") }\n"
      "entry(main)\n";
  uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  CHECK(run_source(source, sizeof(source) - 1u, "uint-trailing-zeros", 19u,
                   output, sizeof(output), &result) == W_SEED_NATIVE0_OK);
  size_t count_trailing_count = 0u;
  for (size_t index = 0u; index < storage.hir_program.value_count;
       index += 1u)
    if (storage.hir_program.values[index].kind ==
            W_SEED_HIR0_VALUE_UNARY_U64 &&
        storage.hir_program.values[index].unary_operator ==
            W_SEED_HIR0_UNARY_COUNT_TRAILING_ZEROS)
      count_trailing_count += 1u;
  CHECK(count_trailing_count == 1u && storage.hir_program.call_count == 4u);
  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(
            &storage.hir_program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  CHECK(selection.has_local_calls && !selection.has_cfg &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "llvm.intr.cttz") == 1u &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "is_zero_poison = false") == 1u &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "\"llvm.intr.trap\"() : () -> ()"));

  static const char *const rejected[] = {
      "fn bad(value: UInt): UInt { return "
      "UInt.countTrailingZeros(value) }\nentry(bad)\n",
      "fn bad(value: Int): UInt { return "
      "u64.countTrailingZeros(value) }\nentry(bad)\n",
      "fn bad(): UInt { return u64.countTrailingZeros() }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return "
      "u64.countTrailingZeros(value, value) }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return "
      "u64.countTrailingZeros(value: value) }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return "
      "u64?.countTrailingZeros(value) }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return "
      "u64.countTrailingZeros(u64.countTrailingZeros(value)) }\nentry(bad)\n",
      "const u64: UInt = 1_u64\n"
      "fn bad(value: UInt): UInt { return "
      "u64.countTrailingZeros(value) }\nentry(bad)\n",
      "fn bad(value: UInt): Int { return "
      "u64.countTrailingZeros(value) }\nentry(bad)\n"};
  for (size_t index = 0u; index < sizeof(rejected) / sizeof(rejected[0]);
       index += 1u) {
    (void)memset(output, 0xabu, sizeof(output));
    (void)memset(&result, 0xacu, sizeof(result));
    const w_seed_native0_result snapshot = result;
    CHECK(run_source((const uint8_t *)rejected[index], strlen(rejected[index]),
                     "uint-trailing-zeros-bad", 23u, output, sizeof(output),
                     &result) != W_SEED_NATIVE0_OK);
    CHECK(memcmp(&result, &snapshot, sizeof(result)) == 0);
    for (size_t byte = 0u; byte < sizeof(output); byte += 1u)
      CHECK(output[byte] == 0xabu);
  }
  return true;
}

static bool test_u64_reversed_bits_slice(void) {
  static const uint8_t source[] =
      "fn reverse(value: UInt): UInt { return u64.reversedBits(value) }\n"
      "fn main() { let zero = reverse(value: 0_u64) "
      "let pattern = reverse(value: 0x0123456789abcdef_u64) "
      "print(\"${zero}/${pattern}\") }\n"
      "entry(main)\n";
  uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  CHECK(run_source(source, sizeof(source) - 1u, "uint-reversed-bits", 18u,
                   output, sizeof(output), &result) == W_SEED_NATIVE0_OK);
  size_t reversed_count = 0u;
  for (size_t index = 0u; index < storage.hir_program.value_count;
       index += 1u)
    if (storage.hir_program.values[index].kind ==
            W_SEED_HIR0_VALUE_UNARY_U64 &&
        storage.hir_program.values[index].unary_operator ==
            W_SEED_HIR0_UNARY_REVERSED_BITS)
      reversed_count += 1u;
  CHECK(reversed_count == 1u && storage.hir_program.call_count == 3u);
  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(
            &storage.hir_program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  CHECK(selection.has_local_calls && !selection.has_cfg &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "llvm.intr.bitreverse") == 1u &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "\"llvm.intr.trap\"() : () -> ()"));

  static const char *const rejected[] = {
      "fn bad(value: UInt): UInt { return "
      "UInt.reversedBits(value) }\nentry(bad)\n",
      "fn bad(value: Int): UInt { return "
      "u64.reversedBits(value) }\nentry(bad)\n",
      "fn bad(): UInt { return u64.reversedBits() }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return "
      "u64.reversedBits(value, value) }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return "
      "u64.reversedBits(value: value) }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return "
      "u64?.reversedBits(value) }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return "
      "u64.reversedBits(u64.reversedBits(value)) }\nentry(bad)\n",
      "const u64: UInt = 1_u64\n"
      "fn bad(value: UInt): UInt { return "
      "u64.reversedBits(value) }\nentry(bad)\n",
      "fn bad(value: UInt): Int { return "
      "u64.reversedBits(value) }\nentry(bad)\n"};
  for (size_t index = 0u; index < sizeof(rejected) / sizeof(rejected[0]);
       index += 1u) {
    (void)memset(output, 0xabu, sizeof(output));
    (void)memset(&result, 0xacu, sizeof(result));
    const w_seed_native0_result snapshot = result;
    CHECK(run_source((const uint8_t *)rejected[index], strlen(rejected[index]),
                     "uint-reversed-bits-bad", 22u, output, sizeof(output),
                     &result) != W_SEED_NATIVE0_OK);
    CHECK(memcmp(&result, &snapshot, sizeof(result)) == 0);
    for (size_t byte = 0u; byte < sizeof(output); byte += 1u)
      CHECK(output[byte] == 0xabu);
  }
  return true;
}

static bool test_u64_reversed_bytes_slice(void) {
  static const uint8_t source[] =
      "fn reverse(value: UInt): UInt { return u64.reversedBytes(value) }\n"
      "fn main() { let zero = reverse(value: 0_u64) "
      "let pattern = reverse(value: 0x0123456789abcdef_u64) "
      "print(\"${zero}/${pattern}\") }\n"
      "entry(main)\n";
  uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  CHECK(run_source(source, sizeof(source) - 1u, "uint-reversed-bytes", 19u,
                   output, sizeof(output), &result) == W_SEED_NATIVE0_OK);
  size_t reversed_count = 0u;
  for (size_t index = 0u; index < storage.hir_program.value_count;
       index += 1u)
    if (storage.hir_program.values[index].kind ==
            W_SEED_HIR0_VALUE_UNARY_U64 &&
        storage.hir_program.values[index].unary_operator ==
            W_SEED_HIR0_UNARY_REVERSED_BYTES)
      reversed_count += 1u;
  CHECK(reversed_count == 1u && storage.hir_program.call_count == 3u);
  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(
            &storage.hir_program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  CHECK(selection.has_local_calls && !selection.has_cfg &&
        count_bytes(output, result.mlir.written.mlir_bytes,
                    "llvm.intr.bswap") == 1u &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "\"llvm.intr.trap\"() : () -> ()"));

  static const char *const rejected[] = {
      "fn bad(value: UInt): UInt { return "
      "UInt.reversedBytes(value) }\nentry(bad)\n",
      "fn bad(value: Int): UInt { return "
      "u64.reversedBytes(value) }\nentry(bad)\n",
      "fn bad(): UInt { return u64.reversedBytes() }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return "
      "u64.reversedBytes(value, value) }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return "
      "u64.reversedBytes(value: value) }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return "
      "u64?.reversedBytes(value) }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return "
      "u64.reversedBytes(u64.reversedBytes(value)) }\nentry(bad)\n",
      "const u64: UInt = 1_u64\n"
      "fn bad(value: UInt): UInt { return "
      "u64.reversedBytes(value) }\nentry(bad)\n",
      "fn bad(value: UInt): Int { return "
      "u64.reversedBytes(value) }\nentry(bad)\n"};
  for (size_t index = 0u; index < sizeof(rejected) / sizeof(rejected[0]);
       index += 1u) {
    (void)memset(output, 0xabu, sizeof(output));
    (void)memset(&result, 0xacu, sizeof(result));
    const w_seed_native0_result snapshot = result;
    CHECK(run_source((const uint8_t *)rejected[index], strlen(rejected[index]),
                     "uint-reversed-bytes-bad", 23u, output, sizeof(output),
                     &result) != W_SEED_NATIVE0_OK);
    CHECK(memcmp(&result, &snapshot, sizeof(result)) == 0);
    for (size_t byte = 0u; byte < sizeof(output); byte += 1u)
      CHECK(output[byte] == 0xabu);
  }
  return true;
}

static bool test_u64_wrapping_negate_slice(void) {
  static const uint8_t source[] =
      "fn wrap(value: UInt): UInt { return u64.wrappingNegate(value) }\n"
      "fn main() { let result = wrap(value: 1_u64) print(\"${result}\") }\n"
      "entry(main)\n";
  uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  CHECK(run_source(source, sizeof(source) - 1u, "uint-wrapping-negate", 20u,
                   output, sizeof(output), &result) == W_SEED_NATIVE0_OK);
  size_t wrapping_count = 0u;
  for (size_t index = 0u; index < storage.hir_program.value_count;
       index += 1u)
    if (storage.hir_program.values[index].kind ==
            W_SEED_HIR0_VALUE_UNARY_U64 &&
        storage.hir_program.values[index].unary_operator ==
            W_SEED_HIR0_UNARY_WRAPPING_NEGATE)
      wrapping_count += 1u;
  const size_t wrapper_start =
      find_bytes(output, result.mlir.written.mlir_bytes,
                 "llvm.func internal @w_fn_0", 0u);
  const size_t entry_start =
      find_bytes(output, result.mlir.written.mlir_bytes,
                 "llvm.func internal @w_fn_1", wrapper_start);
  CHECK(wrapping_count == 1u && wrapper_start != SIZE_MAX &&
        entry_start != SIZE_MAX &&
        count_bytes(output + wrapper_start, entry_start - wrapper_start,
                    "_wrapping_negate_zero = llvm.mlir.constant(0 : i64)") ==
            1u &&
        count_bytes(output + wrapper_start, entry_start - wrapper_start,
                    " = llvm.sub ") == 1u &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "llvm.call @w_seed_checked_subtract_u64") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "llvm.intr.usub.with.overflow"));

  static const uint8_t rejected[][220] = {
      "fn bad(value: UInt): UInt { return UInt.wrappingNegate(value) }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return u64.wrappingNegate(1_i64) }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return u64.wrappingNegate() }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return u64.wrappingNegate(value, 1_u64) }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return u64.wrappingNegate(value: value) }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return u64?.wrappingNegate(value) }\nentry(bad)\n",
      "fn bad(value: UInt): UInt { return u64.wrappingNegate(u64.wrappingNegate(value)) }\nentry(bad)\n",
      "const u64: UInt = 1_u64\nfn bad(value: UInt): UInt { return u64.wrappingNegate(value) }\nentry(bad)\n"};
  for (size_t index = 0u; index < sizeof(rejected) / sizeof(rejected[0]);
       index += 1u) {
    (void)memset(output, 0x97u, sizeof(output));
    (void)memset(&result, 0x98u, sizeof(result));
    const w_seed_native0_result snapshot = result;
    CHECK(run_source(rejected[index], strlen((const char *)rejected[index]),
                     "uint-wrapping-negate-bad", 24u, output, sizeof(output),
                     &result) != W_SEED_NATIVE0_OK);
    CHECK(memcmp(&result, &snapshot, sizeof(result)) == 0);
    for (size_t byte = 0u; byte < sizeof(output); byte += 1u)
      CHECK(output[byte] == 0x97u);
  }
  return true;
}

static bool test_fixed_integer_bit_primitive_width_slice(void) {
  static const struct {
    const char *type;
    const char *pattern;
    const char *zero;
    const char *negative;
    const char *rotate_count;
    uint16_t bit_width;
    bool is_signed;
  } INTEGERS[] = {
      {"i8", "0x52_i8", "0_i8", "~1_i8", "9_u64", 8u, true},
      {"u8", "0x96_u8", "0_u8", "0x96_u8", "9_u64", 8u, false},
      {"i16", "0x1234_i16", "0_i16", "~1_i16", "17_u64", 16u, true},
      {"u16", "0x89ab_u16", "0_u16", "0x89ab_u16", "17_u64", 16u,
       false},
      {"i32", "0x12345678_i32", "0_i32", "~1_i32", "33_u64", 32u,
       true},
      {"u32", "0x89abcdef_u32", "0_u32", "0x89abcdef_u32", "33_u64",
       32u, false},
      {"i64", "0x0123456789abcd6e_i64", "0_i64", "~1_i64", "65_u64",
       64u, true},
      {"u64", "0xfedcba9876543210_u64", "0_u64",
       "0xfedcba9876543210_u64", "65_u64", 64u, false},
  };
  static const char *const OPERATIONS[] = {
      "countOnes",      "countZeros",      "countLeadingZeros",
      "countTrailingZeros", "reversedBits", "reversedBytes",
      "rotatedLeft",    "rotatedRight",
  };
  uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  for (size_t integer = 0u;
       integer < sizeof(INTEGERS) / sizeof(INTEGERS[0]); integer += 1u) {
    char source[W_SEED_NATIVE0_MAX_SOURCE_BYTES];
    size_t source_length = 0u;
    CHECK(append_source_text(source, sizeof(source), &source_length,
                             "fn bitMatrix() { "));
    size_t binding = 0u;
    for (size_t operation = 0u;
         operation < sizeof(OPERATIONS) / sizeof(OPERATIONS[0]);
         operation += 1u) {
      const char *argument = INTEGERS[integer].pattern;
      if (operation == 2u || operation == 3u)
        argument = INTEGERS[integer].zero;
      else if ((operation == 4u || operation == 5u) &&
               INTEGERS[integer].is_signed)
        argument = INTEGERS[integer].negative;
      char line[160];
      const bool rotation = operation == 6u || operation == 7u;
      const int line_length =
          rotation
              ? snprintf(line, sizeof(line),
                         "let bit%u = %s.%s(%s, %s) ",
                         (unsigned)binding,
                         INTEGERS[integer].type, OPERATIONS[operation],
                         argument, INTEGERS[integer].rotate_count)
              : snprintf(line, sizeof(line), "let bit%u = %s.%s(%s) ",
                         (unsigned)binding, INTEGERS[integer].type,
                         OPERATIONS[operation], argument);
      CHECK(line_length > 0 && (size_t)line_length < sizeof(line) &&
            append_source_text(source, sizeof(source), &source_length, line));
      binding += 1u;
    }
    CHECK(binding == 8u &&
          append_source_text(source, sizeof(source), &source_length,
                             "print(\""));
    for (size_t index = 0u; index < binding; index += 1u) {
      char interpolation[32];
      const int interpolation_length = snprintf(
          interpolation, sizeof(interpolation), "${bit%u}%s",
          (unsigned)index, index + 1u == binding ? "" : "/");
      CHECK(interpolation_length > 0 &&
            (size_t)interpolation_length < sizeof(interpolation) &&
            append_source_text(source, sizeof(source), &source_length,
                               interpolation));
    }
    CHECK(append_source_text(source, sizeof(source), &source_length,
                             "\") }\nentry(bitMatrix)\n"));

    w_seed_native0_result result;
    CHECK(run_source((const uint8_t *)source, source_length,
                     "fixed-bit-primitives",
                     sizeof("fixed-bit-primitives") - 1u, output,
                     sizeof(output), &result) == W_SEED_NATIVE0_OK);
    w_seed_native_subset0_program selection;
    CHECK(w_seed_native_subset0_select_program(
              &storage.hir_program, &storage.hir_result, &selection) ==
          W_SEED_NATIVE_SUBSET0_OK);
    size_t unary_count = 0u;
    size_t rotated_left_count = 0u;
    size_t rotated_right_count = 0u;
    for (size_t index = 0u; index < storage.hir_program.value_count;
         index += 1u) {
      const w_seed_hir0_value *value = &storage.hir_program.values[index];
      if ((value->kind == W_SEED_HIR0_VALUE_UNARY_I64 ||
           value->kind == W_SEED_HIR0_VALUE_UNARY_U64) &&
          value->unary_operator >= W_SEED_HIR0_UNARY_COUNT_ONES &&
          value->unary_operator <= W_SEED_HIR0_UNARY_REVERSED_BYTES)
        unary_count += 1u;
      if ((value->kind == W_SEED_HIR0_VALUE_BINARY_I64 ||
           value->kind == W_SEED_HIR0_VALUE_BINARY_U64) &&
          value->binary_operator == W_SEED_HIR0_BINARY_ROTATED_LEFT)
        rotated_left_count += 1u;
      if ((value->kind == W_SEED_HIR0_VALUE_BINARY_I64 ||
           value->kind == W_SEED_HIR0_VALUE_BINARY_U64) &&
          value->binary_operator == W_SEED_HIR0_BINARY_ROTATED_RIGHT)
        rotated_right_count += 1u;
    }
    CHECK(unary_count == 6u && rotated_left_count == 1u &&
          rotated_right_count == 1u && !selection.has_local_calls &&
          selection.has_interpolation && !selection.has_cfg);
    const size_t mlir_bytes = result.mlir.written.mlir_bytes;
    CHECK(count_bytes(output, mlir_bytes, "llvm.intr.fshl") == 1u &&
          count_bytes(output, mlir_bytes, "llvm.intr.fshr") == 1u &&
          count_bytes(output, mlir_bytes, "llvm.intr.ctpop") == 2u &&
          count_bytes(output, mlir_bytes, "llvm.intr.ctlz") == 1u &&
          count_bytes(output, mlir_bytes, "llvm.intr.cttz") == 1u &&
          count_bytes(output, mlir_bytes, "llvm.intr.bitreverse") == 1u &&
          count_bytes(output, mlir_bytes, "llvm.intr.bswap") ==
              (INTEGERS[integer].bit_width == 8u ? 0u : 1u) &&
          count_bytes(output, mlir_bytes, "is_zero_poison = false") == 2u &&
          !contains_bytes(output, mlir_bytes,
                          "@w_seed_rotated_left_u64") &&
          !contains_bytes(output, mlir_bytes,
                          "@w_seed_rotated_right_u64") &&
          !contains_bytes(output, mlir_bytes,
                          "\"llvm.intr.trap\"() : () -> ()"));

    char type_suffix[40];
    char zero_poison_suffix[96];
    char width_constant[80];
    char rotate_mask[80];
    const int type_suffix_length = snprintf(
        type_suffix, sizeof(type_suffix), ") : (i%u) -> i%u",
        (unsigned)INTEGERS[integer].bit_width,
        (unsigned)INTEGERS[integer].bit_width);
    const int zero_poison_suffix_length = snprintf(
        zero_poison_suffix, sizeof(zero_poison_suffix),
        ") <{is_zero_poison = false}> : (i%u) -> i%u",
        (unsigned)INTEGERS[integer].bit_width,
        (unsigned)INTEGERS[integer].bit_width);
    const int width_constant_length = snprintf(
        width_constant, sizeof(width_constant),
        "_bit_width = llvm.mlir.constant(%u : i%u)",
        (unsigned)INTEGERS[integer].bit_width,
        (unsigned)INTEGERS[integer].bit_width);
    const int rotate_mask_length = snprintf(
        rotate_mask, sizeof(rotate_mask),
        "_rotate_mask = llvm.mlir.constant(%u : i64)",
        (unsigned)INTEGERS[integer].bit_width - 1u);
    CHECK(type_suffix_length > 0 &&
          (size_t)type_suffix_length < sizeof(type_suffix) &&
          zero_poison_suffix_length > 0 &&
          (size_t)zero_poison_suffix_length < sizeof(zero_poison_suffix) &&
          width_constant_length > 0 &&
          (size_t)width_constant_length < sizeof(width_constant) &&
          rotate_mask_length > 0 &&
          (size_t)rotate_mask_length < sizeof(rotate_mask));
    const size_t width = INTEGERS[integer].bit_width;
    CHECK(count_mlir_lines_with_fragment_and_type(
              output, mlir_bytes, "llvm.intr.ctpop", type_suffix) == 2u &&
          count_mlir_lines_with_fragment_and_type(
              output, mlir_bytes, "llvm.intr.ctlz",
              zero_poison_suffix) == 1u &&
          count_mlir_lines_with_fragment_and_type(
              output, mlir_bytes, "llvm.intr.cttz",
              zero_poison_suffix) == 1u &&
          count_mlir_lines_with_fragment_and_type(
              output, mlir_bytes, "llvm.intr.bitreverse", type_suffix) ==
              1u &&
          count_mlir_lines_with_fragment_and_type(
              output, mlir_bytes, "llvm.intr.bswap", type_suffix) ==
              (width == 8u ? 0u : 1u));
    char rotation_suffix[64];
    const int rotation_suffix_length = snprintf(
        rotation_suffix, sizeof(rotation_suffix),
        ") : (i%u, i%u, i%u) -> i%u", (unsigned)width,
        (unsigned)width, (unsigned)width, (unsigned)width);
    CHECK(rotation_suffix_length > 0 &&
          (size_t)rotation_suffix_length < sizeof(rotation_suffix) &&
          count_mlir_lines_with_fragment_and_type(
              output, mlir_bytes, "llvm.intr.fshl", rotation_suffix) == 1u &&
          count_mlir_lines_with_fragment_and_type(
              output, mlir_bytes, "llvm.intr.fshr", rotation_suffix) == 1u &&
          count_bytes(output, mlir_bytes, width_constant) == 1u &&
          count_bytes(output, mlir_bytes, rotate_mask) == 2u);
    CHECK(count_bytes(output, mlir_bytes, "_bit_input = llvm.trunc ") ==
              (width < 64u ? 6u : 0u) &&
          count_bytes(output, mlir_bytes, "_rotate_input = llvm.trunc ") ==
              (width < 64u ? 2u : 0u) &&
          count_bytes(output, mlir_bytes, "llvm.zext %v") ==
              (width == 64u ? 0u : (INTEGERS[integer].is_signed ? 4u : 8u)) &&
          count_bytes(output, mlir_bytes, "llvm.sext %v") ==
              (width < 64u && INTEGERS[integer].is_signed ? 6u : 0u) &&
          count_bytes(output, mlir_bytes, "_bit_raw = llvm.or ") ==
              (width == 8u ? 1u : 0u));
  }
  return true;
}

static bool test_unsigned_unary_u64_slice(void) {
  static const uint8_t source[] =
      "fn invert(value: UInt): UInt { return ~value }\n"
      "fn main() { let high = ~9223372036854775808_u64 "
      "let all = ~18446744073709551615_u64 "
      "let zero = ~0_u64 "
      "let runtime = invert(value: high) "
      "print(\"${high}/${all}/${zero}/${runtime}\") }\n"
      "entry(main)\n";
  static uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  CHECK(run_source(source, sizeof(source) - 1u, "uint-unary", 10u, output,
                   sizeof(output), &result) == W_SEED_NATIVE0_OK);

  size_t unary_u64_count = 0u;
  for (size_t index = 0u; index < storage.hir_program.value_count;
       index += 1u)
    if (storage.hir_program.values[index].kind ==
        W_SEED_HIR0_VALUE_UNARY_U64)
      unary_u64_count += 1u;
  CHECK(unary_u64_count == 4u);

  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(
            &storage.hir_program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  CHECK(!selection.has_cfg && selection.has_local_calls &&
        selection.has_interpolation && selection.maximum_stdout_bytes == 84u);
  CHECK(count_bytes(output, result.mlir.written.mlir_bytes, "llvm.xor") >=
            unary_u64_count &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "llvm.mlir.constant(-9223372036854775808 : i64) : i64") &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "llvm.mlir.constant(-1 : i64) : i64"));

  /* A U64 unary value cannot widen the existing scalar CFG subset. */
  static const uint8_t cfg_source[] =
      "fn choose(flag: Bool, value: UInt): UInt { return if flag { ~value } "
      "else { value } }\n"
      "fn main() { let value = choose(flag: true, value: 0_u64) "
      "print(\"${value}\") }\n"
      "entry(main)\n";
  (void)memset(output, 0x81u, sizeof(output));
  (void)memset(&result, 0x82u, sizeof(result));
  const w_seed_native0_result cfg_snapshot = result;
  CHECK(run_source(cfg_source, sizeof(cfg_source) - 1u, "uint-unary-cfg", 14u,
                   output, sizeof(output), &result) != W_SEED_NATIVE0_OK);
  CHECK(memcmp(&result, &cfg_snapshot, sizeof(result)) == 0);
  for (size_t byte = 0u; byte < sizeof(output); byte += 1u)
    CHECK(output[byte] == 0x81u);

  /* The loop recognizers remain i64-carrier-only even when the U64 value is
   * otherwise a valid linear expression. */
  static const uint8_t loop_source[] =
      "fn looped(limit: i64, value: UInt) { var index = 0 "
      "while index < limit { let inverted = ~value index = index + 1 } }\n"
      "fn main() { looped(limit: 1, value: 0_u64) print(\"ok\") }\n"
      "entry(main)\n";
  (void)memset(output, 0x91u, sizeof(output));
  (void)memset(&result, 0x92u, sizeof(result));
  const w_seed_native0_result loop_snapshot = result;
  CHECK(run_source(loop_source, sizeof(loop_source) - 1u,
                   "uint-unary-loop", 15u, output, sizeof(output), &result) !=
        W_SEED_NATIVE0_OK);
  CHECK(memcmp(&result, &loop_snapshot, sizeof(result)) == 0);
  for (size_t byte = 0u; byte < sizeof(output); byte += 1u)
    CHECK(output[byte] == 0x91u);

  /* UInt negation is invalid at the source/HIR boundary and must not reach
   * Native0's emitter. */
  static const uint8_t invalid_source[] =
      "fn invalid(value: UInt): UInt { return -value }\n"
      "entry(invalid)\n";
  (void)memset(output, 0xa1u, sizeof(output));
  (void)memset(&result, 0xa2u, sizeof(result));
  const w_seed_native0_result invalid_snapshot = result;
  CHECK(run_source(invalid_source, sizeof(invalid_source) - 1u,
                   "uint-unary-invalid", 18u, output, sizeof(output),
                   &result) != W_SEED_NATIVE0_OK);
  CHECK(memcmp(&result, &invalid_snapshot, sizeof(result)) == 0);
  for (size_t byte = 0u; byte < sizeof(output); byte += 1u)
    CHECK(output[byte] == 0xa1u);
  return true;
}

static bool test_virtual_structured_task_product(void) {
  static const uint8_t source[] =
      "fn prepare(value: i64): i64 { return value }\n"
      "entry { let left = async prepare(value: 20) "
      "let right = async prepare(value: 22) "
      "let first = await left let second = await right "
      "print(\"Prepared ${first + second}\") }\n";
  static uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  const w_seed_native0_status status =
      run_source(source, sizeof(source) - 1u, "async-join", 10u, output,
                 sizeof(output), &result);
  CHECK(status == W_SEED_NATIVE0_OK);
  CHECK(contains_bytes(output, result.mlir.written.mlir_bytes,
                       "llvm.call @w_fn_0"));
  CHECK(!contains_bytes(output, result.mlir.written.mlir_bytes, "@w_task"));
  CHECK(!contains_bytes(output, result.mlir.written.mlir_bytes,
                        "@w_async_"));
  return true;
}

static bool test_virtual_static_yield_helper_product(void) {
  static const uint8_t source[] =
      "fn stage(value: i64): i64 { return value + 1 }\n"
      "async fn prepare(value: i64): i64 { "
      "let staged = stage(value: value) await execution#yield() "
      "let doubled = staged * 2 await execution#yield() "
      "return doubled }\n"
      "entry { let pending = async prepare(value: 43) "
      "let result = await pending print(\"Prepared ${result}\") }\n";
  static uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  const w_seed_native0_status status =
      run_source(source, sizeof(source) - 1u, "async-yield-helper", 18u,
                 output, sizeof(output), &result);
  CHECK(status == W_SEED_NATIVE0_OK);
  CHECK(storage.hir_program.function_count == 3u &&
        storage.hir_program.functions[1].is_async &&
        storage.hir_program.functions[1].suspension ==
            W_SEED_HIR0_SUSPENSION_MAY &&
        storage.hir_program.functions[1].direct_entry ==
            W_SEED_HIR0_DIRECT_ENTRY_ABSENT &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "llvm.call @w_fn_0") &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "llvm.call @w_fn_1") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes, "@w_task") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes,
                        "@w_async_") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes, "@WRT") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes, "yield"));
  return true;
}

static bool test_async_direct_entry_product(void) {
  static const uint8_t source[] =
      "async fn pause(value: i64): i64 { return value }\n"
      "entry { let pending = async pause(value: 42) "
      "let result = await pending print(\"Resumed ${result}\") }\n";
  static uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  const w_seed_native0_status status =
      run_source(source, sizeof(source) - 1u, "async-direct-entry", 17u,
                 output, sizeof(output), &result);
  CHECK(status == W_SEED_NATIVE0_OK);
  CHECK(storage.hir_program.functions[0].is_async &&
        storage.hir_program.functions[0].direct_entry ==
            W_SEED_HIR0_DIRECT_ENTRY_AVAILABLE &&
        contains_bytes(output, result.mlir.written.mlir_bytes,
                       "llvm.call @w_fn_0") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes, "@w_task") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes, "@w_async_") &&
        !contains_bytes(output, result.mlir.written.mlir_bytes, "@WRT"));
  return true;
}

static bool test_process_parallel_native_admission(void) {
  static const uint8_t source[] =
      "import { Arguments as ProcessArguments, Context as ProcessContext, "
      "ExitCode as ProcessExitCode } from std.process\n"
      "fn select(missing: Bool): i64 { return if missing { 0 } else { 40 } }\n"
      "fn increment(value: i64): i64 { return value + 1 }\n"
      "async fn run(args: ProcessArguments, ctx: ProcessContext): "
      "ProcessExitCode { let seed = select(missing: args.isEmpty) "
      "let pending = spawn<.domain> increment(value: seed) "
      "let value = await pending return .success }\n"
      "entry(run)\n";
  static uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  (void)memset(output, 0xa7, sizeof(output));
  uint8_t output_before[sizeof(output)];
  (void)memcpy(output_before, output, sizeof(output));
  w_seed_native0_result result;
  (void)memset(&result, 0x6d, sizeof(result));
  const w_seed_native0_result result_before = result;
  const w_seed_native0_status status =
      run_source(source, sizeof(source) - 1u, "process-parallel", 16u, output,
                 sizeof(output), &result);
  CHECK(status == W_SEED_NATIVE0_UNSUPPORTED);
  CHECK(memcmp(output, output_before, sizeof(output)) == 0);
  CHECK(memcmp(&result, &result_before, sizeof(result)) == 0);
  CHECK(storage.input.domain_count == 1u);
  CHECK(storage.input.domains == storage.domains);
  CHECK(storage.domains[0].mode == W_SEED_FRONTEND_DOMAIN_MODE_CONCURRENT);
  CHECK(storage.domains[0].capabilities ==
        W_SEED_FRONTEND_DOMAIN_CAPABILITY_PARALLEL);
  CHECK(w_seed_hir0_verify(&storage.hir_program, &storage.hir_result));
  w_seed_parallel_selection0 selection;
  CHECK(w_seed_parallel_selection0_select(
            &storage.hir_program, &storage.hir_result, &selection) ==
            W_SEED_PARALLEL_SELECTION0_OK &&
        selection.task_count == 1u &&
        w_seed_parallel_selection0_verify(
            &storage.hir_program, &storage.hir_result, &selection));
  return true;
}

/* One combined NativeSubset matrix keeps the logical integer package visible
 * to the selector without teaching this stage a new value kind per width. The
 * same source now crosses the generic narrow-integer MLIR0 route. */
static bool test_fixed_integer_wrapping_native_admission(void) {
  static const uint8_t source[] =
      "entry { "
      "let signed8 = i8.wrappingAdd(127_i8, 1_i8) "
      "let signed16 = i16.wrappingSubtract(-32767_i16, 2_i16) "
      "let signed32 = i32.wrappingMultiply(2147483647_i32, 2_i32) "
      "let signed64Min = i64.wrappingAdd(9223372036854775807_i64, 1_i64) "
      "let signed64NegatedMax = i64.wrappingNegate(-9223372036854775807_i64) "
      "let unsigned8 = u8.wrappingPower(2_u8, 8_u64) "
      "let unsigned16 = u16.wrappingShiftLeft(32769_u16, 1_u64) "
      "let unsigned32 = u32.wrappingNegate(1_u32) "
      "let unsigned64 = u64.wrappingAdd(18446744073709551615_u64, 1_u64) "
      "let signedAlias = Int.wrappingPower(-2, 63_u64) "
      "let signedAliasZero = Int.wrappingPower(-2, 64_u64) "
      "let zeroPowerZero = UInt.wrappingPower(0, 0_u64) "
      "let nested = i8.wrappingAdd(i8.wrappingAdd(127_i8, 1_i8), 1_i8) "
      "let unsignedAlias = UInt.wrappingSubtract(0, 1) print(\"ok\") }\n";
  uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  const w_seed_native0_status status =
      run_source(source, sizeof(source) - 1u, "integer-wrapping-native", 23u,
                 output, sizeof(output), &result);
  CHECK(status == W_SEED_NATIVE0_OK && result.status == W_SEED_NATIVE0_OK &&
        result.mlir.status == W_SEED_MLIR0_OK &&
        result.mlir.written.mlir_bytes != 0u);
  CHECK(storage.hir_result.status == W_SEED_HIR0_OK &&
        w_seed_hir0_verify(&storage.hir_program, &storage.hir_result));
  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(
            &storage.hir_program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);
  size_t operation_counts[5] = {0u};
  size_t unary_negate_count = 0u;
  size_t generic_signed = 0u;
  size_t generic_unsigned = 0u;
  for (size_t index = 0u; index < storage.hir_program.value_count; index += 1u) {
    const w_seed_hir0_value *value = &storage.hir_program.values[index];
    size_t operation = SIZE_MAX;
    if ((value->kind == W_SEED_HIR0_VALUE_BINARY_I64 ||
         value->kind == W_SEED_HIR0_VALUE_BINARY_U64) &&
        value->binary_operator >= W_SEED_HIR0_BINARY_WRAPPING_ADD &&
        value->binary_operator <= W_SEED_HIR0_BINARY_WRAPPING_SHIFT_LEFT)
      operation = (size_t)value->binary_operator -
                  (size_t)W_SEED_HIR0_BINARY_WRAPPING_ADD;
    else if ((value->kind == W_SEED_HIR0_VALUE_UNARY_I64 ||
              value->kind == W_SEED_HIR0_VALUE_UNARY_U64) &&
             value->unary_operator == W_SEED_HIR0_UNARY_WRAPPING_NEGATE)
      unary_negate_count += 1u;
    if (operation < sizeof(operation_counts) / sizeof(operation_counts[0]))
      operation_counts[operation] += 1u;
    if (value->type_index < storage.hir_program.type_count &&
        storage.hir_program.types[value->type_index].kind ==
            W_SEED_HIR0_TYPE_INTEGER) {
      if (storage.hir_program.types[value->type_index].integer_is_signed)
        generic_signed += 1u;
      else
        generic_unsigned += 1u;
    }
  }
  CHECK(operation_counts[0] == 5u && operation_counts[1] == 2u &&
        operation_counts[2] == 1u && operation_counts[3] == 4u &&
        operation_counts[4] == 1u && unary_negate_count == 2u &&
        generic_signed != 0u &&
        generic_unsigned != 0u && !selection.has_interpolation);

  static const uint8_t dynamic_negate[] =
      "fn negate(value: i8): i8 { return -value }\n"
      "entry { let result = "
      "i8.wrappingAdd(negate(value: 1_i8), 2_i8) print(\"ok\") }\n";
  const w_seed_native0_status dynamic_status =
      run_source(dynamic_negate, sizeof(dynamic_negate) - 1u,
                 "integer-narrow-dynamic-negate", 29u, output,
                 sizeof(output), &result);
  CHECK(dynamic_status == W_SEED_NATIVE0_OK &&
        result.mlir.status == W_SEED_MLIR0_OK &&
        result.mlir.written.mlir_bytes != 0u);
  CHECK(storage.hir_result.status == W_SEED_HIR0_OK &&
        w_seed_hir0_verify(&storage.hir_program, &storage.hir_result));
  CHECK(w_seed_native_subset0_select_program(
            &storage.hir_program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);

  static const char *const rejected[] = {
      "entry { let bad = i8.wrappingAdd(1_i8, 1_i16) }\n",
      "entry { let bad = i8.wrappingPower(1_i8, 1_i64) }\n",
      "entry { let bad = i8.wrappingShiftLeft(1_i8, 8_u64) }\n",
      "entry { let bad = i8.wrappingShiftLeft(1_i8, 9_u64) }\n",
      "entry { let i8 = 1_i8 let bad = i8.wrappingAdd(1_i8, 1_i8) }\n"};
  for (size_t index = 0u; index < sizeof(rejected) / sizeof(rejected[0]);
       index += 1u) {
    (void)memset(output, 0xa9u, sizeof(output));
    (void)memset(&result, 0xb0u, sizeof(result));
    const w_seed_native0_result snapshot = result;
    CHECK(run_source((const uint8_t *)rejected[index], strlen(rejected[index]),
                     "integer-wrapping-native-bad", 27u, output,
                     sizeof(output), &result) != W_SEED_NATIVE0_OK);
    CHECK(memcmp(&result, &snapshot, sizeof(result)) == 0);
    for (size_t byte = 0u; byte < sizeof(output); byte += 1u)
      CHECK(output[byte] == 0xa9u);
  }
  return true;
}

static bool test_checked_integer_arithmetic_native_admission(void) {
  typedef struct {
    const char *name;
    const char *type;
    const char *suffix;
    bool is_signed;
    uint16_t bit_width;
  } integer_case;
  static const integer_case INTEGERS[] = {
      {"i8", "i8", "i8", true, 8u},
      {"u8", "u8", "u8", false, 8u},
      {"i16", "i16", "i16", true, 16u},
      {"u16", "u16", "u16", false, 16u},
      {"i32", "i32", "i32", true, 32u},
      {"u32", "u32", "u32", false, 32u},
      {"i64", "i64", "i64", true, 64u},
      {"u64", "u64", "u64", false, 64u},
      {"int_alias", "Int", "i64", true, 64u},
      {"uint_alias", "UInt", "u64", false, 64u},
  };
  static const char *const SIGNED_MAX[] = {
      "127", "32767", "2147483647", "9223372036854775807"};
  static const char *const UNSIGNED_MAX[] = {
      "255", "65535", "4294967295", "18446744073709551615"};
  static const char *const OPERATORS[] = {"+", "-", "*"};
  char source[8192];
  size_t source_length = 0u;
  (void)memset(source, 0, sizeof(source));
  for (size_t integer_index = 0u;
       integer_index < sizeof(INTEGERS) / sizeof(INTEGERS[0]);
       integer_index += 1u) {
    const integer_case *integer = &INTEGERS[integer_index];
    CHECK(append_test_source(
        source, sizeof(source), &source_length,
        "fn checked_%s(left: %s, right: %s): %s { let sum = left + right "
        "let difference = left - right let product = left * right return sum }\n",
        integer->name, integer->type, integer->type, integer->type));
  }
  CHECK(append_test_source(source, sizeof(source), &source_length, "entry { "));
  for (size_t integer_index = 0u;
       integer_index < sizeof(INTEGERS) / sizeof(INTEGERS[0]);
       integer_index += 1u) {
    const integer_case *integer = &INTEGERS[integer_index];
    CHECK(append_test_source(
        source, sizeof(source), &source_length,
        "let use_%s = checked_%s(left: 6_%s, right: 3_%s) ",
        integer->name, integer->name, integer->suffix, integer->suffix));
  }
  CHECK(append_test_source(source, sizeof(source), &source_length,
                           "print(\"checked arithmetic\") }\n"));

  uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  const w_seed_native0_status status = run_source(
      (const uint8_t *)source, source_length, "checked-arithmetic-native",
      sizeof("checked-arithmetic-native") - 1u, output, sizeof(output),
      &result);
  CHECK(status == W_SEED_NATIVE0_OK && result.status == W_SEED_NATIVE0_OK &&
        result.mlir.status == W_SEED_MLIR0_OK &&
        result.mlir.written.mlir_bytes != 0u &&
        w_seed_hir0_verify(&storage.hir_program, &storage.hir_result));
  w_seed_native_subset0_program selection;
  CHECK(w_seed_native_subset0_select_program(
            &storage.hir_program, &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);

  size_t operations[2][4][3] = {{{0u}}};
  size_t operation_count = 0u;
  for (size_t value_index = 0u;
       value_index < storage.hir_program.value_count; value_index += 1u) {
    const w_seed_hir0_value *value = &storage.hir_program.values[value_index];
    if ((value->binary_operator != W_SEED_HIR0_BINARY_ADD &&
         value->binary_operator != W_SEED_HIR0_BINARY_SUBTRACT &&
         value->binary_operator != W_SEED_HIR0_BINARY_MULTIPLY) ||
        (value->kind != W_SEED_HIR0_VALUE_BINARY_I64 &&
         value->kind != W_SEED_HIR0_VALUE_BINARY_U64))
      continue;
    CHECK(value->type_index < storage.hir_program.type_count &&
          value->left_value < storage.hir_program.value_count &&
          value->right_value < storage.hir_program.value_count);
    const w_seed_hir0_type *type =
        &storage.hir_program.types[value->type_index];
    const bool is_signed = value->kind == W_SEED_HIR0_VALUE_BINARY_I64;
    CHECK(type->integer_is_signed == is_signed &&
          type->integer_bit_width >= 8u && type->integer_bit_width <= 64u &&
          type->integer_bit_width % 8u == 0u);
    const w_seed_hir0_type *left_type = &storage.hir_program.types[
        storage.hir_program.values[value->left_value].type_index];
    const w_seed_hir0_type *right_type = &storage.hir_program.types[
        storage.hir_program.values[value->right_value].type_index];
    CHECK(left_type->integer_is_signed == is_signed &&
          right_type->integer_is_signed == is_signed &&
          left_type->integer_bit_width == type->integer_bit_width &&
          right_type->integer_bit_width == type->integer_bit_width);
    const size_t sign_index = is_signed ? 0u : 1u;
    const size_t width_index =
        type->integer_bit_width == 8u
            ? 0u
            : (type->integer_bit_width == 16u
                   ? 1u
                   : (type->integer_bit_width == 32u ? 2u : 3u));
    const size_t operator_index =
        (size_t)value->binary_operator - (size_t)W_SEED_HIR0_BINARY_ADD;
    CHECK(width_index < 4u && operator_index < 3u);
    operations[sign_index][width_index][operator_index] += 1u;
    operation_count += 1u;
  }
  CHECK(operation_count == 30u);
  for (size_t sign_index = 0u; sign_index < 2u; sign_index += 1u)
    for (size_t width_index = 0u; width_index < 4u; width_index += 1u)
      for (size_t operation_index = 0u; operation_index < 3u;
           operation_index += 1u)
        CHECK(operations[sign_index][width_index][operation_index] ==
              (width_index == 3u ? 2u : 1u));

  /* Compile-time invalid operations are rejected before artifact publication
   * for every signedness, logical width, and ordinary arithmetic operator. */
  for (size_t width_index = 0u; width_index < 4u; width_index += 1u) {
    for (size_t sign_index = 0u; sign_index < 2u; sign_index += 1u) {
      for (size_t operation_index = 0u; operation_index < 3u;
           operation_index += 1u) {
        const bool is_signed = sign_index == 0u;
        const char *const width_name[] = {"8", "16", "32", "64"};
        const char *const left = is_signed
                                     ? SIGNED_MAX[width_index]
                                     : (operation_index == 1u
                                            ? "0"
                                            : UNSIGNED_MAX[width_index]);
        const char *const left_sign =
            is_signed && operation_index == 1u ? "-" : "";
        const char *const right = is_signed
                                      ? (operation_index == 1u
                                             ? "2"
                                             : (operation_index == 0u ? "1"
                                                                      : "2"))
                                      : (operation_index == 1u ? "1"
                                                               : (operation_index == 0u
                                                                      ? "1"
                                                                      : "2"));
        const char *const sign_prefix = is_signed ? "i" : "u";
        const char *const op = OPERATORS[operation_index];
        char failure_source[512];
        const int written = snprintf(
            failure_source, sizeof(failure_source),
            "entry { print(\"before\") let bad = %s%s_%s%s %s %s_%s%s "
            "print(\"after\") }\n",
            left_sign, left, sign_prefix, width_name[width_index], op, right,
            sign_prefix, width_name[width_index]);
        CHECK(written > 0 && (size_t)written < sizeof(failure_source));
        (void)memset(output, 0xa9u, sizeof(output));
        (void)memset(&result, 0xb0u, sizeof(result));
        const w_seed_native0_result result_before = result;
        const w_seed_native0_status rejected = run_source(
            (const uint8_t *)failure_source, (size_t)written,
            "checked-arithmetic-overflow", sizeof("checked-arithmetic-overflow") - 1u,
            output, sizeof(output), &result);
        CHECK(rejected != W_SEED_NATIVE0_OK &&
              memcmp(&result, &result_before, sizeof(result)) == 0);
        for (size_t byte = 0u; byte < sizeof(output); byte += 1u)
          CHECK(output[byte] == 0xa9u);
      }
    }
  }
  return true;
}

static bool test_checked_integer_shifts_native_admission(void) {
  typedef struct {
    const char *name;
    const char *type;
    const char *suffix;
    bool is_signed;
    size_t width_index;
  } integer_case;
  typedef struct {
    uint16_t bit_width;
    const char *count;
    const char *last_count;
    const char *signed_half;
    const char *unsigned_half;
    const char *unsigned_sign_bit;
    const char *unsigned_maximum;
  } width_case;
  static const width_case WIDTHS[] = {
      {8u, "8", "7", "-64", "127", "128", "255"},
      {16u, "16", "15", "-16384", "32767", "32768", "65535"},
      {32u, "32", "31", "-1073741824", "2147483647", "2147483648",
       "4294967295"},
      {64u, "64", "63", "-4611686018427387904",
       "9223372036854775807", "9223372036854775808",
       "18446744073709551615"},
  };
  static const integer_case INTEGERS[] = {
      {"i8", "i8", "i8", true, 0u},
      {"u8", "u8", "u8", false, 0u},
      {"i16", "i16", "i16", true, 1u},
      {"u16", "u16", "u16", false, 1u},
      {"i32", "i32", "i32", true, 2u},
      {"u32", "u32", "u32", false, 2u},
      {"i64", "i64", "i64", true, 3u},
      {"u64", "u64", "u64", false, 3u},
      {"int_alias", "Int", "i64", true, 3u},
      {"uint_alias", "UInt", "u64", false, 3u}};
  uint8_t output[W_SEED_MLIR0_MAX_BYTES];
  w_seed_native0_result result;
  w_seed_native_subset0_program selection;
  size_t operation_counts[2][4][2] = {{{0u}}};
  uint32_t narrow_signed_type = W_SEED_HIR0_NONE;
  uint32_t narrow_unsigned_type = W_SEED_HIR0_NONE;
  uint32_t narrow_shift = W_SEED_HIR0_NONE;
  for (size_t group_start = 0u;
       group_start < sizeof(INTEGERS) / sizeof(INTEGERS[0]);
       group_start += 4u) {
    const size_t group_end =
        group_start + 4u < sizeof(INTEGERS) / sizeof(INTEGERS[0])
            ? group_start + 4u
            : sizeof(INTEGERS) / sizeof(INTEGERS[0]);
    char source[W_SEED_NATIVE0_MAX_SOURCE_BYTES + 1u];
    size_t source_length = 0u;
    uint64_t expected_results[32];
    bool expected_success[32] = {false};
    size_t expected_count = 0u;
    (void)memset(source, 0, sizeof(source));
    for (size_t integer_index = group_start; integer_index < group_end;
         integer_index += 1u) {
      const integer_case *integer = &INTEGERS[integer_index];
      CHECK(append_test_source(source, sizeof(source), &source_length,
                               "fn l_%s(v:%s,c:UInt):%s{return v<<c}\n"
                               "fn r_%s(v:%s,c:UInt):%s{return v>>c}\n",
                               integer->name, integer->type, integer->type,
                               integer->name, integer->type, integer->type));
    }
    CHECK(
        append_test_source(source, sizeof(source), &source_length, "entry { "));
    for (size_t integer_index = group_start; integer_index < group_end;
         integer_index += 1u) {
      const integer_case *integer = &INTEGERS[integer_index];
      const width_case *width = &WIDTHS[integer->width_index];
      if (integer->is_signed) {
        CHECK(append_test_source(source, sizeof(source), &source_length,
                                 "let left_%s=l_%s(v:%s_%s,c:1_u64) ",
                                 integer->name, integer->name,
                                 width->signed_half, integer->suffix));
        const int64_t signed_minimum =
            width->bit_width == 64u
                ? INT64_MIN
                : -(int64_t)(UINT64_C(1) << (width->bit_width - 1u));
        expected_results[expected_count] = (uint64_t)signed_minimum;
        expected_success[expected_count++] = true;
        CHECK(append_test_source(source, sizeof(source), &source_length,
                                 "let right_%s=r_%s(v:%s_%s,c:%s_u64) ",
                                 integer->name, integer->name,
                                 width->signed_half, integer->suffix,
                                 width->last_count));
        expected_results[expected_count] = UINT64_MAX;
        expected_success[expected_count++] = true;
        CHECK(append_test_source(source, sizeof(source), &source_length,
                                 "let zero_%s=l_%s(v:%s_%s,c:0_u64) ",
                                 integer->name, integer->name,
                                 width->signed_half, integer->suffix));
        const int64_t signed_half =
            -(int64_t)(UINT64_C(1) << (width->bit_width - 2u));
        expected_results[expected_count] = (uint64_t)signed_half;
        expected_success[expected_count++] = true;
      } else {
        CHECK(append_test_source(source, sizeof(source), &source_length,
                                 "let left_%s=l_%s(v:%s_%s,c:1_u64) ",
                                 integer->name, integer->name,
                                 width->unsigned_half, integer->suffix));
        const uint64_t unsigned_half =
            (UINT64_C(1) << (width->bit_width - 1u)) - UINT64_C(1);
        expected_results[expected_count] = unsigned_half * UINT64_C(2);
        expected_success[expected_count++] = true;
        CHECK(append_test_source(source, sizeof(source), &source_length,
                                 "let right_%s=r_%s(v:%s_%s,c:%s_u64) ",
                                 integer->name, integer->name,
                                 width->unsigned_sign_bit, integer->suffix,
                                 width->last_count));
        expected_results[expected_count] = UINT64_C(1);
        expected_success[expected_count++] = true;
        CHECK(append_test_source(source, sizeof(source), &source_length,
                                 "let zero_%s=l_%s(v:%s_%s,c:0_u64) ",
                                 integer->name, integer->name,
                                 width->unsigned_half, integer->suffix));
        expected_results[expected_count] = unsigned_half;
        expected_success[expected_count++] = true;
      }

      CHECK(append_test_source(
          source, sizeof(source), &source_length,
          "let bad_left_count_%s=l_%s(v:1_%s,c:%s_u64) "
          "let bad_right_count_%s=r_%s(v:1_%s,c:%s_u64) ",
          integer->name, integer->name, integer->suffix, width->count,
          integer->name, integer->name, integer->suffix, width->count));
      expected_success[expected_count++] = false;
      expected_success[expected_count++] = false;
      if (integer->is_signed) {
        CHECK(append_test_source(
            source, sizeof(source), &source_length,
            "let bad_positive_%s=l_%s(v:%s_%s,c:1_u64) "
            "let bad_negative_%s=l_%s(v:%s_%s,c:2_u64) ",
            integer->name, integer->name, width->unsigned_half,
            integer->suffix, integer->name, integer->name,
            width->signed_half, integer->suffix));
        expected_success[expected_count++] = false;
        expected_success[expected_count++] = false;
      } else {
        CHECK(append_test_source(
            source, sizeof(source), &source_length,
            "let bad_unsigned_%s=l_%s(v:%s_%s,c:1_u64) ", integer->name,
            integer->name, width->unsigned_maximum, integer->suffix));
        expected_success[expected_count++] = false;
      }
    }
    CHECK(expected_count != 0u && expected_count <= 32u &&
          append_test_source(source, sizeof(source), &source_length,
                             "print(\"checked shifts\") }\n"));

    const w_seed_native0_status status = run_source(
        (const uint8_t *)source, source_length, "checked-integer-shifts-native",
        sizeof("checked-integer-shifts-native") - 1u, output, sizeof(output),
        &result);
    CHECK(status == W_SEED_NATIVE0_OK);
    CHECK(result.status == W_SEED_NATIVE0_OK);
    CHECK(result.mlir.status == W_SEED_MLIR0_OK);
    CHECK(result.mlir.written.mlir_bytes != 0u);
    CHECK(w_seed_hir0_verify(&storage.hir_program, &storage.hir_result));

    CHECK(w_seed_native_subset0_select_program(
              &storage.hir_program, &storage.hir_result, &selection) ==
          W_SEED_NATIVE_SUBSET0_OK);
    size_t evaluated_calls = 0u;
    for (size_t call_index = 0u; call_index < storage.hir_program.call_count;
         call_index += 1u) {
      const w_seed_hir0_call *call = &storage.hir_program.calls[call_index];
      CHECK(call->callee_identity < storage.hir_program.identity_count);
      if (storage.hir_program.identities[call->callee_identity].kind !=
          W_SEED_HIR0_IDENTITY_FUNCTION)
        continue;
      CHECK(evaluated_calls < expected_count);
      size_t budget = 256u;
      int64_t evaluated = 0;
      const bool evaluated_success =
          w_seed_scalar_evaluator0_evaluate_call(
              &storage.hir_program, (uint32_t)call_index, &budget, &evaluated);
      CHECK(evaluated_success == expected_success[evaluated_calls]);
      if (evaluated_success)
        CHECK((uint64_t)evaluated == expected_results[evaluated_calls]);
      evaluated_calls += 1u;
    }
    CHECK(evaluated_calls == expected_count);

    for (size_t value_index = 0u; value_index < storage.hir_program.value_count;
         value_index += 1u) {
      const w_seed_hir0_value *value = &storage.hir_program.values[value_index];
      if ((value->kind != W_SEED_HIR0_VALUE_BINARY_I64 &&
           value->kind != W_SEED_HIR0_VALUE_BINARY_U64) ||
          (value->binary_operator != W_SEED_HIR0_BINARY_SHIFT_LEFT &&
           value->binary_operator != W_SEED_HIR0_BINARY_SHIFT_RIGHT))
        continue;
      CHECK(value->type_index < storage.hir_program.type_count &&
            value->left_value < storage.hir_program.value_count &&
            value->right_value < storage.hir_program.value_count);
      const w_seed_hir0_type *type =
          &storage.hir_program.types[value->type_index];
      const w_seed_hir0_type *left_type =
          &storage.hir_program
               .types[storage.hir_program.values[value->left_value].type_index];
      const w_seed_hir0_type *count_type =
          &storage.hir_program.types
               [storage.hir_program.values[value->right_value].type_index];
      const bool is_signed = value->kind == W_SEED_HIR0_VALUE_BINARY_I64;
      CHECK(type->integer_is_signed == is_signed &&
            left_type->integer_is_signed == is_signed &&
            left_type->integer_bit_width == type->integer_bit_width &&
            !count_type->integer_is_signed &&
            count_type->integer_bit_width == 64u &&
            storage.hir_program.values[value->left_value].type_index ==
                value->type_index);
      const size_t sign_index = is_signed ? 0u : 1u;
      const size_t width_index =
          type->integer_bit_width == 8u
              ? 0u
              : (type->integer_bit_width == 16u
                     ? 1u
                     : (type->integer_bit_width == 32u ? 2u : 3u));
      const size_t operator_index =
          value->binary_operator == W_SEED_HIR0_BINARY_SHIFT_LEFT ? 0u : 1u;
      CHECK(width_index < 4u);
      operation_counts[sign_index][width_index][operator_index] += 1u;
      if (is_signed && type->integer_bit_width == 8u &&
          narrow_shift == W_SEED_HIR0_NONE)
        narrow_shift = (uint32_t)value_index;
      if (type->integer_bit_width == 8u) {
        if (is_signed && narrow_signed_type == W_SEED_HIR0_NONE)
          narrow_signed_type = value->type_index;
        if (!is_signed && narrow_unsigned_type == W_SEED_HIR0_NONE)
          narrow_unsigned_type = value->type_index;
      }
    }
    if (group_start == 0u) {
      CHECK(narrow_shift != W_SEED_HIR0_NONE &&
            narrow_signed_type != W_SEED_HIR0_NONE &&
            narrow_unsigned_type != W_SEED_HIR0_NONE);
      const w_seed_hir0_value saved_shift = storage.hir_values[narrow_shift];
      const w_seed_hir0_type saved_type = storage.hir_types[narrow_signed_type];
      const uint32_t saved_count_type =
          storage.hir_values[saved_shift.right_value].type_index;
      w_seed_native_subset0_program selection_before = selection;
      storage.hir_values[narrow_shift].type_index = narrow_unsigned_type;
      CHECK(w_seed_native_subset0_select_program(
                &storage.hir_program, &storage.hir_result, &selection) ==
                W_SEED_NATIVE_SUBSET0_INVALID &&
            memcmp(&selection, &selection_before, sizeof(selection)) == 0);
      storage.hir_values[narrow_shift] = saved_shift;
      storage.hir_values[saved_shift.right_value].type_index =
          narrow_signed_type;
      CHECK(w_seed_native_subset0_select_program(
                &storage.hir_program, &storage.hir_result, &selection) ==
                W_SEED_NATIVE_SUBSET0_INVALID &&
            memcmp(&selection, &selection_before, sizeof(selection)) == 0);
      storage.hir_values[saved_shift.right_value].type_index = saved_count_type;
      storage.hir_types[narrow_signed_type].integer_bit_width = 24u;
      CHECK(w_seed_native_subset0_select_program(
                &storage.hir_program, &storage.hir_result, &selection) ==
                W_SEED_NATIVE_SUBSET0_INVALID &&
            memcmp(&selection, &selection_before, sizeof(selection)) == 0);
      storage.hir_types[narrow_signed_type] = saved_type;
      CHECK(w_seed_hir0_verify(&storage.hir_program, &storage.hir_result));
    }
  }

  for (size_t sign_index = 0u; sign_index < 2u; sign_index += 1u)
    for (size_t width_index = 0u; width_index < 4u; width_index += 1u)
      for (size_t operator_index = 0u; operator_index < 2u;
           operator_index += 1u)
        CHECK(operation_counts[sign_index][width_index][operator_index] ==
              (width_index == 3u ? 2u : 1u));

  CHECK(narrow_shift != W_SEED_HIR0_NONE &&
        narrow_signed_type != W_SEED_HIR0_NONE &&
        narrow_unsigned_type != W_SEED_HIR0_NONE);

  char constant_source[W_SEED_NATIVE0_MAX_SOURCE_BYTES + 1u];
  size_t constant_source_length = 0u;
  (void)memset(constant_source, 0, sizeof(constant_source));
  CHECK(append_test_source(constant_source, sizeof(constant_source),
                           &constant_source_length, "entry { "));
  for (size_t integer_index = 0u;
       integer_index < sizeof(INTEGERS) / sizeof(INTEGERS[0]);
       integer_index += 1u) {
    const integer_case *integer = &INTEGERS[integer_index];
    const width_case *width = &WIDTHS[integer->width_index];
    if (integer->is_signed) {
      CHECK(append_test_source(
          constant_source, sizeof(constant_source), &constant_source_length,
          "let cl_%s=%s_%s << 1_u64 let cr_%s=%s_%s >> %s_u64 ", integer->name,
          width->signed_half, integer->suffix, integer->name,
          width->signed_half, integer->suffix, width->last_count));
    } else {
      CHECK(append_test_source(
          constant_source, sizeof(constant_source), &constant_source_length,
          "let cl_%s=%s_%s << 1_u64 let cr_%s=%s_%s >> %s_u64 ", integer->name,
          width->unsigned_half, integer->suffix, integer->name,
          width->unsigned_sign_bit, integer->suffix, width->last_count));
    }
  }
  CHECK(append_test_source(constant_source, sizeof(constant_source),
                           &constant_source_length,
                           "print(\"constant shifts\") }\n"));
  const w_seed_native0_status constant_status =
      run_source((const uint8_t *)constant_source, constant_source_length,
                 "checked-integer-shifts-constant",
                 sizeof("checked-integer-shifts-constant") - 1u, output,
                 sizeof(output), &result);
  CHECK(constant_status == W_SEED_NATIVE0_OK &&
        result.status == W_SEED_NATIVE0_OK &&
        result.mlir.status == W_SEED_MLIR0_OK &&
        result.mlir.written.mlir_bytes != 0u &&
        w_seed_hir0_verify(&storage.hir_program, &storage.hir_result));
  CHECK(w_seed_native_subset0_select_program(&storage.hir_program,
                                             &storage.hir_result, &selection) ==
        W_SEED_NATIVE_SUBSET0_OK);

  /* Every logical width rejects count == width for both operators and both
   * signednesses. Maximum-positive and minimum-negative left shifts separately
   * prove checked signed representability at both ends. */
  const char *const OPERATOR[] = {"<<", ">>"};
  for (size_t integer_index = 0u;
       integer_index < sizeof(INTEGERS) / sizeof(INTEGERS[0]);
       integer_index += 1u) {
    const integer_case *integer = &INTEGERS[integer_index];
    const width_case *width = &WIDTHS[integer->width_index];
    for (size_t operation_index = 0u; operation_index < 2u;
         operation_index += 1u) {
      char bad_source[384];
      const int written =
          snprintf(bad_source, sizeof(bad_source),
                   "entry { let bad = 1_%s %s %s_u64 print(\"after\") }\n",
                   integer->suffix, OPERATOR[operation_index], width->count);
      CHECK(written > 0 && (size_t)written < sizeof(bad_source));
      (void)memset(output, 0xa9u, sizeof(output));
      (void)memset(&result, 0xb0u, sizeof(result));
      const w_seed_native0_result snapshot = result;
      const w_seed_native0_status rejected = run_source(
          (const uint8_t *)bad_source, (size_t)written,
          "checked-shift-width-trap", sizeof("checked-shift-width-trap") - 1u,
          output, sizeof(output), &result);
      CHECK(rejected != W_SEED_NATIVE0_OK &&
            storage.frontend_result.status == W_SEED_FRONTEND_OK &&
            storage.hir_result.status == W_SEED_HIR0_OK &&
            w_seed_hir0_verify(&storage.hir_program, &storage.hir_result) &&
            w_seed_native_subset0_select_program(
                &storage.hir_program, &storage.hir_result, &selection) ==
                W_SEED_NATIVE_SUBSET0_UNSUPPORTED &&
            memcmp(&result, &snapshot, sizeof(result)) == 0);
      for (size_t byte = 0u; byte < sizeof(output); byte += 1u)
        CHECK(output[byte] == 0xa9u);
    }

    const char *const overflow_value =
        integer->is_signed ? width->unsigned_half : width->unsigned_maximum;
    char overflow_source[384];
    const int written =
        snprintf(overflow_source, sizeof(overflow_source),
                 "entry { let bad = %s_%s << 1_u64 print(\"after\") }\n",
                 overflow_value, integer->suffix);
    CHECK(written > 0 && (size_t)written < sizeof(overflow_source));
    (void)memset(output, 0xa9u, sizeof(output));
    (void)memset(&result, 0xb0u, sizeof(result));
    const w_seed_native0_result snapshot = result;
    const w_seed_native0_status rejected = run_source(
        (const uint8_t *)overflow_source, (size_t)written,
        "checked-shift-overflow", sizeof("checked-shift-overflow") - 1u, output,
        sizeof(output), &result);
    CHECK(rejected != W_SEED_NATIVE0_OK &&
          storage.frontend_result.status == W_SEED_FRONTEND_OK &&
          storage.hir_result.status == W_SEED_HIR0_OK &&
          w_seed_hir0_verify(&storage.hir_program, &storage.hir_result) &&
          w_seed_native_subset0_select_program(
              &storage.hir_program, &storage.hir_result, &selection) ==
              W_SEED_NATIVE_SUBSET0_UNSUPPORTED &&
          memcmp(&result, &snapshot, sizeof(result)) == 0);
    for (size_t byte = 0u; byte < sizeof(output); byte += 1u)
      CHECK(output[byte] == 0xa9u);

    if (integer->is_signed) {
      char negative_overflow_source[384];
      const int negative_written =
          snprintf(negative_overflow_source, sizeof(negative_overflow_source),
                   "entry { let bad = %s_%s << 2_u64 print(\"after\") }\n",
                   width->signed_half, integer->suffix);
      CHECK(negative_written > 0 &&
            (size_t)negative_written < sizeof(negative_overflow_source));
      (void)memset(output, 0xa9u, sizeof(output));
      (void)memset(&result, 0xb0u, sizeof(result));
      const w_seed_native0_result negative_snapshot = result;
      const w_seed_native0_status negative_rejected = run_source(
          (const uint8_t *)negative_overflow_source, (size_t)negative_written,
          "checked-shift-negative-overflow",
          sizeof("checked-shift-negative-overflow") - 1u, output,
          sizeof(output), &result);
      CHECK(negative_rejected != W_SEED_NATIVE0_OK &&
            storage.frontend_result.status == W_SEED_FRONTEND_OK &&
            storage.hir_result.status == W_SEED_HIR0_OK &&
            w_seed_hir0_verify(&storage.hir_program, &storage.hir_result) &&
            w_seed_native_subset0_select_program(
                &storage.hir_program, &storage.hir_result, &selection) ==
                W_SEED_NATIVE_SUBSET0_UNSUPPORTED &&
            memcmp(&result, &negative_snapshot, sizeof(result)) == 0);
      for (size_t byte = 0u; byte < sizeof(output); byte += 1u)
        CHECK(output[byte] == 0xa9u);
    }
  }
  static const uint8_t huge_count[] =
      "entry { let bad = 1_i8 << 18446744073709551615_u64 }\n";
  (void)memset(output, 0xa9u, sizeof(output));
  (void)memset(&result, 0xb0u, sizeof(result));
  const w_seed_native0_result snapshot = result;
  const w_seed_native0_status huge_rejected = run_source(
      huge_count, sizeof(huge_count) - 1u, "checked-shift-huge-count",
      sizeof("checked-shift-huge-count") - 1u, output, sizeof(output), &result);
  CHECK(huge_rejected != W_SEED_NATIVE0_OK &&
        storage.frontend_result.status == W_SEED_FRONTEND_OK &&
        storage.hir_result.status == W_SEED_HIR0_OK &&
        w_seed_hir0_verify(&storage.hir_program, &storage.hir_result) &&
        w_seed_native_subset0_select_program(&storage.hir_program,
                                             &storage.hir_result, &selection) ==
            W_SEED_NATIVE_SUBSET0_UNSUPPORTED &&
        memcmp(&result, &snapshot, sizeof(result)) == 0);
  for (size_t byte = 0u; byte < sizeof(output); byte += 1u)
    CHECK(output[byte] == 0xa9u);
  return true;
}

int main(void) {
  const bool products =
      test_virtual_structured_task_product() &&
      test_virtual_static_yield_helper_product() &&
      test_async_direct_entry_product() && test_signed_comparison_products() &&
      test_products() && test_strict_float_native_admission() &&
      test_float_bits_native_admission() &&
      test_unsigned_binary_u64_slice() &&
      test_u64_wrapping_add_slice() && test_u64_saturating_add_slice() &&
      test_u64_saturating_subtract_slice() &&
      test_u64_saturating_multiply_slice() &&
      test_u64_saturating_policy_slice() &&
      test_u64_wrapping_subtract_slice() &&
      test_u64_wrapping_multiply_slice() && test_u64_wrapping_power_slice() &&
      test_u64_overflowing_power_slice() &&
      test_u64_overflowing_add_slice() &&
      test_u64_overflowing_family_slice() &&
      test_u64_wrapping_shift_left_slice() &&
      test_fixed_integer_shift_policy_native_matrix() &&
      test_u64_masked_shift_left_slice() &&
      test_u64_masked_shift_right_slice() &&
      test_u64_logical_shift_right_slice() && test_u64_rotated_left_slice() &&
      test_u64_rotated_right_slice() && test_u64_count_ones_slice() &&
      test_u64_count_zeros_slice() && test_u64_count_leading_zeros_slice() &&
      test_u64_count_trailing_zeros_slice() && test_u64_reversed_bits_slice() &&
      test_u64_reversed_bytes_slice() &&
      test_fixed_integer_bit_primitive_width_slice() &&
      test_u64_wrapping_negate_slice() &&
      test_unsigned_unary_u64_slice() && test_enum_frontend_storage() &&
      test_enum_payload_native_lowering() &&
      test_bool_payload_native_lowering() &&
      test_enum_switch_native_lowering() &&
      test_enum_subset_switch_native_lowering() &&
      test_process_handler_catalog_and_artifact() &&
      test_process_input0_public_artifact() && test_panic_native_routes() &&
      test_process_integer_exactly_adapter() &&
      test_process_runtime_float_rounding_subset() &&
      test_process_arguments_count_public_artifact() &&
      test_process_arguments_count_ordered_native() &&
      test_process_stdout_bounds() &&
      test_process_enum_payload_public_artifact() &&
      test_process_parallel_native_admission() &&
      test_fixed_integer_wrapping_native_admission() &&
      test_checked_integer_arithmetic_native_admission() &&
      test_checked_integer_shifts_native_admission();
  const bool logical = products && test_logical_native_selector() &&
                       test_multi_carrier_native_subset_selector() &&
                       test_break_continue_multi_carrier_native_subset_selector() &&
                       test_post_loop_continuation_native_subset() &&
                       test_unary_i64_native_selector() &&
                       test_integer_prefix_native_matrix() &&
                       test_implicit_integer_widening_native() &&
                       test_numeric_widen_native() &&
                       test_explicit_integer_truncating_bits_native() &&
                       test_explicit_integer_saturating_native() &&
                       test_scalar_if_value_native() &&
                       test_nested_scalar_if_value_native() &&
                       test_terminal_branch_returns_native() &&
                       test_missing_terminal_return_remains_unsupported() &&
                       test_direct_scalar_call_return_remains_unsupported();
  const bool nested = logical && test_nested_depth_and_linear_analysis();
  const bool failures = nested && test_failures_and_capacity();
  const bool aliases = failures && test_aliases() &&
                       test_integer_comparison_family_capacity();
  (void)remove(TEST_PATH);
  if (!aliases) return 1;
  (void)puts("seed Native0: bounded source to verified HIR0 to MLIR0 passed");
  return 0;
}
