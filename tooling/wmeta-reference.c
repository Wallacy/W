#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W_META_HEADER_BYTES 32u
#define W_META_DIRECTORY_LIMIT (64u * 1024u * 1024u)
#define W_META_PAYLOAD_LIMIT (UINT64_C(16) * 1024u * 1024u * 1024u)
#define W_META_CHUNK_LIMIT (UINT64_C(1024) * 1024u * 1024u)
#define W_META_ENTRY_LIMIT 1048576u
#define W_META_COLLECTION_LIMIT 1048576u
#define W_META_ITEM_LIMIT 4194304u
#define W_META_TEXT_LIMIT (1024u * 1024u)
#define W_META_BYTE_STRING_LIMIT (UINT64_C(1024) * 1024u * 1024u)
#define W_META_NESTING_LIMIT 64u

static const uint8_t W_META_MAGIC[8] = {
    0x57, 0x4d, 0x65, 0x74, 0x61, 0x31, 0x0d, 0x0a,
};

typedef struct {
  const uint8_t *data;
  size_t length;
  size_t offset;
  uint64_t items;
  const char *error;
} CborReader;

typedef struct {
  uint16_t kind;
  uint8_t identity[32];
  uint16_t schema_major;
  uint16_t schema_minor;
  bool critical;
  uint64_t length;
  uint64_t offset;
  uint64_t digest_algorithm;
  uint8_t digest[32];
} WMetaEntry;

typedef struct {
  uint32_t state[8];
  uint64_t bit_count;
  uint8_t block[64];
  size_t used;
} Sha256;

static bool cbor_error(CborReader *reader, const char *code) {
  if (reader->error == NULL) reader->error = code;
  return false;
}

static int compare_bytes(const uint8_t *left, size_t left_length,
                         const uint8_t *right, size_t right_length) {
  size_t common = left_length < right_length ? left_length : right_length;
  int order = memcmp(left, right, common);
  if (order != 0) return order;
  if (left_length < right_length) return -1;
  if (left_length > right_length) return 1;
  return 0;
}

static bool cbor_argument(CborReader *reader, uint8_t additional,
                          uint64_t *value) {
  size_t width;
  uint64_t minimum;
  if (additional < 24u) {
    *value = additional;
    return true;
  }
  switch (additional) {
    case 24u: width = 1u; minimum = 24u; break;
    case 25u: width = 2u; minimum = UINT64_C(0x100); break;
    case 26u: width = 4u; minimum = UINT64_C(0x10000); break;
    case 27u: width = 8u; minimum = UINT64_C(0x100000000); break;
    case 31u: return cbor_error(reader, "indefiniteCbor");
    default: return cbor_error(reader, "malformedCbor");
  }
  if (width > reader->length - reader->offset) {
    return cbor_error(reader, "truncatedCbor");
  }
  *value = 0u;
  for (size_t index = 0; index < width; ++index) {
    *value = (*value << 8u) | reader->data[reader->offset++];
  }
  if (*value < minimum) return cbor_error(reader, "nonCanonicalCborInteger");
  return true;
}

static bool cbor_head(CborReader *reader, unsigned depth, uint8_t *major,
                      uint8_t *additional, uint64_t *argument) {
  if (depth > W_META_NESTING_LIMIT) {
    return cbor_error(reader, "cborNestingLimitExceeded");
  }
  if (++reader->items > W_META_ITEM_LIMIT) {
    return cbor_error(reader, "cborItemLimitExceeded");
  }
  if (reader->offset >= reader->length) {
    return cbor_error(reader, "truncatedCbor");
  }
  uint8_t initial = reader->data[reader->offset++];
  *major = initial >> 5u;
  *additional = initial & 0x1fu;
  if (*major == 7u) {
    if (*additional == 25u || *additional == 26u || *additional == 27u) {
      return cbor_error(reader, "cborFloatForbidden");
    }
    if (*additional == 31u) return cbor_error(reader, "indefiniteCbor");
    *argument = *additional;
    return true;
  }
  return cbor_argument(reader, *additional, argument);
}

