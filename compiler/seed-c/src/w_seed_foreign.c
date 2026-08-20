#include "w_seed_foreign.h"

#include <limits.h>
#include <string.h>

_Static_assert(CHAR_BIT == 8, "w_seed_foreign requires 8-bit bytes");

typedef struct {
  uint32_t state[8];
  uint64_t bit_count;
  uint8_t block[64];
  size_t block_length;
} sha256_state;

static const uint32_t SHA256_K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
    0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
    0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
    0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
    0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
};

static uint32_t rotate_right(uint32_t value, unsigned int amount) {
  return (value >> amount) | (value << (32u - amount));
}

static uint32_t read_u32_be(const uint8_t *bytes) {
  return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) |
         ((uint32_t)bytes[2] << 8) | (uint32_t)bytes[3];
}

static void write_u32_be(uint8_t *bytes, uint32_t value) {
  bytes[0] = (uint8_t)(value >> 24);
  bytes[1] = (uint8_t)(value >> 16);
  bytes[2] = (uint8_t)(value >> 8);
  bytes[3] = (uint8_t)value;
}

static void sha256_transform(sha256_state *state, const uint8_t *block) {
  uint32_t words[64];
  for (size_t index = 0; index < 16; index += 1) {
    words[index] = read_u32_be(block + index * 4);
  }
  for (size_t index = 16; index < 64; index += 1) {
    const uint32_t first = words[index - 15];
    const uint32_t second = words[index - 2];
    const uint32_t small_sigma0 = rotate_right(first, 7) ^
                                  rotate_right(first, 18) ^ (first >> 3);
    const uint32_t small_sigma1 = rotate_right(second, 17) ^
                                  rotate_right(second, 19) ^ (second >> 10);
    words[index] = words[index - 16] + small_sigma0 + words[index - 7] +
                   small_sigma1;
  }

  uint32_t a = state->state[0];
  uint32_t b = state->state[1];
  uint32_t c = state->state[2];
  uint32_t d = state->state[3];
  uint32_t e = state->state[4];
  uint32_t f = state->state[5];
  uint32_t g = state->state[6];
  uint32_t h = state->state[7];
  for (size_t index = 0; index < 64; index += 1) {
    const uint32_t big_sigma1 = rotate_right(e, 6) ^ rotate_right(e, 11) ^
                                rotate_right(e, 25);
    const uint32_t choose = (e & f) ^ ((~e) & g);
    const uint32_t temporary1 = h + big_sigma1 + choose + SHA256_K[index] +
                                words[index];
    const uint32_t big_sigma0 = rotate_right(a, 2) ^ rotate_right(a, 13) ^
                                rotate_right(a, 22);
    const uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
    const uint32_t temporary2 = big_sigma0 + majority;
    h = g;
    g = f;
    f = e;
    e = d + temporary1;
    d = c;
    c = b;
    b = a;
    a = temporary1 + temporary2;
  }
  state->state[0] += a;
  state->state[1] += b;
  state->state[2] += c;
  state->state[3] += d;
  state->state[4] += e;
  state->state[5] += f;
  state->state[6] += g;
  state->state[7] += h;
}

static void sha256_init(sha256_state *state) {
  static const uint32_t initial[8] = {
      0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
      0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u,
  };
  (void)memset(state, 0, sizeof(*state));
  (void)memcpy(state->state, initial, sizeof(initial));
}

static void sha256_update(sha256_state *state, const uint8_t *bytes,
                          size_t length) {
  state->bit_count += (uint64_t)length * UINT64_C(8);
  while (length != 0) {
    const size_t available = sizeof(state->block) - state->block_length;
    const size_t amount = length < available ? length : available;
    (void)memcpy(state->block + state->block_length, bytes, amount);
    state->block_length += amount;
    bytes += amount;
    length -= amount;
    if (state->block_length == sizeof(state->block)) {
      sha256_transform(state, state->block);
      state->block_length = 0;
    }
  }
}

static void sha256_final(sha256_state *state, uint8_t digest[32]) {
  const uint64_t bit_count = state->bit_count;
  state->block[state->block_length] = 0x80u;
  state->block_length += 1;
  if (state->block_length > 56) {
    (void)memset(state->block + state->block_length, 0,
                 sizeof(state->block) - state->block_length);
    sha256_transform(state, state->block);
    state->block_length = 0;
  }
  (void)memset(state->block + state->block_length, 0,
               56 - state->block_length);
  for (size_t index = 0; index < 8; index += 1) {
    state->block[56 + index] = (uint8_t)(bit_count >> (56 - index * 8));
  }
  sha256_transform(state, state->block);
  for (size_t index = 0; index < 8; index += 1) {
    write_u32_be(digest + index * 4, state->state[index]);
  }
}

