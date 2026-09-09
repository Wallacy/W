#include "w_seed_native0.h"
#include "../src/w_seed_native_subset0.h"

#include <stdbool.h>
#include <stddef.h>
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
static const w_seed_mlir0_target TARGET = {
    W_SEED_MLIR0_TARGET_X86_64_UNKNOWN_LINUX_GNU};

static bool write_source(const uint8_t *bytes, size_t length) {
  FILE *file = fopen(TEST_PATH, "wb");
  if (file == NULL) return false;
  const size_t written =
      length == 0u ? 0u : fwrite(bytes, sizeof(uint8_t), length, file);
  const int close_result = fclose(file);
  return written == length && close_result == 0;
}

static w_seed_native0_status run_source(
    const uint8_t *bytes, size_t length, const char *identity,
    size_t identity_length, uint8_t *output_bytes, size_t output_capacity,
    w_seed_native0_result *result) {
  if (!write_source(bytes, length)) return W_SEED_NATIVE0_SOURCE;
  const w_seed_native0_input input = {
      .path = TEST_PATH,
      .path_length = sizeof(TEST_PATH) - 1u,
      .logical_source_id = {identity, identity_length},
      .target = TARGET};
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
  CHECK(strcmp(W_SEED_NATIVE0_SCHEMA_VERSION, "w-seed-native0-6") == 0);
  CHECK(strcmp(W_SEED_MLIR0_SCHEMA_VERSION, "w-seed-mlir0-15") == 0);
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
  CHECK(program->terminators[both_start + 1u].incoming_value !=
            W_SEED_HIR0_NONE &&
        program->terminators[both_start + 2u].incoming_value !=
            W_SEED_HIR0_NONE &&
        program->terminators[either_start + 1u].incoming_value !=
            W_SEED_HIR0_NONE &&
        program->terminators[either_start + 2u].incoming_value !=
            W_SEED_HIR0_NONE);

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
  CHECK(run_source(source, sizeof(source) - 1u, "restaurant-scalar-if", 20u,
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

static bool test_scalar_if_remains_unsupported(void) {
  static const uint8_t source[] =
      "fn invalid(left: Bool): Bool { if left { return true } else { "
      "return false } }\n"
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
      TARGET};
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
                       "llvm.call @w_seed_checked_multiply_i64(%v0, %v1) : "
                       "(i64, i64) -> i64"));
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
      TARGET};
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
      TARGET};
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
      TARGET};
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

  union {
    w_seed_native0_input input;
    w_seed_native0_storage storage;
  } input_storage_alias = {{
      TEST_PATH,
      sizeof(TEST_PATH) - 1u,
      {"input-storage", 13u},
      TARGET}};
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
      TARGET}};
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

  union {
    w_seed_native0_output output;
    w_seed_native0_storage storage;
  } output_storage_alias = {{output, sizeof(output)}};
  const w_seed_native0_input output_storage_input = {
      TEST_PATH,
      sizeof(TEST_PATH) - 1u,
      {"output-storage", 15u},
      TARGET};
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
      TARGET};
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
  CHECK(run_source(source, sizeof(source) - 1u, "comparison", 10u,
                    output, sizeof(output), &result) == W_SEED_NATIVE0_OK);
  CHECK(contains_bytes(output, result.mlir.written.mlir_bytes,
                        "llvm.icmp \"sle\" %p0, %p1 : i64"));
  CHECK(contains_bytes(output, result.mlir.written.mlir_bytes, "llvm.cond_br %v"));
  static const char *const rejected[] = {
      "fn main() { print(\"${true == false}\") }\nentry(main)\n",
      "fn main() { let same = \"a\" == \"b\" print(\"${same}\") }\nentry(main)\n",
      "fn main() { let same = 3 == true print(\"${same}\") }\nentry(main)\n",
      "fn test(a: i64) { if a / 1 > 0 { print(\"unsafe arithmetic\") } }\n"
      "fn main() { test(a: 1) }\nentry(main)\n",
      "fn main() { let x = 9223372036854775807 + 1 "
      "print(\"${x > 0}\") }\nentry(main)\n",
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

int main(void) {
  (void)fprintf(stderr, "native0 storage bytes: %llu\n",
                (unsigned long long)sizeof(w_seed_native0_storage));
  const bool products = test_signed_comparison_products() && test_products();
  const bool logical = products && test_logical_native_selector() &&
                       test_scalar_if_value_native() &&
                       test_scalar_if_remains_unsupported() &&
                       test_direct_scalar_call_return_remains_unsupported();
  const bool nested = logical && test_nested_depth_and_linear_analysis();
  const bool failures = nested && test_failures_and_capacity();
  const bool aliases = failures && test_aliases();
  (void)remove(TEST_PATH);
  if (!aliases) return 1;
  (void)puts("seed Native0: bounded source to verified HIR0 to MLIR0 passed");
  return 0;
}
