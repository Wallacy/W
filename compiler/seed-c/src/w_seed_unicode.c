#include "w_seed_unicode.h"

static bool in_ranges(uint32_t code_point,
                      const w_seed_unicode_range *ranges, size_t count) {
  size_t low = 0;
  size_t high = count;
  while (low < high) {
    const size_t middle = low + (high - low) / 2;
    const w_seed_unicode_range range = ranges[middle];
    if (code_point < range.start) {
      high = middle;
    } else if (code_point > range.end) {
      low = middle + 1;
    } else {
      return true;
    }
  }
  return false;
}

bool w_seed_unicode_is_default_ignorable(uint32_t code_point) {
  if (code_point > UINT32_C(0x10FFFF)) return false;
  return in_ranges(code_point, w_seed_unicode_default_ignorable_ranges,
                   w_seed_unicode_default_ignorable_range_count);
}

bool w_seed_unicode_is_identifier_start(uint32_t code_point) {
  if (code_point > UINT32_C(0x10FFFF) ||
      w_seed_unicode_is_default_ignorable(code_point)) {
    return false;
  }
  /* LOW LINE is the W profile addition to UAX #31 XID_Start. */
  if (code_point == UINT32_C(0x00005F)) return true;
  return in_ranges(code_point, w_seed_unicode_xid_start_ranges,
                   w_seed_unicode_xid_start_range_count);
}

bool w_seed_unicode_is_identifier_continue(uint32_t code_point) {
  if (code_point > UINT32_C(0x10FFFF) ||
      w_seed_unicode_is_default_ignorable(code_point)) {
    return false;
  }
  return in_ranges(code_point, w_seed_unicode_xid_continue_ranges,
                   w_seed_unicode_xid_continue_range_count);
}
