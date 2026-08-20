#include "w_seed_parser.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

enum {
  PROBE_SOURCE_CAPACITY = 16 * 1024 * 1024,
  PROBE_LEXER_FRAMES = 512,
  PROBE_TOKENS = 128,
  PROBE_NODES = 65536,
  PROBE_PARSE_FRAMES = 4096,
  PROBE_ISSUES = 1024,
};

static const w_seed_foreign_limits PROBE_FOREIGN_LIMITS = {
    64u * 1024u,
    256u,
};

static uint8_t input_bytes[PROBE_SOURCE_CAPACITY];
static w_seed_lexer_frame lexer_frames[PROBE_LEXER_FRAMES];
static w_seed_parse_token tokens[PROBE_TOKENS];
static w_seed_cst_node nodes[PROBE_NODES];
static w_seed_parse_frame parse_frames[PROBE_PARSE_FRAMES];
static w_seed_parse_issue issues[PROBE_ISSUES];

static const char *status_name(w_seed_parse_status status) {
  switch (status) {
    case W_SEED_PARSE_COMPLETE:
      return "complete";
    case W_SEED_PARSE_RECOVERED:
      return "recovered";
    case W_SEED_PARSE_FATAL:
      return "fatal";
  }
  return "unknown";
}

int main(void) {
#ifdef _WIN32
  (void)_setmode(_fileno(stdin), _O_BINARY);
#endif
  size_t length = 0;
  while (length < PROBE_SOURCE_CAPACITY) {
    const size_t room = PROBE_SOURCE_CAPACITY - length;
    const size_t count = fread(input_bytes + length, 1, room, stdin);
    length += count;
    if (count < room) {
      if (ferror(stdin) != 0) {
        (void)fputs("error=read\n", stderr);
        return 2;
      }
      break;
    }
  }
  if (length == PROBE_SOURCE_CAPACITY && fgetc(stdin) != EOF) {
    (void)fputs("error=source-too-large\n", stderr);
    return 2;
  }

  const w_seed_byte_view bytes = {input_bytes, length};
  w_seed_source source;
  w_seed_source_error source_error;
  if (!w_seed_source_init(bytes, &source, &source_error)) {
    (void)fprintf(stderr, "error=source kind=%d offset=%" PRIuMAX "\n",
                  source_error.kind, (uintmax_t)source_error.byte_offset);
    return 2;
  }
  w_seed_parser parser;
  w_seed_lex_error lex_error;
  const w_seed_span bounds = {0, length};
  if (!w_seed_parser_init(
          &source, bounds, PROBE_FOREIGN_LIMITS, lexer_frames,
          PROBE_LEXER_FRAMES, tokens,
          PROBE_TOKENS, nodes, PROBE_NODES, parse_frames, PROBE_PARSE_FRAMES,
          issues, PROBE_ISSUES, &parser, &lex_error)) {
    (void)fprintf(stderr, "error=lexer kind=%d start=%" PRIuMAX "\n",
                  lex_error.kind, (uintmax_t)lex_error.primary.start_byte);
    return 2;
  }
  w_seed_parse_result result;
  if (!w_seed_parser_parse(&parser, &result)) {
    (void)fputs("error=parse-init\n", stderr);
    return 2;
  }
  (void)printf("RESULT status=%s nodes=%" PRIuMAX " leaves=%" PRIuMAX
               " issues=%" PRIuMAX " consumed=%" PRIuMAX " root=%" PRIu32
               " length=%" PRIuMAX "\n",
               status_name(result.status), (uintmax_t)result.node_count,
               (uintmax_t)result.leaf_count, (uintmax_t)result.issue_count,
               (uintmax_t)result.consumed_byte, result.root,
               (uintmax_t)length);
  for (size_t index = 0; index < result.node_count; index += 1) {
    const w_seed_cst_node *node = &nodes[index];
    (void)printf("NODE index=%" PRIuMAX " kind=%d flags=%u start=%" PRIuMAX
                 " end=%" PRIuMAX " first=%" PRIu32 " next=%" PRIu32 "\n",
                 (uintmax_t)index, node->kind, (unsigned int)node->flags,
                 (uintmax_t)node->raw_span.start_byte,
                 (uintmax_t)node->raw_span.end_byte, node->first_child,
                 node->next_sibling);
  }
  for (size_t index = 0; index < result.issue_count; index += 1) {
    const w_seed_parse_issue *issue = &issues[index];
    (void)printf("ISSUE index=%" PRIuMAX " kind=%d start=%" PRIuMAX
                 " end=%" PRIuMAX " actual=%d expected=%u\n",
                 (uintmax_t)index, issue->kind,
                 (uintmax_t)issue->primary.start_byte,
                 (uintmax_t)issue->primary.end_byte, issue->actual_kind,
                 (unsigned int)issue->expected_mask);
  }
  return 0;
}
