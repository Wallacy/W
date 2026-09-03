#ifndef W_SEED_DIAGNOSTIC_H
#define W_SEED_DIAGNOSTIC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "w_seed_lexer.h"
#include "w_seed_parser.h"
#include "w_seed_frontend.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  W_SEED_DIAGNOSTIC_OK = 0,
  W_SEED_DIAGNOSTIC_NO_RECORD,
  W_SEED_DIAGNOSTIC_CAPACITY,
  W_SEED_DIAGNOSTIC_UNSUPPORTED,
  W_SEED_DIAGNOSTIC_INVALID,
} w_seed_diagnostic_status;

typedef struct {
  w_seed_diagnostic_status status;
  size_t required_bytes;
  size_t written_bytes;
  size_t primary_byte;
} w_seed_diagnostic_result;

/* Build the bounded source.format W-FMT-0001 record. All identity strings and
 * byte spans are caller-owned. The canonical bytes are already produced by a
 * clean formatter call; this adapter does not claim semantic validation. */
w_seed_diagnostic_status w_seed_diagnostic_format_record(
    const char *instance, size_t instance_length, const char *source_id,
    size_t source_id_length, const uint8_t *source, size_t source_length,
    const uint8_t *canonical, size_t canonical_length, uint8_t *output,
    size_t output_capacity, w_seed_diagnostic_result *result);

/* Map only unterminated literal/comment terminal facts covered by W-LEX-0001.
 * Facts use stable semantic names: string-literal with quote, raw-quote,
 * triple-quote, or raw-triple-quote; byte-string-literal with byte-quote;
 * byte-scalar-literal with byte-apostrophe; and block-comment with
 * block-comment-close. Apostrophe- and quote-delimited ordinary strings share
 * string-literal. Other lexer facts are explicitly unsupported and must not be
 * mislabeled. */
w_seed_diagnostic_status w_seed_diagnostic_lex_record(
    const char *instance, size_t instance_length, const char *source_id,
    size_t source_id_length, const w_seed_lex_error *error, size_t source_length,
    uint8_t *output, size_t output_capacity,
    w_seed_diagnostic_result *result);

/* Map current parser issue facts to the catalog's W-PARSE IDs. The adapter
 * emits one record per selected issue and does not infer semantic facts. */
w_seed_diagnostic_status w_seed_diagnostic_parse_record(
    const char *instance, size_t instance_length, const char *source_id,
    size_t source_id_length, const w_seed_parse_issue *issue, size_t source_length,
    uint8_t *output, size_t output_capacity,
    w_seed_diagnostic_result *result);

/* The frontend adapter receives the complete caller-owned source inventory and
 * typed frontend output. It validates every source view, one diagnostic, and
 * all of its related ranges before measuring or writing a D0 record. */
typedef struct {
  const w_seed_source *sources;
  const w_seed_frontend_text *source_ids;
  size_t source_count;
  const w_seed_frontend_output *frontend_output;
  size_t diagnostic_count;
  size_t diagnostic_fact_count;
  size_t diagnostic_item_count;
  size_t diagnostic_label_count;
} w_seed_diagnostic_frontend_context;

/* Map one of the 17 active frontend diagnostics to D0. Unknown frontend
 * codes return UNSUPPORTED. The output contains one record without a trailing
 * newline, so callers can compose JSONL atomically. */
w_seed_diagnostic_status w_seed_diagnostic_frontend_record(
    const char *instance, size_t instance_length,
    const w_seed_diagnostic_frontend_context *context,
    size_t diagnostic_index, uint8_t *output, size_t output_capacity,
    w_seed_diagnostic_result *result);

#ifdef __cplusplus
}
#endif

#endif
