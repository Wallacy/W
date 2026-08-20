#include "w_seed_diagnostic.h"
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
  PROBE_DIAGNOSTIC_OUTPUT = 32 * 1024 * 1024,
};

static uint8_t input_bytes[PROBE_SOURCE_CAPACITY];
static uint8_t canonical_bytes[PROBE_FORMAT_OUTPUT];
static uint8_t diagnostic_bytes[PROBE_DIAGNOSTIC_OUTPUT];
static w_seed_lexer_frame lexer_frames[PROBE_LEXER_FRAMES];
static w_seed_parse_token tokens[PROBE_TOKENS];
static w_seed_cst_node nodes[PROBE_NODES];
static w_seed_parse_frame parse_frames[PROBE_PARSE_FRAMES];
static w_seed_parse_issue issues[PROBE_ISSUES];
static w_seed_format_token format_tokens[PROBE_FORMAT_TOKENS];
static w_seed_format_group format_groups[PROBE_FORMAT_GROUPS];

static const w_seed_foreign_limits FOREIGN_LIMITS = {64u * 1024u, 256u};

int main(int argc, char **argv) {
#ifdef _WIN32
  (void)_setmode(_fileno(stdin), _O_BINARY);
  (void)_setmode(_fileno(stdout), _O_BINARY);
#endif
  if (argc != 3) return 2;
  const size_t instance_length = strlen(argv[1]);
  const size_t source_id_length = strlen(argv[2]);
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
    return 3;
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
    return 4;
  }
  w_seed_parse_result parse_result;
  if (!w_seed_parser_parse(&parser, &parse_result) ||
      parse_result.status != W_SEED_PARSE_COMPLETE) {
    return 5;
  }
  const w_seed_formatter_buffers formatter_buffers = {
      format_tokens, sizeof(format_tokens) / sizeof(format_tokens[0]),
      format_groups, sizeof(format_groups) / sizeof(format_groups[0])};
  w_seed_format_result format_result;
  if (w_seed_formatter_format(&parser, &formatter_buffers, canonical_bytes,
                              sizeof(canonical_bytes), &format_result) !=
      W_SEED_FORMAT_OK) {
    return 6;
  }
  w_seed_diagnostic_result diagnostic_result;
  const w_seed_diagnostic_status status = w_seed_diagnostic_format_record(
      argv[1], instance_length, argv[2], source_id_length, input_bytes, length,
      canonical_bytes, format_result.written_bytes, NULL, 0,
      &diagnostic_result);
  if (status == W_SEED_DIAGNOSTIC_NO_RECORD) return 0;
  if (status != W_SEED_DIAGNOSTIC_CAPACITY ||
      diagnostic_result.required_bytes > sizeof(diagnostic_bytes)) {
    return 7;
  }
  if (w_seed_diagnostic_format_record(
          argv[1], instance_length, argv[2], source_id_length, input_bytes,
          length, canonical_bytes, format_result.written_bytes,
          diagnostic_bytes, sizeof(diagnostic_bytes), &diagnostic_result) !=
      W_SEED_DIAGNOSTIC_OK) {
    return 8;
  }
  if (fwrite(diagnostic_bytes, 1, diagnostic_result.written_bytes, stdout) !=
      diagnostic_result.written_bytes) {
    return 9;
  }
  return 0;
}
