#ifndef W_SEED_LEXER_H
#define W_SEED_LEXER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "w_seed_source.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  W_SEED_LEX_ITEM_SOURCE_PREFIX = 0,
  W_SEED_LEX_ITEM_TRIVIA,
  W_SEED_LEX_ITEM_WORD,
  W_SEED_LEX_ITEM_NUMBER,
  W_SEED_LEX_ITEM_PUNCTUATION,
  W_SEED_LEX_ITEM_LITERAL_EVENT,
  W_SEED_LEX_ITEM_FOREIGN_BODY,
  W_SEED_LEX_ITEM_UNKNOWN,
  W_SEED_LEX_ITEM_EOF,
} w_seed_lex_item_kind;

typedef enum {
  W_SEED_SOURCE_PREFIX_UTF8_BOM = 0,
} w_seed_source_prefix_kind;

typedef enum {
  W_SEED_TRIVIA_SPACE = 0,
  W_SEED_TRIVIA_NEWLINE,
  W_SEED_TRIVIA_LINE_COMMENT,
  W_SEED_TRIVIA_BLOCK_COMMENT,
} w_seed_trivia_kind;

typedef enum {
  W_SEED_LITERAL_NONE = 0,
  W_SEED_LITERAL_STRING,
  W_SEED_LITERAL_RAW_STRING,
  W_SEED_LITERAL_MULTILINE_STRING,
  W_SEED_LITERAL_RAW_MULTILINE_STRING,
  W_SEED_LITERAL_BYTE_STRING,
  W_SEED_LITERAL_SCALAR,
  W_SEED_LITERAL_BYTE_SCALAR,
} w_seed_literal_kind;

typedef enum {
  W_SEED_LITERAL_START = 0,
  W_SEED_LITERAL_TEXT,
  W_SEED_INTERPOLATION_START,
  W_SEED_INTERPOLATION_END,
  W_SEED_LITERAL_END,
} w_seed_literal_event_kind;

enum {
  W_SEED_NUMBER_RADIX = 1u << 0,
  W_SEED_NUMBER_FRACTION = 1u << 1,
  W_SEED_NUMBER_EXPONENT = 1u << 2,
  W_SEED_NUMBER_SUFFIX = 1u << 3,
  W_SEED_NUMBER_QUANTITY = 1u << 4,
};

typedef enum {
  W_SEED_LEX_ERROR_NONE = 0,
  W_SEED_LEX_ERROR_NULL_ARGUMENT,
  W_SEED_LEX_ERROR_BAD_SPAN,
  W_SEED_LEX_ERROR_UNTERMINATED_LITERAL,
  W_SEED_LEX_ERROR_UNTERMINATED_COMMENT,
  W_SEED_LEX_ERROR_UNSUPPORTED_UNICODE_IDENTIFIER,
  W_SEED_LEX_ERROR_UNSUPPORTED_CONTROL,
  W_SEED_LEX_ERROR_OPAQUE_RANGE,
  W_SEED_LEX_ERROR_OPAQUE_UNCLAIMED,
  W_SEED_LEX_ERROR_FRAME_LIMIT,
} w_seed_lex_error_kind;

typedef struct {
  w_seed_lex_item_kind kind;
  w_seed_span span;
  union {
    w_seed_source_prefix_kind source_prefix;
    w_seed_trivia_kind trivia;
    struct {
      uint32_t flags;
    } token;
    struct {
      w_seed_literal_event_kind event;
      w_seed_literal_kind literal;
    } literal;
  } payload;
} w_seed_lex_item;

typedef struct {
  w_seed_lex_error_kind kind;
  w_seed_span primary;
  w_seed_span opening;
  w_seed_literal_kind literal;
  bool reached_eof;
} w_seed_lex_error;

typedef enum {
  W_SEED_LEXER_FRAME_LITERAL = 0,
  W_SEED_LEXER_FRAME_INTERPOLATION,
} w_seed_lexer_frame_kind;

typedef struct {
  w_seed_lexer_frame_kind kind;
  w_seed_literal_kind literal;
  w_seed_span opening;
  size_t close_length;
  size_t brace_depth;
  bool allows_interpolation;
  bool raw;
} w_seed_lexer_frame;

typedef struct {
  const w_seed_source *source;
  w_seed_span bounds;
  size_t cursor;
  w_seed_lexer_frame *frames;
  size_t frame_count;
  size_t frame_capacity;
  bool source_prefix_pending;
  bool terminal;
  bool opaque_required;
  bool opaque_claimed;
  w_seed_span opaque_span;
} w_seed_lexer;

/* Initialize a lexer over a validated source span. The caller owns frames. */
bool w_seed_lexer_init(const w_seed_source *source, w_seed_span bounds,
                       w_seed_lexer_frame *frames, size_t frame_capacity,
                       w_seed_lexer *lexer, w_seed_lex_error *error);

/* Require an external opaque-body claim before the current cursor advances. */
bool w_seed_lexer_require_opaque(w_seed_lexer *lexer,
                                 w_seed_lex_error *error);

/* Claim exactly the current cursor range as one externally-scanned body. */
bool w_seed_lexer_claim_opaque(w_seed_lexer *lexer, w_seed_span body,
                               w_seed_lex_error *error);

/* Return one lossless item. EOF is an item; false is a terminal error. */
bool w_seed_lexer_next(w_seed_lexer *lexer, w_seed_lex_item *item,
                       w_seed_lex_error *error);

/* Return the next byte offset that the lexer will consume. */
size_t w_seed_lexer_offset(const w_seed_lexer *lexer);

#ifdef __cplusplus
}
#endif

#endif
