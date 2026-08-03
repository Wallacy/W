#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define MAX_FIELDS 64U

typedef enum {
  W_OK = 0,
  W_BUFFER,
  W_TRUNCATED,
  W_UNUSED_PRESENCE,
  W_TRAILING,
  W_INVALID_BOOL,
  W_NON_MINIMAL,
  W_CONTROL_OVERFLOW,
  W_DUPLICATE,
  W_FIELD_OVERFLOW,
  W_COUNT_OVERFLOW,
  W_MISSING_REQUIRED,
  W_INVALID_KIND,
  W_UNSUPPORTED_KIND,
} WError;

typedef struct {
  uint16_t id;
  bool urgent_present;
  bool urgent;
} MenuKey;

typedef struct {
  uint32_t id;
  uint8_t kind;
  uint32_t length;
} DirectoryEntry;

static WError write_leb(uint32_t value, uint8_t *out, size_t capacity, size_t *written) {
  size_t cursor = 0;

  do {
    if (cursor == capacity) {
      return W_BUFFER;
    }

    uint8_t byte = (uint8_t)(value % 128U);
    value /= 128U;
    if (value != 0U) {
      byte |= 0x80U;
    }
    out[cursor++] = byte;
  } while (value != 0U);

  *written = cursor;
  return W_OK;
}

static WError read_leb(
    const uint8_t *data,
    size_t length,
    size_t *cursor,
    uint32_t *value) {
  const size_t start = *cursor;
  uint32_t result = 0;

  for (size_t index = 0; index < 5U; index += 1U) {
    if (*cursor >= length) {
      return W_TRUNCATED;
    }

    const uint8_t byte = data[(*cursor)++];
    if (index == 4U && ((byte & 0x7fU) > 0x0fU || (byte & 0x80U) != 0U)) {
      return W_CONTROL_OVERFLOW;
    }

    result |= ((uint32_t)(byte & 0x7fU)) << (index * 7U);
    if ((byte & 0x80U) == 0U) {
      uint8_t canonical[5];
      size_t canonical_length = 0;
      WError error = write_leb(result, canonical, sizeof(canonical), &canonical_length);
      if (error != W_OK) {
        return error;
      }

      if (canonical_length != *cursor - start ||
          memcmp(canonical, data + start, canonical_length) != 0) {
        return W_NON_MINIMAL;
      }

      *value = result;
      return W_OK;
    }
  }

  return W_CONTROL_OVERFLOW;
}

static WError encode_exact(
    const MenuKey *value,
    uint8_t *out,
    size_t capacity,
    size_t *written) {
  const size_t required = value->urgent_present ? 4U : 3U;
  if (capacity < required) {
    return W_BUFFER;
  }

  out[0] = value->urgent_present ? 1U : 0U;
  out[1] = (uint8_t)(value->id & 0xffU);
  out[2] = (uint8_t)(value->id >> 8U);
  if (value->urgent_present) {
    out[3] = value->urgent ? 1U : 0U;
  }

  *written = required;
  return W_OK;
}

static WError decode_exact(
    const uint8_t *data,
    size_t length,
    MenuKey *value) {
  if (length < 3U) {
    return W_TRUNCATED;
  }
  if ((data[0] & 0xfeU) != 0U) {
    return W_UNUSED_PRESENCE;
  }

  value->id = (uint16_t)data[1] | ((uint16_t)data[2] << 8U);
  value->urgent_present = data[0] == 1U;
  value->urgent = false;

  if (!value->urgent_present) {
    return length == 3U ? W_OK : W_TRAILING;
  }
  if (length < 4U) {
    return W_TRUNCATED;
  }
  if (length != 4U) {
    return W_TRAILING;
  }
  if (data[3] > 1U) {
    return W_INVALID_BOOL;
  }

  value->urgent = data[3] == 1U;
  return W_OK;
}

static WError encode_compatible(
    const MenuKey *value,
    uint8_t *out,
    size_t capacity,
    size_t *written) {
  const size_t required = value->urgent_present ? 10U : 6U;
  if (capacity < required) {
    return W_BUFFER;
  }

  size_t cursor = 0;
  out[cursor++] = value->urgent_present ? 2U : 1U;
  out[cursor++] = 1U;
  out[cursor++] = 3U;
  out[cursor++] = 2U;

  if (value->urgent_present) {
    out[cursor++] = 1U;
    out[cursor++] = 1U;
    out[cursor++] = 1U;
  }

  out[cursor++] = (uint8_t)(value->id & 0xffU);
  out[cursor++] = (uint8_t)(value->id >> 8U);
  if (value->urgent_present) {
    out[cursor++] = value->urgent ? 1U : 0U;
  }

  *written = cursor;
  return W_OK;
}

static WError validate_unknown_scalar(uint8_t kind, uint32_t block_length) {
  if (kind != 1U && kind != 3U && kind != 14U && kind != 15U) {
    return W_UNSUPPORTED_KIND;
  }
  if ((kind == 1U && block_length != 1U) ||
      (kind == 3U && block_length != 2U)) {
    return W_INVALID_KIND;
  }
  return W_OK;
}

