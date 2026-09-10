#ifndef W_SEED_PROCESS0_H
#define W_SEED_PROCESS0_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* PROCESS0 is an internal seed-only provider kernel. It does not discover an
 * operating-system startup vector and does not expose the std.process ABI. */
#define W_SEED_PROCESS0_ABI_VERSION "w-seed-process0-1"

typedef enum {
  W_SEED_PROCESS0_ENCODING_NONE = 0,
  W_SEED_PROCESS0_ENCODING_POSIX_BYTES = 1,
  W_SEED_PROCESS0_ENCODING_WINDOWS_UTF16 = 2,
} w_seed_process0_encoding;

/* `length` is bytes for POSIX_BYTES and uint16_t units for WINDOWS_UTF16.
 * A NULL data pointer is valid only for an empty argument. */
typedef struct {
  w_seed_process0_encoding encoding;
  const void *data;
  size_t length;
} w_seed_process0_native_arg;

/* The caller selects this vector explicitly. In particular, this type does
 * not define whether a host's program name is included. The descriptor table
 * and every backing range remain immutable until root_finalize succeeds. */
typedef struct {
  w_seed_process0_encoding encoding;
  const w_seed_process0_native_arg *items;
  size_t count;
} w_seed_process0_arg_vector;

typedef enum {
  W_SEED_PROCESS0_OWNER_NONE = 0,
  W_SEED_PROCESS0_OWNER_ARGUMENTS = 1,
  W_SEED_PROCESS0_OWNER_CONTEXT = 2,
} w_seed_process0_owner_kind;

typedef enum {
  W_SEED_PROCESS0_OK = 0,
  W_SEED_PROCESS0_INVALID,
  W_SEED_PROCESS0_OVERLAP,
  W_SEED_PROCESS0_OUT_OF_RANGE,
  W_SEED_PROCESS0_INACTIVE,
  W_SEED_PROCESS0_BUSY,
} w_seed_process0_status;

typedef struct w_seed_process0_root w_seed_process0_root;
typedef struct w_seed_process0_arguments w_seed_process0_arguments;
typedef struct w_seed_process0_context w_seed_process0_context;

/* A view is borrowed from the caller's immutable vector. Its root and
 * generation identify the lifetime; no retain/release operation is provided.
 * The caller must end all such borrows before root_finalize or reuse. */
typedef struct {
  w_seed_process0_encoding encoding;
  const void *data;
  size_t length;
  const w_seed_process0_root *root;
  uint64_t generation;
} w_seed_process0_native_view;

typedef struct {
  w_seed_process0_status status;
  size_t count;
} w_seed_process0_count_result;

typedef struct {
  w_seed_process0_status status;
  w_seed_process0_native_view view;
} w_seed_process0_get_result;

typedef struct {
  w_seed_process0_status status;
  bool found;
} w_seed_process0_contains_result;

/* All three state objects are caller-owned. Root storage and the two wrapper
 * storages must be zero-initialized before their first use. A later init is
 * allowed only after a successful finalize; the generation never wraps. The
 * vector, descriptor table, and backing data are borrowed without copying and
 * must remain live and unmodified while the root is live. The C caller also
 * owns pointer-provenance, alignment, lifetime, and no-concurrent-mutation
 * obligations that this bounded kernel cannot prove for arbitrary pointers.
 * All three state objects stay address-stable until successful finalization.
 * These records are provider state, not the future movable W-value ABI.
 * Reuse preserves the root generation. Rebinding wrapper storage to a different
 * root requires zeroing it after the previous root has been finalized. */
struct w_seed_process0_root {
  const w_seed_process0_root *self;
  const w_seed_process0_native_arg *items;
  size_t count;
  w_seed_process0_encoding encoding;
  uint64_t generation;
  w_seed_process0_arguments *arguments_owner;
  w_seed_process0_context *context_owner;
  bool live;
  bool arguments_live;
  bool context_live;
};

/* A copied live handle is not a second owner: the self address check makes a
 * copied alias inactive. The compiler's non-Copy rules remain responsible for
 * preventing such copies in W; this is only a bounded C misuse guard. */
struct w_seed_process0_arguments {
  const w_seed_process0_arguments *self;
  w_seed_process0_root *root;
  uint64_t generation;
  w_seed_process0_owner_kind kind;
  bool live;
};

struct w_seed_process0_context {
  const w_seed_process0_context *self;
  w_seed_process0_root *root;
  uint64_t generation;
  w_seed_process0_owner_kind kind;
  bool live;
};

/* Validation is complete before any destination is changed. On every failure
 * root and wrappers remain unchanged; the returned status is the failure
 * channel. */
w_seed_process0_status w_seed_process0_root_init(
    const w_seed_process0_arg_vector *vector, w_seed_process0_root *root,
    w_seed_process0_arguments *arguments, w_seed_process0_context *context);

/* Finalization is separate from wrapper release. It succeeds only after both
 * published owner obligations have been released, and never changes the
 * caller's vector or its backing storage. */
w_seed_process0_status w_seed_process0_root_finalize(
    w_seed_process0_root *root);

w_seed_process0_count_result w_seed_process0_arguments_count(
    const w_seed_process0_arguments *arguments);

w_seed_process0_get_result w_seed_process0_arguments_get(
    const w_seed_process0_arguments *arguments, size_t index);

/* Text contains is intentionally absent: the specified W-UTF-8-to-native
 * conversion is not implemented here. This operation compares one exact native
 * representation. */
w_seed_process0_contains_result w_seed_process0_arguments_contains_native(
    const w_seed_process0_arguments *arguments,
    w_seed_process0_native_arg needle);

/* Each successful drop invalidates exactly one published wrapper and only
 * clears that wrapper's root obligation. It never drains or finalizes root. */
w_seed_process0_status w_seed_process0_arguments_drop(
    w_seed_process0_arguments *arguments);
w_seed_process0_status w_seed_process0_context_drop(
    w_seed_process0_context *context);

#ifdef __cplusplus
}
#endif

#endif