static bool utf8_valid(const uint8_t *data, size_t length) {
  size_t offset = 0u;
  while (offset < length) {
    uint8_t first = data[offset++];
    if (first <= 0x7fu) continue;
    if (first >= 0xc2u && first <= 0xdfu) {
      if (offset >= length || (data[offset++] & 0xc0u) != 0x80u) return false;
      continue;
    }
    if (first >= 0xe0u && first <= 0xefu) {
      if (offset + 1u >= length) return false;
      uint8_t second = data[offset++];
      uint8_t third = data[offset++];
      if ((second & 0xc0u) != 0x80u || (third & 0xc0u) != 0x80u) return false;
      if (first == 0xe0u && second < 0xa0u) return false;
      if (first == 0xedu && second >= 0xa0u) return false;
      continue;
    }
    if (first >= 0xf0u && first <= 0xf4u) {
      if (offset + 2u >= length) return false;
      uint8_t second = data[offset++];
      uint8_t third = data[offset++];
      uint8_t fourth = data[offset++];
      if ((second & 0xc0u) != 0x80u || (third & 0xc0u) != 0x80u ||
          (fourth & 0xc0u) != 0x80u) return false;
      if (first == 0xf0u && second < 0x90u) return false;
      if (first == 0xf4u && second >= 0x90u) return false;
      continue;
    }
    return false;
  }
  return true;
}

static bool cbor_skip(CborReader *reader, unsigned depth);

static bool cbor_skip_map(CborReader *reader, unsigned depth, uint64_t count) {
  const uint8_t *previous = NULL;
  size_t previous_length = 0u;
  for (uint64_t index = 0; index < count; ++index) {
    size_t key_start = reader->offset;
    uint8_t major;
    uint8_t additional;
    uint64_t argument;
    if (!cbor_head(reader, depth + 1u, &major, &additional, &argument)) return false;
    (void)additional;
    (void)argument;
    if (major != 0u) return cbor_error(reader, "cborMapKeyNotUnsigned");
    size_t key_length = reader->offset - key_start;
    if (previous != NULL) {
      int order = compare_bytes(previous, previous_length,
                                reader->data + key_start, key_length);
      if (order == 0) return cbor_error(reader, "duplicateCborMapKey");
      if (order > 0) return cbor_error(reader, "nonCanonicalCborMapOrder");
    }
    previous = reader->data + key_start;
    previous_length = key_length;
    if (!cbor_skip(reader, depth + 1u)) return false;
  }
  return true;
}

static bool cbor_skip(CborReader *reader, unsigned depth) {
  uint8_t major;
  uint8_t additional;
  uint64_t argument;
  if (!cbor_head(reader, depth, &major, &additional, &argument)) return false;
  (void)additional;
  switch (major) {
    case 0u:
    case 1u:
      return true;
    case 2u:
    case 3u: {
      uint64_t limit = major == 2u ? W_META_BYTE_STRING_LIMIT : W_META_TEXT_LIMIT;
      if (argument > limit || argument > SIZE_MAX) {
        return cbor_error(reader, "cborLimitExceeded");
      }
      size_t length = (size_t)argument;
      if (length > reader->length - reader->offset) {
        return cbor_error(reader, "truncatedCbor");
      }
      if (major == 3u && !utf8_valid(reader->data + reader->offset, length)) {
        return cbor_error(reader, "invalidCborUtf8");
      }
      reader->offset += length;
      return true;
    }
    case 4u:
      if (argument > W_META_COLLECTION_LIMIT) {
        return cbor_error(reader, "cborLimitExceeded");
      }
      for (uint64_t index = 0; index < argument; ++index) {
        if (!cbor_skip(reader, depth + 1u)) return false;
      }
      return true;
    case 5u:
      if (argument > W_META_COLLECTION_LIMIT) {
        return cbor_error(reader, "cborLimitExceeded");
      }
      return cbor_skip_map(reader, depth, argument);
    case 6u:
      return cbor_error(reader, "cborTagForbidden");
    case 7u:
      if (additional == 20u || additional == 21u || additional == 22u) return true;
      return cbor_error(reader, "unsupportedCborValue");
    default:
      return cbor_error(reader, "malformedCbor");
  }
}

static bool cbor_unsigned(CborReader *reader, unsigned depth, uint64_t *value,
                          const char *code) {
  uint8_t major;
  uint8_t additional;
  if (!cbor_head(reader, depth, &major, &additional, value)) return false;
  (void)additional;
  if (major != 0u) return cbor_error(reader, code);
  return true;
}

