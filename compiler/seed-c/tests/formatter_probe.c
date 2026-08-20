#include "w_seed_formatter.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

enum {
  PROBE_SOURCE_CAPACITY = 16 * 1024 * 1024,
  PROBE_LEXER_FRAMES = 512,
  PROBE_TOKENS = 4096,
  PROBE_NODES = 65536,
  PROBE_PARSE_FRAMES = 4096,
  PROBE_ISSUES = 1024,
  PROBE_FORMAT_TOKENS = 65536,
  PROBE_FORMAT_GROUPS = 65536,
  PROBE_FORMAT_OUTPUT = 16 * 1024 * 1024,
};

static uint8_t input_bytes[PROBE_SOURCE_CAPACITY];
static uint8_t output_bytes[PROBE_FORMAT_OUTPUT];
static w_seed_lexer_frame lexer_frames[PROBE_LEXER_FRAMES];
static w_seed_parse_token tokens[PROBE_TOKENS];
static w_seed_cst_node nodes[PROBE_NODES];
static w_seed_parse_frame parse_frames[PROBE_PARSE_FRAMES];
static w_seed_parse_issue issues[PROBE_ISSUES];
static w_seed_format_token format_tokens[PROBE_FORMAT_TOKENS];
static w_seed_format_group format_groups[PROBE_FORMAT_GROUPS];

static const w_seed_foreign_limits FOREIGN_LIMITS = {64u * 1024u, 256u};

int main(void) {
#ifdef _WIN32
  (void)_setmode(_fileno(stdin), _O_BINARY);
  (void)_setmode(_fileno(stdout), _O_BINARY);
#endif
  size_t length = 0;
  while (length < sizeof(input_bytes)) {
    const size_t room = sizeof(input_bytes) - length;
    const size_t count = fread(input_bytes + length, 1, room, stdin);
    length += count;
    if (count < room) {
      if (ferror(stdin) != 0) return 2;
      break;
    }
  }
  if (length == sizeof(input_bytes) && fgetc(stdin) != EOF) return 2;
  w_seed_source source;
  w_seed_source_error source_error;
  if (!w_seed_source_init((w_seed_byte_view){input_bytes, length}, &source,
                          &source_error)) {
    return 2;
  }
  w_seed_parser parser;
  w_seed_lex_error lex_error;
  if (!w_seed_parser_init(
          &source, (w_seed_span){0, length}, FOREIGN_LIMITS, lexer_frames,
          sizeof(lexer_frames) / sizeof(lexer_frames[0]), tokens,
          sizeof(tokens) / sizeof(tokens[0]), nodes,
          sizeof(nodes) / sizeof(nodes[0]), parse_frames,
          sizeof(parse_frames) / sizeof(parse_frames[0]), issues,
          sizeof(issues) / sizeof(issues[0]), &parser, &lex_error)) {
    return 2;
  }
  w_seed_parse_result parse_result;
  if (!w_seed_parser_parse(&parser, &parse_result) ||
      parse_result.status != W_SEED_PARSE_COMPLETE) {
    (void)fprintf(stderr, "parse status=%d issues=%" PRIuMAX "\n",
                  parse_result.status, (uintmax_t)parse_result.issue_count);
    return 3;
  }
  const w_seed_formatter_buffers buffers = {
      format_tokens, sizeof(format_tokens) / sizeof(format_tokens[0]),
      format_groups, sizeof(format_groups) / sizeof(format_groups[0])};
  w_seed_format_result format_result;
  const w_seed_format_status status = w_seed_formatter_format(
      &parser, &buffers, output_bytes, sizeof(output_bytes), &format_result);
  if (status != W_SEED_FORMAT_OK) {
    (void)fprintf(stderr, "format status=%d required=%" PRIuMAX "\n", status,
                  (uintmax_t)format_result.required_bytes);
    return 4;
  }
  if (fwrite(output_bytes, 1, format_result.written_bytes, stdout) !=
      format_result.written_bytes) {
    return 5;
  }
  return 0;
}
