#include "w_seed_diagnostic.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

enum {
  PROBE_SOURCE_CAPACITY = 64 * 1024,
  PROBE_LEXER_FRAMES = 128,
  PROBE_TOKENS = 256,
  PROBE_NODES = 4096,
  PROBE_PARSE_FRAMES = 256,
  PROBE_ISSUES = 128,
  PROBE_OUTPUT = 64 * 1024,
};

static uint8_t source_bytes[PROBE_SOURCE_CAPACITY];
static uint8_t output_bytes[PROBE_OUTPUT];
static w_seed_lexer_frame lexer_frames[PROBE_LEXER_FRAMES];
static w_seed_parse_token tokens[PROBE_TOKENS];
static w_seed_cst_node nodes[PROBE_NODES];
static w_seed_parse_frame parse_frames[PROBE_PARSE_FRAMES];
static w_seed_parse_issue issues[PROBE_ISSUES];

static const w_seed_foreign_limits FOREIGN_LIMITS = {64u * 1024u, 256u};

static int read_source(size_t *length) {
  *length = 0;
  while (*length < sizeof(source_bytes)) {
    const size_t room = sizeof(source_bytes) - *length;
    const size_t count = fread(source_bytes + *length, 1, room, stdin);
    *length += count;
    if (count < room) {
      if (ferror(stdin) != 0) return 2;
      break;
    }
  }
  if (*length == sizeof(source_bytes) && fgetc(stdin) != EOF) return 2;
  return 0;
}

static int emit_lex(const char *instance, size_t instance_length,
                    const char *source_id, size_t source_id_length,
                    size_t source_length) {
  w_seed_source source;
  w_seed_source_error source_error;
  if (!w_seed_source_init((w_seed_byte_view){source_bytes, source_length},
                          &source, &source_error)) {
    return 3;
  }
  w_seed_lexer lexer;
  w_seed_lex_error error;
  if (!w_seed_lexer_init(&source, (w_seed_span){0, source_length},
                         lexer_frames, PROBE_LEXER_FRAMES, &lexer, &error)) {
    return 4;
  }
  w_seed_lex_item item;
  while (w_seed_lexer_next(&lexer, &item, &error)) {
    if (item.kind == W_SEED_LEX_ITEM_EOF) return 5;
  }
  w_seed_diagnostic_result result;
  const w_seed_diagnostic_status measured = w_seed_diagnostic_lex_record(
      instance, instance_length, source_id, source_id_length, &error,
      source_length, NULL, 0, &result);
  if (measured != W_SEED_DIAGNOSTIC_CAPACITY ||
      result.required_bytes > sizeof(output_bytes)) {
    return 6;
  }
  if (w_seed_diagnostic_lex_record(
          instance, instance_length, source_id, source_id_length, &error,
          source_length, output_bytes, sizeof(output_bytes), &result) !=
      W_SEED_DIAGNOSTIC_OK) {
    return 7;
  }
  if (fwrite(output_bytes, 1, result.written_bytes, stdout) !=
      result.written_bytes) {
    return 8;
  }
  return 0;
}

static int emit_parse(const char *instance, size_t instance_length,
                      const char *source_id, size_t source_id_length,
                      size_t source_length) {
  w_seed_source source;
  w_seed_source_error source_error;
  if (!w_seed_source_init((w_seed_byte_view){source_bytes, source_length},
                          &source, &source_error)) {
    return 3;
  }
  w_seed_parser parser;
  w_seed_lex_error lex_error;
  if (!w_seed_parser_init(
          &source, (w_seed_span){0, source_length}, FOREIGN_LIMITS,
          lexer_frames, PROBE_LEXER_FRAMES, tokens, PROBE_TOKENS, nodes,
          PROBE_NODES, parse_frames, PROBE_PARSE_FRAMES, issues, PROBE_ISSUES,
          &parser, &lex_error)) {
    return 4;
  }
  w_seed_parse_result parsed;
  if (!w_seed_parser_parse(&parser, &parsed) || parsed.issue_count == 0) {
    return 5;
  }
  for (size_t index = 0; index < parsed.issue_count; index += 1) {
    w_seed_diagnostic_result result;
    const w_seed_diagnostic_status measured = w_seed_diagnostic_parse_record(
        instance, instance_length, source_id, source_id_length, &issues[index],
        source_length, NULL, 0, &result);
    if (measured == W_SEED_DIAGNOSTIC_UNSUPPORTED) continue;
    if (measured != W_SEED_DIAGNOSTIC_CAPACITY ||
        result.required_bytes > sizeof(output_bytes)) {
      return 6;
    }
    if (w_seed_diagnostic_parse_record(
            instance, instance_length, source_id, source_id_length,
            &issues[index], source_length, output_bytes, sizeof(output_bytes),
            &result) != W_SEED_DIAGNOSTIC_OK) {
      return 7;
    }
    if (fwrite(output_bytes, 1, result.written_bytes, stdout) !=
        result.written_bytes) {
      return 8;
    }
    return 0;
  }
  return 9;
}

int main(int argc, char **argv) {
#ifdef _WIN32
  (void)_setmode(_fileno(stdin), _O_BINARY);
  (void)_setmode(_fileno(stdout), _O_BINARY);
#endif
  if (argc != 4) return 2;
  size_t source_length = 0;
  if (read_source(&source_length) != 0) return 2;
  const size_t instance_length = strlen(argv[2]);
  const size_t source_id_length = strlen(argv[3]);
  if (strcmp(argv[1], "lex") == 0) {
    return emit_lex(argv[2], instance_length, argv[3], source_id_length,
                    source_length);
  }
  if (strcmp(argv[1], "parse") == 0) {
    return emit_parse(argv[2], instance_length, argv[3], source_id_length,
                      source_length);
  }
  return 2;
}