static void clear_error(w_seed_foreign_error *error) {
  if (error == NULL) return;
  (void)memset(error, 0, sizeof(*error));
  error->kind = W_SEED_FOREIGN_ERROR_NONE;
  error->terminal_state = W_SEED_FOREIGN_TERMINAL_NONE;
}

static void clear_result(w_seed_foreign_source_validation *result,
                         w_seed_foreign_limits limits) {
  (void)memset(result, 0, sizeof(*result));
  result->profile = W_SEED_FOREIGN_PROFILE_C_INLINE_1;
  result->maximum_body_bytes = limits.maximum_body_bytes;
  result->maximum_nesting = limits.maximum_nesting;
  result->terminal_state = W_SEED_FOREIGN_TERMINAL_NONE;
}

static bool fail_scan(w_seed_foreign_source_validation *result,
                      w_seed_foreign_error *error, w_seed_foreign_error_kind kind,
                      w_seed_foreign_terminal_state terminal, size_t offset,
                      size_t end_offset, bool has_close, size_t close_byte) {
  result->terminal_state = terminal;
  result->digest_valid = false;
  if (error != NULL) {
    error->kind = kind;
    error->terminal_state = terminal;
    error->primary.start_byte = offset;
    error->primary.end_byte = end_offset;
    error->opening.start_byte = 0;
    error->opening.end_byte = 1;
    error->has_close = has_close;
    error->close_byte = close_byte;
  }
  return false;
}

static bool valid_utf8(w_seed_byte_view input, size_t *bad_offset) {
  w_seed_source source;
  w_seed_source_error source_error;
  if (w_seed_source_init(input, &source, &source_error)) return true;
  if (bad_offset != NULL) *bad_offset = source_error.byte_offset;
  return false;
}

static void digest_byte(sha256_state *state, uint8_t byte) {
  sha256_update(state, &byte, 1);
}

static void digest_pair(sha256_state *state, uint8_t first, uint8_t second) {
  const uint8_t bytes[2] = {first, second};
  sha256_update(state, bytes, sizeof(bytes));
}

static bool line_end(const uint8_t *bytes, size_t length, size_t index,
                     size_t *width) {
  if (index >= length) return false;
  if (bytes[index] == 0x0Au) {
    *width = 1;
    return true;
  }
  if (bytes[index] == 0x0Du && index + 1 < length &&
      bytes[index + 1] == 0x0Au) {
    *width = 2;
    return true;
  }
  return false;
}

static bool horizontal_space(uint8_t byte) {
  return byte == 0x20u || byte == 0x09u || byte == 0x0Bu || byte == 0x0Cu;
}

static bool nesting_step(w_seed_foreign_source_validation *result,
                         w_seed_foreign_error *error, size_t offset,
                         size_t *depth) {
  if (*depth == SIZE_MAX || *depth >= result->maximum_nesting) {
    return fail_scan(result, error, W_SEED_FOREIGN_ERROR_NESTING_LIMIT,
                     W_SEED_FOREIGN_TERMINAL_NESTING_LIMIT, offset, offset + 1u,
                     false, 0);
  }
  *depth += 1;
  if (*depth > result->maximum_nesting_observed) {
    result->maximum_nesting_observed = *depth;
  }
  return true;
}

