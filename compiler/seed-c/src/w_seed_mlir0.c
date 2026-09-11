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
static const char MLIR0_PROCESS_SCHEMA_COMMENT[] =
    "// " W_SEED_MLIR0_PROCESS_SCHEMA_VERSION "\n";
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

static const char MLIR0_WINDOWS_SCHEMA_COMMENT[] =
    "// " W_SEED_MLIR0_WINDOWS_SCHEMA_VERSION "\n";
static const char MLIR0_WINDOWS_PREFIX[] =
    "module attributes {llvm.target_triple = \"" W_SEED_MLIR0_TARGET_TRIPLE_WINDOWS
    "\"} {\n"
    "  llvm.mlir.global private constant @w_seed_mlir0_payload(\"";
static const char MLIR0_WINDOWS_GLOBAL_MIDDLE[] =
    "\") : !llvm.array<";
#define MLIR0_WINDOWS_BUFFER_GLOBAL_TEXT                                      \
  "  llvm.mlir.global internal @w_seed_mlir0_buffer() : !llvm.array<4097 x i8> {\n" \
  "    %buffer_zero = llvm.mlir.zero : !llvm.array<4097 x i8>\n"            \
  "    llvm.return %buffer_zero : !llvm.array<4097 x i8>\n"                 \
  "  }\n"
static const char MLIR0_WINDOWS_BUFFER_GLOBAL[] =
    MLIR0_WINDOWS_BUFFER_GLOBAL_TEXT;
static const char MLIR0_WINDOWS_GLOBAL_SUFFIX[] =
    " x i8>\n"
    MLIR0_WINDOWS_BUFFER_GLOBAL_TEXT
    "  llvm.func @GetStdHandle(%n: i32) -> !llvm.ptr\n"
    "  llvm.func @WriteFile(%handle: !llvm.ptr, %buffer: !llvm.ptr, %count: i32, %written: !llvm.ptr, %overlapped: !llvm.ptr) -> i32\n"
    "  llvm.func @ExitProcess(%code: i32)\n"
    "  llvm.func @mainCRTStartup() {\n"
    "    %stdout = llvm.mlir.constant(-11 : i32) : i32\n"
    "    %length = llvm.mlir.constant(";
static const char MLIR0_WINDOWS_LENGTH_MIDDLE[] =
    " : i64) : i64\n"
    "    %length32 = llvm.mlir.constant(";
static const char MLIR0_WINDOWS_LENGTH32_MIDDLE[] =
    " : i32) : i32\n"
    "    %zero = llvm.mlir.constant(0 : i32) : i32\n"
    "    %zero64 = llvm.mlir.constant(0 : i64) : i64\n"
    "    %one = llvm.mlir.constant(1 : i64) : i64\n"
    "    %base = llvm.mlir.addressof @w_seed_mlir0_payload : !llvm.ptr\n"
    "    %data = llvm.getelementptr %base[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<";
static const char MLIR0_WINDOWS_GEP_SUFFIX[] =
    " x i8>\n"
    "    %written = llvm.alloca %one x i32 : (i64) -> !llvm.ptr\n"
    "    llvm.store %zero, %written : i32, !llvm.ptr\n"
    "    %null = llvm.inttoptr %zero64 : i64 to !llvm.ptr\n"
    "    %handle = llvm.call @GetStdHandle(%stdout) : (i32) -> !llvm.ptr\n"
    "    %write_ok = llvm.call @WriteFile(%handle, %data, %length32, %written, %null) : (!llvm.ptr, !llvm.ptr, i32, !llvm.ptr, !llvm.ptr) -> i32\n"
    "    %success = llvm.mlir.constant(0 : i32) : i32\n"
    "    %failure = llvm.mlir.constant(1 : i32) : i32\n"
    "    %write_actual = llvm.load %written : !llvm.ptr -> i32\n"
    "    %write_ok_flag = llvm.icmp \"ne\" %write_ok, %zero : i32\n"
    "    %write_complete = llvm.icmp \"eq\" %write_actual, %length32 : i32\n"
    "    %write_valid = llvm.and %write_ok_flag, %write_complete : i1\n"
    "    %status = llvm.select %write_valid, %success, %failure : i1, i32\n"
    "    llvm.call @ExitProcess(%status) : (i32) -> ()\n"
    "    llvm.return\n"
    "  }\n"
    "}\n";

static const char MLIR0_WINDOWS_RUNTIME_HELPER[] =
    "  llvm.func @GetStdHandle(%n: i32) -> !llvm.ptr\n"
    "  llvm.func @WriteFile(%handle: !llvm.ptr, %buffer: !llvm.ptr, %count: i32, %written: !llvm.ptr, %overlapped: !llvm.ptr) -> i32\n"
    "  llvm.func @ExitProcess(%code: i32)\n"
    "  llvm.func internal @w_seed_write(%buffer: !llvm.ptr, %count: i64) -> i64 {\n"
    "    %write_zero32 = llvm.mlir.constant(0 : i32) : i32\n"
    "    %write_zero64 = llvm.mlir.constant(0 : i64) : i64\n"
    "    %write_one = llvm.mlir.constant(1 : i64) : i64\n"
    "    %write_stdout = llvm.mlir.constant(-11 : i32) : i32\n"
    "    %write_count32 = llvm.trunc %count : i64 to i32\n"
    "    %write_address = llvm.alloca %write_one x i32 : (i64) -> !llvm.ptr\n"
    "    llvm.store %write_zero32, %write_address : i32, !llvm.ptr\n"
    "    %write_null = llvm.inttoptr %write_zero64 : i64 to !llvm.ptr\n"
    "    %write_handle = llvm.call @GetStdHandle(%write_stdout) : (i32) -> !llvm.ptr\n"
    "    %write_ok = llvm.call @WriteFile(%write_handle, %buffer, %write_count32, %write_address, %write_null) : (!llvm.ptr, !llvm.ptr, i32, !llvm.ptr, !llvm.ptr) -> i32\n"
    "    %write_actual = llvm.load %write_address : !llvm.ptr -> i32\n"
    "    %write_ok_flag = llvm.icmp \"ne\" %write_ok, %write_zero32 : i32\n"
    "    %write_complete = llvm.icmp \"eq\" %write_actual, %write_count32 : i32\n"
    "    %write_valid = llvm.and %write_ok_flag, %write_complete : i1\n"
    "    %write_result = llvm.select %write_valid, %count, %write_zero64 : i1, i64\n"
    "    llvm.return %write_result : i64\n"
    "  }\n";

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

/* Language-level signed-i64 arithmetic is routed through these closed
 * helpers. The LLVM overflow intrinsics produce the mathematical result and
 * an overflow flag; the flagged edge terminates at the bounded fault
 * boundary, so no wrapped result can reach user output or a later value. */
static const char MLIR0_CHECKED_I64_ADD_HELPER[] =
    "  llvm.func internal @w_seed_checked_add_i64(%left: i64, %right: i64) -> i64 {\n"
    "    %pair = \"llvm.intr.sadd.with.overflow\"(%left, %right) : (i64, i64) -> !llvm.struct<(i64, i1)>\n"
    "    %value = llvm.extractvalue %pair[0] : !llvm.struct<(i64, i1)>\n"
    "    %overflow = llvm.extractvalue %pair[1] : !llvm.struct<(i64, i1)>\n"
    "    llvm.cond_br %overflow, ^checked_overflow, ^checked_ok\n"
    "  ^checked_overflow:\n"
    "    \"llvm.intr.trap\"() : () -> ()\n"
    "    llvm.unreachable\n"
    "  ^checked_ok:\n"
    "    llvm.return %value : i64\n"
    "  }\n";

static const char MLIR0_CHECKED_I64_SUBTRACT_HELPER[] =
    "  llvm.func internal @w_seed_checked_subtract_i64(%left: i64, %right: i64) -> i64 {\n"
    "    %pair = \"llvm.intr.ssub.with.overflow\"(%left, %right) : (i64, i64) -> !llvm.struct<(i64, i1)>\n"
    "    %value = llvm.extractvalue %pair[0] : !llvm.struct<(i64, i1)>\n"
    "    %overflow = llvm.extractvalue %pair[1] : !llvm.struct<(i64, i1)>\n"
    "    llvm.cond_br %overflow, ^checked_overflow, ^checked_ok\n"
    "  ^checked_overflow:\n"
    "    \"llvm.intr.trap\"() : () -> ()\n"
    "    llvm.unreachable\n"
    "  ^checked_ok:\n"
    "    llvm.return %value : i64\n"
    "  }\n";

static const char MLIR0_CHECKED_I64_MULTIPLY_HELPER[] =
    "  llvm.func internal @w_seed_checked_multiply_i64(%left: i64, %right: i64) -> i64 {\n"
    "    %pair = \"llvm.intr.smul.with.overflow\"(%left, %right) : (i64, i64) -> !llvm.struct<(i64, i1)>\n"
    "    %value = llvm.extractvalue %pair[0] : !llvm.struct<(i64, i1)>\n"
    "    %overflow = llvm.extractvalue %pair[1] : !llvm.struct<(i64, i1)>\n"
    "    llvm.cond_br %overflow, ^checked_overflow, ^checked_ok\n"
    "  ^checked_overflow:\n"
    "    \"llvm.intr.trap\"() : () -> ()\n"
    "    llvm.unreachable\n"
    "  ^checked_ok:\n"
    "    llvm.return %value : i64\n"
    "  }\n";

static const char MLIR0_CHECKED_I64_DIVIDE_HELPER[] =
    "  llvm.func internal @w_seed_checked_divide_i64(%left: i64, %right: i64) -> i64 {\n"
    "    %zero = llvm.mlir.constant(0 : i64) : i64\n"
    "    %negative_one = llvm.mlir.constant(-1 : i64) : i64\n"
    "    %minimum = llvm.mlir.constant(-9223372036854775808 : i64) : i64\n"
    "    %zero_divisor = llvm.icmp \"eq\" %right, %zero : i64\n"
    "    %minimum_left = llvm.icmp \"eq\" %left, %minimum : i64\n"
    "    %negative_one_right = llvm.icmp \"eq\" %right, %negative_one : i64\n"
    "    %overflow = llvm.and %minimum_left, %negative_one_right : i1\n"
    "    %invalid = llvm.or %zero_divisor, %overflow : i1\n"
    "    llvm.cond_br %invalid, ^checked_fault, ^checked_ok\n"
    "  ^checked_fault:\n"
    "    \"llvm.intr.trap\"() : () -> ()\n"
    "    llvm.unreachable\n"
    "  ^checked_ok:\n"
    "    %value = llvm.sdiv %left, %right : i64\n"
    "    llvm.return %value : i64\n"
    "  }\n";

static const char MLIR0_CHECKED_I64_REMAINDER_HELPER[] =
    "  llvm.func internal @w_seed_checked_remainder_i64(%left: i64, %right: i64) -> i64 {\n"
    "    %zero = llvm.mlir.constant(0 : i64) : i64\n"
    "    %negative_one = llvm.mlir.constant(-1 : i64) : i64\n"
    "    %minimum = llvm.mlir.constant(-9223372036854775808 : i64) : i64\n"
    "    %zero_divisor = llvm.icmp \"eq\" %right, %zero : i64\n"
    "    llvm.cond_br %zero_divisor, ^checked_fault, ^checked_nonzero\n"
    "  ^checked_fault:\n"
    "    \"llvm.intr.trap\"() : () -> ()\n"
    "    llvm.unreachable\n"
    "  ^checked_nonzero:\n"
    "    %minimum_left = llvm.icmp \"eq\" %left, %minimum : i64\n"
    "    %negative_one_right = llvm.icmp \"eq\" %right, %negative_one : i64\n"
    "    %overflow_pair = llvm.and %minimum_left, %negative_one_right : i1\n"
    "    llvm.cond_br %overflow_pair, ^checked_minimum, ^checked_ok\n"
    "  ^checked_minimum:\n"
    "    llvm.return %zero : i64\n"
    "  ^checked_ok:\n"
    "    %value = llvm.srem %left, %right : i64\n"
    "    llvm.return %value : i64\n"
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
   (sizeof(MLIR0_CHECKED_I64_ADD_HELPER) - 1u) +                        \
   (sizeof(MLIR0_CHECKED_I64_SUBTRACT_HELPER) - 1u) +                   \
   (sizeof(MLIR0_CHECKED_I64_MULTIPLY_HELPER) - 1u) +                   \
   (sizeof(MLIR0_BOOL_HELPER) - 1u) + MLIR0_DYNAMIC_SKELETON_MAX_BYTES + \
   ((size_t)MLIR0_MAX_STDOUT_BYTES * MLIR0_ESCAPE_BYTES_PER_INPUT) +         \
   ((size_t)MLIR0_DYNAMIC_MAX_ACTIONS * MLIR0_DYNAMIC_ACTION_MAX_BYTES) +    \
   ((size_t)W_SEED_NATIVE_SUBSET0_MAX_VALUES *                               \
    MLIR0_DYNAMIC_VALUE_MAX_BYTES))

_Static_assert(CHAR_BIT == 8, "w-seed MLIR0 requires 8-bit bytes");
_Static_assert(MLIR0_MAX_STDOUT_BYTES <= 9999u,
               "w-seed MLIR0 decimal fields must cover the bounded stdout size");
_Static_assert(MLIR0_FIXED_LITERAL_BYTES == 887u,
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
         (target->kind == W_SEED_MLIR0_TARGET_X86_64_UNKNOWN_LINUX_GNU ||
          target->kind == W_SEED_MLIR0_TARGET_X86_64_PC_WINDOWS_MSVC);
}

