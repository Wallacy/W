#ifndef W_SEED_UNICODE_H
#define W_SEED_UNICODE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Internal Unicode 17.0.0 classifier data. The lexer owns no code-point copy. */
typedef struct {
  uint32_t start;
  uint32_t end;
} w_seed_unicode_range;

bool w_seed_unicode_is_default_ignorable(uint32_t code_point);
bool w_seed_unicode_is_identifier_start(uint32_t code_point);
bool w_seed_unicode_is_identifier_continue(uint32_t code_point);

extern const w_seed_unicode_range w_seed_unicode_xid_start_ranges[];
extern const size_t w_seed_unicode_xid_start_range_count;
extern const w_seed_unicode_range w_seed_unicode_xid_continue_ranges[];
extern const size_t w_seed_unicode_xid_continue_range_count;
extern const w_seed_unicode_range w_seed_unicode_default_ignorable_ranges[];
extern const size_t w_seed_unicode_default_ignorable_range_count;

#ifdef __cplusplus
}
#endif

#endif
