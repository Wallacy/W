#ifndef W_SEED_PARSER_H
#define W_SEED_PARSER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "w_seed_lexer.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The parser is an internal, caller-owned seed API. It does not allocate. */
typedef uint32_t w_seed_cst_index;

#define W_SEED_CST_NONE UINT32_MAX

typedef enum {
  W_SEED_CST_DOCUMENT = 0,
  W_SEED_CST_MODULE_HEADER,
  W_SEED_CST_FUNCTION,
  W_SEED_CST_PARAMETER_LIST,
  W_SEED_CST_PARAMETER,
  W_SEED_CST_RETURN_TYPE,
  W_SEED_CST_ENTRY,
  W_SEED_CST_BLOCK,
  W_SEED_CST_LET_STATEMENT,
  W_SEED_CST_RETURN_STATEMENT,
  W_SEED_CST_IF_STATEMENT,
  W_SEED_CST_IF_EXPRESSION,
  W_SEED_CST_REPEAT_STATEMENT,
  W_SEED_CST_LABEL,
  W_SEED_CST_BREAK_STATEMENT,
  W_SEED_CST_CONTINUE_STATEMENT,
  W_SEED_CST_EXPRESSION_STATEMENT,
  W_SEED_CST_EXPRESSION,
  W_SEED_CST_TYPE,
  W_SEED_CST_PARENTHESES,
  W_SEED_CST_ARRAY,
  W_SEED_CST_ERROR,
  W_SEED_CST_MISSING,
  W_SEED_CST_SOURCE_PREFIX,
  W_SEED_CST_TRIVIA,
  W_SEED_CST_WORD,
  W_SEED_CST_NUMBER,
  W_SEED_CST_PUNCTUATION,
  W_SEED_CST_LITERAL_EVENT,
  W_SEED_CST_FOREIGN_BODY,
  W_SEED_CST_UNKNOWN,
} w_seed_cst_kind;

enum {
  W_SEED_CST_FLAG_RAW_LEAF = 1u << 0,
  W_SEED_CST_FLAG_TRIVIA = 1u << 1,
  W_SEED_CST_FLAG_ERROR = 1u << 2,
  W_SEED_CST_FLAG_MISSING = 1u << 3,
};

typedef enum {
  W_SEED_PARSE_COMPLETE = 0,
  W_SEED_PARSE_RECOVERED,
  W_SEED_PARSE_FATAL,
} w_seed_parse_status;

/* Internal issue kinds. The comments record the future D0 adapter mapping. */
typedef enum {
  W_SEED_PARSE_ISSUE_NONE = 0,
  W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN,       /* W-PARSE-0001 */
  W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE,   /* W-PARSE-0002 */
  W_SEED_PARSE_ISSUE_NO_CONTINUATION_OWNER, /* W-PARSE-0004 */
  W_SEED_PARSE_ISSUE_MIXED_ROOT,             /* W-PARSE-0006 */
  W_SEED_PARSE_ISSUE_UNSUPPORTED_ROOT,
  W_SEED_PARSE_ISSUE_UNSUPPORTED_FORM,
  W_SEED_PARSE_ISSUE_SPACED_HEAD, /* W-PARSE-0013 */
  W_SEED_PARSE_ISSUE_VALUE_IF_MISSING_ELSE, /* W-PARSE-0021 */
  W_SEED_PARSE_ISSUE_FOREIGN_UNSUPPORTED,
  W_SEED_PARSE_ISSUE_LEXER,
  W_SEED_PARSE_ISSUE_CAPACITY,
} w_seed_parse_issue_kind;

enum {
  W_SEED_PARSE_EXPECT_WORD = 1u << 0,
  W_SEED_PARSE_EXPECT_PUNCTUATION = 1u << 1,
  W_SEED_PARSE_EXPECT_EXPRESSION = 1u << 2,
  W_SEED_PARSE_EXPECT_STATEMENT = 1u << 3,
};

typedef struct {
  w_seed_cst_kind kind;
  uint16_t flags;
  w_seed_span raw_span;
  w_seed_cst_index first_child;
  w_seed_cst_index next_sibling;
} w_seed_cst_node;

/* Caller-owned parser lookahead storage. A view never owns source bytes. */
typedef struct {
  w_seed_lex_item item;
} w_seed_parse_token;

typedef struct {
  w_seed_span span;
  w_seed_cst_index origin_leaf;
  uint8_t flags;
} w_seed_parse_token_view;

enum {
  W_SEED_PARSE_TOKEN_VIEW_VIRTUAL = 1u << 0,
};

typedef struct {
  w_seed_cst_kind kind;
  w_seed_cst_index node;
  w_seed_cst_index last_child;
} w_seed_parse_frame;

typedef struct {
  w_seed_parse_issue_kind kind;
  w_seed_span primary;
  w_seed_span owner;
  w_seed_lex_item_kind actual_kind;
  uint32_t expected_mask;
} w_seed_parse_issue;

typedef struct {
  w_seed_parse_status status;
  w_seed_cst_index root;
  size_t node_count;
  size_t leaf_count;
  size_t issue_count;
  size_t consumed_byte;
} w_seed_parse_result;

typedef struct {
  const w_seed_source *source;
  w_seed_lexer lexer;
  w_seed_lexer_frame *lexer_frames;
  size_t lexer_frame_capacity;
  w_seed_parse_token *token_cache;
  size_t token_capacity;
  size_t token_count;
  w_seed_cst_node *nodes;
  size_t node_capacity;
  size_t node_count;
  size_t leaf_count;
  w_seed_parse_frame *frames;
  size_t frame_capacity;
  size_t frame_count;
  w_seed_parse_issue *issues;
  size_t issue_capacity;
  size_t issue_count;
  w_seed_parse_status status;
  bool parsed;
  w_seed_cst_index root;
  size_t last_token_end;
  bool has_last_token;
  bool virtual_close_active;
  size_t virtual_close_offset;
  w_seed_cst_index virtual_close_leaf;
} w_seed_parser;

/* Initialize over a validated source span. All storage remains caller-owned. */
bool w_seed_parser_init(const w_seed_source *source, w_seed_span bounds,
                        w_seed_lexer_frame *lexer_frames,
                        size_t lexer_frame_capacity,
                        w_seed_parse_token *token_cache,
                        size_t token_capacity, w_seed_cst_node *nodes,
                        size_t node_capacity, w_seed_parse_frame *frames,
                        size_t frame_capacity, w_seed_parse_issue *issues,
                        size_t issue_capacity, w_seed_parser *parser,
                        w_seed_lex_error *lex_error);

/*
 * Parse one document. The result aliases parser-owned caller buffers. The
 * parser is single-use: a second call returns false without changing the
 * result or caller-owned buffers.
 */
bool w_seed_parser_parse(w_seed_parser *parser, w_seed_parse_result *result);

#ifdef __cplusplus
}
#endif

#endif
