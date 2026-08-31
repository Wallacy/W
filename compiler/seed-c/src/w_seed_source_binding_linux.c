#include "w_seed_source_binding_linux.h"

#include <string.h>

#include "w_seed_ephemeral_provider_linux.h"
#include "w_seed_sha256.h"

static w_seed_source_binding_link_result link_result(
    w_seed_source_binding_link_status status,
    w_seed_source_binding_link_phase phase) {
  w_seed_source_binding_link_result result;
  (void)memset(&result, 0, sizeof(result));
  result.status = status;
  result.phase = phase;
  return result;
}

#if defined(__linux__)

typedef struct {
  uint64_t mount_id;
  uint64_t device_major;
  uint64_t device_minor;
  uint64_t inode;
} binding_linux_identity;

static bool text_equal(w_seed_frontend_text left, w_seed_frontend_text right) {
  return left.length == right.length &&
         (left.length == 0u || memcmp(left.data, right.data, left.length) == 0);
}

static bool parse_hex(const char *data, size_t start, uint64_t *value) {
  if (data == NULL || value == NULL) return false;
  uint64_t parsed = 0u;
  for (size_t index = 0u; index < 16u; index += 1u) {
    const uint8_t byte = (uint8_t)data[start + index];
    uint8_t nibble = 0u;
    if (byte >= (uint8_t)'0' && byte <= (uint8_t)'9')
      nibble = (uint8_t)(byte - (uint8_t)'0');
    else if (byte >= (uint8_t)'a' && byte <= (uint8_t)'f')
      nibble = (uint8_t)(byte - (uint8_t)'a' + 10u);
    else
      return false;
    parsed = (parsed << 4u) | (uint64_t)nibble;
  }
  *value = parsed;
  return true;
}

static bool parse_token(w_seed_frontend_text token, char prefix,
                        binding_linux_identity *identity) {
  if (identity == NULL || token.data == NULL ||
      token.length != (size_t)W_SEED_EPHEMERAL_PROVIDER_LINUX_V2_TOKEN_BYTES ||
      token.data[0] != prefix || token.data[17u] != '-' ||
      token.data[34u] != '-' || token.data[51u] != '-')
    return false;
  return parse_hex(token.data, 1u, &identity->mount_id) &&
         parse_hex(token.data, 18u, &identity->device_major) &&
         parse_hex(token.data, 35u, &identity->device_minor) &&
         parse_hex(token.data, 52u, &identity->inode) &&
         identity->mount_id != 0u && identity->inode != 0u;
}

static bool identity_equal(const binding_linux_identity *left,
                           const binding_linux_identity *right) {
  return left != NULL && right != NULL && left->mount_id == right->mount_id &&
         left->device_major == right->device_major &&
         left->device_minor == right->device_minor &&
         left->inode == right->inode;
}

static bool retained_identity(
    const w_seed_owner_guard_linux_context *context, size_t slot_index,
    w_seed_owner_guard_linux_slot_kind kind,
    binding_linux_identity *identity) {
  if (context == NULL || identity == NULL ||
      slot_index >= (size_t)W_SEED_OWNER_GUARD_LINUX_MAX_HANDLES)
    return false;
  const w_seed_owner_guard_linux_slot *slot = &context->slots[slot_index];
  if (!slot->used || slot->fd < 0 || slot->kind != kind ||
      slot->identity.mount_id == 0u || slot->identity.inode == 0u)
    return false;
  identity->mount_id = slot->identity.mount_id;
  identity->device_major = slot->identity.device_major;
  identity->device_minor = slot->identity.device_minor;
  identity->inode = slot->identity.inode;
  return true;
}

static bool hash_u64(w_seed_sha256_state *state, uint64_t value) {
  if (state == NULL) return false;
  uint8_t bytes[sizeof(value)];
  for (size_t index = 0u; index < sizeof(bytes); index += 1u)
    bytes[index] = (uint8_t)(value >> (56u - (unsigned int)(index * 8u)));
  w_seed_sha256_update(state, bytes, sizeof(bytes));
  return true;
}