static bool cbor_array(CborReader *reader, unsigned depth, uint64_t *count,
                       const char *code) {
  uint8_t major;
  uint8_t additional;
  if (!cbor_head(reader, depth, &major, &additional, count)) return false;
  (void)additional;
  if (major != 4u) return cbor_error(reader, code);
  if (*count > W_META_COLLECTION_LIMIT) return cbor_error(reader, "cborLimitExceeded");
  return true;
}

static bool cbor_map(CborReader *reader, unsigned depth, uint64_t *count,
                     const char *code) {
  uint8_t major;
  uint8_t additional;
  if (!cbor_head(reader, depth, &major, &additional, count)) return false;
  (void)additional;
  if (major != 5u) return cbor_error(reader, code);
  if (*count > W_META_COLLECTION_LIMIT) return cbor_error(reader, "cborLimitExceeded");
  return true;
}

static bool cbor_boolean(CborReader *reader, unsigned depth, bool *value,
                         const char *code) {
  uint8_t major;
  uint8_t additional;
  uint64_t argument;
  if (!cbor_head(reader, depth, &major, &additional, &argument)) return false;
  (void)argument;
  if (major != 7u || (additional != 20u && additional != 21u)) {
    return cbor_error(reader, code);
  }
  *value = additional == 21u;
  return true;
}

static bool cbor_bytes(CborReader *reader, unsigned depth, const uint8_t **value,
                       size_t expected_length, const char *code) {
  uint8_t major;
  uint8_t additional;
  uint64_t argument;
  if (!cbor_head(reader, depth, &major, &additional, &argument)) return false;
  (void)additional;
  if (major != 2u || argument != expected_length) return cbor_error(reader, code);
  if (argument > reader->length - reader->offset) {
    return cbor_error(reader, "truncatedCbor");
  }
  *value = reader->data + reader->offset;
  reader->offset += (size_t)argument;
  return true;
}

static bool ordered_key(CborReader *reader, unsigned depth,
                        const uint8_t **previous, size_t *previous_length,
                        uint64_t *key) {
  size_t start = reader->offset;
  if (!cbor_unsigned(reader, depth, key, "cborMapKeyNotUnsigned")) return false;
  size_t length = reader->offset - start;
  if (*previous != NULL) {
    int order = compare_bytes(*previous, *previous_length,
                              reader->data + start, length);
    if (order == 0) return cbor_error(reader, "duplicateCborMapKey");
    if (order > 0) return cbor_error(reader, "nonCanonicalCborMapOrder");
  }
  *previous = reader->data + start;
  *previous_length = length;
  return true;
}

static bool parse_schema(CborReader *reader, unsigned depth, WMetaEntry *entry) {
  uint64_t count;
  uint64_t major;
  uint64_t minor;
  if (!cbor_array(reader, depth, &count, "malformedChunkSchema")) return false;
  if (count != 2u) return cbor_error(reader, "malformedChunkSchema");
  if (!cbor_unsigned(reader, depth + 1u, &major, "malformedChunkSchema") ||
      !cbor_unsigned(reader, depth + 1u, &minor, "malformedChunkSchema")) return false;
  if (major > UINT16_MAX || minor > UINT16_MAX) {
    return cbor_error(reader, "malformedChunkSchema");
  }
  entry->schema_major = (uint16_t)major;
  entry->schema_minor = (uint16_t)minor;
  return true;
}

static bool parse_digest(CborReader *reader, unsigned depth, WMetaEntry *entry) {
  uint64_t count;
  const uint8_t *digest;
  if (!cbor_array(reader, depth, &count, "malformedWMetaDigest")) return false;
  if (count != 2u) return cbor_error(reader, "malformedWMetaDigest");
  if (!cbor_unsigned(reader, depth + 1u, &entry->digest_algorithm,
                     "malformedWMetaDigest")) return false;
  if (!cbor_bytes(reader, depth + 1u, &digest, 32u,
                  "malformedWMetaDigest")) return false;
  memcpy(entry->digest, digest, 32u);
  return true;
}