static WError decode_compatible(
    const uint8_t *data,
    size_t length,
    MenuKey *value) {
  size_t cursor = 0;
  uint32_t count = 0;
  WError error = read_leb(data, length, &cursor, &count);
  if (error != W_OK) {
    return error;
  }
  if (count > MAX_FIELDS) {
    return W_COUNT_OVERFLOW;
  }

  DirectoryEntry entries[MAX_FIELDS];
  uint32_t previous_id = 0;
  for (uint32_t index = 0; index < count; index += 1U) {
    uint32_t delta = 0;
    error = read_leb(data, length, &cursor, &delta);
    if (error != W_OK) {
      return error;
    }
    if (delta == 0U) {
      return W_DUPLICATE;
    }
    if (previous_id > UINT32_MAX - delta) {
      return W_FIELD_OVERFLOW;
    }

    if (cursor >= length) {
      return W_TRUNCATED;
    }
    entries[index].id = previous_id + delta;
    entries[index].kind = data[cursor++];
    error = read_leb(data, length, &cursor, &entries[index].length);
    if (error != W_OK) {
      return error;
    }
    previous_id = entries[index].id;
  }

  bool found_id = false;
  value->id = 0;
  value->urgent_present = false;
  value->urgent = false;

  for (uint32_t index = 0; index < count; index += 1U) {
    const DirectoryEntry entry = entries[index];
    if ((uint64_t)entry.length > (uint64_t)(length - cursor)) {
      return W_TRUNCATED;
    }
    const uint8_t *block = data + cursor;
    cursor += entry.length;

    if (entry.id == 1U) {
      if (entry.kind != 3U || entry.length != 2U) {
        return W_INVALID_KIND;
      }
      value->id = (uint16_t)block[0] | ((uint16_t)block[1] << 8U);
      found_id = true;
    } else if (entry.id == 2U) {
      if (entry.kind != 1U || entry.length != 1U) {
        return W_INVALID_KIND;
      }
      if (block[0] > 1U) {
        return W_INVALID_BOOL;
      }
      value->urgent_present = true;
      value->urgent = block[0] == 1U;
    } else {
      error = validate_unknown_scalar(entry.kind, entry.length);
      if (error != W_OK) {
        return error;
      }
    }
  }

  if (!found_id) {
    return W_MISSING_REQUIRED;
  }
  return cursor == length ? W_OK : W_TRAILING;
}

static bool vector_equals(
    const uint8_t *actual,
    size_t actual_length,
    const uint8_t *expected,
    size_t expected_length) {
  return actual_length == expected_length &&
      memcmp(actual, expected, expected_length) == 0;
}

static bool same_menu_key(const MenuKey *left, const MenuKey *right) {
  return left->id == right->id &&
      left->urgent_present == right->urgent_present &&
      left->urgent == right->urgent;
}

int main(void) {
  const MenuKey absent = {42U, false, false};
  const MenuKey present = {42U, true, true};
  const uint8_t exact_absent[] = {0x00U, 0x2aU, 0x00U};
  const uint8_t exact_present[] = {0x01U, 0x2aU, 0x00U, 0x01U};
  const uint8_t compatible_absent[] = {0x01U, 0x01U, 0x03U, 0x02U, 0x2aU, 0x00U};
  const uint8_t compatible_present[] = {
      0x02U, 0x01U, 0x03U, 0x02U, 0x01U,
      0x01U, 0x01U, 0x2aU, 0x00U, 0x01U,
  };
  uint8_t buffer[64];
  size_t written = 0;
  MenuKey decoded;

  if (encode_exact(&absent, buffer, sizeof(buffer), &written) != W_OK ||
      !vector_equals(buffer, written, exact_absent, sizeof(exact_absent)) ||
      decode_exact(buffer, written, &decoded) != W_OK ||
      !same_menu_key(&decoded, &absent)) {
    return 1;
  }
  if (encode_exact(&present, buffer, sizeof(buffer), &written) != W_OK ||
      !vector_equals(buffer, written, exact_present, sizeof(exact_present)) ||
      decode_exact(buffer, written, &decoded) != W_OK ||
      !same_menu_key(&decoded, &present)) {
    return 2;
  }
  if (encode_compatible(&absent, buffer, sizeof(buffer), &written) != W_OK ||
      !vector_equals(buffer, written, compatible_absent, sizeof(compatible_absent)) ||
      decode_compatible(buffer, written, &decoded) != W_OK ||
      !same_menu_key(&decoded, &absent)) {
    return 3;
  }
  if (encode_compatible(&present, buffer, sizeof(buffer), &written) != W_OK ||
      !vector_equals(buffer, written, compatible_present, sizeof(compatible_present)) ||
      decode_compatible(buffer, written, &decoded) != W_OK ||
      !same_menu_key(&decoded, &present)) {
    return 4;
  }

  const uint8_t unknown_scalar[] = {
      0x02U, 0x01U, 0x03U, 0x02U, 0x02U, 0x0eU, 0x02U,
      0x2aU, 0x00U, 0xaaU, 0xbbU,
  };
  if (decode_compatible(unknown_scalar, sizeof(unknown_scalar), &decoded) != W_OK ||
      !same_menu_key(&decoded, &absent)) {
    return 5;
  }

  const uint8_t non_minimal[] = {0x81U, 0x00U};
  const uint8_t duplicate[] = {
      0x02U, 0x01U, 0x03U, 0x02U, 0x00U, 0x01U, 0x01U,
      0x2aU, 0x00U, 0x01U,
  };
  const uint8_t invalid_bool[] = {
      0x02U, 0x01U, 0x03U, 0x02U, 0x01U, 0x01U, 0x01U,
      0x2aU, 0x00U, 0x02U,
  };
  if (decode_compatible(non_minimal, sizeof(non_minimal), &decoded) != W_NON_MINIMAL ||
      decode_compatible(duplicate, sizeof(duplicate), &decoded) != W_DUPLICATE ||
      decode_compatible(invalid_bool, sizeof(invalid_bool), &decoded) != W_INVALID_BOOL) {
    return 6;
  }

  puts("exact.absent=00 2A 00");
  puts("exact.present=01 2A 00 01");
  puts("compatible.absent=01 01 03 02 2A 00");
  puts("compatible.present=02 01 03 02 01 01 01 2A 00 01");
  return 0;
}
