#include "w_seed_process0.h"

#include <limits.h>
#include <string.h>

_Static_assert(CHAR_BIT == 8,
               "w_seed_process0 requires 8-bit bytes");
_Static_assert(sizeof(uint16_t) == 2u,
               "w_seed_process0 requires 16-bit uint16_t units");

#ifndef UINTPTR_MAX
#error "w_seed_process0 requires uintptr_t"
#endif

typedef struct {
  uintptr_t begin;
  uintptr_t end;
  bool present;
} process0_range;

static bool pointer_aligned(const void *pointer, size_t alignment) {
  if (pointer == NULL || alignment == 0u) return false;
  return ((uintptr_t)pointer % (uintptr_t)alignment) == 0u;
}

/* This is a bounded integer-range check, not a claim that an arbitrary C
 * pointer has valid provenance. The caller remains responsible for that. */
static bool make_range(const void *pointer, size_t bytes,
                       process0_range *range) {
  if (range == NULL) return false;
  *range = (process0_range){0u, 0u, false};
  if (bytes == 0u) return true;
  if (pointer == NULL) return false;

  const uintptr_t begin = (uintptr_t)pointer;
  const uintmax_t begin_value = (uintmax_t)begin;
  const uintmax_t maximum = (uintmax_t)UINTPTR_MAX;
  if ((uintmax_t)bytes > maximum - begin_value) return false;
  const uintptr_t end = begin + (uintptr_t)bytes;
  if (end < begin) return false;
  *range = (process0_range){begin, end, true};
  return true;
}

static bool ranges_overlap(process0_range left, process0_range right) {
  return left.present && right.present && left.begin < right.end &&
         right.begin < left.end;
}

static bool encoding_valid(w_seed_process0_encoding encoding) {
  return encoding == W_SEED_PROCESS0_ENCODING_POSIX_BYTES ||
         encoding == W_SEED_PROCESS0_ENCODING_WINDOWS_UTF16;
}

static bool native_bytes(w_seed_process0_encoding encoding, size_t length,
                         size_t *bytes) {
  if (bytes == NULL) return false;
  if (encoding == W_SEED_PROCESS0_ENCODING_POSIX_BYTES) {
    *bytes = length;
    return true;
  }
  if (encoding != W_SEED_PROCESS0_ENCODING_WINDOWS_UTF16 ||
      length > SIZE_MAX / sizeof(uint16_t))
    return false;
  *bytes = length * sizeof(uint16_t);
  return true;
}

static bool native_arg_shape(const w_seed_process0_native_arg *argument,
                             w_seed_process0_encoding root_encoding,
                             process0_range *data_range) {
  if (argument == NULL || !encoding_valid(root_encoding) ||
      argument->encoding != root_encoding)
    return false;

  size_t bytes = 0u;
  if (!native_bytes(root_encoding, argument->length, &bytes)) return false;
  if (bytes != 0u) {
    if (argument->data == NULL) return false;
    if (root_encoding == W_SEED_PROCESS0_ENCODING_WINDOWS_UTF16 &&
        !pointer_aligned(argument->data, _Alignof(uint16_t)))
      return false;
  }
  return make_range(argument->data, bytes, data_range);
}

static bool vector_items_range(const w_seed_process0_arg_vector *vector,
                               process0_range *items_range) {
  if (vector == NULL || items_range == NULL) return false;
  *items_range = (process0_range){0u, 0u, false};
  if (vector->count == 0u) return true;
  if (vector->items == NULL ||
      !pointer_aligned(vector->items, _Alignof(w_seed_process0_native_arg)) ||
      vector->count > SIZE_MAX / sizeof(*vector->items))
    return false;
  return make_range(vector->items,
                    vector->count * sizeof(*vector->items), items_range);
}

static bool root_items_shape(const w_seed_process0_root *root,
                             process0_range *items_range) {
  if (root == NULL || items_range == NULL || !encoding_valid(root->encoding))
    return false;
  const w_seed_process0_arg_vector vector = {
      root->encoding, root->items, root->count};
  return vector_items_range(&vector, items_range);
}