static bool parse_entry(CborReader *reader, unsigned depth, WMetaEntry *entry) {
  uint64_t count;
  uint64_t kind = 0u;
  const uint8_t *identity = NULL;
  const uint8_t *previous = NULL;
  size_t previous_length = 0u;
  unsigned fields = 0u;
  memset(entry, 0, sizeof(*entry));
  if (!cbor_map(reader, depth, &count, "malformedWMetaEntry")) return false;
  for (uint64_t index = 0; index < count; ++index) {
    uint64_t key;
    if (!ordered_key(reader, depth + 1u, &previous, &previous_length, &key)) return false;
    switch (key) {
      case 0u:
        if (!cbor_unsigned(reader, depth + 1u, &kind, "malformedWMetaEntry")) return false;
        if (kind == 0u || kind > UINT16_MAX) return cbor_error(reader, "malformedWMetaEntry");
        entry->kind = (uint16_t)kind;
        fields |= 1u << 0u;
        break;
      case 1u:
        if (!cbor_bytes(reader, depth + 1u, &identity, 32u,
                        "malformedWMetaIdentity")) return false;
        memcpy(entry->identity, identity, 32u);
        fields |= 1u << 1u;
        break;
      case 2u:
        if (!parse_schema(reader, depth + 1u, entry)) return false;
        fields |= 1u << 2u;
        break;
      case 3u:
        if (!cbor_boolean(reader, depth + 1u, &entry->critical,
                          "malformedWMetaEntry")) return false;
        fields |= 1u << 3u;
        break;
      case 4u:
        if (!cbor_unsigned(reader, depth + 1u, &entry->length,
                           "malformedWMetaEntry")) return false;
        fields |= 1u << 4u;
        break;
      case 5u:
        if (!parse_digest(reader, depth + 1u, entry)) return false;
        fields |= 1u << 5u;
        break;
      default:
        if (!cbor_skip(reader, depth + 1u)) return false;
        break;
    }
  }
  if ((fields & 0x3fu) != 0x3fu) return cbor_error(reader, "malformedWMetaEntry");
  return true;
}

static bool known_kind(uint16_t kind) {
  return (kind >= 1u && kind <= 5u) || (kind >= 16u && kind <= 19u);
}

static bool profile_allows(uint64_t profile, uint16_t kind) {
  if (profile == 1u) return kind >= 1u && kind <= 5u;
  if (profile == 2u) return kind >= 16u && kind <= 19u;
  return false;
}

static bool profile_requires(uint64_t profile, uint16_t kind) {
  if (profile == 1u) return kind == 1u || kind == 2u;
  if (profile == 2u) return kind >= 16u && kind <= 19u;
  return false;
}

static const char *validate_entries(uint64_t profile, WMetaEntry *entries,
                                    size_t count, uint64_t payload_length) {
  unsigned required_counts[4] = {0u, 0u, 0u, 0u};
  uint64_t offset = 0u;
  for (size_t index = 0; index < count; ++index) {
    WMetaEntry *entry = &entries[index];
    if (index > 0u) {
      WMetaEntry *prior = &entries[index - 1u];
      int order = prior->kind == entry->kind
          ? memcmp(prior->identity, entry->identity, 32u)
          : (prior->kind < entry->kind ? -1 : 1);
      if (order == 0) return "duplicateWMetaEntry";
      if (order > 0) return "unsortedWMetaEntries";
    }
    bool known = known_kind(entry->kind);
    if (known && !profile_allows(profile, entry->kind)) {
      return "wmetaProfileChunkMismatch";
    }
    if (!known && entry->critical) return "unknownCriticalWMetaChunk";
    if (known && entry->schema_major != 1u && entry->critical) {
      return "unsupportedCriticalWMetaSchema";
    }
    if (profile_requires(profile, entry->kind)) {
      if (!entry->critical) return "wmetaCoreChunkMustBeCritical";
      unsigned slot = profile == 1u ? entry->kind - 1u : entry->kind - 16u;
      if (++required_counts[slot] > 1u) return "duplicateWMetaCoreChunk";
    }
    if (entry->length == 0u || entry->length > W_META_CHUNK_LIMIT) {
      return "wmetaChunkLimitExceeded";
    }
    entry->offset = offset;
    if (entry->length > W_META_PAYLOAD_LIMIT - offset) {
      return "wmetaPayloadLimitExceeded";
    }
    offset += entry->length;
  }
  unsigned required = profile == 1u ? 2u : 4u;
  for (unsigned index = 0u; index < required; ++index) {
    if (required_counts[index] != 1u) return "missingWMetaCoreChunk";
  }
  if (offset != payload_length) return "wmetaPayloadLengthMismatch";
  return NULL;
}

