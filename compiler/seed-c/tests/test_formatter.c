#include "w_seed_formatter.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum {
  TEST_LEXER_FRAMES = 128,
  TEST_TOKENS = 512,
  TEST_NODES = 4096,
  TEST_PARSE_FRAMES = 256,
  TEST_ISSUES = 64,
  TEST_FORMAT_TOKENS = 4096,
  TEST_FORMAT_GROUPS = 4096,
  TEST_OUTPUT = 8192,
};

static bool parse_text(const uint8_t *bytes, size_t length,
                       w_seed_parser *parser, w_seed_parse_result *result,
                       w_seed_lexer_frame *lexer_frames,
                       w_seed_parse_token *tokens, w_seed_cst_node *nodes,
                       w_seed_parse_frame *parse_frames,
                       w_seed_parse_issue *issues) {
  static w_seed_source storage;
  w_seed_source_error source_error;
  if (!w_seed_source_init((w_seed_byte_view){bytes, length}, &storage,
                          &source_error)) {
    return false;
  }
  w_seed_lex_error lex_error;
  if (!w_seed_parser_init(
          &storage, (w_seed_span){0, length},
          (w_seed_foreign_limits){64u * 1024u, 256u}, lexer_frames,
          TEST_LEXER_FRAMES, tokens, TEST_TOKENS, nodes, TEST_NODES,
          parse_frames, TEST_PARSE_FRAMES, issues, TEST_ISSUES, parser,
          &lex_error)) {
    return false;
  }
  return w_seed_parser_parse(parser, result);
}

static bool all_bytes_equal(const uint8_t *bytes, size_t length,
                            uint8_t value) {
  for (size_t index = 0; index < length; index += 1) {
    if (bytes[index] != value) return false;
  }
  return true;
}

int main(void) {
  static const uint8_t complete[] = "fn f(){return 1}\n";
  static const uint8_t recovered[] = "fn f( {\n";
  static w_seed_lexer_frame lexer_frames[TEST_LEXER_FRAMES];
  static w_seed_parse_token tokens[TEST_TOKENS];
  static w_seed_cst_node nodes[TEST_NODES];
  static w_seed_parse_frame parse_frames[TEST_PARSE_FRAMES];
  static w_seed_parse_issue issues[TEST_ISSUES];
  static w_seed_format_token format_tokens[TEST_FORMAT_TOKENS];
  static w_seed_format_group format_groups[TEST_FORMAT_GROUPS];
  static uint8_t output[TEST_OUTPUT + 8];
  const w_seed_formatter_buffers buffers = {
      format_tokens, TEST_FORMAT_TOKENS, format_groups, TEST_FORMAT_GROUPS,
  };

  w_seed_parser parser;
  w_seed_parse_result parse_result;
  if (!parse_text(complete, sizeof(complete) - 1, &parser, &parse_result,
                  lexer_frames, tokens, nodes, parse_frames, issues) ||
      parse_result.status != W_SEED_PARSE_COMPLETE) {
    return 1;
  }
  w_seed_format_result measured;
  if (w_seed_formatter_format(&parser, &buffers, NULL, 0, &measured) !=
          W_SEED_FORMAT_CAPACITY ||
      measured.required_bytes == 0 || measured.written_bytes != 0) {
    return 2;
  }
  const size_t required = measured.required_bytes;
  if (required + 8 > sizeof(output)) return 3;
  (void)memset(output, 0xA5, sizeof(output));
  w_seed_format_result exact;
  if (w_seed_formatter_format(&parser, &buffers, output, required, &exact) !=
          W_SEED_FORMAT_OK ||
      exact.written_bytes != required ||
      !all_bytes_equal(output + required, sizeof(output) - required, 0xA5)) {
    return 4;
  }
  (void)memset(output, 0xA5, sizeof(output));
  w_seed_format_result short_result;
  if (required == 0 ||
      w_seed_formatter_format(&parser, &buffers, output, required - 1,
                               &short_result) != W_SEED_FORMAT_CAPACITY ||
      short_result.written_bytes != 0 ||
      !all_bytes_equal(output, sizeof(output), 0xA5)) {
    return 5;
  }

  static w_seed_format_token narrow_tokens[TEST_FORMAT_TOKENS];
  static w_seed_format_group narrow_groups[TEST_FORMAT_GROUPS];
  static uint8_t narrow_output[TEST_OUTPUT];
  if (measured.token_count < 2 || measured.group_count < 2) return 6;
  const w_seed_formatter_buffers narrow_token_buffers = {
      narrow_tokens, measured.token_count - 1, format_groups,
      TEST_FORMAT_GROUPS};
  (void)memset(narrow_output, 0xA5, sizeof(narrow_output));
  w_seed_format_result narrow_token_result;
  if (w_seed_formatter_format(&parser, &narrow_token_buffers, narrow_output,
                               sizeof(narrow_output), &narrow_token_result) !=
          W_SEED_FORMAT_CAPACITY ||
      narrow_token_result.written_bytes != 0 ||
      !all_bytes_equal(narrow_output, sizeof(narrow_output), 0xA5)) {
    return 7;
  }
  const w_seed_formatter_buffers narrow_group_buffers = {
      format_tokens, TEST_FORMAT_TOKENS, narrow_groups,
      measured.group_count - 1};
  (void)memset(narrow_output, 0xA5, sizeof(narrow_output));
  w_seed_format_result narrow_group_result;
  if (w_seed_formatter_format(&parser, &narrow_group_buffers, narrow_output,
                               sizeof(narrow_output), &narrow_group_result) !=
          W_SEED_FORMAT_CAPACITY ||
      narrow_group_result.written_bytes != 0 ||
      !all_bytes_equal(narrow_output, sizeof(narrow_output), 0xA5)) {
    return 8;
  }

  w_seed_parser recovered_parser;
  w_seed_parse_result recovered_result;
  if (!parse_text(recovered, sizeof(recovered) - 1, &recovered_parser,
                  &recovered_result, lexer_frames, tokens, nodes, parse_frames,
                  issues) || recovered_result.status == W_SEED_PARSE_COMPLETE) {
    return 9;
  }
  w_seed_format_result rejected;
  if (w_seed_formatter_format(&recovered_parser, &buffers, output,
                              sizeof(output), &rejected) !=
      W_SEED_FORMAT_REJECTED) {
    return 10;
  }
  return 0;
}