static bool root_zero_state(const w_seed_process0_root *root) {
  return root->self == NULL && root->items == NULL && root->count == 0u &&
         root->encoding == W_SEED_PROCESS0_ENCODING_NONE &&
         root->generation == 0u && root->arguments_owner == NULL &&
         root->context_owner == NULL && !root->live &&
         !root->arguments_live && !root->context_live;
}

static w_seed_process0_status next_generation(
    const w_seed_process0_root *root, uint64_t *generation) {
  if (root == NULL || generation == NULL) return W_SEED_PROCESS0_INVALID;

  if (root->self == NULL) {
    if (!root_zero_state(root)) return W_SEED_PROCESS0_INVALID;
    *generation = 1u;
    return W_SEED_PROCESS0_OK;
  }

  if (root->self != root) return W_SEED_PROCESS0_INVALID;
  if (root->live || root->arguments_live || root->context_live)
    return W_SEED_PROCESS0_BUSY;
  if (root->generation == 0u || root->generation == UINT64_MAX)
    return W_SEED_PROCESS0_INVALID;
  if (root->items != NULL || root->count != 0u ||
      root->encoding != W_SEED_PROCESS0_ENCODING_NONE ||
      root->arguments_owner != NULL || root->context_owner != NULL)
    return W_SEED_PROCESS0_INVALID;
  *generation = root->generation + 1u;
  return W_SEED_PROCESS0_OK;
}

static w_seed_process0_status arguments_status(
    const w_seed_process0_arguments *arguments,
    w_seed_process0_root **root_out) {
  if (arguments == NULL ||
      !pointer_aligned(arguments, _Alignof(w_seed_process0_arguments)))
    return W_SEED_PROCESS0_INVALID;
  if (arguments->kind != W_SEED_PROCESS0_OWNER_ARGUMENTS)
    return W_SEED_PROCESS0_INVALID;
  if (arguments->self != arguments || !arguments->live)
    return W_SEED_PROCESS0_INACTIVE;

  w_seed_process0_root *root = arguments->root;
  if (root == NULL ||
      !pointer_aligned(root, _Alignof(w_seed_process0_root)))
    return W_SEED_PROCESS0_INACTIVE;
  if (root->self != root || !root->live ||
      root->generation != arguments->generation ||
      root->arguments_owner != arguments || !root->arguments_live)
    return W_SEED_PROCESS0_INACTIVE;

  process0_range ignored_items_range;
  if (!root_items_shape(root, &ignored_items_range))
    return W_SEED_PROCESS0_INVALID;
  if (root_out != NULL) *root_out = root;
  return W_SEED_PROCESS0_OK;
}

static w_seed_process0_status context_status(
    const w_seed_process0_context *context,
    w_seed_process0_root **root_out) {
  if (context == NULL ||
      !pointer_aligned(context, _Alignof(w_seed_process0_context)))
    return W_SEED_PROCESS0_INVALID;
  if (context->kind != W_SEED_PROCESS0_OWNER_CONTEXT)
    return W_SEED_PROCESS0_INVALID;
  if (context->self != context || !context->live)
    return W_SEED_PROCESS0_INACTIVE;

  w_seed_process0_root *root = context->root;
  if (root == NULL || !pointer_aligned(root, _Alignof(w_seed_process0_root)))
    return W_SEED_PROCESS0_INACTIVE;
  if (root->self != root || !root->live ||
      root->generation != context->generation ||
      root->context_owner != context || !root->context_live)
    return W_SEED_PROCESS0_INACTIVE;

  process0_range ignored_items_range;
  if (!root_items_shape(root, &ignored_items_range))
    return W_SEED_PROCESS0_INVALID;
  if (root_out != NULL) *root_out = root;
  return W_SEED_PROCESS0_OK;
}