static bool target_is_windows(const w_seed_mlir0_target *target) {
  return target != NULL &&
         target->kind == W_SEED_MLIR0_TARGET_X86_64_PC_WINDOWS_MSVC;
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
  const bool windows = target_is_windows(target);
  if (!append_literal(artifact, capacity, &offset,
                      windows ? MLIR0_WINDOWS_SCHEMA_COMMENT
                              : MLIR0_SCHEMA_COMMENT) ||
      !append_literal(artifact, capacity, &offset,
                      windows ? MLIR0_WINDOWS_PREFIX : MLIR0_PREFIX))
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
      !append_literal(artifact, capacity, &offset,
                      windows ? MLIR0_WINDOWS_GLOBAL_MIDDLE
                              : MLIR0_GLOBAL_MIDDLE) ||
      !append_size(artifact, capacity, &offset, stdout_bytes) ||
      !append_literal(artifact, capacity, &offset,
                      windows ? MLIR0_WINDOWS_GLOBAL_SUFFIX
                              : MLIR0_GLOBAL_SUFFIX) ||
      !append_size(artifact, capacity, &offset, stdout_bytes) ||
      !append_literal(artifact, capacity, &offset,
                      windows ? MLIR0_WINDOWS_LENGTH_MIDDLE
                              : MLIR0_LENGTH_MIDDLE) ||
      !append_size(artifact, capacity, &offset, stdout_bytes) ||
      !append_literal(artifact, capacity, &offset,
                      windows ? MLIR0_WINDOWS_LENGTH32_MIDDLE
                              : MLIR0_GEP_SUFFIX))
    return false;
  if (windows) {
    if (!append_size(artifact, capacity, &offset, stdout_bytes) ||
        !append_literal(artifact, capacity, &offset,
                        MLIR0_WINDOWS_GEP_SUFFIX))
      return false;
  } else if (!append_size(artifact, capacity, &offset, stdout_bytes) ||
             !append_literal(artifact, capacity, &offset,
                             MLIR0_RETURN_SUFFIX))
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
  bool has_checked_add;
  bool has_checked_subtract;
  bool has_checked_multiply;
  bool has_checked_divide;
  bool has_checked_remainder;
  bool reachable_values[W_SEED_NATIVE_SUBSET0_MAX_VALUES];
} mlir0_dynamic_plan;

static void note_checked_binary_operator(
    w_seed_hir0_binary_operator operation, bool *has_add, bool *has_subtract,
    bool *has_multiply, bool *has_divide, bool *has_remainder);

static bool mark_reachable_value_tree(
    const w_seed_hir0_program *program, uint32_t value_index,
    bool reachable[W_SEED_NATIVE_SUBSET0_MAX_VALUES], bool *has_add,
    bool *has_subtract, bool *has_multiply, bool *has_divide,
    bool *has_remainder, size_t depth);

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
          if ((effective->kind != W_SEED_HIR0_VALUE_CONST_BOOL &&
               effective->kind != W_SEED_HIR0_VALUE_BINARY_I64) ||
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
  for (size_t action_index = 0u; action_index < candidate.action_count;
       action_index += 1u)
    if (candidate.actions[action_index].kind != MLIR0_DYNAMIC_TEXT &&
        !mark_reachable_value_tree(
            program, candidate.actions[action_index].value_index,
            candidate.reachable_values, &candidate.has_checked_add,
            &candidate.has_checked_subtract, &candidate.has_checked_multiply,
            &candidate.has_checked_divide,
            &candidate.has_checked_remainder, 0u))
      return false;
  *plan = candidate;
  return true;
}

static bool mlir0_value_is_constant_i64(const w_seed_hir0_program *program,
                                        uint32_t value_index, size_t depth) {
  if (program == NULL || depth > 256u || value_index >= program->value_count)
    return false;
  const w_seed_hir0_value *value = &program->values[value_index];
  if (value->type_index >= program->type_count ||
      program->types[value->type_index].kind != W_SEED_HIR0_TYPE_I64)
    return false;
  if (value->kind == W_SEED_HIR0_VALUE_CONST_I64) return true;
  if (value->kind == W_SEED_HIR0_VALUE_UNARY_I64)
    return value->unary_operator == W_SEED_HIR0_UNARY_NEGATE &&
           value->left_value != W_SEED_HIR0_NONE &&
           mlir0_value_is_constant_i64(program, value->left_value,
                                       depth + 1u);
  return value->kind == W_SEED_HIR0_VALUE_BINARY_I64 &&
         value->binary_operator <= W_SEED_HIR0_BINARY_REMAINDER &&
         mlir0_value_is_constant_i64(program, value->left_value, depth + 1u) &&
         mlir0_value_is_constant_i64(program, value->right_value, depth + 1u);
}

static const char *binary_operation(w_seed_hir0_binary_operator operation) {
  switch (operation) {
    case W_SEED_HIR0_BINARY_ADD:
      return NULL;
    case W_SEED_HIR0_BINARY_SUBTRACT:
      return NULL;
    case W_SEED_HIR0_BINARY_MULTIPLY:
      return NULL;
    case W_SEED_HIR0_BINARY_DIVIDE:
      return "llvm.sdiv";
    case W_SEED_HIR0_BINARY_REMAINDER:
      return "llvm.srem";
    case W_SEED_HIR0_BINARY_EQUAL:
      return "llvm.icmp \"eq\"";
    case W_SEED_HIR0_BINARY_NOT_EQUAL:
      return "llvm.icmp \"ne\"";
    case W_SEED_HIR0_BINARY_LESS:
      return "llvm.icmp \"slt\"";
    case W_SEED_HIR0_BINARY_LESS_EQUAL:
      return "llvm.icmp \"sle\"";
    case W_SEED_HIR0_BINARY_GREATER:
      return "llvm.icmp \"sgt\"";
    case W_SEED_HIR0_BINARY_GREATER_EQUAL:
      return "llvm.icmp \"sge\"";
  }
  return NULL;
}

static const char *checked_binary_helper(
    w_seed_hir0_binary_operator operation) {
  switch (operation) {
    case W_SEED_HIR0_BINARY_ADD:
      return "@w_seed_checked_add_i64";
    case W_SEED_HIR0_BINARY_SUBTRACT:
      return "@w_seed_checked_subtract_i64";
    case W_SEED_HIR0_BINARY_MULTIPLY:
      return "@w_seed_checked_multiply_i64";
    case W_SEED_HIR0_BINARY_DIVIDE:
      return "@w_seed_checked_divide_i64";
    case W_SEED_HIR0_BINARY_REMAINDER:
      return "@w_seed_checked_remainder_i64";
    default:
      return NULL;
  }
}

static void note_checked_binary_operator(
    w_seed_hir0_binary_operator operation, bool *has_add, bool *has_subtract,
    bool *has_multiply, bool *has_divide, bool *has_remainder) {
  if (has_add == NULL || has_subtract == NULL || has_multiply == NULL ||
      has_divide == NULL || has_remainder == NULL)
    return;
  if (operation == W_SEED_HIR0_BINARY_ADD)
    *has_add = true;
  else if (operation == W_SEED_HIR0_BINARY_SUBTRACT)
    *has_subtract = true;
  else if (operation == W_SEED_HIR0_BINARY_MULTIPLY)
    *has_multiply = true;
  else if (operation == W_SEED_HIR0_BINARY_DIVIDE)
    *has_divide = true;
  else if (operation == W_SEED_HIR0_BINARY_REMAINDER)
    *has_remainder = true;
}

static bool mark_reachable_value_tree(
    const w_seed_hir0_program *program, uint32_t value_index,
    bool reachable[W_SEED_NATIVE_SUBSET0_MAX_VALUES], bool *has_add,
    bool *has_subtract, bool *has_multiply, bool *has_divide,
    bool *has_remainder, size_t depth) {
  if (program == NULL || reachable == NULL || has_add == NULL ||
      has_subtract == NULL || has_multiply == NULL || has_divide == NULL ||
      has_remainder == NULL || depth > 256u ||
      value_index >= program->value_count ||
      value_index >= W_SEED_NATIVE_SUBSET0_MAX_VALUES)
    return false;
  if (reachable[value_index]) return true;
  const w_seed_hir0_value *value = &program->values[value_index];
  if (value->type_index >= program->type_count) return false;
  reachable[value_index] = true;
  if (value->kind == W_SEED_HIR0_VALUE_BINDING_READ) {
    return value->binding_index < program->binding_count &&
           mark_reachable_value_tree(
               program, program->bindings[value->binding_index].initializer_value,
               reachable, has_add, has_subtract, has_multiply, has_divide,
               has_remainder, depth + 1u);
  }
  if (value->kind == W_SEED_HIR0_VALUE_UNARY_BOOL ||
      value->kind == W_SEED_HIR0_VALUE_UNARY_I64) {
    if (value->kind == W_SEED_HIR0_VALUE_UNARY_I64 &&
        !mlir0_value_is_constant_i64(program, value_index, 0u))
      *has_subtract = true;
    return value->left_value != W_SEED_HIR0_NONE &&
           mark_reachable_value_tree(program, value->left_value, reachable,
                                     has_add, has_subtract, has_multiply,
                                     has_divide, has_remainder, depth + 1u);
  }
  if (value->kind == W_SEED_HIR0_VALUE_INTERPOLATED_STRING) {
    if (value->first_interpolation_segment >
            program->interpolation_segment_count ||
        value->interpolation_segment_count >
            program->interpolation_segment_count -
                value->first_interpolation_segment)
      return false;
    for (size_t ordinal = 0u; ordinal < value->interpolation_segment_count;
         ordinal += 1u) {
      const w_seed_hir0_interpolation_segment *segment =
          &program->interpolation_segments[value->first_interpolation_segment +
                                           ordinal];
      if (segment->kind == W_SEED_HIR0_INTERPOLATION_VALUE &&
          !mark_reachable_value_tree(program, segment->value_index, reachable,
                                     has_add, has_subtract, has_multiply,
                                     has_divide, has_remainder, depth + 1u))
        return false;
    }
    return true;
  }
  if (value->kind == W_SEED_HIR0_VALUE_BINARY_I64) {
    if (!((value->binary_operator == W_SEED_HIR0_BINARY_DIVIDE ||
           value->binary_operator == W_SEED_HIR0_BINARY_REMAINDER) &&
          mlir0_value_is_constant_i64(program, value_index, 0u)))
      note_checked_binary_operator(value->binary_operator, has_add,
                                   has_subtract, has_multiply, has_divide,
                                   has_remainder);
    return mark_reachable_value_tree(program, value->left_value, reachable,
                                     has_add, has_subtract, has_multiply,
                                     has_divide, has_remainder, depth + 1u) &&
           mark_reachable_value_tree(program, value->right_value, reachable,
                                     has_add, has_subtract, has_multiply,
                                     has_divide, has_remainder, depth + 1u);
  }
  return true;
}

static bool append_checked_i64_helpers(
    bool has_add, bool has_subtract, bool has_multiply, bool has_divide,
    bool has_remainder, uint8_t *artifact, size_t capacity, size_t *offset) {
  if (has_add &&
      !append_literal(artifact, capacity, offset, MLIR0_CHECKED_I64_ADD_HELPER))
    return false;
  if (has_subtract &&
      !append_literal(artifact, capacity, offset,
                      MLIR0_CHECKED_I64_SUBTRACT_HELPER))
    return false;
  if (has_multiply &&
      !append_literal(artifact, capacity, offset,
                      MLIR0_CHECKED_I64_MULTIPLY_HELPER))
    return false;
  if (has_divide &&
      !append_literal(artifact, capacity, offset,
                      MLIR0_CHECKED_I64_DIVIDE_HELPER))
    return false;
  if (has_remainder &&
      !append_literal(artifact, capacity, offset,
                      MLIR0_CHECKED_I64_REMAINDER_HELPER))
    return false;
  return true;
}

static bool append_program_value_operand(
    const w_seed_hir0_program *program, uint32_t value_index,
    uint32_t function_index, uint8_t *artifact, size_t capacity,
    size_t *offset);

static const char *program_type_name(const w_seed_hir0_program *program,
                                     uint32_t type_index);

static bool append_binary_value_operation(
    const w_seed_hir0_program *program, uint32_t value_index,
    uint32_t function_index, uint8_t *artifact, size_t capacity,
    size_t *offset) {
  if (program == NULL || artifact == NULL || offset == NULL ||
      value_index >= program->value_count ||
      program->values[value_index].kind != W_SEED_HIR0_VALUE_BINARY_I64)
    return false;
  const w_seed_hir0_value *value = &program->values[value_index];
  const bool constant_division =
      (value->binary_operator == W_SEED_HIR0_BINARY_DIVIDE ||
       value->binary_operator == W_SEED_HIR0_BINARY_REMAINDER) &&
      mlir0_value_is_constant_i64(program, value_index, 0u);
  const char *helper = constant_division
                           ? NULL
                           : checked_binary_helper(value->binary_operator);
  if (!append_literal(artifact, capacity, offset, "    %v") ||
      !append_size(artifact, capacity, offset, value_index) ||
      !append_literal(artifact, capacity, offset, " = "))
    return false;
  if (helper != NULL)
    return append_literal(artifact, capacity, offset, "llvm.call ") &&
           append_literal(artifact, capacity, offset, helper) &&
           append_literal(artifact, capacity, offset, "(") &&
           append_program_value_operand(program, value->left_value,
                                        function_index, artifact, capacity,
                                        offset) &&
           append_literal(artifact, capacity, offset, ", ") &&
           append_program_value_operand(program, value->right_value,
                                        function_index, artifact, capacity,
                                        offset) &&
           append_literal(artifact, capacity, offset,
                          ") : (i64, i64) -> i64\n");
  const char *operation = binary_operation(value->binary_operator);
  return operation != NULL &&
         append_literal(artifact, capacity, offset, operation) &&
         append_literal(artifact, capacity, offset, " ") &&
         append_program_value_operand(program, value->left_value,
                                     function_index, artifact, capacity,
                                     offset) &&
         append_literal(artifact, capacity, offset, ", ") &&
         append_program_value_operand(program, value->right_value,
                                     function_index, artifact, capacity,
                                     offset) &&
         append_literal(artifact, capacity, offset, " : i64\n");
}