static const char *parse_directory(const uint8_t *data, size_t length,
                                   uint64_t payload_length,
                                   WMetaEntry **entries_out,
                                   size_t *entry_count_out) {
  CborReader reader = {data, length, 0u, 0u, NULL};
  uint64_t root_count;
  uint64_t schema = 0u;
  uint64_t profile = 0u;
  uint64_t entry_count = 0u;
  bool have_schema = false;
  bool have_profile = false;
  bool have_features = false;
  bool have_entries = false;
  bool feature_present = false;
  WMetaEntry *entries = NULL;
  const uint8_t *previous = NULL;
  size_t previous_length = 0u;

  if (!cbor_map(&reader, 0u, &root_count, "malformedWMetaDirectory")) {
    return reader.error;
  }
  for (uint64_t index = 0; index < root_count; ++index) {
    uint64_t key;
    if (!ordered_key(&reader, 1u, &previous, &previous_length, &key)) goto failed;
    if (key == 0u) {
      if (!cbor_unsigned(&reader, 1u, &schema, "malformedWMetaDirectory")) goto failed;
      have_schema = true;
    } else if (key == 1u) {
      if (!cbor_unsigned(&reader, 1u, &profile, "malformedWMetaDirectory")) goto failed;
      have_profile = true;
    } else if (key == 2u) {
      uint64_t feature_count;
      uint64_t prior = 0u;
      bool have_prior = false;
      if (!cbor_array(&reader, 1u, &feature_count, "malformedWMetaDirectory")) goto failed;
      for (uint64_t feature_index = 0; feature_index < feature_count; ++feature_index) {
        uint64_t feature;
        if (!cbor_unsigned(&reader, 2u, &feature, "malformedWMetaFeature")) goto failed;
        if (have_prior && feature <= prior) {
          reader.error = feature == prior ? "duplicateWMetaFeature" : "unsortedWMetaFeatures";
          goto failed;
        }
        prior = feature;
        have_prior = true;
      }
      feature_present = feature_count != 0u;
      have_features = true;
    } else if (key == 3u) {
      if (!cbor_array(&reader, 1u, &entry_count, "malformedWMetaDirectory")) goto failed;
      if (entry_count > W_META_ENTRY_LIMIT || entry_count > SIZE_MAX / sizeof(*entries)) {
        reader.error = "wmetaEntryLimitExceeded";
        goto failed;
      }
      entries = calloc((size_t)entry_count, sizeof(*entries));
      if (entries == NULL && entry_count != 0u) {
        reader.error = "wmetaOracleOutOfMemory";
        goto failed;
      }
      for (uint64_t entry_index = 0; entry_index < entry_count; ++entry_index) {
        if (!parse_entry(&reader, 2u, &entries[entry_index])) goto failed;
      }
      have_entries = true;
    } else if (!cbor_skip(&reader, 1u)) {
      goto failed;
    }
  }
  if (reader.offset != reader.length) {
    reader.error = "trailingCborData";
    goto failed;
  }
  if (!have_schema || !have_profile || !have_features || !have_entries) {
    reader.error = "malformedWMetaDirectory";
    goto failed;
  }
  if (schema != 1u) {
    reader.error = "unsupportedWMetaDirectorySchema";
    goto failed;
  }
  if (profile != 1u && profile != 2u) {
    reader.error = "unknownWMetaProfile";
    goto failed;
  }
  if (feature_present) {
    reader.error = "unknownRequiredWMetaFeature";
    goto failed;
  }
  reader.error = validate_entries(profile, entries, (size_t)entry_count,
                                  payload_length);
  if (reader.error != NULL) goto failed;
  *entries_out = entries;
  *entry_count_out = (size_t)entry_count;
  return NULL;

failed:
  free(entries);
  return reader.error;
}

static uint32_t rotate_right(uint32_t value, unsigned count) {
  return (value >> count) | (value << (32u - count));
}

