#ifndef W_SEED_FOREIGN_H
#define W_SEED_FOREIGN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "w_seed_source.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The seed scanner validates one locked lexical profile only. */
typedef enum {
  W_SEED_FOREIGN_PROFILE_C_INLINE_1 = 0,
} w_seed_foreign_profile;

typedef enum {
  W_SEED_FOREIGN_TERMINAL_NONE = 0,
  W_SEED_FOREIGN_TERMINAL_CLOSED,
  W_SEED_FOREIGN_TERMINAL_INVALID_UTF8,
  W_SEED_FOREIGN_TERMINAL_NUL,
  W_SEED_FOREIGN_TERMINAL_MISSING_OPEN,
  W_SEED_FOREIGN_TERMINAL_MISSING_CLOSE,
  W_SEED_FOREIGN_TERMINAL_UNTERMINATED_LITERAL,
  W_SEED_FOREIGN_TERMINAL_UNTERMINATED_COMMENT,
  W_SEED_FOREIGN_TERMINAL_PREPROCESSOR_DIRECTIVE,
  W_SEED_FOREIGN_TERMINAL_LINE_SPLICE,
  W_SEED_FOREIGN_TERMINAL_BODY_LIMIT,
  W_SEED_FOREIGN_TERMINAL_NESTING_LIMIT,
  W_SEED_FOREIGN_TERMINAL_INVALID_LIMIT,
} w_seed_foreign_terminal_state;

typedef enum {
  W_SEED_FOREIGN_ERROR_NONE = 0,
  W_SEED_FOREIGN_ERROR_NULL_ARGUMENT,
  W_SEED_FOREIGN_ERROR_INVALID_LIMIT,
  W_SEED_FOREIGN_ERROR_INVALID_UTF8,
  W_SEED_FOREIGN_ERROR_NUL,
  W_SEED_FOREIGN_ERROR_MISSING_OPEN,
  W_SEED_FOREIGN_ERROR_MISSING_CLOSE,
  W_SEED_FOREIGN_ERROR_UNTERMINATED_LITERAL,
  W_SEED_FOREIGN_ERROR_UNTERMINATED_COMMENT,
  W_SEED_FOREIGN_ERROR_PREPROCESSOR_DIRECTIVE,
  W_SEED_FOREIGN_ERROR_LINE_SPLICE,
  W_SEED_FOREIGN_ERROR_BODY_LIMIT,
  W_SEED_FOREIGN_ERROR_NESTING_LIMIT,
} w_seed_foreign_error_kind;

typedef struct {
  size_t maximum_body_bytes;
  size_t maximum_nesting;
} w_seed_foreign_limits;

/*
 * This record is source-validation evidence only. It has no adapter, lock,
 * recipe, or publication identity. All offsets are relative to the input
 * slice, which starts with the W opening brace.
 */
typedef struct {
  w_seed_foreign_profile profile;
  size_t body_start_byte;
  size_t body_end_byte;
  size_t close_byte;
  size_t next_byte;
  size_t maximum_body_bytes;
  size_t maximum_nesting;
  size_t maximum_nesting_observed;
  w_seed_foreign_terminal_state terminal_state;
  uint8_t body_digest[32];
  bool digest_valid;
} w_seed_foreign_source_validation;

typedef struct {
  w_seed_foreign_error_kind kind;
  w_seed_foreign_terminal_state terminal_state;
  w_seed_span primary;
  w_seed_span opening;
  bool has_close;
  size_t close_byte;
} w_seed_foreign_error;

/* Scan one C inline body without allocating or reading host state. */
bool w_seed_foreign_scan_c_inline_1(
    w_seed_byte_view input, w_seed_foreign_limits limits,
    w_seed_foreign_source_validation *result, w_seed_foreign_error *error);

#ifdef __cplusplus
}
#endif

#endif