static bool hash_text(w_seed_sha256_state *state, w_seed_frontend_text text) {
  if (state == NULL || (text.length != 0u && text.data == NULL) ||
      text.length > (size_t)UINT64_MAX ||
      !hash_u64(state, (uint64_t)text.length))
    return false;
  if (text.length != 0u)
    w_seed_sha256_update(state, (const uint8_t *)text.data, text.length);
  return true;
}

static bool hash_identity(w_seed_sha256_state *state,
                          const binding_linux_identity *identity) {
  return identity != NULL && hash_u64(state, identity->mount_id) &&
         hash_u64(state, identity->device_major) &&
         hash_u64(state, identity->device_minor) &&
         hash_u64(state, identity->inode);
}

static w_seed_source_binding_link_result linux_link_compose(
    const w_seed_source_binding_link *link,
    const w_seed_source_binding_link_input *input) {
  if (link == NULL || input == NULL || link->owner != link ||
      link->context == NULL ||
      link->context_size != sizeof(w_seed_owner_guard_linux_context) ||
      input->facts == NULL || input->fact_count == 0u ||
      input->root_fact_index != 0u ||
      input->guard_generation == 0u ||
      input->fact_count > (size_t)W_SEED_EPHEMERAL_GRAPH_MAX_SOURCES)
    return link_result(W_SEED_SOURCE_BINDING_LINK_INVALID,
                       W_SEED_SOURCE_BINDING_LINK_PHASE_VALIDATE);
  const w_seed_owner_guard_linux_context *context = link->context;
  if (!context->initialized || !context->native_supported)
    return link_result(W_SEED_SOURCE_BINDING_LINK_UNSUPPORTED,
                       W_SEED_SOURCE_BINDING_LINK_PHASE_OWNER);
  if (!context->session_live || context->active_generation == 0u ||
      context->active_generation != input->guard_generation)
    return link_result(W_SEED_SOURCE_BINDING_LINK_MUTATED,
                       W_SEED_SOURCE_BINDING_LINK_PHASE_OWNER);
  if (context->level_count == 0u ||
      context->level_count > W_SEED_OWNER_GUARD_MAX_LEVELS ||
      context->candidate_count == 0u ||
      context->candidate_count > context->level_count ||
      context->source_slot >=
          (size_t)W_SEED_OWNER_GUARD_LINUX_MAX_HANDLES)
    return link_result(W_SEED_SOURCE_BINDING_LINK_INVALID,
                       W_SEED_SOURCE_BINDING_LINK_PHASE_OWNER);
  binding_linux_identity start_parent;
  binding_linux_identity source;
  if (!retained_identity(context, context->directory_slots[0u],
                         W_SEED_OWNER_GUARD_LINUX_SLOT_DIRECTORY,
                         &start_parent) ||
      !retained_identity(context, context->source_slot,
                         W_SEED_OWNER_GUARD_LINUX_SLOT_SOURCE, &source))
    return link_result(W_SEED_SOURCE_BINDING_LINK_IO,
                       W_SEED_SOURCE_BINDING_LINK_PHASE_OWNER);

  const w_seed_ephemeral_graph_provider_facts *root =
      &input->facts[input->root_fact_index];
  static const char provider_id[] = W_SEED_EPHEMERAL_PROVIDER_LINUX_V2_ID;
  if (!text_equal(root->provider_id,
                  (w_seed_frontend_text){provider_id, sizeof(provider_id) - 1u}))
    return link_result(W_SEED_SOURCE_BINDING_LINK_UNSUPPORTED,
                       W_SEED_SOURCE_BINDING_LINK_PHASE_PROVIDER);
  binding_linux_identity root_token;
  binding_linux_identity owner_token;
  binding_linux_identity canonical;
  if (!parse_token(root->root_token, 'r', &root_token) ||
      !parse_token(root->source_provider_owner_token, 'o', &owner_token) ||
      !parse_token(root->canonical_token, 'c', &canonical))
    return link_result(W_SEED_SOURCE_BINDING_LINK_INVALID,
                       W_SEED_SOURCE_BINDING_LINK_PHASE_PROVIDER);
  if (!identity_equal(&root_token, &start_parent) ||
      !identity_equal(&owner_token, &start_parent) ||
      !identity_equal(&canonical, &source))
    return link_result(W_SEED_SOURCE_BINDING_LINK_MISMATCH,
                       W_SEED_SOURCE_BINDING_LINK_PHASE_COMPARE);
  for (size_t index = 0u; index < input->fact_count; index += 1u) {
    const w_seed_ephemeral_graph_provider_facts *facts = &input->facts[index];
    if (!text_equal(facts->provider_id,
                    (w_seed_frontend_text){provider_id,
                                           sizeof(provider_id) - 1u}))
      return link_result(W_SEED_SOURCE_BINDING_LINK_UNSUPPORTED,
                         W_SEED_SOURCE_BINDING_LINK_PHASE_PROVIDER);
    binding_linux_identity fact_root;
    binding_linux_identity fact_owner;
    binding_linux_identity fact_canonical;
    if (!parse_token(facts->root_token, 'r', &fact_root) ||
        !parse_token(facts->source_provider_owner_token, 'o', &fact_owner) ||
        !parse_token(facts->canonical_token, 'c', &fact_canonical))
      return link_result(W_SEED_SOURCE_BINDING_LINK_INVALID,
                         W_SEED_SOURCE_BINDING_LINK_PHASE_PROVIDER);
    if (!identity_equal(&fact_root, &root_token) ||
        !identity_equal(&fact_owner, &owner_token))
      return link_result(W_SEED_SOURCE_BINDING_LINK_MISMATCH,
                         W_SEED_SOURCE_BINDING_LINK_PHASE_COMPARE);
    for (size_t prior = 0u; prior < index; prior += 1u) {
      if (text_equal(facts->canonical_token,
                     input->facts[prior].canonical_token))
        return link_result(W_SEED_SOURCE_BINDING_LINK_MISMATCH,
                           W_SEED_SOURCE_BINDING_LINK_PHASE_COMPARE);
    }
  }

  w_seed_sha256_state digest;
  w_seed_sha256_init(&digest);
  static const char digest_tag[] = "w.seed.bnd0.linux-link/2";
  w_seed_sha256_update(&digest, (const uint8_t *)digest_tag,
                       sizeof(digest_tag) - 1u);
  if (!hash_identity(&digest, &start_parent) || !hash_identity(&digest, &source) ||
      !hash_u64(&digest, input->guard_generation) ||
      !hash_u64(&digest, (uint64_t)input->fact_count))
    return link_result(W_SEED_SOURCE_BINDING_LINK_IO,
                       W_SEED_SOURCE_BINDING_LINK_PHASE_COMMIT);
  for (size_t index = 0u; index < input->fact_count; index += 1u) {
    const w_seed_ephemeral_graph_provider_facts *facts = &input->facts[index];
    if (!hash_text(&digest, facts->provider_id) ||
        !hash_text(&digest, facts->root_token) ||
        !hash_text(&digest, facts->source_provider_owner_token) ||
        !hash_text(&digest, facts->canonical_token) ||
        !hash_u64(&digest, (uint64_t)facts->snapshot_before_byte_count) ||
        !hash_u64(&digest, (uint64_t)facts->snapshot_after_byte_count) ||
        !hash_u64(&digest, (uint64_t)facts->symlink))
      return link_result(W_SEED_SOURCE_BINDING_LINK_IO,
                         W_SEED_SOURCE_BINDING_LINK_PHASE_COMMIT);
  }
  w_seed_source_binding_link_result result = link_result(
      W_SEED_SOURCE_BINDING_LINK_OK, W_SEED_SOURCE_BINDING_LINK_PHASE_COMMIT);
  w_seed_sha256_final(&digest, result.link_digest);
  return result;
}

#else

static w_seed_source_binding_link_result linux_link_compose(
    const w_seed_source_binding_link *link,
    const w_seed_source_binding_link_input *input) {
  (void)link;
  (void)input;
  return link_result(W_SEED_SOURCE_BINDING_LINK_UNSUPPORTED,
                     W_SEED_SOURCE_BINDING_LINK_PHASE_VALIDATE);
}

#endif

bool w_seed_source_binding_linux_link(
    const w_seed_owner_guard_linux_context *context,
    w_seed_source_binding_link *link) {
  if (context == NULL || link == NULL) return false;
  *link = (w_seed_source_binding_link){
      link, context, sizeof(*context), linux_link_compose};
  return true;
}