static bool destination_ranges_valid(
    const w_seed_process0_arg_vector *vector,
    const w_seed_process0_root *root,
    const w_seed_process0_arguments *arguments,
    const w_seed_process0_context *context, process0_range *vector_range,
    process0_range *root_range, process0_range *arguments_range,
    process0_range *context_range) {
  if (vector == NULL || root == NULL || arguments == NULL || context == NULL ||
      !pointer_aligned(vector, _Alignof(w_seed_process0_arg_vector)) ||
      !pointer_aligned(root, _Alignof(w_seed_process0_root)) ||
      !pointer_aligned(arguments, _Alignof(w_seed_process0_arguments)) ||
      !pointer_aligned(context, _Alignof(w_seed_process0_context)))
    return false;
  return make_range(vector, sizeof(*vector), vector_range) &&
         make_range(root, sizeof(*root), root_range) &&
         make_range(arguments, sizeof(*arguments), arguments_range) &&
         make_range(context, sizeof(*context), context_range);
}

static bool destination_overlap(process0_range vector_range,
                                process0_range root_range,
                                process0_range arguments_range,
                                process0_range context_range) {
  return ranges_overlap(vector_range, root_range) ||
         ranges_overlap(vector_range, arguments_range) ||
         ranges_overlap(vector_range, context_range) ||
         ranges_overlap(root_range, arguments_range) ||
         ranges_overlap(root_range, context_range) ||
         ranges_overlap(arguments_range, context_range);
}

w_seed_process0_status w_seed_process0_root_init(
    const w_seed_process0_arg_vector *vector, w_seed_process0_root *root,
    w_seed_process0_arguments *arguments, w_seed_process0_context *context) {
  process0_range vector_range;
  process0_range root_range;
  process0_range arguments_range;
  process0_range context_range;
  if (!destination_ranges_valid(
          vector, root, arguments, context, &vector_range, &root_range,
          &arguments_range, &context_range))
    return W_SEED_PROCESS0_INVALID;
  if (destination_overlap(vector_range, root_range, arguments_range,
                          context_range))
    return W_SEED_PROCESS0_OVERLAP;
  if (!encoding_valid(vector->encoding)) return W_SEED_PROCESS0_INVALID;

  process0_range items_range;
  if (!vector_items_range(vector, &items_range))
    return W_SEED_PROCESS0_INVALID;
  if (ranges_overlap(items_range, root_range) ||
      ranges_overlap(items_range, arguments_range) ||
      ranges_overlap(items_range, context_range))
    return W_SEED_PROCESS0_OVERLAP;

  for (size_t index = 0u; index < vector->count; index += 1u) {
    process0_range data_range;
    if (!native_arg_shape(&vector->items[index], vector->encoding,
                          &data_range))
      return W_SEED_PROCESS0_INVALID;
    if (ranges_overlap(data_range, root_range) ||
        ranges_overlap(data_range, arguments_range) ||
        ranges_overlap(data_range, context_range))
      return W_SEED_PROCESS0_OVERLAP;
  }

  uint64_t generation = 0u;
  const w_seed_process0_status state_status =
      next_generation(root, &generation);
  if (state_status != W_SEED_PROCESS0_OK) return state_status;

  /* A destination wrapper may be zero storage or the released wrapper from
   * this exact finalized root. Never dereference a non-matching root pointer
   * while deciding whether the destination can be overwritten. */
  if (arguments->self == NULL) {
    if (arguments->root != NULL || arguments->generation != 0u ||
        arguments->kind != W_SEED_PROCESS0_OWNER_NONE || arguments->live)
      return W_SEED_PROCESS0_INVALID;
  } else {
    if (arguments->self != arguments) return W_SEED_PROCESS0_INVALID;
    if (arguments->live) return W_SEED_PROCESS0_BUSY;
    if (root->self != root || arguments->root != root ||
        arguments->generation != root->generation ||
        arguments->kind != W_SEED_PROCESS0_OWNER_ARGUMENTS)
      return W_SEED_PROCESS0_INVALID;
  }
  if (context->self == NULL) {
    if (context->root != NULL || context->generation != 0u ||
        context->kind != W_SEED_PROCESS0_OWNER_NONE || context->live)
      return W_SEED_PROCESS0_INVALID;
  } else {
    if (context->self != context) return W_SEED_PROCESS0_INVALID;
    if (context->live) return W_SEED_PROCESS0_BUSY;
    if (root->self != root || context->root != root ||
        context->generation != root->generation ||
        context->kind != W_SEED_PROCESS0_OWNER_CONTEXT)
      return W_SEED_PROCESS0_INVALID;
  }

  *arguments = (w_seed_process0_arguments){
      arguments, root, generation, W_SEED_PROCESS0_OWNER_ARGUMENTS, true};
  *context = (w_seed_process0_context){
      context, root, generation, W_SEED_PROCESS0_OWNER_CONTEXT, true};
  *root = (w_seed_process0_root){
      root, vector->items, vector->count, vector->encoding, generation,
      arguments, context, true, true, true};
  return W_SEED_PROCESS0_OK;
}

