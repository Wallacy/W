#ifndef W_SEED_OWNER_GUARD_H
#define W_SEED_OWNER_GUARD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "w_seed_source.h"

#ifdef __cplusplus
extern "C" {
#endif

/* OWN0 is an internal observation guard. It does not select an owner, parse a
 * manifest, or authorize an ephemeral context. */
#define W_SEED_OWNER_GUARD_MAX_LEVELS 256u
#define W_SEED_OWNER_GUARD_MAX_PATH_BYTES 4096u
#define W_SEED_OWNER_GUARD_NO_CANDIDATE SIZE_MAX
#define W_SEED_OWNER_GUARD_NO_LEVEL SIZE_MAX

typedef enum {
  W_SEED_OWNER_GUARD_BACKEND_OK = 0,
  W_SEED_OWNER_GUARD_BACKEND_CAPACITY,
  W_SEED_OWNER_GUARD_BACKEND_MUTATED,
  W_SEED_OWNER_GUARD_BACKEND_BOUNDARY,
  W_SEED_OWNER_GUARD_BACKEND_REPARSE,
  W_SEED_OWNER_GUARD_BACKEND_UNSUPPORTED,
  W_SEED_OWNER_GUARD_BACKEND_IO,
  W_SEED_OWNER_GUARD_BACKEND_INVALID,
  W_SEED_OWNER_GUARD_BACKEND_FAULT,
} w_seed_owner_guard_backend_status;

typedef enum {
  W_SEED_OWNER_GUARD_BACKEND_PHASE_NONE = 0,
  W_SEED_OWNER_GUARD_BACKEND_PHASE_VALIDATE,
  W_SEED_OWNER_GUARD_BACKEND_PHASE_OPEN_START,
  W_SEED_OWNER_GUARD_BACKEND_PHASE_LOOKUP_CANDIDATE,
  W_SEED_OWNER_GUARD_BACKEND_PHASE_OPEN_PARENT,
  W_SEED_OWNER_GUARD_BACKEND_PHASE_REVALIDATE_DIRECTORY,
  W_SEED_OWNER_GUARD_BACKEND_PHASE_REVALIDATE_CANDIDATE,
  W_SEED_OWNER_GUARD_BACKEND_PHASE_REVALIDATE_PARENT,
  W_SEED_OWNER_GUARD_BACKEND_PHASE_COMMIT,
} w_seed_owner_guard_backend_phase;

/* Backend observations contain no native handle or filesystem identity. The
 * backend retains those values in its session context. candidate_index is
 * dense in leaf-to-root order, or NO_CANDIDATE for an absent literal lookup.
 * root_terminal is true only for the final directory observation. */
typedef struct {
  size_t directory_ordinal;
  size_t candidate_index;
  bool root_terminal;
} w_seed_owner_guard_observation;

typedef struct {
  w_seed_owner_guard_backend_status status;
  w_seed_owner_guard_backend_phase phase;
  size_t level_index;
  size_t required_level_capacity;
  uint64_t generation;
  size_t level_count;
  size_t candidate_count;
} w_seed_owner_guard_backend_result;

/* An OK begin creates one tentative backend session. Before the core publishes
 * that session, abort_begin closes it once in exact reverse acquisition order.
 * abort_begin does not inspect the generation because an OK envelope can
 * contain a malformed generation. It must complete cleanup and cannot report
 * failure. A backend that cannot guarantee that cleanup must return a non-OK
 * begin result before it creates a tentative session. Every non-OK begin result
 * has already cleaned all internal acquisitions; the core does not call
 * abort_begin or destroy for it.
 *
 * After publication, the successful begin session remains live until destroy.
 * A successful or failed revalidate keeps that same session live. The backend
 * context owns a monotonic nonzero generation counter and must reject wrap or
 * reuse. destroy applies only to a published session and rejects zero or stale
 * generations. For the active generation, it closes the session once in exact
 * reverse acquisition order and never closes its borrowed base handle. The
 * core calls abort_begin exactly once after any malformed OK begin envelope or
 * publication failure, and never calls destroy for that tentative session.
 *
 * begin discovers the upward ancestry once. Begin and revalidate may perform
 * the safe downward source binding from the borrowed base by using the bounded
 * source_path copy to prove path-to-source identity. Revalidate uses retained
 * handles for the upward ancestry and must recheck all these facts:
 *
 * - the borrowed base handle is live and has the original identity;
 * - the exact source path still resolves to the original source identity;
 * - the source still has the original start-parent identity;
 * - every retained directory has the original identity and parent edge;
 * - the final retained directory is still the same root terminal;
 * - every candidate lookup resolves the same entry, type, spelling evidence,
 *   and identity;
 * - every absence remains an exact not-found result.
 *
 * Revalidate must not reopen the upward ancestry by a textual path or use a
 * textual fallback. A no-op revalidation does not conform. lookup uses the backend's
 * fixed literal `build.w`; the caller cannot provide another spelling. Any
 * entry resolved by that lookup is a candidate. A reparse point, mount or
 * volume boundary, marker problem, or missing identity is a blocking non-OK
 * status. */
typedef struct {
  void *context;
  w_seed_owner_guard_backend_result (*begin)(
      void *context, w_seed_byte_view source_path,
      w_seed_owner_guard_observation *observations,
      size_t observation_capacity);
  w_seed_owner_guard_backend_result (*revalidate)(
      void *context, uint64_t generation,
      w_seed_owner_guard_observation *observations,
      size_t observation_capacity);
  void (*abort_begin)(void *context);
  void (*destroy)(void *context, uint64_t generation);
} w_seed_owner_guard_backend;

/* Scratch and publication use disjoint caller-owned ranges. The backend may
 * change staged and revalidation observations on every callback. The core
 * changes published_candidates only after a complete successful begin.
 * Callers must not mutate any range while the guard is live. */
typedef struct {
  w_seed_owner_guard_observation *staged;
  size_t staged_capacity;
  w_seed_owner_guard_observation *revalidation;
  size_t revalidation_capacity;
  struct w_seed_owner_guard_candidate_ref *published_candidates;
  size_t published_candidate_capacity;
} w_seed_owner_guard_storage;

typedef struct w_seed_owner_guard_candidate_ref {
  uint64_t generation;
  size_t directory_ordinal;
  size_t candidate_index;
} w_seed_owner_guard_candidate_ref;

typedef enum {
  W_SEED_OWNER_GUARD_ZERO = 0,
  W_SEED_OWNER_GUARD_LIVE_OBSERVED,
  W_SEED_OWNER_GUARD_LIVE_RECONFIRMED,
  W_SEED_OWNER_GUARD_FAILED,
} w_seed_owner_guard_lifecycle;

typedef enum {
  W_SEED_OWNER_GUARD_DISPOSITION_NONE = 0,
  W_SEED_OWNER_GUARD_CANDIDATES_OBSERVED,
  W_SEED_OWNER_GUARD_NO_CANDIDATE_OBSERVED,
  W_SEED_OWNER_GUARD_CANDIDATES_RECONFIRMED,
  W_SEED_OWNER_GUARD_NO_CANDIDATE_RECONFIRMED,
} w_seed_owner_guard_disposition;

typedef enum {
  W_SEED_OWNER_GUARD_OK = 0,
  W_SEED_OWNER_GUARD_INVALID,
  W_SEED_OWNER_GUARD_CAPACITY,
  W_SEED_OWNER_GUARD_MUTATED,
  W_SEED_OWNER_GUARD_BOUNDARY,
  W_SEED_OWNER_GUARD_REPARSE,
  W_SEED_OWNER_GUARD_UNSUPPORTED,
  W_SEED_OWNER_GUARD_IO,
  W_SEED_OWNER_GUARD_FAULT,
} w_seed_owner_guard_status;

typedef enum {
  W_SEED_OWNER_GUARD_PHASE_NONE = 0,
  W_SEED_OWNER_GUARD_PHASE_VALIDATE,
  W_SEED_OWNER_GUARD_PHASE_BEGIN,
  W_SEED_OWNER_GUARD_PHASE_REVALIDATE,
  W_SEED_OWNER_GUARD_PHASE_COMMIT,
} w_seed_owner_guard_phase;

typedef struct {
  w_seed_byte_view source_path;
  size_t max_levels;
  w_seed_owner_guard_storage storage;
  w_seed_owner_guard_backend backend;
  /* context and this size form one mutable range. The context must not hide
   * mutable pointees outside that range. It must outlive the guard session. */
  size_t backend_context_size;
} w_seed_owner_guard_input;

typedef struct w_seed_owner_guard {
  const struct w_seed_owner_guard *owner;
  w_seed_owner_guard_backend backend;
  w_seed_owner_guard_storage storage;
  uint64_t generation;
  size_t directory_count;
  size_t candidate_count;
  size_t max_levels;
  size_t backend_context_size;
  w_seed_owner_guard_lifecycle lifecycle;
  w_seed_owner_guard_disposition disposition;
  bool session_live;
} w_seed_owner_guard;

typedef struct {
  w_seed_owner_guard_lifecycle lifecycle;
  w_seed_owner_guard_disposition disposition;
  uint64_t generation;
  size_t directory_count;
  const w_seed_owner_guard_candidate_ref *candidates;
  size_t candidate_count;
  bool root_terminal;
} w_seed_owner_guard_view;

typedef struct {
  w_seed_owner_guard_status status;
  w_seed_owner_guard_phase phase;
  w_seed_owner_guard_backend_status backend_status;
  w_seed_owner_guard_backend_phase backend_phase;
  size_t level_index;
  size_t required_level_capacity;
  size_t required_candidate_capacity;
  uint64_t generation;
} w_seed_owner_guard_result;

/* begin requires a zero guard. It preserves the guard and published candidate
 * range on every non-OK return. Scratch and backend context may change. */
w_seed_owner_guard_status w_seed_owner_guard_begin(
    const w_seed_owner_guard_input *input, w_seed_owner_guard *guard,
    w_seed_owner_guard_result *result);

/* A view is descriptive evidence only. NO_CANDIDATE_OBSERVED is not an
 * authorization. Only a later consumer may define policy after a successful
 * NO_CANDIDATE_RECONFIRMED observation. */
bool w_seed_owner_guard_get_view(const w_seed_owner_guard *guard,
                                 w_seed_owner_guard_view *view);

/* Return one descriptive reference from a live guard. The reference alone is
 * never authority. A future MAN0 reader must receive the live guard, this
 * generation, and candidate_index together. */
bool w_seed_owner_guard_get_candidate(
    const w_seed_owner_guard *guard, size_t candidate_index,
    w_seed_owner_guard_candidate_ref *candidate);

/* Revalidate the retained session. It promotes an observed candidate or
 * absence disposition to its reconfirmed disposition. Any non-OK result makes
 * the guard FAILED and invalidates all earlier views. */
w_seed_owner_guard_status w_seed_owner_guard_revalidate(
    w_seed_owner_guard *guard, w_seed_owner_guard_result *result);

/* Idempotent for a zero guard and for the original guard after one destroy.
 * A copied guard fails owner-self validation and never closes handles. */
void w_seed_owner_guard_destroy(w_seed_owner_guard *guard);

/* Composition gap: OWN0 rechecks its source inside one adapter session, but
 * it does not prove that ACQ0 consumed that same source. A future MAN0/WSP0
 * composer must bind an opaque source-session receipt to the ACQ0 root
 * provider token or receipt. Native identities must remain adapter-private. */

#ifdef __cplusplus
}
#endif

#endif