static bool append_unary_i64_operation(
    const w_seed_hir0_program *program, uint32_t value_index,
    uint32_t function_index, uint8_t *artifact, size_t capacity,
    size_t *offset) {
  if (program == NULL || artifact == NULL || offset == NULL ||
      value_index >= program->value_count)
    return false;
  const w_seed_hir0_value *value = &program->values[value_index];
  if (value->kind != W_SEED_HIR0_VALUE_UNARY_I64 ||
      value->unary_operator != W_SEED_HIR0_UNARY_NEGATE ||
      value->left_value == W_SEED_HIR0_NONE)
    return false;
  const bool constant =
      mlir0_value_is_constant_i64(program, value_index, 0u);
  if (!append_literal(artifact, capacity, offset, "    %v") ||
      !append_size(artifact, capacity, offset, value_index) ||
      !append_literal(artifact, capacity, offset,
                      "_neg_zero = llvm.mlir.constant(0 : i64) : i64\n") ||
      !append_literal(artifact, capacity, offset, "    %v") ||
      !append_size(artifact, capacity, offset, value_index) ||
      !append_literal(artifact, capacity, offset,
                      constant
                          ? " = llvm.sub "
                          : " = llvm.call @w_seed_checked_subtract_i64("))
    return false;
  if (!constant &&
      !append_literal(artifact, capacity, offset, "%v"))
    return false;
  if (!constant && !append_size(artifact, capacity, offset, value_index))
    return false;
  if (!constant &&
      !append_literal(artifact, capacity, offset, "_neg_zero, "))
    return false;
  if (constant &&
      (!append_literal(artifact, capacity, offset, "%v") ||
       !append_size(artifact, capacity, offset, value_index) ||
       !append_literal(artifact, capacity, offset, "_neg_zero, ")))
    return false;
  return append_program_value_operand(program, value->left_value,
                                      function_index, artifact, capacity,
                                      offset) &&
         append_literal(artifact, capacity, offset,
                        constant ? " : i64\n" : ") : (i64, i64) -> i64\n");
}

static bool append_program_block_argument_name(
    const w_seed_hir0_program *program, uint32_t block_argument_index,
    uint32_t function_index, uint8_t *artifact, size_t capacity,
    size_t *offset) {
  if (program == NULL || artifact == NULL || offset == NULL ||
      block_argument_index >= program->block_argument_count ||
      function_index >= program->function_count)
    return false;
  const w_seed_hir0_block_argument *argument =
      &program->block_arguments[block_argument_index];
  if (argument->owner_block >= program->block_count ||
      argument->type_index >= program->type_count ||
      (program->types[argument->type_index].kind != W_SEED_HIR0_TYPE_I64 &&
       program->types[argument->type_index].kind != W_SEED_HIR0_TYPE_BOOL))
    return false;
  const w_seed_hir0_block *block = &program->blocks[argument->owner_block];
  if (block->owner_function != function_index ||
      block->block_argument_count == 0u ||
      block->first_block_argument == W_SEED_HIR0_NONE ||
      argument->ordinal >= block->block_argument_count ||
      block->first_block_argument + argument->ordinal != block_argument_index)
    return false;
  return append_literal(artifact, capacity, offset, "%arg") &&
         append_size(artifact, capacity, offset, block_argument_index);
}