static w_seed_process0_status arguments_released_status(
    const w_seed_process0_root *root) {
  const w_seed_process0_arguments *arguments =
      root == NULL ? NULL : root->arguments_owner;
  if (arguments == NULL ||
      !pointer_aligned(arguments, _Alignof(w_seed_process0_arguments)))
    return W_SEED_PROCESS0_INVALID;
  if (arguments->self != arguments ||
      arguments->kind != W_SEED_PROCESS0_OWNER_ARGUMENTS ||
      arguments->root != root || arguments->generation != root->generation)
    return W_SEED_PROCESS0_INVALID;
  return arguments->live ? W_SEED_PROCESS0_BUSY : W_SEED_PROCESS0_OK;
}

static w_seed_process0_status context_released_status(
    const w_seed_process0_root *root) {
  const w_seed_process0_context *context =
      root == NULL ? NULL : root->context_owner;
  if (context == NULL ||
      !pointer_aligned(context, _Alignof(w_seed_process0_context)))
    return W_SEED_PROCESS0_INVALID;
  if (context->self != context ||
      context->kind != W_SEED_PROCESS0_OWNER_CONTEXT ||
      context->root != root || context->generation != root->generation)
    return W_SEED_PROCESS0_INVALID;
  return context->live ? W_SEED_PROCESS0_BUSY : W_SEED_PROCESS0_OK;
}

w_seed_process0_status w_seed_process0_root_finalize(
    w_seed_process0_root *root) {
  if (root == NULL || !pointer_aligned(root, _Alignof(w_seed_process0_root)))
    return W_SEED_PROCESS0_INVALID;
  if (root->self == NULL || root->self != root) {
    return root->self == NULL ? W_SEED_PROCESS0_INACTIVE
                              : W_SEED_PROCESS0_INVALID;
  }
  if (!root->live) return W_SEED_PROCESS0_INACTIVE;
  if (!encoding_valid(root->encoding)) return W_SEED_PROCESS0_INVALID;
  process0_range items_range;
  if (!root_items_shape(root, &items_range)) return W_SEED_PROCESS0_INVALID;

  if (root->arguments_live || root->context_live)
    return W_SEED_PROCESS0_BUSY;
  const w_seed_process0_status arguments_released =
      arguments_released_status(root);
  const w_seed_process0_status context_released = context_released_status(root);
  if (arguments_released == W_SEED_PROCESS0_BUSY ||
      context_released == W_SEED_PROCESS0_BUSY)
    return W_SEED_PROCESS0_BUSY;
  if (arguments_released != W_SEED_PROCESS0_OK ||
      context_released != W_SEED_PROCESS0_OK)
    return W_SEED_PROCESS0_INVALID;

  root->items = NULL;
  root->count = 0u;
  root->encoding = W_SEED_PROCESS0_ENCODING_NONE;
  root->arguments_owner = NULL;
  root->context_owner = NULL;
  root->live = false;
  root->arguments_live = false;
  root->context_live = false;
  return W_SEED_PROCESS0_OK;
}