bool w_seed_foreign_scan_c_inline_1(
    w_seed_byte_view input, w_seed_foreign_limits limits,
    w_seed_foreign_source_validation *result, w_seed_foreign_error *error) {
  clear_error(error);
  if (result == NULL || (input.length != 0 && input.data == NULL)) {
    if (error != NULL) {
      error->kind = W_SEED_FOREIGN_ERROR_NULL_ARGUMENT;
      error->terminal_state = W_SEED_FOREIGN_TERMINAL_NONE;
    }
    return false;
  }
  clear_result(result, limits);
  if (input.length > UINT64_MAX / UINT64_C(8)) {
    return fail_scan(result, error, W_SEED_FOREIGN_ERROR_INVALID_LIMIT,
                     W_SEED_FOREIGN_TERMINAL_INVALID_LIMIT, 0, 0, false, 0);
  }

  if (input.length == 0 || input.data[0] != (uint8_t)'{') {
    return fail_scan(result, error, W_SEED_FOREIGN_ERROR_MISSING_OPEN,
                     W_SEED_FOREIGN_TERMINAL_MISSING_OPEN, 0,
                     input.length == 0 ? 0 : 1u, false, 0);
  }

  enum lexical_state {
    STATE_NORMAL,
    STATE_LINE_COMMENT,
    STATE_BLOCK_COMMENT,
    STATE_SINGLE_QUOTE,
    STATE_DOUBLE_QUOTE,
    STATE_DIRECTIVE,
  } state = STATE_NORMAL;
  sha256_state digest;
  sha256_init(&digest);
  size_t index = 1;
  size_t depth = 0;
  bool line_start = true;
  bool escaped = false;
  bool saw_directive = false;
  size_t directive_offset = 0;

  while (index < input.length) {
    const size_t body_length = index - 1;
    if (body_length > limits.maximum_body_bytes) {
      return fail_scan(result, error, W_SEED_FOREIGN_ERROR_BODY_LIMIT,
                       W_SEED_FOREIGN_TERMINAL_BODY_LIMIT, index, index + 1u,
                       false, 0);
    }
    const uint8_t byte = input.data[index];
    const uint8_t next = index + 1 < input.length ? input.data[index + 1] : 0;

    if (byte == 0) {
      return fail_scan(result, error, W_SEED_FOREIGN_ERROR_NUL,
                       W_SEED_FOREIGN_TERMINAL_NUL, index, index + 1u, false, 0);
    }

    if (state == STATE_NORMAL && byte == (uint8_t)'}' && depth == 0) {
      size_t bad_offset = 0;
      const w_seed_byte_view scanned = {input.data, index + 1u};
      if (!valid_utf8(scanned, &bad_offset)) {
        return fail_scan(result, error, W_SEED_FOREIGN_ERROR_INVALID_UTF8,
                         W_SEED_FOREIGN_TERMINAL_INVALID_UTF8, bad_offset,
                         bad_offset < scanned.length ? bad_offset + 1u
                                                      : bad_offset,
                         false, 0);
      }
      result->body_start_byte = 1;
      result->body_end_byte = index;
      result->close_byte = index;
      result->next_byte = index + 1;
      result->terminal_state = saw_directive
                                   ? W_SEED_FOREIGN_TERMINAL_PREPROCESSOR_DIRECTIVE
                                   : W_SEED_FOREIGN_TERMINAL_CLOSED;
      if (saw_directive) {
        if (error != NULL) {
          error->primary.start_byte = directive_offset;
          error->primary.end_byte = directive_offset + 1;
        }
        return fail_scan(result, error,
                         W_SEED_FOREIGN_ERROR_PREPROCESSOR_DIRECTIVE,
                         W_SEED_FOREIGN_TERMINAL_PREPROCESSOR_DIRECTIVE,
                         directive_offset, directive_offset + 1u, true, index);
      }
      sha256_final(&digest, result->body_digest);
      result->digest_valid = true;
      result->terminal_state = W_SEED_FOREIGN_TERMINAL_CLOSED;
      if (error != NULL) error->terminal_state = result->terminal_state;
      return true;
    }

    if (state == STATE_LINE_COMMENT || state == STATE_DIRECTIVE) {
      size_t width = 0;
      if (byte == (uint8_t)'\\' && line_end(input.data, input.length, index + 1,
                                             &width)) {
        digest_byte(&digest, byte);
        sha256_update(&digest, input.data + index + 1, width);
        index += 1 + width;
        continue;
      }
      digest_byte(&digest, byte);
      if (byte == 0x0Au || byte == 0x0Du) {
        state = STATE_NORMAL;
        line_start = true;
      }
      index += 1;
      continue;
    }

    if (state == STATE_BLOCK_COMMENT) {
      if (byte == (uint8_t)'*' && next == (uint8_t)'/') {
        digest_pair(&digest, byte, next);
        state = STATE_NORMAL;
        index += 2;
        continue;
      }
      digest_byte(&digest, byte);
      if (byte == 0x0Au || byte == 0x0Du) line_start = true;
      index += 1;
      continue;
    }

    if (state == STATE_SINGLE_QUOTE || state == STATE_DOUBLE_QUOTE) {
      const uint8_t delimiter = state == STATE_SINGLE_QUOTE ? (uint8_t)0x27u
                                                             : (uint8_t)'"';
      digest_byte(&digest, byte);
      if (escaped) {
        size_t width = 0;
        if ((byte == 0x0Au || byte == 0x0Du) &&
            line_end(input.data, input.length, index, &width)) {
          if (width == 2) {
            digest_byte(&digest, input.data[index + 1]);
            index += 2;
          } else {
            index += 1;
          }
          escaped = false;
          continue;
        }
        escaped = false;
        index += 1;
        continue;
      }
      if (byte == (uint8_t)'\\') {
        escaped = true;
        index += 1;
        continue;
      }
      if (byte == delimiter) {
        state = STATE_NORMAL;
        line_start = false;
        index += 1;
        continue;
      }
      if (byte == 0x0Au || byte == 0x0Du) {
        return fail_scan(result, error,
                         W_SEED_FOREIGN_ERROR_UNTERMINATED_LITERAL,
                         W_SEED_FOREIGN_TERMINAL_UNTERMINATED_LITERAL, index,
                         index + 1u, false, 0);
      }
      index += 1;
      continue;
    }

    size_t splice_width = 0;
    if (byte == (uint8_t)'\\' &&
        line_end(input.data, input.length, index + 1, &splice_width)) {
      return fail_scan(result, error, W_SEED_FOREIGN_ERROR_LINE_SPLICE,
                       W_SEED_FOREIGN_TERMINAL_LINE_SPLICE, index, index + 1u,
                       false, 0);
    }
    if (byte == (uint8_t)'/' && next == (uint8_t)'/') {
      digest_pair(&digest, byte, next);
      state = STATE_LINE_COMMENT;
      line_start = false;
      index += 2;
      continue;
    }
    if (byte == (uint8_t)'/' && next == (uint8_t)'*') {
      digest_pair(&digest, byte, next);
      state = STATE_BLOCK_COMMENT;
      index += 2;
      continue;
    }
    if (byte == (uint8_t)'"') {
      digest_byte(&digest, byte);
      state = STATE_DOUBLE_QUOTE;
      escaped = false;
      line_start = false;
      index += 1;
      continue;
    }
    if (byte == (uint8_t)0x27u) {
      digest_byte(&digest, byte);
      state = STATE_SINGLE_QUOTE;
      escaped = false;
      line_start = false;
      index += 1;
      continue;
    }
    if (line_start && byte == (uint8_t)'#') {
      digest_byte(&digest, byte);
      state = STATE_DIRECTIVE;
      saw_directive = true;
      directive_offset = index;
      line_start = false;
      index += 1;
      continue;
    }
    if (line_start && byte == (uint8_t)'%' && next == (uint8_t)':') {
      digest_pair(&digest, byte, next);
      state = STATE_DIRECTIVE;
      saw_directive = true;
      directive_offset = index;
      line_start = false;
      index += 2;
      continue;
    }
    if (byte == (uint8_t)'<' && next == (uint8_t)'%') {
      digest_pair(&digest, byte, next);
      if (!nesting_step(result, error, index, &depth)) return false;
      line_start = false;
      index += 2;
      continue;
    }
    if (byte == (uint8_t)'%' && next == (uint8_t)'>') {
      digest_pair(&digest, byte, next);
      if (depth != 0) depth -= 1;
      line_start = false;
      index += 2;
      continue;
    }
    digest_byte(&digest, byte);
    if (byte == (uint8_t)'{') {
      if (!nesting_step(result, error, index, &depth)) return false;
    } else if (byte == (uint8_t)'}' && depth != 0) {
      depth -= 1;
    }
    if (byte == 0x0Au || byte == 0x0Du) {
      line_start = true;
    } else if (!horizontal_space(byte)) {
      line_start = false;
    }
    index += 1;
  }

  size_t bad_offset = 0;
  if (!valid_utf8(input, &bad_offset)) {
    return fail_scan(result, error, W_SEED_FOREIGN_ERROR_INVALID_UTF8,
                     W_SEED_FOREIGN_TERMINAL_INVALID_UTF8, bad_offset,
                     bad_offset < input.length ? bad_offset + 1u : bad_offset,
                     false, 0);
  }

  if (state == STATE_SINGLE_QUOTE || state == STATE_DOUBLE_QUOTE) {
    return fail_scan(result, error, W_SEED_FOREIGN_ERROR_UNTERMINATED_LITERAL,
                     W_SEED_FOREIGN_TERMINAL_UNTERMINATED_LITERAL, index, index,
                     false, 0);
  }
  if (state == STATE_BLOCK_COMMENT) {
    return fail_scan(result, error,
                     W_SEED_FOREIGN_ERROR_UNTERMINATED_COMMENT,
                     W_SEED_FOREIGN_TERMINAL_UNTERMINATED_COMMENT, index, index,
                     false, 0);
  }
  if (state == STATE_DIRECTIVE || saw_directive) {
    return fail_scan(result, error,
                     W_SEED_FOREIGN_ERROR_PREPROCESSOR_DIRECTIVE,
                     W_SEED_FOREIGN_TERMINAL_PREPROCESSOR_DIRECTIVE, index,
                     index, false, 0);
  }
  return fail_scan(result, error, W_SEED_FOREIGN_ERROR_MISSING_CLOSE,
                   W_SEED_FOREIGN_TERMINAL_MISSING_CLOSE, index, index, false,
                   0);
}