static void sha256_transform(Sha256 *sha, const uint8_t block[64]) {
  static const uint32_t constants[64] = {
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
  uint32_t words[64];
  for (unsigned index = 0u; index < 16u; ++index) {
    unsigned start = index * 4u;
    words[index] = ((uint32_t)block[start] << 24u) |
                   ((uint32_t)block[start + 1u] << 16u) |
                   ((uint32_t)block[start + 2u] << 8u) |
                   block[start + 3u];
  }
  for (unsigned index = 16u; index < 64u; ++index) {
    uint32_t left = words[index - 15u];
    uint32_t right = words[index - 2u];
    uint32_t s0 = rotate_right(left, 7u) ^ rotate_right(left, 18u) ^ (left >> 3u);
    uint32_t s1 = rotate_right(right, 17u) ^ rotate_right(right, 19u) ^ (right >> 10u);
    words[index] = words[index - 16u] + s0 + words[index - 7u] + s1;
  }
  uint32_t a = sha->state[0];
  uint32_t b = sha->state[1];
  uint32_t c = sha->state[2];
  uint32_t d = sha->state[3];
  uint32_t e = sha->state[4];
  uint32_t f = sha->state[5];
  uint32_t g = sha->state[6];
  uint32_t h = sha->state[7];
  for (unsigned index = 0u; index < 64u; ++index) {
    uint32_t s1 = rotate_right(e, 6u) ^ rotate_right(e, 11u) ^ rotate_right(e, 25u);
    uint32_t choice = (e & f) ^ (~e & g);
    uint32_t temp1 = h + s1 + choice + constants[index] + words[index];
    uint32_t s0 = rotate_right(a, 2u) ^ rotate_right(a, 13u) ^ rotate_right(a, 22u);
    uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
    uint32_t temp2 = s0 + majority;
    h = g;
    g = f;
    f = e;
    e = d + temp1;
    d = c;
    c = b;
    b = a;
    a = temp1 + temp2;
  }
  sha->state[0] += a;
  sha->state[1] += b;
  sha->state[2] += c;
  sha->state[3] += d;
  sha->state[4] += e;
  sha->state[5] += f;
  sha->state[6] += g;
  sha->state[7] += h;
}

static void sha256_init(Sha256 *sha) {
  static const uint32_t initial[8] = {
      0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
      0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u,
  };
  memcpy(sha->state, initial, sizeof(initial));
  sha->bit_count = 0u;
  sha->used = 0u;
}

static void sha256_update(Sha256 *sha, const uint8_t *data, size_t length) {
  sha->bit_count += (uint64_t)length * 8u;
  while (length != 0u) {
    size_t available = 64u - sha->used;
    size_t count = length < available ? length : available;
    memcpy(sha->block + sha->used, data, count);
    sha->used += count;
    data += count;
    length -= count;
    if (sha->used == 64u) {
      sha256_transform(sha, sha->block);
      sha->used = 0u;
    }
  }
}

static void sha256_final(Sha256 *sha, uint8_t digest[32]) {
  sha->block[sha->used++] = 0x80u;
  if (sha->used > 56u) {
    memset(sha->block + sha->used, 0, 64u - sha->used);
    sha256_transform(sha, sha->block);
    sha->used = 0u;
  }
  memset(sha->block + sha->used, 0, 56u - sha->used);
  for (unsigned index = 0u; index < 8u; ++index) {
    sha->block[63u - index] = (uint8_t)(sha->bit_count >> (index * 8u));
  }
  sha256_transform(sha, sha->block);
  for (unsigned index = 0u; index < 8u; ++index) {
    digest[index * 4u] = (uint8_t)(sha->state[index] >> 24u);
    digest[index * 4u + 1u] = (uint8_t)(sha->state[index] >> 16u);
    digest[index * 4u + 2u] = (uint8_t)(sha->state[index] >> 8u);
    digest[index * 4u + 3u] = (uint8_t)sha->state[index];
  }
}

static uint16_t read_u16(const uint8_t *data) {
  return (uint16_t)(((uint16_t)data[0] << 8u) | data[1]);
}

static uint32_t read_u32(const uint8_t *data) {
  return ((uint32_t)data[0] << 24u) | ((uint32_t)data[1] << 16u) |
         ((uint32_t)data[2] << 8u) | data[3];
}

static uint64_t read_u64(const uint8_t *data) {
  uint64_t value = 0u;
  for (unsigned index = 0u; index < 8u; ++index) value = (value << 8u) | data[index];
  return value;
}

static const char *validate_chunk(const uint8_t *data, size_t length) {
  CborReader reader = {data, length, 0u, 0u, NULL};
  if (!cbor_skip(&reader, 0u)) return reader.error;
  if (reader.offset != reader.length) return "trailingCborData";
  return NULL;
}

static const char *validate_wmeta(const uint8_t *data, size_t length, int mode) {
  if (length < W_META_HEADER_BYTES) return "truncatedWMetaHeader";
  if (memcmp(data, W_META_MAGIC, sizeof(W_META_MAGIC)) != 0) return "invalidWMetaMagic";
  if (read_u16(data + 8u) != W_META_HEADER_BYTES) return "unsupportedWMetaHeader";
  if (read_u16(data + 10u) != 1u) return "unsupportedWMetaDirectorySchema";
  if (read_u32(data + 12u) != 0u) return "unsupportedWMetaHeaderFlags";
  uint64_t directory_length = read_u64(data + 16u);
  uint64_t payload_length = read_u64(data + 24u);
  if (directory_length == 0u || directory_length > W_META_DIRECTORY_LIMIT) {
    return "wmetaDirectoryLimitExceeded";
  }
  if (payload_length > W_META_PAYLOAD_LIMIT) return "wmetaPayloadLimitExceeded";
  if (directory_length > UINT64_MAX - W_META_HEADER_BYTES ||
      payload_length > UINT64_MAX - W_META_HEADER_BYTES - directory_length) {
    return "wmetaContainerLengthMismatch";
  }
  uint64_t total = W_META_HEADER_BYTES + directory_length + payload_length;
  if (total != length) return "wmetaContainerLengthMismatch";
  size_t directory_size = (size_t)directory_length;
  const uint8_t *payload = data + W_META_HEADER_BYTES + directory_size;
  WMetaEntry *entries = NULL;
  size_t entry_count = 0u;
  const char *error = parse_directory(data + W_META_HEADER_BYTES, directory_size,
                                      payload_length, &entries, &entry_count);
  if (error != NULL) return error;
  if (mode != 0) {
    for (size_t index = 0u; index < entry_count; ++index) {
      WMetaEntry *entry = &entries[index];
      if (mode == 1 && !entry->critical) continue;
      if (entry->digest_algorithm != 1u) {
        error = entry->critical ? "unsupportedCriticalWMetaDigest"
                                : "unsupportedWMetaDigest";
        break;
      }
      Sha256 sha;
      uint8_t digest[32];
      sha256_init(&sha);
      sha256_update(&sha, payload + (size_t)entry->offset, (size_t)entry->length);
      sha256_final(&sha, digest);
      if (memcmp(digest, entry->digest, 32u) != 0) {
        error = "wmetaChunkDigestMismatch";
        break;
      }
      error = validate_chunk(payload + (size_t)entry->offset, (size_t)entry->length);
      if (error != NULL) break;
    }
  }
  free(entries);
  return error;
}

static uint8_t *read_file(const char *path, size_t *length) {
  FILE *file = fopen(path, "rb");
  if (file == NULL) return NULL;
  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    return NULL;
  }
  long measured = ftell(file);
  if (measured < 0 || fseek(file, 0, SEEK_SET) != 0) {
    fclose(file);
    return NULL;
  }
  *length = (size_t)measured;
  uint8_t *data = malloc(*length == 0u ? 1u : *length);
  if (data == NULL || fread(data, 1u, *length, file) != *length) {
    free(data);
    fclose(file);
    return NULL;
  }
  fclose(file);
  return data;
}

int main(int argument_count, char **arguments) {
  if (argument_count != 3) {
    fputs("usage: wmeta-reference FILE directory|core|full\n", stderr);
    return 2;
  }
  int mode;
  if (strcmp(arguments[2], "directory") == 0) mode = 0;
  else if (strcmp(arguments[2], "core") == 0) mode = 1;
  else if (strcmp(arguments[2], "full") == 0) mode = 2;
  else {
    fputs("unknownWMetaOpenMode\n", stdout);
    return 0;
  }
  size_t length = 0u;
  uint8_t *data = read_file(arguments[1], &length);
  if (data == NULL) {
    fputs("cannotReadWMetaOracleInput\n", stderr);
    return 2;
  }
  const char *error = validate_wmeta(data, length, mode);
  free(data);
  puts(error == NULL ? "accepted" : error);
  return 0;
}