w_seed_process0_count_result w_seed_process0_arguments_count(
    const w_seed_process0_arguments *arguments) {
  w_seed_process0_root *root = NULL;
  const w_seed_process0_status status = arguments_status(arguments, &root);
  if (status != W_SEED_PROCESS0_OK)
    return (w_seed_process0_count_result){status, 0u};
  return (w_seed_process0_count_result){W_SEED_PROCESS0_OK, root->count};
}

w_seed_process0_get_result w_seed_process0_arguments_get(
    const w_seed_process0_arguments *arguments, size_t index) {
  const w_seed_process0_native_view empty_view = {0};
  w_seed_process0_root *root = NULL;
  const w_seed_process0_status status = arguments_status(arguments, &root);
  if (status != W_SEED_PROCESS0_OK)
    return (w_seed_process0_get_result){status, empty_view};
  if (index >= root->count)
    return (w_seed_process0_get_result){W_SEED_PROCESS0_OUT_OF_RANGE,
                                        empty_view};

  process0_range data_range;
  const w_seed_process0_native_arg *argument = &root->items[index];
  if (!native_arg_shape(argument, root->encoding, &data_range))
    return (w_seed_process0_get_result){W_SEED_PROCESS0_INVALID, empty_view};
  const w_seed_process0_native_view view = {
      root->encoding, argument->data, argument->length, root,
      root->generation};
  return (w_seed_process0_get_result){W_SEED_PROCESS0_OK, view};
}

w_seed_process0_contains_result w_seed_process0_arguments_contains_native(
    const w_seed_process0_arguments *arguments,
    w_seed_process0_native_arg needle) {
  w_seed_process0_root *root = NULL;
  const w_seed_process0_status status = arguments_status(arguments, &root);
  if (status != W_SEED_PROCESS0_OK)
    return (w_seed_process0_contains_result){status, false};

  process0_range needle_range;
  if (!native_arg_shape(&needle, root->encoding, &needle_range))
    return (w_seed_process0_contains_result){W_SEED_PROCESS0_INVALID, false};
  size_t needle_bytes = 0u;
  if (!native_bytes(root->encoding, needle.length, &needle_bytes))
    return (w_seed_process0_contains_result){W_SEED_PROCESS0_INVALID, false};

  for (size_t index = 0u; index < root->count; index += 1u) {
    process0_range item_range;
    const w_seed_process0_native_arg *item = &root->items[index];
    if (!native_arg_shape(item, root->encoding, &item_range))
      return (w_seed_process0_contains_result){W_SEED_PROCESS0_INVALID, false};
    if (item->length != needle.length) continue;
    if (needle_bytes == 0u ||
        memcmp(item->data, needle.data, needle_bytes) == 0)
      return (w_seed_process0_contains_result){W_SEED_PROCESS0_OK, true};
  }
  return (w_seed_process0_contains_result){W_SEED_PROCESS0_OK, false};
}

w_seed_process0_status w_seed_process0_arguments_drop(
    w_seed_process0_arguments *arguments) {
  w_seed_process0_root *root = NULL;
  const w_seed_process0_status status = arguments_status(arguments, &root);
  if (status != W_SEED_PROCESS0_OK) return status;
  arguments->live = false;
  root->arguments_live = false;
  return W_SEED_PROCESS0_OK;
}

w_seed_process0_status w_seed_process0_context_drop(
    w_seed_process0_context *context) {
  w_seed_process0_root *root = NULL;
  const w_seed_process0_status status = context_status(context, &root);
  if (status != W_SEED_PROCESS0_OK) return status;
  context->live = false;
  root->context_live = false;
  return W_SEED_PROCESS0_OK;
}