static bool append_value_operations(const w_seed_hir0_program *program,
                                    const mlir0_dynamic_plan *plan,
                                    uint8_t *artifact, size_t capacity,
                                    size_t *offset) {
  if (program == NULL || plan == NULL || artifact == NULL || offset == NULL)
    return false;
  for (size_t index = 0u; index < program->value_count; index += 1u) {
    if (!plan->reachable_values[index]) continue;
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
      if (!append_binary_value_operation(program, (uint32_t)index, 0u,
                                         artifact, capacity, offset))
        return false;
    } else if (value->kind == W_SEED_HIR0_VALUE_UNARY_I64) {
      if (!append_unary_i64_operation(program, (uint32_t)index, 0u, artifact,
                                      capacity, offset))
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
  const bool windows = target_is_windows(target);
  if (!append_literal(artifact, capacity, &offset,
                      windows ? MLIR0_WINDOWS_SCHEMA_COMMENT
                              : MLIR0_SCHEMA_COMMENT) ||
      !append_literal(
          artifact, capacity, &offset,
          windows
              ? "module attributes {llvm.target_triple = \"" W_SEED_MLIR0_TARGET_TRIPLE_WINDOWS
                "\"} {\n"
                "  llvm.mlir.global private constant @w_seed_mlir0_text(\""
              : "module attributes {llvm.target_triple = \"" W_SEED_MLIR0_TARGET_TRIPLE
                "\"} {\n"
                "  llvm.mlir.global private constant @w_seed_mlir0_text(\"") ||
      !append_escaped_bytes(artifact, capacity, &offset, plan.text,
                            plan.text_bytes) ||
      !append_literal(artifact, capacity, &offset, "\") : !llvm.array<") ||
      !append_size(artifact, capacity, &offset, plan.text_bytes) ||
      !append_literal(artifact, capacity, &offset, " x i8>\n") ||
      (windows &&
       !append_literal(artifact, capacity, &offset,
                       MLIR0_WINDOWS_BUFFER_GLOBAL)) ||
      !append_literal(artifact, capacity, &offset, MLIR0_RUNTIME_HELPERS) ||
      !append_checked_i64_helpers(plan.has_checked_add,
                                  plan.has_checked_subtract,
                                  plan.has_checked_multiply,
                                  plan.has_checked_divide,
                                  plan.has_checked_remainder, artifact,
                                  capacity, &offset) ||
      (plan.has_bool &&
       !append_literal(artifact, capacity, &offset, MLIR0_BOOL_HELPER)))
    return false;
  if (windows) {
    if (!append_literal(artifact, capacity, &offset,
                        MLIR0_WINDOWS_RUNTIME_HELPER))
      return false;
  } else if (!append_literal(
                 artifact, capacity, &offset,
                 "  llvm.func @write(%fd: i32, %buffer: !llvm.ptr, %count: i64) -> i64\n"))
    return false;
  if (!append_literal(artifact, capacity, &offset,
                      "  llvm.func @main() -> i32 {\n") ||
      (windows
           ? !append_literal(
                 artifact, capacity, &offset,
                 "    %buffer_base = llvm.mlir.addressof @w_seed_mlir0_buffer : !llvm.ptr\n"
                 "    %buffer = llvm.getelementptr %buffer_base[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<4097 x i8>\n")
           : !append_literal(
                 artifact, capacity, &offset,
                 "    %capacity = llvm.mlir.constant(4097 : i64) : i64\n"
                 "    %buffer = llvm.alloca %capacity x i8 : (i64) -> !llvm.ptr\n")) ||
      !append_literal(artifact, capacity, &offset,
                      "    %text_base = llvm.mlir.addressof @w_seed_mlir0_text : !llvm.ptr\n") ||
      !append_value_operations(program, &plan, artifact, capacity, &offset) ||
      !append_literal(artifact, capacity, &offset,
                      "    %cursor0 = llvm.mlir.constant(0 : i64) : i64\n") ||
      !append_dynamic_actions(&plan, artifact, capacity, &offset))
    return false;
  if (windows) {
    if (!append_literal(artifact, capacity, &offset,
                        "    %written = llvm.call @w_seed_write(%buffer, %cursor"))
      return false;
  } else if (!append_literal(
                 artifact, capacity, &offset,
                 "    %fd = llvm.mlir.constant(1 : i32) : i32\n"
                 "    %written = llvm.call @write(%fd, %buffer, %cursor"))
    return false;
  if (!append_size(artifact, capacity, &offset, plan.action_count) ||
      !append_literal(artifact, capacity, &offset,
                      windows
                          ? ") : (!llvm.ptr, i64) -> i64\n"
                            "    %equal = llvm.icmp \"eq\" %written, %cursor"
                          : ") : (i32, !llvm.ptr, i64) -> i64\n"
                            "    %equal = llvm.icmp \"eq\" %written, %cursor") ||
      !append_size(artifact, capacity, &offset, plan.action_count) ||
      !append_literal(
          artifact, capacity, &offset,
          " : i64\n"
          "    %success = llvm.mlir.constant(0 : i32) : i32\n"
          "    %failure = llvm.mlir.constant(1 : i32) : i32\n"
          "    %status = llvm.select %equal, %success, %failure : i1, i32\n") ||
      !append_literal(artifact, capacity, &offset,
                      "    llvm.return %status : i32\n"
                      "  }\n"))
    return false;
  if (windows &&
      !append_literal(artifact, capacity, &offset,
                      "  llvm.func @mainCRTStartup() {\n"
                      "    %status = llvm.call @main() : () -> i32\n"
                      "    llvm.call @ExitProcess(%status) : (i32) -> ()\n"
                      "    llvm.return\n"
                      "  }\n"))
    return false;
  if (!append_literal(artifact, capacity, &offset, "}\n")) return false;
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
  bool reachable_functions[W_SEED_NATIVE_SUBSET0_MAX_FUNCTIONS];
  bool omitted_functions[W_SEED_NATIVE_SUBSET0_MAX_FUNCTIONS];
  bool has_bool;
  bool has_checked_add;
  bool has_checked_subtract;
  bool has_checked_multiply;
  bool has_checked_divide;
  bool has_checked_remainder;
  bool reachable_values[W_SEED_NATIVE_SUBSET0_MAX_VALUES];
} mlir0_program_plan;

/* The program selector verifies every function so malformed dead code cannot
 * cross the adapter boundary.  Emission, however, starts at the named entry
 * and follows only local function calls from reached bodies.  This keeps
 * helper declarations demand-driven: an unused function containing checked
 * arithmetic does not make that helper part of the reachable artifact. */
static bool mark_program_reachable_functions(
    const w_seed_hir0_program *program, uint32_t function_index,
    bool reachable[W_SEED_NATIVE_SUBSET0_MAX_FUNCTIONS], size_t depth) {
  if (program == NULL || reachable == NULL ||
      function_index >= program->function_count ||
      function_index >= W_SEED_NATIVE_SUBSET0_MAX_FUNCTIONS ||
      depth > W_SEED_NATIVE_SUBSET0_MAX_FUNCTIONS)
    return false;
  if (reachable[function_index]) return true;
  reachable[function_index] = true;
  const w_seed_hir0_function *function = &program->functions[function_index];
  if (function->first_block >= program->block_count ||
      function->block_count == 0u ||
      function->block_count > program->block_count - function->first_block)
    return false;
  for (size_t block_ordinal = 0u; block_ordinal < function->block_count;
       block_ordinal += 1u) {
    const size_t block_index = (size_t)function->first_block + block_ordinal;
    const w_seed_hir0_block *block = &program->blocks[block_index];
    if (block->owner_function != function_index ||
        (size_t)block->first_instruction > program->instruction_count ||
        block->instruction_count >
            program->instruction_count - block->first_instruction)
      return false;
    for (size_t instruction_ordinal = 0u;
         instruction_ordinal < block->instruction_count;
         instruction_ordinal += 1u) {
      const w_seed_hir0_instruction *instruction =
          &program->instructions[(size_t)block->first_instruction +
                                 instruction_ordinal];
      if (instruction->kind != W_SEED_HIR0_INSTRUCTION_CALL) continue;
      if (instruction->call_index >= program->call_count) return false;
      const w_seed_hir0_call *call = &program->calls[instruction->call_index];
      if (call->callee_identity >= program->identity_count) return false;
      const w_seed_hir0_identity *callee =
          &program->identities[call->callee_identity];
      if (callee->kind != W_SEED_HIR0_IDENTITY_FUNCTION) continue;
      if (callee->target_index >= program->function_count ||
          !mark_program_reachable_functions(
              program, callee->target_index, reachable, depth + 1u))
        return false;
    }
  }
  return true;
}

static bool mark_program_reachable_values(
    const w_seed_hir0_program *program,
    const bool reachable_functions[W_SEED_NATIVE_SUBSET0_MAX_FUNCTIONS],
    bool reachable[W_SEED_NATIVE_SUBSET0_MAX_VALUES], bool *has_add,
    bool *has_subtract, bool *has_multiply, bool *has_divide,
    bool *has_remainder) {
  if (program == NULL || reachable_functions == NULL || reachable == NULL ||
      has_add == NULL || has_subtract == NULL || has_multiply == NULL ||
      has_divide == NULL || has_remainder == NULL)
    return false;
  for (size_t function_index = 0u;
       function_index < program->function_count; function_index += 1u) {
    if (!reachable_functions[function_index]) continue;
    const w_seed_hir0_function *function = &program->functions[function_index];
    if (function->first_block >= program->block_count ||
        function->block_count >
            program->block_count - function->first_block)
      return false;
    for (size_t block_ordinal = 0u; block_ordinal < function->block_count;
         block_ordinal += 1u) {
      const size_t block_index = (size_t)function->first_block + block_ordinal;
      const w_seed_hir0_block *block = &program->blocks[block_index];
      if ((size_t)block->first_instruction > program->instruction_count ||
          block->instruction_count >
              program->instruction_count - block->first_instruction ||
          block->terminator_index >= program->terminator_count)
        return false;
      for (size_t instruction_ordinal = 0u;
           instruction_ordinal < block->instruction_count;
           instruction_ordinal += 1u) {
        const w_seed_hir0_instruction *instruction =
            &program->instructions[(size_t)block->first_instruction +
                                   instruction_ordinal];
        if (instruction->kind == W_SEED_HIR0_INSTRUCTION_BINDING) {
          if (instruction->binding_index >= program->binding_count ||
              !mark_reachable_value_tree(
                  program,
                   program->bindings[instruction->binding_index]
                       .initializer_value,
                   reachable, has_add, has_subtract, has_multiply, has_divide,
                   has_remainder, 0u))
            return false;
          continue;
        }
        if (instruction->kind != W_SEED_HIR0_INSTRUCTION_CALL ||
            instruction->call_index >= program->call_count)
          return false;
        const w_seed_hir0_call *call = &program->calls[instruction->call_index];
        if (call->first_argument > program->argument_count ||
            call->argument_count >
                program->argument_count - call->first_argument)
          return false;
        for (size_t argument_ordinal = 0u;
             argument_ordinal < call->argument_count; argument_ordinal += 1u)
          if (!mark_reachable_value_tree(
                  program,
                  program->arguments[(size_t)call->first_argument +
                                     argument_ordinal]
                       .value_index,
                   reachable, has_add, has_subtract, has_multiply, has_divide,
                   has_remainder, 0u))
            return false;
      }
      const w_seed_hir0_terminator *terminator =
          &program->terminators[block->terminator_index];
      if (terminator->kind == W_SEED_HIR0_TERMINATOR_BRANCH) {
        if (!mark_reachable_value_tree(
                program, terminator->value_index, reachable, has_add,
                has_subtract, has_multiply, has_divide, has_remainder, 0u))
          return false;
      } else if (terminator->kind == W_SEED_HIR0_TERMINATOR_JUMP) {
        for (size_t edge_ordinal = 0u;
             edge_ordinal < terminator->edge_argument_count; edge_ordinal += 1u) {
          if (terminator->first_edge_argument == W_SEED_HIR0_NONE ||
              (size_t)terminator->first_edge_argument + edge_ordinal >=
                  program->edge_argument_count ||
              !mark_reachable_value_tree(
                  program,
                  program->edge_arguments[(size_t)terminator->first_edge_argument +
                                          edge_ordinal]
                      .value_index,
                  reachable, has_add, has_subtract, has_multiply, has_divide,
                  has_remainder, 0u))
            return false;
        }
      } else if (terminator->kind == W_SEED_HIR0_TERMINATOR_RETURN_VALUE &&
                 !mark_reachable_value_tree(
                     program, terminator->value_index, reachable, has_add,
                     has_subtract, has_multiply, has_divide, has_remainder,
                     0u)) {
        return false;
      } else if (terminator->kind != W_SEED_HIR0_TERMINATOR_RETURN_UNIT &&
                 terminator->kind != W_SEED_HIR0_TERMINATOR_RETURN_VALUE) {
        return false;
      }
    }
  }
  return true;
}

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
  if (program->entry_count != 1u ||
      !mark_program_reachable_functions(
          program, program->entries[0].target_function,
          candidate.reachable_functions, 0u))
    return false;
  for (size_t call_index = 0u; call_index < program->call_count;
       call_index += 1u) {
    const w_seed_hir0_call *call = &program->calls[call_index];
    if (call->owner_block >= program->block_count)
      return false;
    const uint32_t owner_function =
        program->blocks[call->owner_block].owner_function;
    if (owner_function >= program->function_count)
      return false;
    if (call->callee_identity >= program->identity_count) return false;
    const w_seed_hir0_identity *callee =
        &program->identities[call->callee_identity];
    if (!candidate.reachable_functions[owner_function]) {
      continue;
    }
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
  for (size_t function_index = 0u;
       function_index < program->function_count; function_index += 1u) {
    candidate.omitted_functions[function_index] =
        !candidate.reachable_functions[function_index];
  }
  if (!mark_program_reachable_values(
          program, candidate.reachable_functions, candidate.reachable_values,
          &candidate.has_checked_add, &candidate.has_checked_subtract,
          &candidate.has_checked_multiply, &candidate.has_checked_divide,
          &candidate.has_checked_remainder))
    return false;
  candidate.has_bool = false;
  for (size_t action_index = 0u; action_index < candidate.action_count;
       action_index += 1u)
    if (candidate.actions[action_index].kind == MLIR0_DYNAMIC_BOOL &&
        candidate.actions[action_index].value_index <
            W_SEED_NATIVE_SUBSET0_MAX_VALUES &&
        candidate.reachable_values[candidate.actions[action_index].value_index])
      candidate.has_bool = true;
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
  if (value->kind == W_SEED_HIR0_VALUE_UNARY_BOOL ||
      value->kind == W_SEED_HIR0_VALUE_UNARY_I64)
    return value->type_index < program->type_count &&
           ((value->kind == W_SEED_HIR0_VALUE_UNARY_BOOL &&
             program->types[value->type_index].kind == W_SEED_HIR0_TYPE_BOOL) ||
            (value->kind == W_SEED_HIR0_VALUE_UNARY_I64 &&
             program->types[value->type_index].kind == W_SEED_HIR0_TYPE_I64)) &&
           append_literal(artifact, capacity, offset, "%v") &&
           append_size(artifact, capacity, offset, value_index);
  if (value->kind == W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ)
    return append_program_block_argument_name(
        program, value->block_argument_index, function_index, artifact,
        capacity, offset);
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
      value->kind == W_SEED_HIR0_VALUE_CALL_RESULT ||
      value->kind == W_SEED_HIR0_VALUE_BLOCK_ARGUMENT_READ) {
    emitted[value_index] = true;
    return true;
  }
  if (value->kind == W_SEED_HIR0_VALUE_UNARY_BOOL) {
    if (value->type_index >= program->type_count ||
        program->types[value->type_index].kind != W_SEED_HIR0_TYPE_BOOL ||
        value->unary_operator != W_SEED_HIR0_UNARY_NOT ||
        value->left_value == W_SEED_HIR0_NONE ||
        value->right_value != W_SEED_HIR0_NONE ||
        value->binding_index != W_SEED_HIR0_NONE ||
        value->parameter_index != W_SEED_HIR0_NONE ||
        value->call_index != W_SEED_HIR0_NONE ||
        value->block_argument_index != W_SEED_HIR0_NONE ||
        !append_program_value_tree(program, value->left_value, function_index,
                                   emitted, artifact, capacity, offset,
                                   depth + 1u) ||
        !append_literal(artifact, capacity, offset, "    %v") ||
        !append_size(artifact, capacity, offset, value_index) ||
        !append_literal(artifact, capacity, offset,
                        "_not_mask = llvm.mlir.constant(true) : i1\n") ||
        !append_literal(artifact, capacity, offset, "    %v") ||
        !append_size(artifact, capacity, offset, value_index) ||
        !append_literal(artifact, capacity, offset, " = llvm.xor ") ||
        !append_program_value_operand(program, value->left_value,
                                      function_index, artifact, capacity,
                                      offset) ||
        !append_literal(artifact, capacity, offset, ", %v") ||
        !append_size(artifact, capacity, offset, value_index) ||
        !append_literal(artifact, capacity, offset, "_not_mask : i1\n"))
      return false;
    emitted[value_index] = true;
    return true;
  }
  if (value->kind == W_SEED_HIR0_VALUE_UNARY_I64) {
    if (value->type_index >= program->type_count ||
        program->types[value->type_index].kind != W_SEED_HIR0_TYPE_I64 ||
        value->unary_operator != W_SEED_HIR0_UNARY_NEGATE ||
        value->left_value == W_SEED_HIR0_NONE ||
        value->right_value != W_SEED_HIR0_NONE ||
        value->binding_index != W_SEED_HIR0_NONE ||
        value->parameter_index != W_SEED_HIR0_NONE ||
        value->call_index != W_SEED_HIR0_NONE ||
        value->block_argument_index != W_SEED_HIR0_NONE ||
        !append_program_value_tree(program, value->left_value, function_index,
                                   emitted, artifact, capacity, offset,
                                   depth + 1u) ||
        !append_unary_i64_operation(program, value_index, function_index,
                                    artifact, capacity, offset))
      return false;
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
    if (!append_program_value_tree(program, value->left_value, function_index,
                                   emitted, artifact, capacity, offset,
                                   depth + 1u) ||
        !append_program_value_tree(program, value->right_value, function_index,
                                   emitted, artifact, capacity, offset,
                                   depth + 1u) ||
        !append_binary_value_operation(program, value_index, function_index,
                                       artifact, capacity, offset))
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

#if 0
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
#endif

static bool append_program_block_label(uint8_t *artifact, size_t capacity,
                                       size_t *offset, uint32_t function_index,
                                       uint32_t block_index, bool definition) {
  return append_literal(artifact, capacity, offset, "^w_fn_") &&
         append_size(artifact, capacity, offset, function_index) &&
         append_literal(artifact, capacity, offset, "_b_") &&
         append_size(artifact, capacity, offset, block_index) &&
         (!definition || append_literal(artifact, capacity, offset, ":"));
}

static bool append_program_block_definition(
    const w_seed_hir0_program *program, uint32_t function_index,
    uint32_t block_index, const w_seed_hir0_block *block, uint8_t *artifact,
    size_t capacity, size_t *offset) {
  if (program == NULL || block == NULL || artifact == NULL || offset == NULL ||
      (block->block_argument_count == 0u &&
       block->first_block_argument != W_SEED_HIR0_NONE) ||
      (block->block_argument_count != 0u &&
       block->first_block_argument == W_SEED_HIR0_NONE))
    return false;
  if (!append_program_block_label(artifact, capacity, offset, function_index,
                                 block_index, false))
    return false;
  if (block->block_argument_count != 0u) {
    if (!append_literal(artifact, capacity, offset, "(")) return false;
    for (size_t ordinal = 0u; ordinal < block->block_argument_count;
         ordinal += 1u) {
      const uint32_t argument_index =
          (uint32_t)((size_t)block->first_block_argument + ordinal);
      const char *type = program_type_name(
          program, program->block_arguments[argument_index].type_index);
      if (type == NULL ||
          (ordinal != 0u && !append_literal(artifact, capacity, offset, ", ")) ||
          !append_program_block_argument_name(
              program, argument_index, function_index, artifact, capacity,
              offset) ||
          !append_literal(artifact, capacity, offset, ": ") ||
          !append_literal(artifact, capacity, offset, type))
        return false;
    }
    if (!append_literal(artifact, capacity, offset, ")")) return false;
  }
  return append_literal(artifact, capacity, offset, ":");
}

static bool append_program_function(
    const w_seed_hir0_program *program, const mlir0_program_plan *plan,
    size_t function_index, uint8_t *artifact, size_t capacity,
    size_t *offset) {
  if (program == NULL || plan == NULL || artifact == NULL || offset == NULL ||
      function_index >= program->function_count)
    return false;
  const w_seed_hir0_function *function = &program->functions[function_index];
  if (function->block_count == 0u ||
      function->first_block >= program->block_count ||
      function->block_count > program->block_count - function->first_block)
    return false;
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
  if (!append_literal(artifact, capacity, offset, ")")) return false;
  if (function->return_type != 0u) {
    const char *return_type = program_type_name(program, function->return_type);
    if (return_type == NULL || !append_literal(artifact, capacity, offset,
                                                " -> ") ||
        !append_literal(artifact, capacity, offset, return_type))
      return false;
  }
  if (!append_literal(artifact, capacity, offset,
                      " {\n    %text_base = llvm.mlir.addressof "
                      "@w_seed_mlir0_text : !llvm.ptr\n"))
    return false;
  bool emitted[W_SEED_NATIVE_SUBSET0_MAX_VALUES] = {false};
  for (size_t ordinal = 0u; ordinal < function->block_count; ordinal += 1u) {
    const size_t block_index = (size_t)function->first_block + ordinal;
    const w_seed_hir0_block *block = &program->blocks[block_index];
    if (block->owner_function != function_index ||
        block->terminator_index >= program->terminator_count)
      return false;
    if (ordinal == 0u && block->block_argument_count != 0u) return false;
    if (ordinal != 0u &&
        (!append_literal(artifact, capacity, offset, "  ") ||
         !append_program_block_definition(
             program, (uint32_t)function_index, (uint32_t)block_index, block,
             artifact, capacity, offset) ||
         !append_literal(artifact, capacity, offset, "\n")))
      return false;
    for (size_t instruction_ordinal = 0u;
         instruction_ordinal < block->instruction_count;
         instruction_ordinal += 1u) {
      const w_seed_hir0_instruction *instruction =
          &program->instructions[(size_t)block->first_instruction +
                                 instruction_ordinal];
      if (instruction->kind == W_SEED_HIR0_INSTRUCTION_BINDING) {
        if (instruction->binding_index >= program->binding_count ||
            !append_program_value_tree(
                program,
                program->bindings[instruction->binding_index]
                    .initializer_value,
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
      if (call->callee_identity >= program->identity_count) return false;
      const w_seed_hir0_identity *callee =
          &program->identities[call->callee_identity];
      if (callee->kind == W_SEED_HIR0_IDENTITY_HOST_PRELUDE) {
        if (!append_program_print_actions(program, plan,
                                          instruction->call_index,
                                          (uint32_t)function_index, artifact,
                                          capacity, offset))
          return false;
      } else if (callee->kind == W_SEED_HIR0_IDENTITY_FUNCTION) {
        if (!append_program_local_call(program, call,
                                       (uint32_t)function_index, artifact,
                                       capacity, offset))
          return false;
      } else {
        return false;
      }
    }
    const w_seed_hir0_terminator *terminator =
        &program->terminators[block->terminator_index];
    if (terminator->kind == W_SEED_HIR0_TERMINATOR_BRANCH) {
      if (terminator->value_index >= program->value_count ||
          !append_program_value_tree(
              program, terminator->value_index, (uint32_t)function_index,
              emitted, artifact, capacity, offset, 0u) ||
          !append_literal(artifact, capacity, offset, "    llvm.cond_br ") ||
          !append_program_value_operand(
              program, terminator->value_index, (uint32_t)function_index,
              artifact, capacity, offset) ||
          !append_literal(artifact, capacity, offset, ", ") ||
          !append_program_block_label(
              artifact, capacity, offset, (uint32_t)function_index,
              terminator->target_block, false) ||
          !append_literal(artifact, capacity, offset, ", ") ||
          !append_program_block_label(
              artifact, capacity, offset, (uint32_t)function_index,
              terminator->else_block, false) ||
          !append_literal(artifact, capacity, offset, "\n"))
        return false;
    } else if (terminator->kind == W_SEED_HIR0_TERMINATOR_JUMP) {
      if ((terminator->edge_argument_count != 0u &&
           terminator->first_edge_argument == W_SEED_HIR0_NONE) ||
          (terminator->edge_argument_count > program->edge_argument_count) ||
          (terminator->edge_argument_count != 0u &&
           (size_t)terminator->first_edge_argument >
               program->edge_argument_count - terminator->edge_argument_count))
        return false;
      for (size_t edge_ordinal = 0u;
           edge_ordinal < terminator->edge_argument_count; edge_ordinal += 1u) {
        const w_seed_hir0_edge_argument *edge =
            &program->edge_arguments[(size_t)terminator->first_edge_argument +
                                     edge_ordinal];
        if (!append_program_value_tree(
                program, edge->value_index, (uint32_t)function_index, emitted,
                artifact, capacity, offset, 0u))
          return false;
      }
      if (!append_literal(artifact, capacity, offset, "    llvm.br ") ||
          !append_program_block_label(
              artifact, capacity, offset, (uint32_t)function_index,
              terminator->target_block, false))
        return false;
      if (terminator->edge_argument_count != 0u) {
        if (!append_literal(artifact, capacity, offset, "(")) return false;
        for (size_t edge_ordinal = 0u;
             edge_ordinal < terminator->edge_argument_count; edge_ordinal += 1u) {
          const w_seed_hir0_edge_argument *edge =
              &program->edge_arguments[(size_t)terminator->first_edge_argument +
                                       edge_ordinal];
          if ((edge_ordinal != 0u &&
               !append_literal(artifact, capacity, offset, ", ")) ||
              !append_program_value_operand(
                  program, edge->value_index, (uint32_t)function_index, artifact,
                  capacity, offset))
            return false;
        }
        if (!append_literal(artifact, capacity, offset, " : ")) return false;
        for (size_t edge_ordinal = 0u;
             edge_ordinal < terminator->edge_argument_count; edge_ordinal += 1u) {
          const w_seed_hir0_edge_argument *edge =
              &program->edge_arguments[(size_t)terminator->first_edge_argument +
                                       edge_ordinal];
          const char *type = program_type_name(program, edge->type_index);
          if (type == NULL ||
              (edge_ordinal != 0u &&
               !append_literal(artifact, capacity, offset, ", ")) ||
              !append_literal(artifact, capacity, offset, type))
            return false;
        }
        if (!append_literal(artifact, capacity, offset, ")")) return false;
      }
      if (!append_literal(artifact, capacity, offset, "\n")) return false;
    } else if (terminator->kind == W_SEED_HIR0_TERMINATOR_RETURN_UNIT) {
      if (!append_literal(artifact, capacity, offset, "    llvm.return\n"))
        return false;
    } else if (terminator->kind == W_SEED_HIR0_TERMINATOR_RETURN_VALUE) {
      const char *return_type =
          program_type_name(program, function->return_type);
      if (return_type == NULL || terminator->value_index >= program->value_count ||
          !append_program_value_tree(
              program, terminator->value_index, (uint32_t)function_index,
              emitted, artifact, capacity, offset, 0u) ||
          !append_literal(artifact, capacity, offset, "    llvm.return ") ||
          !append_program_value_operand(
              program, terminator->value_index, (uint32_t)function_index,
              artifact, capacity, offset) ||
          !append_literal(artifact, capacity, offset, " : ") ||
          !append_literal(artifact, capacity, offset, return_type) ||
          !append_literal(artifact, capacity, offset, "\n"))
        return false;
    } else {
      return false;
    }
  }
  return append_literal(artifact, capacity, offset, "  }\n");
}

static bool build_program_artifact(
    const w_seed_hir0_program *program,
    const w_seed_native_subset0_program *selection,
    const w_seed_mlir0_target *target, uint8_t *artifact, size_t capacity,
    size_t *written, uint8_t digest[MLIR0_DIGEST_BYTES]) {
  if (program == NULL || selection == NULL ||
      (!selection->has_local_calls && !selection->has_cfg &&
       !selection->has_mutable_bindings &&
       selection->function_count <= 1u) ||
      !target_is_supported(target) || artifact == NULL || written == NULL ||
      digest == NULL || selection->maximum_stdout_bytes > MLIR0_MAX_STDOUT_BYTES)
    return false;
  mlir0_program_plan plan;
  if (!build_program_plan(program, &plan)) return false;
  size_t offset = 0u;
  const bool windows = target_is_windows(target);
  if (!append_literal(artifact, capacity, &offset,
                      windows ? MLIR0_WINDOWS_SCHEMA_COMMENT
                              : MLIR0_SCHEMA_COMMENT) ||
      !append_literal(
          artifact, capacity, &offset,
          windows
              ? "module attributes {llvm.target_triple = \"" W_SEED_MLIR0_TARGET_TRIPLE_WINDOWS
                "\"} {\n"
                "  llvm.mlir.global private constant @w_seed_mlir0_text(\""
              : "module attributes {llvm.target_triple = \"" W_SEED_MLIR0_TARGET_TRIPLE
                "\"} {\n"
                "  llvm.mlir.global private constant @w_seed_mlir0_text(\"") ||
      !append_escaped_bytes(artifact, capacity, &offset, plan.text,
                            plan.text_bytes) ||
      !append_literal(artifact, capacity, &offset, "\") : !llvm.array<") ||
      !append_size(artifact, capacity, &offset, plan.text_bytes) ||
      !append_literal(artifact, capacity, &offset, " x i8>\n") ||
      (windows &&
       !append_literal(artifact, capacity, &offset,
                       MLIR0_WINDOWS_BUFFER_GLOBAL)) ||
      !append_literal(artifact, capacity, &offset, MLIR0_RUNTIME_HELPERS) ||
      !append_checked_i64_helpers(plan.has_checked_add,
                                  plan.has_checked_subtract,
                                  plan.has_checked_multiply,
                                  plan.has_checked_divide,
                                  plan.has_checked_remainder, artifact,
                                  capacity, &offset) ||
      (plan.has_bool &&
       !append_literal(artifact, capacity, &offset, MLIR0_BOOL_HELPER)))
    return false;
  if (windows) {
    if (!append_literal(artifact, capacity, &offset,
                        MLIR0_WINDOWS_RUNTIME_HELPER))
      return false;
  } else if (!append_literal(
                 artifact, capacity, &offset,
                 "  llvm.func @write(%fd: i32, %buffer: !llvm.ptr, %count: i64) -> i64\n"))
    return false;
  for (size_t function = 0u; function < program->function_count;
       function += 1u)
    if (!plan.omitted_functions[function] &&
        !append_program_function(program, &plan, function, artifact, capacity,
                                 &offset))
      return false;
  if (!append_literal(
          artifact, capacity, &offset,
          "  llvm.func @main() -> i32 {\n") ||
      (windows
           ? !append_literal(
                 artifact, capacity, &offset,
                 "    %buffer_base = llvm.mlir.addressof @w_seed_mlir0_buffer : !llvm.ptr\n"
                 "    %buffer = llvm.getelementptr %buffer_base[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<4097 x i8>\n")
           : !append_literal(
                 artifact, capacity, &offset,
                 "    %capacity = llvm.mlir.constant(4097 : i64) : i64\n"
                 "    %buffer = llvm.alloca %capacity x i8 : (i64) -> !llvm.ptr\n")) ||
      !append_literal(
          artifact, capacity, &offset,
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
          "    %length = llvm.load %cursor_address : !llvm.ptr -> i64\n") ||
      (windows
           ? !append_literal(
                 artifact, capacity, &offset,
                 "    %written = llvm.call @w_seed_write(%buffer, %length) : (!llvm.ptr, i64) -> i64\n")
           : !append_literal(
                 artifact, capacity, &offset,
                 "    %fd = llvm.mlir.constant(1 : i32) : i32\n"
                 "    %written = llvm.call @write(%fd, %buffer, %length) : (i32, !llvm.ptr, i64) -> i64\n")) ||
      !append_literal(
          artifact, capacity, &offset,
          "    %equal = llvm.icmp \"eq\" %written, %length : i64\n"
          "    %success = llvm.mlir.constant(0 : i32) : i32\n"
          "    %failure = llvm.mlir.constant(1 : i32) : i32\n"
          "    %status = llvm.select %equal, %success, %failure : i1, i32\n") ||
      !append_literal(artifact, capacity, &offset,
                      "    llvm.return %status : i32\n"
                      "  }\n"))
    return false;
  if (windows &&
      !append_literal(artifact, capacity, &offset,
                      "  llvm.func @mainCRTStartup() {\n"
                      "    %status = llvm.call @main() : () -> i32\n"
                      "    llvm.call @ExitProcess(%status) : (i32) -> ()\n"
                      "    llvm.return\n"
                      "  }\n"))
    return false;
  if (!append_literal(artifact, capacity, &offset, "}\n")) return false;
  *written = offset;
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  w_seed_sha256_update(&state, artifact, offset);
  w_seed_sha256_final(&state, digest);
  return true;
}

/* The public process-input artifact is a deliberately closed Windows root
 * adapter. It parses the UTF-16 command line into a borrowed descriptor table,
 * publishes two explicit root-scoped owners, executes the one verified HIR
 * branch, then releases Context before Arguments and finalizes the root. The
 * integer-array layout is private to this artifact and is not a W-value ABI. */
static const char MLIR0_PROCESS_EXECUTABLE_HELPERS0[] =
    "  llvm.func @GetCommandLineW() -> !llvm.ptr\n"
    "  llvm.func @GetStdHandle(%n: i32) -> !llvm.ptr\n"
    "  llvm.func @WriteFile(%handle: !llvm.ptr, %buffer: !llvm.ptr, %count: i32, %written: !llvm.ptr, %overlapped: !llvm.ptr) -> i32\n"
    "  llvm.func @ExitProcess(%code: i32)\n"
    "  llvm.func internal @w_seed_process_count_arguments(%command_line: !llvm.ptr, %items: !llvm.ptr) -> i64 {\n"
    "    %zero = llvm.mlir.constant(0 : i64) : i64\n"
    "    %one = llvm.mlir.constant(1 : i64) : i64\n"
    "    %two = llvm.mlir.constant(2 : i64) : i64\n"
    "    %three = llvm.mlir.constant(3 : i64) : i64\n"
    "    %max = llvm.mlir.constant(256 : i64) : i64\n"
    "    %minus_one = llvm.mlir.constant(-1 : i64) : i64\n"
    "    %zero16 = llvm.mlir.constant(0 : i16) : i16\n"
    "    %space16 = llvm.mlir.constant(32 : i16) : i16\n"
    "    %tab16 = llvm.mlir.constant(9 : i16) : i16\n"
    "    %quote16 = llvm.mlir.constant(34 : i16) : i16\n"
    "    %encoding = llvm.mlir.constant(2 : i64) : i64\n"
    "    %false = llvm.mlir.constant(false) : i1\n"
    "    %true = llvm.mlir.constant(true) : i1\n"
    "    llvm.br ^scan(%zero, %false, %false, %zero, %zero, %false : i64, i1, i1, i64, i64, i1)\n"
    "  ^scan(%index: i64, %started: i1, %quoted: i1, %token_start: i64, %argument_count: i64, %seen_program: i1):\n"
    "    %address = llvm.getelementptr %command_line[%index] : (!llvm.ptr, i64) -> !llvm.ptr, i16\n"
    "    %character = llvm.load %address : !llvm.ptr -> i16\n"
    "    %at_end = llvm.icmp \"eq\" %character, %zero16 : i16\n"
    "    llvm.cond_br %at_end, ^scan_end(%index, %started, %quoted, %token_start, %argument_count, %seen_program : i64, i1, i1, i64, i64, i1), ^scan_character(%index, %started, %quoted, %token_start, %argument_count, %seen_program, %character : i64, i1, i1, i64, i64, i1, i16)\n"
    "  ^scan_end(%end_index: i64, %end_started: i1, %end_quoted: i1, %end_token_start: i64, %end_argument_count: i64, %end_seen_program: i1):\n"
    "    llvm.cond_br %end_started, ^finish_token(%end_index, %end_token_start, %end_argument_count, %end_seen_program : i64, i64, i64, i1), ^finish_scan(%end_seen_program, %end_argument_count : i1, i64)\n"
    "  ^scan_character(%character_index: i64, %character_started: i1, %character_quoted: i1, %character_token_start: i64, %character_argument_count: i64, %character_seen_program: i1, %scan_character_value: i16):\n"
    "    %is_space = llvm.icmp \"eq\" %scan_character_value, %space16 : i16\n"
    "    %is_tab = llvm.icmp \"eq\" %scan_character_value, %tab16 : i16\n"
    "    %is_whitespace = llvm.or %is_space, %is_tab : i1\n"
    "    %not_quoted = llvm.xor %character_quoted, %true : i1\n"
    "    %is_separator = llvm.and %is_whitespace, %not_quoted : i1\n"
    "    llvm.cond_br %is_separator, ^separator(%character_index, %character_started, %character_quoted, %character_token_start, %character_argument_count, %character_seen_program : i64, i1, i1, i64, i64, i1), ^non_separator(%character_index, %character_started, %character_quoted, %character_token_start, %character_argument_count, %character_seen_program, %scan_character_value : i64, i1, i1, i64, i64, i1, i16)\n"
    "  ^separator(%separator_index: i64, %separator_started: i1, %separator_quoted: i1, %separator_token_start: i64, %separator_argument_count: i64, %separator_seen_program: i1):\n";

static const char MLIR0_PROCESS_EXECUTABLE_HELPERS1[] =
    "    llvm.cond_br %separator_started, ^finish_token(%separator_index, %separator_token_start, %separator_argument_count, %separator_seen_program : i64, i64, i64, i1), ^separator_skip(%separator_index, %separator_argument_count, %separator_seen_program : i64, i64, i1)\n"
    "  ^separator_skip(%skip_index: i64, %skip_argument_count: i64, %skip_seen_program: i1):\n"
    "    %skip_next = llvm.add %skip_index, %one : i64\n"
    "    llvm.br ^scan(%skip_next, %false, %false, %zero, %skip_argument_count, %skip_seen_program : i64, i1, i1, i64, i64, i1)\n"
    "  ^non_separator(%ordinary_index: i64, %ordinary_started: i1, %ordinary_quoted: i1, %ordinary_token_start: i64, %ordinary_argument_count: i64, %ordinary_seen_program: i1, %ordinary_character: i16):\n"
    "    %is_quote = llvm.icmp \"eq\" %ordinary_character, %quote16 : i16\n"
    "    llvm.cond_br %is_quote, ^quote(%ordinary_index, %ordinary_started, %ordinary_quoted, %ordinary_token_start, %ordinary_argument_count, %ordinary_seen_program : i64, i1, i1, i64, i64, i1), ^ordinary(%ordinary_index, %ordinary_started, %ordinary_quoted, %ordinary_token_start, %ordinary_argument_count, %ordinary_seen_program : i64, i1, i1, i64, i64, i1)\n"
    "  ^quote(%quote_index: i64, %quote_started: i1, %quote_quoted: i1, %quote_token_start: i64, %quote_argument_count: i64, %quote_seen_program: i1):\n"
    "    %quote_next_started = llvm.select %quote_started, %quote_started, %true : i1, i1\n"
    "    %quote_next_quoted = llvm.xor %quote_quoted, %true : i1\n"
    "    %quote_next_index = llvm.add %quote_index, %one : i64\n"
    "    %quote_next_start = llvm.select %quote_started, %quote_token_start, %quote_index : i1, i64\n"
    "    llvm.br ^scan(%quote_next_index, %quote_next_started, %quote_next_quoted, %quote_next_start, %quote_argument_count, %quote_seen_program : i64, i1, i1, i64, i64, i1)\n"
    "  ^ordinary(%plain_index: i64, %plain_started: i1, %plain_quoted: i1, %plain_token_start: i64, %plain_argument_count: i64, %plain_seen_program: i1):\n"
    "    %ordinary_next_start = llvm.select %plain_started, %plain_token_start, %plain_index : i1, i64\n"
    "    %ordinary_next_index = llvm.add %plain_index, %one : i64\n"
    "    llvm.br ^scan(%ordinary_next_index, %true, %plain_quoted, %ordinary_next_start, %plain_argument_count, %plain_seen_program : i64, i1, i1, i64, i64, i1)\n"
    "  ^finish_token(%finish_index: i64, %finish_token_start: i64, %finish_argument_count: i64, %finish_seen_program: i1):\n";

static const char MLIR0_PROCESS_EXECUTABLE_HELPERS1B[] =
    "    llvm.cond_br %finish_seen_program, ^store_argument(%finish_index, %finish_token_start, %finish_argument_count : i64, i64, i64), ^mark_program(%finish_index : i64)\n"
    "  ^mark_program(%program_end: i64):\n"
    "    %program_end_address = llvm.getelementptr %command_line[%program_end] : (!llvm.ptr, i64) -> !llvm.ptr, i16\n"
    "    %program_end_character = llvm.load %program_end_address : !llvm.ptr -> i16\n"
    "    %program_at_end = llvm.icmp \"eq\" %program_end_character, %zero16 : i16\n"
    "    %program_next = llvm.add %program_end, %one : i64\n"
    "    llvm.cond_br %program_at_end, ^valid_count(%zero : i64), ^scan(%program_next, %false, %false, %zero, %zero, %true : i64, i1, i1, i64, i64, i1)\n"
    "  ^store_argument(%argument_end: i64, %argument_start: i64, %stored_argument_count: i64):\n"
    "    %too_many = llvm.icmp \"uge\" %stored_argument_count, %max : i64\n"
    "    llvm.cond_br %too_many, ^invalid, ^store_argument_fields(%argument_end, %argument_start, %stored_argument_count : i64, i64, i64)\n"
    "  ^store_argument_fields(%field_end: i64, %field_start: i64, %field_argument_count: i64):\n"
    "    %field_offset = llvm.mul %field_argument_count, %three : i64\n"
    "    %field_encoding_address = llvm.getelementptr %items[%field_offset] : (!llvm.ptr, i64) -> !llvm.ptr, i64\n"
    "    llvm.store %encoding, %field_encoding_address : i64, !llvm.ptr\n"
    "    %field_data_offset = llvm.add %field_offset, %one : i64\n"
    "    %field_data_address = llvm.getelementptr %items[%field_data_offset] : (!llvm.ptr, i64) -> !llvm.ptr, i64\n"
    "    %field_data = llvm.getelementptr %command_line[%field_start] : (!llvm.ptr, i64) -> !llvm.ptr, i16\n"
    "    %field_data_int = llvm.ptrtoint %field_data : !llvm.ptr to i64\n"
    "    llvm.store %field_data_int, %field_data_address : i64, !llvm.ptr\n"
    "    %field_length_offset = llvm.add %field_offset, %two : i64\n"
    "    %field_length_address = llvm.getelementptr %items[%field_length_offset] : (!llvm.ptr, i64) -> !llvm.ptr, i64\n"
    "    %field_length = llvm.sub %field_end, %field_start : i64\n"
    "    llvm.store %field_length, %field_length_address : i64, !llvm.ptr\n"
    "    %stored_next = llvm.add %field_argument_count, %one : i64\n"
    "    %field_end_address = llvm.getelementptr %command_line[%field_end] : (!llvm.ptr, i64) -> !llvm.ptr, i16\n"
    "    %field_end_character = llvm.load %field_end_address : !llvm.ptr -> i16\n"
    "    %field_at_end = llvm.icmp \"eq\" %field_end_character, %zero16 : i16\n"
    "    %field_next = llvm.add %field_end, %one : i64\n"
    "    llvm.cond_br %field_at_end, ^valid_count(%stored_next : i64), ^scan(%field_next, %false, %false, %zero, %stored_next, %true : i64, i1, i1, i64, i64, i1)\n"
    "  ^finish_scan(%finished_seen_program: i1, %finished_argument_count: i64):\n"
    "    llvm.cond_br %finished_seen_program, ^valid_count(%finished_argument_count : i64), ^invalid\n"
    "  ^valid_count(%valid_argument_count: i64):\n"
    "    llvm.return %valid_argument_count : i64\n"
    "  ^invalid:\n"
    "    llvm.return %minus_one : i64\n"
    "  }\n"
    "  llvm.func internal @w_seed_process_root_init(%vector: !llvm.ptr, %root: !llvm.ptr, %arguments: !llvm.ptr, %context: !llvm.ptr) -> i1 {\n";

static const char MLIR0_PROCESS_EXECUTABLE_HELPERS2[] =
    "    %one = llvm.mlir.constant(1 : i64) : i64\n"
    "    %two = llvm.mlir.constant(2 : i64) : i64\n"
    "    %seven = llvm.mlir.constant(7 : i64) : i64\n"
    "    %vector_items_address = llvm.getelementptr %vector[%one] : (!llvm.ptr, i64) -> !llvm.ptr, i64\n"
    "    %vector_items = llvm.load %vector_items_address : !llvm.ptr -> i64\n"
    "    %vector_count_address = llvm.getelementptr %vector[2] : (!llvm.ptr) -> !llvm.ptr, i64\n"
    "    %vector_count = llvm.load %vector_count_address : !llvm.ptr -> i64\n"
    "    %root_self = llvm.ptrtoint %root : !llvm.ptr to i64\n"
    "    %root_self_address = llvm.getelementptr %root[0] : (!llvm.ptr) -> !llvm.ptr, i64\n"
    "    llvm.store %root_self, %root_self_address : i64, !llvm.ptr\n"
    "    %root_items_address = llvm.getelementptr %root[1] : (!llvm.ptr) -> !llvm.ptr, i64\n"
    "    llvm.store %vector_items, %root_items_address : i64, !llvm.ptr\n"
    "    %root_count_address = llvm.getelementptr %root[2] : (!llvm.ptr) -> !llvm.ptr, i64\n"
    "    llvm.store %vector_count, %root_count_address : i64, !llvm.ptr\n"
    "    %root_encoding_address = llvm.getelementptr %root[3] : (!llvm.ptr) -> !llvm.ptr, i64\n"
    "    llvm.store %two, %root_encoding_address : i64, !llvm.ptr\n"
    "    %root_generation_address = llvm.getelementptr %root[4] : (!llvm.ptr) -> !llvm.ptr, i64\n"
    "    llvm.store %one, %root_generation_address : i64, !llvm.ptr\n"
    "    %arguments_int = llvm.ptrtoint %arguments : !llvm.ptr to i64\n"
    "    %arguments_owner_address = llvm.getelementptr %root[5] : (!llvm.ptr) -> !llvm.ptr, i64\n"
    "    llvm.store %arguments_int, %arguments_owner_address : i64, !llvm.ptr\n"
    "    %context_int = llvm.ptrtoint %context : !llvm.ptr to i64\n"
    "    %context_owner_address = llvm.getelementptr %root[6] : (!llvm.ptr) -> !llvm.ptr, i64\n"
    "    llvm.store %context_int, %context_owner_address : i64, !llvm.ptr\n"
    "    %root_flags_address = llvm.getelementptr %root[7] : (!llvm.ptr) -> !llvm.ptr, i64\n"
    "    llvm.store %seven, %root_flags_address : i64, !llvm.ptr\n"
    "    %arguments_self_address = llvm.getelementptr %arguments[0] : (!llvm.ptr) -> !llvm.ptr, i64\n";

static const char MLIR0_PROCESS_EXECUTABLE_HELPERS3[] =
    "    llvm.store %arguments_int, %arguments_self_address : i64, !llvm.ptr\n"
    "    %root_int = llvm.ptrtoint %root : !llvm.ptr to i64\n"
    "    %arguments_root_address = llvm.getelementptr %arguments[1] : (!llvm.ptr) -> !llvm.ptr, i64\n"
    "    llvm.store %root_int, %arguments_root_address : i64, !llvm.ptr\n"
    "    %arguments_generation_address = llvm.getelementptr %arguments[2] : (!llvm.ptr) -> !llvm.ptr, i64\n"
    "    llvm.store %one, %arguments_generation_address : i64, !llvm.ptr\n"
    "    %arguments_kind_address = llvm.getelementptr %arguments[3] : (!llvm.ptr) -> !llvm.ptr, i64\n"
    "    llvm.store %one, %arguments_kind_address : i64, !llvm.ptr\n"
    "    %arguments_live_address = llvm.getelementptr %arguments[4] : (!llvm.ptr) -> !llvm.ptr, i64\n"
    "    llvm.store %one, %arguments_live_address : i64, !llvm.ptr\n"
    "    %context_self_address = llvm.getelementptr %context[0] : (!llvm.ptr) -> !llvm.ptr, i64\n"
    "    llvm.store %context_int, %context_self_address : i64, !llvm.ptr\n"
    "    %context_root_address = llvm.getelementptr %context[1] : (!llvm.ptr) -> !llvm.ptr, i64\n"
    "    llvm.store %root_int, %context_root_address : i64, !llvm.ptr\n"
    "    %context_generation_address = llvm.getelementptr %context[2] : (!llvm.ptr) -> !llvm.ptr, i64\n"
    "    llvm.store %one, %context_generation_address : i64, !llvm.ptr\n"
    "    %context_kind_address = llvm.getelementptr %context[3] : (!llvm.ptr) -> !llvm.ptr, i64\n"
    "    llvm.store %two, %context_kind_address : i64, !llvm.ptr\n"
    "    %context_live_address = llvm.getelementptr %context[4] : (!llvm.ptr) -> !llvm.ptr, i64\n"
    "    llvm.store %one, %context_live_address : i64, !llvm.ptr\n"
    "    %initialized = llvm.mlir.constant(true) : i1\n"
    "    llvm.return %initialized : i1\n"
    "  }\n"
    "  llvm.func internal @w_seed_process_arguments_is_empty(%arguments: !llvm.ptr) -> i1 {\n";

static const char MLIR0_PROCESS_EXECUTABLE_HELPERS4[] =
    "    %root_pointer_address = llvm.getelementptr %arguments[1] : (!llvm.ptr) -> !llvm.ptr, i64\n"
    "    %root_pointer = llvm.load %root_pointer_address : !llvm.ptr -> i64\n"
    "    %root = llvm.inttoptr %root_pointer : i64 to !llvm.ptr\n"
    "    %count_address = llvm.getelementptr %root[2] : (!llvm.ptr) -> !llvm.ptr, i64\n"
    "    %count = llvm.load %count_address : !llvm.ptr -> i64\n"
    "    %zero = llvm.mlir.constant(0 : i64) : i64\n"
    "    %empty = llvm.icmp \"eq\" %count, %zero : i64\n"
    "    llvm.return %empty : i1\n"
    "  }\n"
    "  llvm.func internal @w_seed_process_context_drop(%context: !llvm.ptr) -> i1 {\n";

static const char MLIR0_PROCESS_EXECUTABLE_HELPERS5[] =
    "    %zero = llvm.mlir.constant(0 : i64) : i64\n"
    "    %one = llvm.mlir.constant(1 : i64) : i64\n"
    "    %root_live_mask = llvm.mlir.constant(1 : i64) : i64\n"
    "    %context_live_mask = llvm.mlir.constant(4 : i64) : i64\n"
    "    %live_address = llvm.getelementptr %context[4] : (!llvm.ptr) -> !llvm.ptr, i64\n"
    "    %live = llvm.load %live_address : !llvm.ptr -> i64\n"
    "    %live_ok = llvm.icmp \"eq\" %live, %one : i64\n"
    "    %root_pointer_address = llvm.getelementptr %context[1] : (!llvm.ptr) -> !llvm.ptr, i64\n"
    "    %root_pointer = llvm.load %root_pointer_address : !llvm.ptr -> i64\n"
    "    %root = llvm.inttoptr %root_pointer : i64 to !llvm.ptr\n"
    "    %flags_address = llvm.getelementptr %root[7] : (!llvm.ptr) -> !llvm.ptr, i64\n"
    "    %flags = llvm.load %flags_address : !llvm.ptr -> i64\n"
    "    %root_live = llvm.and %flags, %root_live_mask : i64\n"
    "    %root_live_ok = llvm.icmp \"eq\" %root_live, %root_live_mask : i64\n"
    "    %context_live = llvm.and %flags, %context_live_mask : i64\n"
    "    %context_live_ok = llvm.icmp \"eq\" %context_live, %context_live_mask : i64\n"
    "    %active = llvm.and %live_ok, %root_live_ok : i1\n"
    "    %valid = llvm.and %active, %context_live_ok : i1\n"
    "    llvm.cond_br %valid, ^drop, ^invalid\n"
    "  ^drop:\n"
    "    %new_flags = llvm.xor %flags, %context_live_mask : i64\n"
    "    llvm.store %new_flags, %flags_address : i64, !llvm.ptr\n"
    "    llvm.store %zero, %live_address : i64, !llvm.ptr\n"
    "    %dropped = llvm.mlir.constant(true) : i1\n"
    "    llvm.return %dropped : i1\n"
    "  ^invalid:\n"
    "    %not_dropped = llvm.mlir.constant(false) : i1\n"
    "    llvm.return %not_dropped : i1\n"
    "  }\n"
    "  llvm.func internal @w_seed_process_arguments_drop(%arguments: !llvm.ptr) -> i1 {\n";

static const char MLIR0_PROCESS_EXECUTABLE_HELPERS6[] =
    "    %zero = llvm.mlir.constant(0 : i64) : i64\n"
    "    %one = llvm.mlir.constant(1 : i64) : i64\n"
    "    %root_live_mask = llvm.mlir.constant(1 : i64) : i64\n"
    "    %arguments_live_mask = llvm.mlir.constant(2 : i64) : i64\n"
    "    %live_address = llvm.getelementptr %arguments[4] : (!llvm.ptr) -> !llvm.ptr, i64\n"
    "    %live = llvm.load %live_address : !llvm.ptr -> i64\n"
    "    %live_ok = llvm.icmp \"eq\" %live, %one : i64\n"
    "    %root_pointer_address = llvm.getelementptr %arguments[1] : (!llvm.ptr) -> !llvm.ptr, i64\n"
    "    %root_pointer = llvm.load %root_pointer_address : !llvm.ptr -> i64\n"
    "    %root = llvm.inttoptr %root_pointer : i64 to !llvm.ptr\n"
    "    %flags_address = llvm.getelementptr %root[7] : (!llvm.ptr) -> !llvm.ptr, i64\n"
    "    %flags = llvm.load %flags_address : !llvm.ptr -> i64\n"
    "    %root_live = llvm.and %flags, %root_live_mask : i64\n"
    "    %root_live_ok = llvm.icmp \"eq\" %root_live, %root_live_mask : i64\n"
    "    %arguments_live = llvm.and %flags, %arguments_live_mask : i64\n"
    "    %arguments_live_ok = llvm.icmp \"eq\" %arguments_live, %arguments_live_mask : i64\n"
    "    %active = llvm.and %live_ok, %root_live_ok : i1\n"
    "    %valid = llvm.and %active, %arguments_live_ok : i1\n"
    "    llvm.cond_br %valid, ^drop, ^invalid\n"
    "  ^drop:\n"
    "    %new_flags = llvm.xor %flags, %arguments_live_mask : i64\n"
    "    llvm.store %new_flags, %flags_address : i64, !llvm.ptr\n"
    "    llvm.store %zero, %live_address : i64, !llvm.ptr\n"
    "    %dropped = llvm.mlir.constant(true) : i1\n"
    "    llvm.return %dropped : i1\n"
    "  ^invalid:\n"
    "    %not_dropped = llvm.mlir.constant(false) : i1\n"
    "    llvm.return %not_dropped : i1\n"
    "  }\n"
    "  llvm.func internal @w_seed_process_root_finalize(%root: !llvm.ptr) -> i1 {\n";

static const char MLIR0_PROCESS_EXECUTABLE_HELPERS7[] =
    "    %zero = llvm.mlir.constant(0 : i64) : i64\n"
    "    %root_live = llvm.mlir.constant(1 : i64) : i64\n"
    "    %root_int = llvm.ptrtoint %root : !llvm.ptr to i64\n"
    "    %self_address = llvm.getelementptr %root[0] : (!llvm.ptr) -> !llvm.ptr, i64\n"
    "    %self = llvm.load %self_address : !llvm.ptr -> i64\n"
    "    %self_ok = llvm.icmp \"eq\" %self, %root_int : i64\n"
    "    %flags_address = llvm.getelementptr %root[7] : (!llvm.ptr) -> !llvm.ptr, i64\n"
    "    %flags = llvm.load %flags_address : !llvm.ptr -> i64\n"
    "    %owners_released = llvm.icmp \"eq\" %flags, %root_live : i64\n"
    "    %valid = llvm.and %self_ok, %owners_released : i1\n"
    "    llvm.cond_br %valid, ^finalize, ^invalid\n"
    "  ^finalize:\n"
    "    llvm.store %zero, %self_address : i64, !llvm.ptr\n"
    "    %items_address = llvm.getelementptr %root[1] : (!llvm.ptr) -> !llvm.ptr, i64\n"
    "    llvm.store %zero, %items_address : i64, !llvm.ptr\n"
    "    %count_address = llvm.getelementptr %root[2] : (!llvm.ptr) -> !llvm.ptr, i64\n"
    "    llvm.store %zero, %count_address : i64, !llvm.ptr\n"
    "    llvm.store %zero, %flags_address : i64, !llvm.ptr\n"
    "    %finalized = llvm.mlir.constant(true) : i1\n"
    "    llvm.return %finalized : i1\n"
    "  ^invalid:\n"
    "    %not_finalized = llvm.mlir.constant(false) : i1\n"
    "    llvm.return %not_finalized : i1\n"
    "  }\n"
    "  llvm.func internal @w_seed_write(%buffer: !llvm.ptr, %count: i64) -> i64 {\n"
    "    %zero32 = llvm.mlir.constant(0 : i32) : i32\n"
    "    %zero64 = llvm.mlir.constant(0 : i64) : i64\n"
    "    %one = llvm.mlir.constant(1 : i64) : i64\n"
    "    %stdout = llvm.mlir.constant(-11 : i32) : i32\n"
    "    %count32 = llvm.trunc %count : i64 to i32\n"
    "    %written_address = llvm.alloca %one x i32 : (i64) -> !llvm.ptr\n"
    "    llvm.store %zero32, %written_address : i32, !llvm.ptr\n"
    "    %null = llvm.inttoptr %zero64 : i64 to !llvm.ptr\n"
    "    %handle = llvm.call @GetStdHandle(%stdout) : (i32) -> !llvm.ptr\n"
    "    %write_ok = llvm.call @WriteFile(%handle, %buffer, %count32, %written_address, %null) : (!llvm.ptr, !llvm.ptr, i32, !llvm.ptr, !llvm.ptr) -> i32\n"
    "    %written = llvm.load %written_address : !llvm.ptr -> i32\n"
    "    %write_ok_flag = llvm.icmp \"ne\" %write_ok, %zero32 : i32\n"
    "    %write_complete = llvm.icmp \"eq\" %written, %count32 : i32\n"
    "    %write_valid = llvm.and %write_ok_flag, %write_complete : i1\n"
    "    %write_result = llvm.select %write_valid, %count, %zero64 : i1, i64\n"
    "    llvm.return %write_result : i64\n"
    "  }\n";

static const char MLIR0_PROCESS_EXECUTABLE_MAIN0[] =
    "  llvm.mlir.global private constant @w_seed_process_missing(\"missing\\0A\") : !llvm.array<8 x i8>\n"
    "  llvm.mlir.global private constant @w_seed_process_received(\"received\\0A\") : !llvm.array<9 x i8>\n"
    "  llvm.mlir.global internal @w_seed_process_items() : !llvm.array<768 x i64> {\n"
    "    %items_zero = llvm.mlir.zero : !llvm.array<768 x i64>\n"
    "    llvm.return %items_zero : !llvm.array<768 x i64>\n"
    "  }\n"
    "  llvm.func @mainCRTStartup() {\n"
    "    %zero = llvm.mlir.constant(0 : i64) : i64\n"
    "    %one = llvm.mlir.constant(1 : i64) : i64\n"
    "    %two = llvm.mlir.constant(2 : i64) : i64\n"
    "    %three = llvm.mlir.constant(3 : i64) : i64\n"
    "    %eight = llvm.mlir.constant(8 : i64) : i64\n"
    "    %nine = llvm.mlir.constant(9 : i64) : i64\n"
    "    %minus_one = llvm.mlir.constant(-1 : i64) : i64\n"
    "    %failure = llvm.mlir.constant(3 : i32) : i32\n"
    "    %missing_status = llvm.mlir.constant(2 : i32) : i32\n"
    "    %success_status = llvm.mlir.constant(0 : i32) : i32\n"
    "    %vector_words = llvm.mlir.constant(3 : i64) : i64\n"
    "    %root_words = llvm.mlir.constant(8 : i64) : i64\n"
    "    %owner_words = llvm.mlir.constant(5 : i64) : i64\n"
    "    %items_base = llvm.mlir.addressof @w_seed_process_items : !llvm.ptr\n"
    "    %items = llvm.getelementptr %items_base[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<768 x i64>\n"
    "    %vector = llvm.alloca %vector_words x i64 : (i64) -> !llvm.ptr\n"
    "    %root = llvm.alloca %root_words x i64 : (i64) -> !llvm.ptr\n"
    "    %arguments = llvm.alloca %owner_words x i64 : (i64) -> !llvm.ptr\n"
    "    %context = llvm.alloca %owner_words x i64 : (i64) -> !llvm.ptr\n"
    "    %command_line = llvm.call @GetCommandLineW() : () -> !llvm.ptr\n"
    "    %null = llvm.inttoptr %zero : i64 to !llvm.ptr\n"
    "    %command_line_missing = llvm.icmp \"eq\" %command_line, %null : !llvm.ptr\n"
    "    llvm.cond_br %command_line_missing, ^early_fault, ^capture\n"
    "  ^capture:\n"
    "    %argument_count = llvm.call @w_seed_process_count_arguments(%command_line, %items) : (!llvm.ptr, !llvm.ptr) -> i64\n"
    "    %parse_failed = llvm.icmp \"eq\" %argument_count, %minus_one : i64\n"
    "    llvm.cond_br %parse_failed, ^early_fault, ^vector_items\n"
    "  ^vector_items:\n"
    "    %has_items = llvm.icmp \"ne\" %argument_count, %zero : i64\n"
    "    llvm.cond_br %has_items, ^vector_nonempty, ^vector_empty\n"
    "  ^vector_nonempty:\n"
    "    %items_pointer = llvm.ptrtoint %items : !llvm.ptr to i64\n"
    "    %vector_items_address = llvm.getelementptr %vector[1] : (!llvm.ptr) -> !llvm.ptr, i64\n"
    "    llvm.store %items_pointer, %vector_items_address : i64, !llvm.ptr\n"
    "    llvm.br ^vector_ready\n"
    "  ^vector_empty:\n"
    "    %vector_items_address_empty = llvm.getelementptr %vector[1] : (!llvm.ptr) -> !llvm.ptr, i64\n"
    "    llvm.store %zero, %vector_items_address_empty : i64, !llvm.ptr\n"
    "    llvm.br ^vector_ready\n"
    "  ^vector_ready:\n"
    "    %vector_encoding_address = llvm.getelementptr %vector[0] : (!llvm.ptr) -> !llvm.ptr, i64\n"
    "    llvm.store %two, %vector_encoding_address : i64, !llvm.ptr\n"
    "    %vector_count_address = llvm.getelementptr %vector[2] : (!llvm.ptr) -> !llvm.ptr, i64\n"
    "    llvm.store %argument_count, %vector_count_address : i64, !llvm.ptr\n"
    "    %root_initialized = llvm.call @w_seed_process_root_init(%vector, %root, %arguments, %context) : (!llvm.ptr, !llvm.ptr, !llvm.ptr, !llvm.ptr) -> i1\n"
    "    llvm.cond_br %root_initialized, ^evaluate, ^early_fault\n"
    "  ^evaluate:\n"
    "    %is_empty = llvm.call @w_seed_process_arguments_is_empty(%arguments) : (!llvm.ptr) -> i1\n"
    "    llvm.cond_br %is_empty, ^print_missing, ^print_received\n"
    "  ^print_missing:\n";

static const char MLIR0_PROCESS_EXECUTABLE_MAIN0B[] =
    "    %missing_base = llvm.mlir.addressof @w_seed_process_missing : !llvm.ptr\n"
    "    %missing_data = llvm.getelementptr %missing_base[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<8 x i8>\n"
    "    %missing_written = llvm.call @w_seed_write(%missing_data, %eight) : (!llvm.ptr, i64) -> i64\n"
    "    %missing_ok = llvm.icmp \"eq\" %missing_written, %eight : i64\n"
    "    %missing_result = llvm.select %missing_ok, %missing_status, %failure : i1, i32\n"
    "    llvm.br ^release_context(%missing_result : i32)\n"
    "  ^print_received:\n"
    "    %received_base = llvm.mlir.addressof @w_seed_process_received : !llvm.ptr\n"
    "    %received_data = llvm.getelementptr %received_base[0, 0] : (!llvm.ptr) -> !llvm.ptr, !llvm.array<9 x i8>\n"
    "    %received_written = llvm.call @w_seed_write(%received_data, %nine) : (!llvm.ptr, i64) -> i64\n"
    "    %received_ok = llvm.icmp \"eq\" %received_written, %nine : i64\n"
    "    %received_result = llvm.select %received_ok, %success_status, %failure : i1, i32\n"
    "    llvm.br ^release_context(%received_result : i32)\n"
    "  ^release_context(%exit_status: i32):\n";

static const char MLIR0_PROCESS_EXECUTABLE_MAIN1[] =
    "    %context_released = llvm.call @w_seed_process_context_drop(%context) : (!llvm.ptr) -> i1\n"
    "    llvm.cond_br %context_released, ^release_arguments(%exit_status : i32), ^release_fault\n"
    "  ^release_arguments(%arguments_exit_status: i32):\n"
    "    %arguments_released = llvm.call @w_seed_process_arguments_drop(%arguments) : (!llvm.ptr) -> i1\n"
    "    llvm.cond_br %arguments_released, ^finalize_root(%arguments_exit_status : i32), ^release_fault\n"
    "  ^finalize_root(%finalize_exit_status: i32):\n"
    "    %root_finalized = llvm.call @w_seed_process_root_finalize(%root) : (!llvm.ptr) -> i1\n"
    "    llvm.cond_br %root_finalized, ^exit(%finalize_exit_status : i32), ^release_fault\n"
    "  ^exit(%code: i32):\n"
    "    llvm.call @ExitProcess(%code) : (i32) -> ()\n"
    "    llvm.return\n"
    "  ^early_fault:\n"
    "    llvm.call @ExitProcess(%failure) : (i32) -> ()\n"
    "    llvm.return\n"
    "  ^release_fault:\n"
    "    llvm.call @ExitProcess(%failure) : (i32) -> ()\n"
    "    llvm.return\n"
    "  }\n";

static bool build_process_executable_artifact(
    const w_seed_hir0_program *program,
    const w_seed_native_subset0_process *selection,
    const w_seed_mlir0_target *target, uint8_t *artifact, size_t capacity,
    size_t *written, uint8_t digest[MLIR0_DIGEST_BYTES]) {
  if (program == NULL || selection == NULL || !target_is_windows(target) ||
      artifact == NULL || written == NULL || digest == NULL)
    return false;
  size_t offset = 0u;
  if (!append_literal(artifact, capacity, &offset,
                      "// " W_SEED_MLIR0_PROCESS_EXECUTABLE_SCHEMA_VERSION
                      "\n") ||
      !append_literal(artifact, capacity, &offset,
                      "module attributes {llvm.target_triple = \"") ||
      !append_literal(artifact, capacity, &offset,
                      W_SEED_MLIR0_TARGET_TRIPLE_WINDOWS) ||
      !append_literal(artifact, capacity, &offset, "\"} {\n") ||
      !append_literal(artifact, capacity, &offset,
                      MLIR0_PROCESS_EXECUTABLE_HELPERS0) ||
      !append_literal(artifact, capacity, &offset,
                      MLIR0_PROCESS_EXECUTABLE_HELPERS1) ||
      !append_literal(artifact, capacity, &offset,
                      MLIR0_PROCESS_EXECUTABLE_HELPERS1B) ||
      !append_literal(artifact, capacity, &offset,
                      MLIR0_PROCESS_EXECUTABLE_HELPERS2) ||
      !append_literal(artifact, capacity, &offset,
                      MLIR0_PROCESS_EXECUTABLE_HELPERS3) ||
      !append_literal(artifact, capacity, &offset,
                      MLIR0_PROCESS_EXECUTABLE_HELPERS4) ||
      !append_literal(artifact, capacity, &offset,
                      MLIR0_PROCESS_EXECUTABLE_HELPERS5) ||
      !append_literal(artifact, capacity, &offset,
                      MLIR0_PROCESS_EXECUTABLE_HELPERS6) ||
      !append_literal(artifact, capacity, &offset,
                      MLIR0_PROCESS_EXECUTABLE_HELPERS7) ||
      !append_literal(artifact, capacity, &offset,
                      MLIR0_PROCESS_EXECUTABLE_MAIN0) ||
      !append_literal(artifact, capacity, &offset,
                      MLIR0_PROCESS_EXECUTABLE_MAIN0B) ||
      !append_literal(artifact, capacity, &offset,
                      MLIR0_PROCESS_EXECUTABLE_MAIN1) ||
      !append_literal(artifact, capacity, &offset, "}\n"))
    return false;
  *written = offset;
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  w_seed_sha256_update(&state, artifact, offset);
  w_seed_sha256_final(&state, digest);
  return true;
}

static bool build_process_handler_artifact(
    const w_seed_mlir0_target *target, uint8_t *artifact, size_t capacity,
    size_t *written, uint8_t digest[MLIR0_DIGEST_BYTES]) {
  if (!target_is_supported(target) || artifact == NULL || written == NULL ||
      digest == NULL)
    return false;
  const bool windows = target_is_windows(target);
  const char *triple = windows ? W_SEED_MLIR0_TARGET_TRIPLE_WINDOWS
                               : W_SEED_MLIR0_TARGET_TRIPLE;
  size_t offset = 0u;
  if (!append_literal(artifact, capacity, &offset,
                      MLIR0_PROCESS_SCHEMA_COMMENT) ||
      !append_literal(artifact, capacity, &offset,
                      "module attributes {llvm.target_triple = \"") ||
      !append_literal(artifact, capacity, &offset, triple) ||
      !append_literal(
          artifact, capacity, &offset,
          "\"} {\n"
          "  llvm.func @w_seed_process_entry0_context_drop(%context: !llvm.ptr) -> i32\n"
          "  llvm.func @w_seed_process_entry0_arguments_drop(%arguments: !llvm.ptr) -> i32\n"
          "  llvm.func @w_seed_process_entry0_handler(%arguments: !llvm.ptr, %context: !llvm.ptr) -> i32 {\n"
          "    %zero = llvm.mlir.constant(0 : i32) : i32\n"
          "    %context_status = llvm.call @w_seed_process_entry0_context_drop(%context) : (!llvm.ptr) -> i32\n"
          "    %context_ok = llvm.icmp \"eq\" %context_status, %zero : i32\n"
          "    llvm.cond_br %context_ok, ^context_released, ^release_fault\n"
          "  ^context_released:\n"
          "    %arguments_status = llvm.call @w_seed_process_entry0_arguments_drop(%arguments) : (!llvm.ptr) -> i32\n"
          "    %arguments_ok = llvm.icmp \"eq\" %arguments_status, %zero : i32\n"
          "    llvm.cond_br %arguments_ok, ^owners_released, ^release_fault\n"
          "  ^owners_released:\n"
          "    llvm.return %zero : i32\n"
          "  ^release_fault:\n"
          "    \"llvm.intr.trap\"() : () -> ()\n"
          "    llvm.unreachable\n"
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
      {program->block_arguments, program->block_argument_capacity,
       sizeof(*program->block_arguments)},
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
      {program->external_modules, program->external_module_capacity,
       sizeof(*program->external_modules)},
      {program->external_symbols, program->external_symbol_capacity,
       sizeof(*program->external_symbols)},
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
  if (input->artifact_kind != W_SEED_MLIR0_ARTIFACT_EXECUTABLE &&
      input->artifact_kind != W_SEED_MLIR0_ARTIFACT_PROCESS_HANDLER &&
      input->artifact_kind != W_SEED_MLIR0_ARTIFACT_PROCESS_EXECUTABLE)
    return W_SEED_MLIR0_UNSUPPORTED;
  const bool process_artifact =
      input->artifact_kind == W_SEED_MLIR0_ARTIFACT_PROCESS_HANDLER;
  const bool process_executable =
      input->artifact_kind == W_SEED_MLIR0_ARTIFACT_PROCESS_EXECUTABLE;
  w_seed_native_subset0_process process_selection = {0};
  w_seed_native_subset0_program program_selection = {0};
  const w_seed_native_subset0_status selected =
      process_artifact
          ? w_seed_native_subset0_select_process(input->program,
                                                 input->hir_result,
                                                 &process_selection)
          : process_executable
                ? w_seed_native_subset0_select_process_executable(
                      input->program, input->hir_result, &process_selection)
                : w_seed_native_subset0_select_program(
                      input->program, input->hir_result, &program_selection);
  if (selected == W_SEED_NATIVE_SUBSET0_INVALID)
    return W_SEED_MLIR0_INVALID_HIR;
  if (selected == W_SEED_NATIVE_SUBSET0_UNSUPPORTED)
    return W_SEED_MLIR0_UNSUPPORTED;
  if (input_aliases_outputs(input, target, NULL, counts, result))
    return W_SEED_MLIR0_ALIAS;
  if (!target_is_supported(target)) return W_SEED_MLIR0_UNSUPPORTED;
  if (process_executable && !target_is_windows(target))
    return W_SEED_MLIR0_UNSUPPORTED;
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  uint8_t digest[MLIR0_DIGEST_BYTES];
  size_t written = 0u;
  if (process_artifact) {
    if (!build_process_handler_artifact(target, artifact, sizeof(artifact),
                                        &written, digest))
      return W_SEED_MLIR0_INVALID_HIR;
  } else if (process_executable) {
    if (!build_process_executable_artifact(
            input->program, &process_selection, target, artifact,
            sizeof(artifact), &written, digest))
      return W_SEED_MLIR0_INVALID_HIR;
  } else if (program_selection.has_local_calls || program_selection.has_cfg ||
             program_selection.has_mutable_bindings ||
             program_selection.function_count > 1u) {
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
  if (input->artifact_kind != W_SEED_MLIR0_ARTIFACT_EXECUTABLE &&
      input->artifact_kind != W_SEED_MLIR0_ARTIFACT_PROCESS_HANDLER &&
      input->artifact_kind != W_SEED_MLIR0_ARTIFACT_PROCESS_EXECUTABLE)
    return W_SEED_MLIR0_UNSUPPORTED;
  const bool process_artifact =
      input->artifact_kind == W_SEED_MLIR0_ARTIFACT_PROCESS_HANDLER;
  const bool process_executable =
      input->artifact_kind == W_SEED_MLIR0_ARTIFACT_PROCESS_EXECUTABLE;
  w_seed_native_subset0_process process_selection = {0};
  w_seed_native_subset0_program program_selection = {0};
  const w_seed_native_subset0_status selected =
      process_artifact
          ? w_seed_native_subset0_select_process(input->program,
                                                 input->hir_result,
                                                 &process_selection)
          : process_executable
                ? w_seed_native_subset0_select_process_executable(
                      input->program, input->hir_result, &process_selection)
                : w_seed_native_subset0_select_program(
                      input->program, input->hir_result, &program_selection);
  if (selected == W_SEED_NATIVE_SUBSET0_INVALID)
    return W_SEED_MLIR0_INVALID_HIR;
  if (selected == W_SEED_NATIVE_SUBSET0_UNSUPPORTED)
    return W_SEED_MLIR0_UNSUPPORTED;
  if (input_aliases_outputs(input, target, output, NULL, result))
    return W_SEED_MLIR0_ALIAS;
  if (!target_is_supported(target)) return W_SEED_MLIR0_UNSUPPORTED;
  if (process_executable && !target_is_windows(target))
    return W_SEED_MLIR0_UNSUPPORTED;
  uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  uint8_t digest[MLIR0_DIGEST_BYTES];
  size_t written = 0u;
  if (process_artifact) {
    if (!build_process_handler_artifact(target, artifact, sizeof(artifact),
                                        &written, digest))
      return W_SEED_MLIR0_INVALID_HIR;
  } else if (process_executable) {
    if (!build_process_executable_artifact(
            input->program, &process_selection, target, artifact,
            sizeof(artifact), &written, digest))
      return W_SEED_MLIR0_INVALID_HIR;
  } else if (program_selection.has_local_calls || program_selection.has_cfg ||
             program_selection.has_mutable_bindings ||
             program_selection.function_count > 1u) {
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
