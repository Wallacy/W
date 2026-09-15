#include "w_seed_task_lifecycle0.h"

#include <stddef.h>

typedef struct {
  uintptr_t begin;
  uintptr_t end;
  uint8_t active;
} w_seed_task_lifecycle0_memory_range;

typedef struct {
  struct w_seed_task_lifecycle0_state *state;
  uint32_t next_reserved;
  uint32_t next_published;
  uint32_t next_joined;
  uint32_t next_released;
  uint8_t has_error_candidate;
} w_seed_task_lifecycle0_execution;

typedef struct {
  const char *schema;
  const char *expected_schema;
  size_t schema_size;
  uint32_t scope_generation;
  uint32_t task_count;
  const w_seed_task_lifecycle0_task_spec *tasks;
  uint32_t event_count;
  const w_seed_task_lifecycle0_event *events;
} w_seed_task_lifecycle_view;

/* The reducer uses this compact state instead of a full result.  That keeps
 * the transaction scratch bounded without a stack-probe or a CRT helper. */
typedef struct w_seed_task_lifecycle0_state {
  uint32_t transition_count;
  uint32_t primary_error_task;
  w_seed_task_lifecycle0_scope_record scope;
  w_seed_task_lifecycle0_task_record *tasks;
  size_t task_capacity;
} w_seed_task_lifecycle0_state;

static void zero_bytes(void *destination, size_t count) {
  uint8_t *bytes = (uint8_t *)destination;
  if (bytes == NULL) return;
  for (size_t index = 0u; index < count; index += 1u) bytes[index] = 0u;
}

static void copy_bytes(void *destination, const void *source, size_t count) {
  volatile uint8_t *target = (volatile uint8_t *)destination;
  const volatile uint8_t *input = (const volatile uint8_t *)source;
  if (target == NULL || input == NULL) return;
  for (size_t index = 0u; index < count; index += 1u)
    target[index] = input[index];
}

static bool bytes_equal(const void *left, const void *right, size_t count) {
  const uint8_t *left_bytes = (const uint8_t *)left;
  const uint8_t *right_bytes = (const uint8_t *)right;
  if (left_bytes == NULL || right_bytes == NULL) return false;
  for (size_t index = 0u; index < count; index += 1u)
    if (left_bytes[index] != right_bytes[index]) return false;
  return true;
}

static bool memory_range_make(const void *pointer, size_t count,
                              size_t element_size,
                              w_seed_task_lifecycle0_memory_range *range) {
  if (range == NULL) return false;
  *range = (w_seed_task_lifecycle0_memory_range){0u, 0u, 0u};
  if (count == 0u) return true;
  if (pointer == NULL || element_size == 0u || count > SIZE_MAX / element_size)
    return false;
  const size_t bytes = count * element_size;
  const uintptr_t begin = (uintptr_t)pointer;
  if (bytes > (size_t)UINTPTR_MAX ||
      begin > UINTPTR_MAX - (uintptr_t)bytes)
    return false;
  *range = (w_seed_task_lifecycle0_memory_range){begin,
                                                  begin + (uintptr_t)bytes, 1u};
  return true;
}

static bool memory_ranges_overlap(w_seed_task_lifecycle0_memory_range left,
                                  w_seed_task_lifecycle0_memory_range right) {
  return left.active != 0u && right.active != 0u && left.begin < right.end &&
         right.begin < left.end;
}

static bool schema_valid(const char *schema, const char *expected,
                         size_t size) {
  return schema != NULL && expected != NULL && size != 0u &&
         bytes_equal(schema, expected, size);
}

static bool snapshot_empty(
    const w_seed_task_lifecycle0_cancellation_snapshot *snapshot) {
  return snapshot != NULL && snapshot->generation == 0u &&
         snapshot->request_sequence == 0u && snapshot->source_index == 0u &&
         snapshot->reason == W_SEED_TASK_LIFECYCLE0_CANCEL_REASON_NONE;
}

static bool snapshot_valid(
    const w_seed_task_lifecycle0_cancellation_snapshot *snapshot) {
  return snapshot != NULL && snapshot->generation != 0u &&
         snapshot->reason >=
             W_SEED_TASK_LIFECYCLE0_CANCEL_REASON_ERROR_FAIL_FAST &&
         snapshot->reason <= W_SEED_TASK_LIFECYCLE0_CANCEL_REASON_SUPERSEDED;
}

static bool outcome_valid(const w_seed_task_lifecycle0_outcome *outcome) {
  if (outcome == NULL) return false;
  switch (outcome->kind) {
    case W_SEED_TASK_LIFECYCLE0_OUTCOME_NONE:
      return outcome->success_value == 0 &&
             outcome->error_code == W_SEED_TASK_LIFECYCLE0_ERROR_NONE &&
             snapshot_empty(&outcome->canceled);
    case W_SEED_TASK_LIFECYCLE0_OUTCOME_SUCCESS:
      return outcome->error_code == W_SEED_TASK_LIFECYCLE0_ERROR_NONE &&
             snapshot_empty(&outcome->canceled);
    case W_SEED_TASK_LIFECYCLE0_OUTCOME_ERROR:
      return outcome->success_value == 0 &&
             outcome->error_code >= W_SEED_TASK_LIFECYCLE0_ERROR_FIRST &&
             outcome->error_code <= W_SEED_TASK_LIFECYCLE0_ERROR_LAST &&
             snapshot_empty(&outcome->canceled);
    case W_SEED_TASK_LIFECYCLE0_OUTCOME_CANCELED:
      return outcome->success_value == 0 &&
             outcome->error_code == W_SEED_TASK_LIFECYCLE0_ERROR_NONE &&
             snapshot_valid(&outcome->canceled);
    default:
      return false;
  }
}

static bool outcome_equal(const w_seed_task_lifecycle0_outcome *left,
                          const w_seed_task_lifecycle0_outcome *right) {
  return left != NULL && right != NULL && left->kind == right->kind &&
         left->success_value == right->success_value &&
         left->error_code == right->error_code &&
         left->canceled.generation == right->canceled.generation &&
         left->canceled.request_sequence == right->canceled.request_sequence &&
         left->canceled.source_index == right->canceled.source_index &&
         left->canceled.reason == right->canceled.reason;
}

static bool task_state_is_settling(
    w_seed_task_lifecycle0_task_state state) {
  return state == W_SEED_TASK_LIFECYCLE0_TASK_ACTIVE ||
         state == W_SEED_TASK_LIFECYCLE0_TASK_READY ||
         state == W_SEED_TASK_LIFECYCLE0_TASK_SUSPENDED;
}

static bool task_state_transition_valid(
    w_seed_task_lifecycle0_task_state current,
    w_seed_task_lifecycle0_task_state next) {
  switch (current) {
    case W_SEED_TASK_LIFECYCLE0_TASK_UNINITIALIZED:
      return next == W_SEED_TASK_LIFECYCLE0_TASK_RESERVED;
    case W_SEED_TASK_LIFECYCLE0_TASK_RESERVED:
      return next == W_SEED_TASK_LIFECYCLE0_TASK_PUBLISHED;
    case W_SEED_TASK_LIFECYCLE0_TASK_PUBLISHED:
      return next == W_SEED_TASK_LIFECYCLE0_TASK_ACTIVE ||
             next == W_SEED_TASK_LIFECYCLE0_TASK_READY;
    case W_SEED_TASK_LIFECYCLE0_TASK_ACTIVE:
      return next == W_SEED_TASK_LIFECYCLE0_TASK_READY ||
             next == W_SEED_TASK_LIFECYCLE0_TASK_SUSPENDED ||
             next == W_SEED_TASK_LIFECYCLE0_TASK_BODY_SETTLED;
    case W_SEED_TASK_LIFECYCLE0_TASK_READY:
      return next == W_SEED_TASK_LIFECYCLE0_TASK_ACTIVE ||
             next == W_SEED_TASK_LIFECYCLE0_TASK_SUSPENDED ||
             next == W_SEED_TASK_LIFECYCLE0_TASK_BODY_SETTLED;
    case W_SEED_TASK_LIFECYCLE0_TASK_SUSPENDED:
      return next == W_SEED_TASK_LIFECYCLE0_TASK_ACTIVE ||
             next == W_SEED_TASK_LIFECYCLE0_TASK_READY ||
             next == W_SEED_TASK_LIFECYCLE0_TASK_BODY_SETTLED;
    case W_SEED_TASK_LIFECYCLE0_TASK_BODY_SETTLED:
      return next == W_SEED_TASK_LIFECYCLE0_TASK_CLEANUP;
    case W_SEED_TASK_LIFECYCLE0_TASK_CLEANUP:
      return next == W_SEED_TASK_LIFECYCLE0_TASK_OUTCOME_COMMITTED;
    case W_SEED_TASK_LIFECYCLE0_TASK_OUTCOME_COMMITTED:
      return next == W_SEED_TASK_LIFECYCLE0_TASK_JOINED;
    case W_SEED_TASK_LIFECYCLE0_TASK_JOINED:
      return next == W_SEED_TASK_LIFECYCLE0_TASK_RELEASED;
    case W_SEED_TASK_LIFECYCLE0_TASK_RELEASED:
    default:
      return false;
  }
}

static bool scope_event_kind(w_seed_task_lifecycle0_event_kind kind) {
  switch (kind) {
    case W_SEED_TASK_LIFECYCLE0_EVENT_SCOPE_OPEN:
    case W_SEED_TASK_LIFECYCLE0_EVENT_SCOPE_CANCELLATION_REQUESTED:
    case W_SEED_TASK_LIFECYCLE0_EVENT_SCOPE_DRAINING:
    case W_SEED_TASK_LIFECYCLE0_EVENT_SCOPE_CHILDREN_DRAINED:
    case W_SEED_TASK_LIFECYCLE0_EVENT_SCOPE_OUTCOME_COMMITTED:
    case W_SEED_TASK_LIFECYCLE0_EVENT_SCOPE_JOINED:
    case W_SEED_TASK_LIFECYCLE0_EVENT_SCOPE_RETAINED:
      return true;
    default:
      return false;
  }
}

static bool task_event_kind(w_seed_task_lifecycle0_event_kind kind) {
  switch (kind) {
    case W_SEED_TASK_LIFECYCLE0_EVENT_TASK_RESERVED:
    case W_SEED_TASK_LIFECYCLE0_EVENT_TASK_PUBLISHED:
    case W_SEED_TASK_LIFECYCLE0_EVENT_TASK_ACTIVE:
    case W_SEED_TASK_LIFECYCLE0_EVENT_TASK_READY:
    case W_SEED_TASK_LIFECYCLE0_EVENT_TASK_SUSPENDED:
    case W_SEED_TASK_LIFECYCLE0_EVENT_TASK_BODY_SETTLED:
    case W_SEED_TASK_LIFECYCLE0_EVENT_TASK_CLEANUP:
    case W_SEED_TASK_LIFECYCLE0_EVENT_TASK_OUTCOME_COMMITTED:
    case W_SEED_TASK_LIFECYCLE0_EVENT_TASK_JOINED:
    case W_SEED_TASK_LIFECYCLE0_EVENT_TASK_RELEASED:
    case W_SEED_TASK_LIFECYCLE0_EVENT_TASK_CANCELLATION_REQUESTED:
      return true;
    default:
      return false;
  }
}

static bool transaction_shape_valid(
    const w_seed_task_lifecycle_view *transaction,
    w_seed_task_lifecycle0_status *status) {
  if (status == NULL) return false;
  *status = W_SEED_TASK_LIFECYCLE0_OK;
  if (transaction == NULL) {
    *status = W_SEED_TASK_LIFECYCLE0_INVALID_ARGUMENT;
    return false;
  }
  if (!schema_valid(transaction->schema, transaction->expected_schema,
                    transaction->schema_size) ||
      transaction->scope_generation == 0u) {
    *status = W_SEED_TASK_LIFECYCLE0_MALFORMED;
    return false;
  }
  if (transaction->task_count == 0u) {
    *status = W_SEED_TASK_LIFECYCLE0_MISSING;
    return false;
  }
  if (transaction->event_count == 0u) {
    *status = W_SEED_TASK_LIFECYCLE0_MISSING;
    return false;
  }

  if (transaction->tasks == NULL || transaction->events == NULL) {
    *status = W_SEED_TASK_LIFECYCLE0_INVALID_ARGUMENT;
    return false;
  }
  for (uint32_t task = 0u; task < transaction->task_count; task += 1u) {
    const w_seed_task_lifecycle0_task_spec *spec = &transaction->tasks[task];
    if (spec->generation == 0u || !outcome_valid(&spec->body_outcome) ||
        (spec->body_outcome.kind != W_SEED_TASK_LIFECYCLE0_OUTCOME_SUCCESS &&
         spec->body_outcome.kind != W_SEED_TASK_LIFECYCLE0_OUTCOME_ERROR)) {
      *status = W_SEED_TASK_LIFECYCLE0_INVALID_OUTCOME;
      return false;
    }
    for (uint32_t previous = 0u; previous < task; previous += 1u) {
      if (transaction->tasks[previous].task_id == spec->task_id ||
          transaction->tasks[previous].lexical_index == spec->lexical_index) {
        *status = W_SEED_TASK_LIFECYCLE0_DUPLICATE;
        return false;
      }
    }
    if (spec->lexical_index != task) {
      *status = W_SEED_TASK_LIFECYCLE0_REORDERED;
      return false;
    }
  }

  for (uint32_t event = 0u; event < transaction->event_count; event += 1u) {
    if (transaction->events[event].sequence != event ||
        transaction->events[event].kind ==
            W_SEED_TASK_LIFECYCLE0_EVENT_NONE) {
      *status = W_SEED_TASK_LIFECYCLE0_OUT_OF_ORDER;
      return false;
    }
  }
  return true;
}

static void state_initialize(
    const w_seed_task_lifecycle_view *transaction,
    w_seed_task_lifecycle0_state *state) {
  state->transition_count = 0u;
  state->primary_error_task = W_SEED_TASK_LIFECYCLE0_NONE;
  zero_bytes(&state->scope, sizeof(state->scope));
  zero_bytes(state->tasks,
             (size_t)transaction->task_count * sizeof(*state->tasks));
  state->scope.state = W_SEED_TASK_LIFECYCLE0_SCOPE_UNINITIALIZED;
  state->scope.primary_error_task = W_SEED_TASK_LIFECYCLE0_NONE;
  for (uint32_t task = 0u; task < transaction->task_count; task += 1u) {
    state->tasks[task].task_id = transaction->tasks[task].task_id;
    state->tasks[task].lexical_index = transaction->tasks[task].lexical_index;
    state->tasks[task].generation = transaction->tasks[task].generation;
    state->tasks[task].state =
        W_SEED_TASK_LIFECYCLE0_TASK_UNINITIALIZED;
  }
}

static w_seed_task_lifecycle0_status task_state_set(
    w_seed_task_lifecycle0_task_record *record,
    w_seed_task_lifecycle0_task_state next) {
  if (record == NULL) return W_SEED_TASK_LIFECYCLE0_INVALID_ARGUMENT;
  if (!task_state_transition_valid(record->state, next))
    return W_SEED_TASK_LIFECYCLE0_OUT_OF_ORDER;
  record->state = next;
  if (record->transition_count == UINT32_MAX)
    return W_SEED_TASK_LIFECYCLE0_CAPACITY;
  record->transition_count += 1u;
  return W_SEED_TASK_LIFECYCLE0_OK;
}

static bool task_event_payload_empty(
    const w_seed_task_lifecycle0_event *event) {
  return event != NULL && event->source_index == W_SEED_TASK_LIFECYCLE0_NONE &&
         event->reason == W_SEED_TASK_LIFECYCLE0_CANCEL_REASON_NONE &&
         outcome_valid(&event->outcome) &&
         event->outcome.kind == W_SEED_TASK_LIFECYCLE0_OUTCOME_NONE &&
         snapshot_empty(&event->snapshot);
}

static bool scope_event_payload_empty(
    const w_seed_task_lifecycle0_event *event) {
  return event != NULL && event->target_index == W_SEED_TASK_LIFECYCLE0_NONE &&
         task_event_payload_empty(event);
}

static w_seed_task_lifecycle0_status event_header_valid(
    const w_seed_task_lifecycle_view *transaction,
    const w_seed_task_lifecycle0_event *event,
    uint32_t event_index) {
  if (transaction == NULL || event == NULL)
    return W_SEED_TASK_LIFECYCLE0_INVALID_ARGUMENT;
  if (event->sequence != event_index)
    return W_SEED_TASK_LIFECYCLE0_OUT_OF_ORDER;
  if (scope_event_kind(event->kind)) {
    if (event->target_index != W_SEED_TASK_LIFECYCLE0_NONE ||
        event->generation != transaction->scope_generation)
      return event->target_index != W_SEED_TASK_LIFECYCLE0_NONE
                 ? W_SEED_TASK_LIFECYCLE0_ORPHAN
                 : W_SEED_TASK_LIFECYCLE0_STALE_GENERATION;
    return W_SEED_TASK_LIFECYCLE0_OK;
  }
  if (!task_event_kind(event->kind)) return W_SEED_TASK_LIFECYCLE0_MALFORMED;
  if (event->target_index >= transaction->task_count)
    return W_SEED_TASK_LIFECYCLE0_ORPHAN;
  if (event->generation !=
      transaction->tasks[event->target_index].generation)
    return W_SEED_TASK_LIFECYCLE0_STALE_GENERATION;
  return W_SEED_TASK_LIFECYCLE0_OK;
}

static w_seed_task_lifecycle0_status make_canceled_outcome(
    const w_seed_task_lifecycle0_cancellation_snapshot *snapshot,
    w_seed_task_lifecycle0_outcome *outcome) {
  if (!snapshot_valid(snapshot) || outcome == NULL)
    return W_SEED_TASK_LIFECYCLE0_INVALID_OUTCOME;
  zero_bytes(outcome, sizeof(*outcome));
  outcome->kind = W_SEED_TASK_LIFECYCLE0_OUTCOME_CANCELED;
  copy_bytes(&outcome->canceled, snapshot, sizeof(outcome->canceled));
  return W_SEED_TASK_LIFECYCLE0_OK;
}

static w_seed_task_lifecycle0_status handle_task_event(
    const w_seed_task_lifecycle_view *transaction,
    w_seed_task_lifecycle0_execution *execution,
    const w_seed_task_lifecycle0_event *event) {
  if (transaction == NULL || execution == NULL || execution->state == NULL ||
      event == NULL)
    return W_SEED_TASK_LIFECYCLE0_INVALID_ARGUMENT;
  const uint32_t index = event->target_index;
  w_seed_task_lifecycle0_task_record *record =
      &execution->state->tasks[index];
  const w_seed_task_lifecycle0_task_spec *spec = &transaction->tasks[index];
  const w_seed_task_lifecycle0_scope_state scope_state =
      execution->state->scope.state;

  if (scope_state == W_SEED_TASK_LIFECYCLE0_SCOPE_UNINITIALIZED ||
      scope_state == W_SEED_TASK_LIFECYCLE0_SCOPE_JOINED ||
      scope_state == W_SEED_TASK_LIFECYCLE0_SCOPE_RETAINED)
    return W_SEED_TASK_LIFECYCLE0_OUT_OF_ORDER;

  switch (event->kind) {
    case W_SEED_TASK_LIFECYCLE0_EVENT_TASK_RESERVED:
      if (!task_event_payload_empty(event))
        return W_SEED_TASK_LIFECYCLE0_MALFORMED;
      if (scope_state != W_SEED_TASK_LIFECYCLE0_SCOPE_OPEN &&
          scope_state !=
              W_SEED_TASK_LIFECYCLE0_SCOPE_CANCELLATION_REQUESTED)
        return W_SEED_TASK_LIFECYCLE0_OUT_OF_ORDER;
      if (index != execution->next_reserved)
        return W_SEED_TASK_LIFECYCLE0_REORDERED;
      if (record->state != W_SEED_TASK_LIFECYCLE0_TASK_UNINITIALIZED)
        return W_SEED_TASK_LIFECYCLE0_DUPLICATE;
      {
        const w_seed_task_lifecycle0_status status = task_state_set(
            record, W_SEED_TASK_LIFECYCLE0_TASK_RESERVED);
        if (status != W_SEED_TASK_LIFECYCLE0_OK) return status;
      }
      execution->next_reserved += 1u;
      return W_SEED_TASK_LIFECYCLE0_OK;

    case W_SEED_TASK_LIFECYCLE0_EVENT_TASK_PUBLISHED:
      if (!task_event_payload_empty(event))
        return W_SEED_TASK_LIFECYCLE0_MALFORMED;
      if (scope_state == W_SEED_TASK_LIFECYCLE0_SCOPE_DRAINING ||
          scope_state == W_SEED_TASK_LIFECYCLE0_SCOPE_CHILDREN_DRAINED)
        return W_SEED_TASK_LIFECYCLE0_OUT_OF_ORDER;
      if (index != execution->next_published)
        return W_SEED_TASK_LIFECYCLE0_REORDERED;
      if (execution->next_reserved != transaction->task_count)
        return W_SEED_TASK_LIFECYCLE0_MISSING;
      if (record->state != W_SEED_TASK_LIFECYCLE0_TASK_RESERVED)
        return record->state == W_SEED_TASK_LIFECYCLE0_TASK_PUBLISHED
                   ? W_SEED_TASK_LIFECYCLE0_DUPLICATE
                   : W_SEED_TASK_LIFECYCLE0_OUT_OF_ORDER;
      {
        const w_seed_task_lifecycle0_status status = task_state_set(
            record, W_SEED_TASK_LIFECYCLE0_TASK_PUBLISHED);
        if (status != W_SEED_TASK_LIFECYCLE0_OK) return status;
      }
      execution->next_published += 1u;
      return W_SEED_TASK_LIFECYCLE0_OK;

    case W_SEED_TASK_LIFECYCLE0_EVENT_TASK_ACTIVE:
    case W_SEED_TASK_LIFECYCLE0_EVENT_TASK_READY:
    case W_SEED_TASK_LIFECYCLE0_EVENT_TASK_SUSPENDED: {
      if (!task_event_payload_empty(event))
        return W_SEED_TASK_LIFECYCLE0_MALFORMED;
      if (scope_state == W_SEED_TASK_LIFECYCLE0_SCOPE_CHILDREN_DRAINED)
        return W_SEED_TASK_LIFECYCLE0_OUT_OF_ORDER;
      const w_seed_task_lifecycle0_task_state next =
          event->kind == W_SEED_TASK_LIFECYCLE0_EVENT_TASK_ACTIVE
              ? W_SEED_TASK_LIFECYCLE0_TASK_ACTIVE
          : event->kind == W_SEED_TASK_LIFECYCLE0_EVENT_TASK_READY
              ? W_SEED_TASK_LIFECYCLE0_TASK_READY
              : W_SEED_TASK_LIFECYCLE0_TASK_SUSPENDED;
      return task_state_set(record, next);
    }

    case W_SEED_TASK_LIFECYCLE0_EVENT_TASK_BODY_SETTLED: {
      if (event->source_index != W_SEED_TASK_LIFECYCLE0_NONE ||
          event->reason != W_SEED_TASK_LIFECYCLE0_CANCEL_REASON_NONE ||
          !snapshot_empty(&event->snapshot) ||
          !outcome_valid(&event->outcome) ||
          (event->outcome.kind != W_SEED_TASK_LIFECYCLE0_OUTCOME_SUCCESS &&
           event->outcome.kind != W_SEED_TASK_LIFECYCLE0_OUTCOME_ERROR))
        return W_SEED_TASK_LIFECYCLE0_MALFORMED;
      if (!task_state_is_settling(record->state))
        return W_SEED_TASK_LIFECYCLE0_OUT_OF_ORDER;
      if (!outcome_equal(&event->outcome, &spec->body_outcome))
        return W_SEED_TASK_LIFECYCLE0_INVALID_OUTCOME;
      {
        const w_seed_task_lifecycle0_status status = task_state_set(
            record, W_SEED_TASK_LIFECYCLE0_TASK_BODY_SETTLED);
        if (status != W_SEED_TASK_LIFECYCLE0_OK) return status;
      }
      copy_bytes(&record->candidate, &event->outcome,
                 sizeof(record->candidate));
      record->settled_before_cancellation =
          record->cancellation_requested == 0u ? 1u : 0u;
      if (record->cancellation_requested != 0u) {
        const w_seed_task_lifecycle0_status status = make_canceled_outcome(
            &record->cancellation, &record->outcome);
        if (status != W_SEED_TASK_LIFECYCLE0_OK) return status;
      } else {
        copy_bytes(&record->outcome, &record->candidate,
                   sizeof(record->outcome));
      }
      if (record->candidate.kind == W_SEED_TASK_LIFECYCLE0_OUTCOME_ERROR &&
          record->outcome.kind == W_SEED_TASK_LIFECYCLE0_OUTCOME_ERROR &&
          execution->state->scope.cancellation_requested == 0u &&
          execution->has_error_candidate == 0u) {
        execution->has_error_candidate = 1u;
      }
      return W_SEED_TASK_LIFECYCLE0_OK;
    }

    case W_SEED_TASK_LIFECYCLE0_EVENT_TASK_CLEANUP:
      if (!task_event_payload_empty(event))
        return W_SEED_TASK_LIFECYCLE0_MALFORMED;
      return task_state_set(record, W_SEED_TASK_LIFECYCLE0_TASK_CLEANUP);

    case W_SEED_TASK_LIFECYCLE0_EVENT_TASK_OUTCOME_COMMITTED:
      if (event->source_index != W_SEED_TASK_LIFECYCLE0_NONE ||
          event->reason != W_SEED_TASK_LIFECYCLE0_CANCEL_REASON_NONE ||
          !snapshot_empty(&event->snapshot) || !outcome_valid(&event->outcome))
        return W_SEED_TASK_LIFECYCLE0_MALFORMED;
      if (!outcome_equal(&event->outcome, &record->outcome))
        return W_SEED_TASK_LIFECYCLE0_INVALID_OUTCOME;
      return task_state_set(record,
                            W_SEED_TASK_LIFECYCLE0_TASK_OUTCOME_COMMITTED);

    case W_SEED_TASK_LIFECYCLE0_EVENT_TASK_JOINED:
      if (!task_event_payload_empty(event))
        return W_SEED_TASK_LIFECYCLE0_MALFORMED;
      if (index != execution->next_joined)
        return W_SEED_TASK_LIFECYCLE0_JOIN_ORDER;
      {
        const w_seed_task_lifecycle0_status status = task_state_set(
            record, W_SEED_TASK_LIFECYCLE0_TASK_JOINED);
        if (status != W_SEED_TASK_LIFECYCLE0_OK) return status;
      }
      execution->next_joined += 1u;
      return W_SEED_TASK_LIFECYCLE0_OK;

    case W_SEED_TASK_LIFECYCLE0_EVENT_TASK_RELEASED:
      if (!task_event_payload_empty(event))
        return W_SEED_TASK_LIFECYCLE0_MALFORMED;
      if (index != execution->next_released)
        return W_SEED_TASK_LIFECYCLE0_RELEASE_ORDER;
      {
        const w_seed_task_lifecycle0_status status = task_state_set(
            record, W_SEED_TASK_LIFECYCLE0_TASK_RELEASED);
        if (status != W_SEED_TASK_LIFECYCLE0_OK) return status;
      }
      execution->next_released += 1u;
      return W_SEED_TASK_LIFECYCLE0_OK;

    case W_SEED_TASK_LIFECYCLE0_EVENT_TASK_CANCELLATION_REQUESTED: {
      if (event->source_index != W_SEED_TASK_LIFECYCLE0_NONE ||
          event->reason < W_SEED_TASK_LIFECYCLE0_CANCEL_REASON_USER_REQUEST ||
          event->reason > W_SEED_TASK_LIFECYCLE0_CANCEL_REASON_SUPERSEDED ||
          !outcome_valid(&event->outcome))
        return W_SEED_TASK_LIFECYCLE0_MALFORMED;
      if (event->outcome.kind != W_SEED_TASK_LIFECYCLE0_OUTCOME_NONE)
        return W_SEED_TASK_LIFECYCLE0_MALFORMED;
      if (record->state == W_SEED_TASK_LIFECYCLE0_TASK_RELEASED)
        return W_SEED_TASK_LIFECYCLE0_OUT_OF_ORDER;
      if (record->cancellation_requested != 0u)
        return W_SEED_TASK_LIFECYCLE0_NONMONOTONIC_CANCELLATION;
      const w_seed_task_lifecycle0_cancellation_snapshot expected = {
          record->generation, event->sequence, W_SEED_TASK_LIFECYCLE0_NONE,
          event->reason};
      if (event->snapshot.generation != expected.generation ||
          event->snapshot.request_sequence != expected.request_sequence ||
          event->snapshot.source_index != expected.source_index ||
          event->snapshot.reason != expected.reason)
        return W_SEED_TASK_LIFECYCLE0_MALFORMED;
      record->cancellation_requested = 1u;
      copy_bytes(&record->cancellation, &expected,
                 sizeof(record->cancellation));
      return W_SEED_TASK_LIFECYCLE0_OK;
    }

    default:
      return W_SEED_TASK_LIFECYCLE0_MALFORMED;
  }
}

static bool i64_add_checked(int64_t left, int64_t right, int64_t *sum) {
  if (sum == NULL) return false;
  if ((right > 0 && left > INT64_MAX - right) ||
      (right < 0 && left < INT64_MIN - right))
    return false;
  *sum = left + right;
  return true;
}

static w_seed_task_lifecycle0_status derive_scope_outcome(
    const w_seed_task_lifecycle_view *transaction,
    w_seed_task_lifecycle0_state *state,
    w_seed_task_lifecycle0_outcome *outcome,
    uint32_t *primary_error_task) {
  if (transaction == NULL || state == NULL || outcome == NULL ||
      primary_error_task == NULL)
    return W_SEED_TASK_LIFECYCLE0_INVALID_ARGUMENT;
  zero_bytes(outcome, sizeof(*outcome));
  *primary_error_task = W_SEED_TASK_LIFECYCLE0_NONE;
  uint32_t canceled_task = W_SEED_TASK_LIFECYCLE0_NONE;
  int64_t sum = 0;
  for (uint32_t task = 0u; task < transaction->task_count; task += 1u) {
    const w_seed_task_lifecycle0_task_record *record = &state->tasks[task];
    if (record->state != W_SEED_TASK_LIFECYCLE0_TASK_RELEASED ||
        !outcome_valid(&record->outcome) ||
        record->outcome.kind == W_SEED_TASK_LIFECYCLE0_OUTCOME_NONE)
      return W_SEED_TASK_LIFECYCLE0_MISSING;
    if (record->outcome.kind == W_SEED_TASK_LIFECYCLE0_OUTCOME_ERROR &&
        (*primary_error_task == W_SEED_TASK_LIFECYCLE0_NONE ||
         task < *primary_error_task))
      *primary_error_task = task;
    if (record->outcome.kind == W_SEED_TASK_LIFECYCLE0_OUTCOME_CANCELED &&
        canceled_task == W_SEED_TASK_LIFECYCLE0_NONE)
      canceled_task = task;
    if (record->outcome.kind == W_SEED_TASK_LIFECYCLE0_OUTCOME_SUCCESS &&
        !i64_add_checked(sum, record->outcome.success_value, &sum))
      return W_SEED_TASK_LIFECYCLE0_INVALID_OUTCOME;
  }

  if (*primary_error_task != W_SEED_TASK_LIFECYCLE0_NONE) {
    outcome->kind = W_SEED_TASK_LIFECYCLE0_OUTCOME_ERROR;
    outcome->error_code =
        state->tasks[*primary_error_task].outcome.error_code;
    return W_SEED_TASK_LIFECYCLE0_OK;
  }
  if (state->scope.cancellation_requested != 0u) {
    return make_canceled_outcome(&state->scope.cancellation, outcome);
  }
  if (canceled_task != W_SEED_TASK_LIFECYCLE0_NONE) {
    return make_canceled_outcome(&state->tasks[canceled_task].cancellation,
                                 outcome);
  }
  outcome->kind = W_SEED_TASK_LIFECYCLE0_OUTCOME_SUCCESS;
  outcome->success_value = sum;
  return W_SEED_TASK_LIFECYCLE0_OK;
}

static w_seed_task_lifecycle0_status handle_scope_event(
    const w_seed_task_lifecycle_view *transaction,
    w_seed_task_lifecycle0_execution *execution,
    const w_seed_task_lifecycle0_event *event) {
  if (transaction == NULL || execution == NULL || execution->state == NULL ||
      event == NULL)
    return W_SEED_TASK_LIFECYCLE0_INVALID_ARGUMENT;
  w_seed_task_lifecycle0_scope_record *scope = &execution->state->scope;
  switch (event->kind) {
    case W_SEED_TASK_LIFECYCLE0_EVENT_SCOPE_OPEN:
      if (!scope_event_payload_empty(event))
        return W_SEED_TASK_LIFECYCLE0_MALFORMED;
      if (event->sequence != 0u) return W_SEED_TASK_LIFECYCLE0_OUT_OF_ORDER;
      if (scope->state != W_SEED_TASK_LIFECYCLE0_SCOPE_UNINITIALIZED)
        return W_SEED_TASK_LIFECYCLE0_DUPLICATE;
      scope->state = W_SEED_TASK_LIFECYCLE0_SCOPE_OPEN;
      scope->transition_count += 1u;
      return W_SEED_TASK_LIFECYCLE0_OK;

    case W_SEED_TASK_LIFECYCLE0_EVENT_SCOPE_CANCELLATION_REQUESTED: {
      if (event->target_index != W_SEED_TASK_LIFECYCLE0_NONE ||
          event->reason <
              W_SEED_TASK_LIFECYCLE0_CANCEL_REASON_ERROR_FAIL_FAST ||
          event->reason > W_SEED_TASK_LIFECYCLE0_CANCEL_REASON_SUPERSEDED ||
          !outcome_valid(&event->outcome) ||
          event->outcome.kind != W_SEED_TASK_LIFECYCLE0_OUTCOME_NONE)
        return W_SEED_TASK_LIFECYCLE0_MALFORMED;
      if (scope->state != W_SEED_TASK_LIFECYCLE0_SCOPE_OPEN)
        return scope->cancellation_requested != 0u
                   ? W_SEED_TASK_LIFECYCLE0_NONMONOTONIC_CANCELLATION
                   : W_SEED_TASK_LIFECYCLE0_OUT_OF_ORDER;
      if (event->source_index != W_SEED_TASK_LIFECYCLE0_NONE &&
          event->source_index >= transaction->task_count)
        return W_SEED_TASK_LIFECYCLE0_ORPHAN;
      if ((event->source_index == W_SEED_TASK_LIFECYCLE0_NONE &&
           event->reason ==
               W_SEED_TASK_LIFECYCLE0_CANCEL_REASON_ERROR_FAIL_FAST) ||
          (event->source_index != W_SEED_TASK_LIFECYCLE0_NONE &&
           event->reason !=
               W_SEED_TASK_LIFECYCLE0_CANCEL_REASON_ERROR_FAIL_FAST))
        return W_SEED_TASK_LIFECYCLE0_MALFORMED;
      if (execution->has_error_candidate != 0u &&
          event->source_index == W_SEED_TASK_LIFECYCLE0_NONE)
        return W_SEED_TASK_LIFECYCLE0_MALFORMED;
      if (event->source_index != W_SEED_TASK_LIFECYCLE0_NONE) {
        const w_seed_task_lifecycle0_task_record *source =
            &execution->state->tasks[event->source_index];
        if (source->state < W_SEED_TASK_LIFECYCLE0_TASK_BODY_SETTLED ||
            source->candidate.kind != W_SEED_TASK_LIFECYCLE0_OUTCOME_ERROR ||
            source->outcome.kind != W_SEED_TASK_LIFECYCLE0_OUTCOME_ERROR)
          return W_SEED_TASK_LIFECYCLE0_MALFORMED;
      }
      const w_seed_task_lifecycle0_cancellation_snapshot expected = {
          transaction->scope_generation, event->sequence, event->source_index,
          event->reason};
      if (event->snapshot.generation != expected.generation ||
          event->snapshot.request_sequence != expected.request_sequence ||
          event->snapshot.source_index != expected.source_index ||
          event->snapshot.reason != expected.reason)
        return W_SEED_TASK_LIFECYCLE0_MALFORMED;
      scope->state = W_SEED_TASK_LIFECYCLE0_SCOPE_CANCELLATION_REQUESTED;
      scope->cancellation_requested = 1u;
      copy_bytes(&scope->cancellation, &expected,
                 sizeof(scope->cancellation));
      scope->transition_count += 1u;
      for (uint32_t task = 0u; task < transaction->task_count; task += 1u) {
        w_seed_task_lifecycle0_task_record *record =
            &execution->state->tasks[task];
        if (record->state != W_SEED_TASK_LIFECYCLE0_TASK_RELEASED &&
            record->cancellation_requested == 0u) {
          record->cancellation_requested = 1u;
          copy_bytes(&record->cancellation, &expected,
                     sizeof(record->cancellation));
        }
      }
      return W_SEED_TASK_LIFECYCLE0_OK;
    }

    case W_SEED_TASK_LIFECYCLE0_EVENT_SCOPE_DRAINING:
      if (!scope_event_payload_empty(event))
        return W_SEED_TASK_LIFECYCLE0_MALFORMED;
      if (scope->state != W_SEED_TASK_LIFECYCLE0_SCOPE_OPEN &&
          scope->state !=
              W_SEED_TASK_LIFECYCLE0_SCOPE_CANCELLATION_REQUESTED)
        return W_SEED_TASK_LIFECYCLE0_OUT_OF_ORDER;
      if (execution->next_reserved != transaction->task_count ||
          execution->next_published != transaction->task_count)
        return W_SEED_TASK_LIFECYCLE0_MISSING;
      if (execution->has_error_candidate != 0u &&
          scope->cancellation_requested == 0u)
        return W_SEED_TASK_LIFECYCLE0_OUT_OF_ORDER;
      scope->state = W_SEED_TASK_LIFECYCLE0_SCOPE_DRAINING;
      scope->transition_count += 1u;
      return W_SEED_TASK_LIFECYCLE0_OK;

    case W_SEED_TASK_LIFECYCLE0_EVENT_SCOPE_CHILDREN_DRAINED:
      if (!scope_event_payload_empty(event))
        return W_SEED_TASK_LIFECYCLE0_MALFORMED;
      if (scope->state != W_SEED_TASK_LIFECYCLE0_SCOPE_DRAINING)
        return W_SEED_TASK_LIFECYCLE0_OUT_OF_ORDER;
      for (uint32_t task = 0u; task < transaction->task_count; task += 1u)
        if (execution->state->tasks[task].state !=
            W_SEED_TASK_LIFECYCLE0_TASK_RELEASED)
          return W_SEED_TASK_LIFECYCLE0_MISSING;
      scope->state = W_SEED_TASK_LIFECYCLE0_SCOPE_CHILDREN_DRAINED;
      scope->transition_count += 1u;
      return W_SEED_TASK_LIFECYCLE0_OK;

    case W_SEED_TASK_LIFECYCLE0_EVENT_SCOPE_OUTCOME_COMMITTED: {
      if (event->source_index != W_SEED_TASK_LIFECYCLE0_NONE ||
          event->reason != W_SEED_TASK_LIFECYCLE0_CANCEL_REASON_NONE ||
          !snapshot_empty(&event->snapshot) ||
          !outcome_valid(&event->outcome))
        return W_SEED_TASK_LIFECYCLE0_MALFORMED;
      if (scope->state != W_SEED_TASK_LIFECYCLE0_SCOPE_CHILDREN_DRAINED)
        return W_SEED_TASK_LIFECYCLE0_OUT_OF_ORDER;
      w_seed_task_lifecycle0_outcome expected;
      uint32_t primary_error_task;
      const w_seed_task_lifecycle0_status status = derive_scope_outcome(
          transaction, execution->state, &expected, &primary_error_task);
      if (status != W_SEED_TASK_LIFECYCLE0_OK) return status;
      if (!outcome_equal(&event->outcome, &expected))
        return W_SEED_TASK_LIFECYCLE0_INVALID_OUTCOME;
      copy_bytes(&scope->outcome, &event->outcome, sizeof(scope->outcome));
      scope->primary_error_task = primary_error_task;
      execution->state->primary_error_task = primary_error_task;
      scope->state = W_SEED_TASK_LIFECYCLE0_SCOPE_OUTCOME_COMMITTED;
      scope->transition_count += 1u;
      return W_SEED_TASK_LIFECYCLE0_OK;
    }

    case W_SEED_TASK_LIFECYCLE0_EVENT_SCOPE_JOINED:
      if (!scope_event_payload_empty(event))
        return W_SEED_TASK_LIFECYCLE0_MALFORMED;
      if (scope->state != W_SEED_TASK_LIFECYCLE0_SCOPE_OUTCOME_COMMITTED)
        return W_SEED_TASK_LIFECYCLE0_OUT_OF_ORDER;
      scope->state = W_SEED_TASK_LIFECYCLE0_SCOPE_JOINED;
      scope->transition_count += 1u;
      return W_SEED_TASK_LIFECYCLE0_OK;

    case W_SEED_TASK_LIFECYCLE0_EVENT_SCOPE_RETAINED:
      if (!scope_event_payload_empty(event))
        return W_SEED_TASK_LIFECYCLE0_MALFORMED;
      if (scope->state != W_SEED_TASK_LIFECYCLE0_SCOPE_OUTCOME_COMMITTED)
        return W_SEED_TASK_LIFECYCLE0_OUT_OF_ORDER;
      scope->state = W_SEED_TASK_LIFECYCLE0_SCOPE_RETAINED;
      scope->transition_count += 1u;
      return W_SEED_TASK_LIFECYCLE0_OK;

    default:
      return W_SEED_TASK_LIFECYCLE0_MALFORMED;
  }
}

static uint64_t digest_mix(uint64_t digest, uint8_t byte) {
  digest ^= (uint64_t)byte;
  digest *= UINT64_C(1099511628211);
  return digest;
}

static uint64_t digest_u32(uint64_t digest, uint32_t value) {
  for (uint32_t byte = 0u; byte < 4u; byte += 1u)
    digest = digest_mix(digest, (uint8_t)(value >> (byte * 8u)));
  return digest;
}

static uint64_t digest_u64(uint64_t digest, uint64_t value) {
  for (uint32_t byte = 0u; byte < 8u; byte += 1u)
    digest = digest_mix(digest, (uint8_t)(value >> (byte * 8u)));
  return digest;
}

static uint64_t digest_snapshot(
    uint64_t digest,
    const w_seed_task_lifecycle0_cancellation_snapshot *snapshot) {
  digest = digest_u32(digest, snapshot->generation);
  digest = digest_u32(digest, snapshot->request_sequence);
  digest = digest_u32(digest, snapshot->source_index);
  return digest_u32(digest, (uint32_t)snapshot->reason);
}

static uint64_t digest_outcome(
    uint64_t digest, const w_seed_task_lifecycle0_outcome *outcome) {
  digest = digest_u32(digest, (uint32_t)outcome->kind);
  digest = digest_u64(digest, (uint64_t)outcome->success_value);
  digest = digest_u32(digest, (uint32_t)outcome->error_code);
  return digest_snapshot(digest, &outcome->canceled);
}

static uint64_t digest_event(
    uint64_t digest, const w_seed_task_lifecycle0_event *event) {
  digest = digest_u32(digest, (uint32_t)event->kind);
  digest = digest_u32(digest, event->sequence);
  digest = digest_u32(digest, event->target_index);
  digest = digest_u32(digest, event->generation);
  digest = digest_u32(digest, event->source_index);
  digest = digest_u32(digest, (uint32_t)event->reason);
  digest = digest_outcome(digest, &event->outcome);
  return digest_snapshot(digest, &event->snapshot);
}

static uint64_t transaction_digest(
    const w_seed_task_lifecycle_view *transaction) {
  uint64_t digest = UINT64_C(1469598103934665603);
  for (size_t byte = 0u;
       byte < transaction->schema_size; byte += 1u)
    digest = digest_mix(digest, (uint8_t)transaction->expected_schema[byte]);
  digest = digest_u32(digest, transaction->scope_generation);
  digest = digest_u32(digest, transaction->task_count);
  for (uint32_t task = 0u; task < transaction->task_count; task += 1u) {
    const w_seed_task_lifecycle0_task_spec *spec = &transaction->tasks[task];
    digest = digest_u32(digest, spec->task_id);
    digest = digest_u32(digest, spec->lexical_index);
    digest = digest_u32(digest, spec->generation);
    digest = digest_outcome(digest, &spec->body_outcome);
  }
  digest = digest_u32(digest, transaction->event_count);
  for (uint32_t event = 0u; event < transaction->event_count; event += 1u)
    digest = digest_event(digest, &transaction->events[event]);
  return digest;
}

static w_seed_task_lifecycle0_status reduce_transaction(
    const w_seed_task_lifecycle_view *transaction,
    w_seed_task_lifecycle0_state *state) {
  w_seed_task_lifecycle0_status status;
  if (state == NULL || state->tasks == NULL ||
      state->task_capacity < transaction->task_count)
    return W_SEED_TASK_LIFECYCLE0_CAPACITY;
  if (!transaction_shape_valid(transaction, &status)) return status;
  state_initialize(transaction, state);
  w_seed_task_lifecycle0_execution execution = {
      state, 0u, 0u, 0u, 0u, 0u};

  for (uint32_t event_index = 0u; event_index < transaction->event_count;
       event_index += 1u) {
    const w_seed_task_lifecycle0_event *event =
        &transaction->events[event_index];
    status = event_header_valid(transaction, event, event_index);
    if (status != W_SEED_TASK_LIFECYCLE0_OK) return status;
    if (event->kind == W_SEED_TASK_LIFECYCLE0_EVENT_SCOPE_OPEN &&
        event_index != 0u)
      return W_SEED_TASK_LIFECYCLE0_OUT_OF_ORDER;
    status = scope_event_kind(event->kind)
                 ? handle_scope_event(transaction, &execution, event)
                 : handle_task_event(transaction, &execution, event);
    if (status != W_SEED_TASK_LIFECYCLE0_OK) return status;
    if (state->transition_count == UINT32_MAX)
      return W_SEED_TASK_LIFECYCLE0_CAPACITY;
    state->transition_count += 1u;
  }

  if (state->scope.state != W_SEED_TASK_LIFECYCLE0_SCOPE_JOINED &&
      state->scope.state != W_SEED_TASK_LIFECYCLE0_SCOPE_RETAINED)
    return W_SEED_TASK_LIFECYCLE0_MISSING;
  if (execution.next_reserved != transaction->task_count ||
      execution.next_published != transaction->task_count ||
      execution.next_joined != transaction->task_count ||
      execution.next_released != transaction->task_count)
    return W_SEED_TASK_LIFECYCLE0_MISSING;
  return W_SEED_TASK_LIFECYCLE0_OK;
}

static void result_from_state(
    const w_seed_task_lifecycle_view *transaction,
    const w_seed_task_lifecycle0_state *state,
    w_seed_task_lifecycle0_result *result) {
  zero_bytes(result, sizeof(*result));
  copy_bytes(result->schema, transaction->schema, sizeof(result->schema));
  result->scope_generation = transaction->scope_generation;
  result->task_count = transaction->task_count;
  result->event_count = transaction->event_count;
  result->transition_count = state->transition_count;
  result->primary_error_task = state->primary_error_task;
  copy_bytes(&result->scope, &state->scope, sizeof(result->scope));
  for (uint32_t task = 0u; task < transaction->task_count; task += 1u)
    copy_bytes(&result->tasks[task], &state->tasks[task],
               sizeof(result->tasks[task]));
  for (uint32_t event = 0u; event < transaction->event_count; event += 1u)
    copy_bytes(&result->trace[event], &transaction->events[event],
               sizeof(result->trace[event]));
  result->transaction_digest = transaction_digest(transaction);
}

static void measurement_from_state(
    const w_seed_task_lifecycle_view *transaction,
    const w_seed_task_lifecycle0_state *state,
    w_seed_task_lifecycle0_measurement *measurement) {
  zero_bytes(measurement, sizeof(*measurement));
  copy_bytes(measurement->schema, transaction->schema,
             sizeof(measurement->schema));
  measurement->scope_generation = transaction->scope_generation;
  measurement->task_count = transaction->task_count;
  measurement->event_count = transaction->event_count;
  measurement->transition_count = state->transition_count;
  measurement->primary_error_task = state->primary_error_task;
  copy_bytes(&measurement->scope, &state->scope, sizeof(measurement->scope));
  for (uint32_t task = 0u; task < transaction->task_count; task += 1u)
    copy_bytes(&measurement->tasks[task], &state->tasks[task],
               sizeof(measurement->tasks[task]));
  measurement->transaction_digest = transaction_digest(transaction);
}

static bool scope_record_equal(
    const w_seed_task_lifecycle0_scope_record *left,
    const w_seed_task_lifecycle0_scope_record *right) {
  return left != NULL && right != NULL && left->state == right->state &&
         left->cancellation_requested == right->cancellation_requested &&
         left->reserved[0] == right->reserved[0] &&
         left->reserved[1] == right->reserved[1] &&
         left->reserved[2] == right->reserved[2] &&
         left->transition_count == right->transition_count &&
         left->primary_error_task == right->primary_error_task &&
         left->cancellation.generation == right->cancellation.generation &&
         left->cancellation.request_sequence ==
             right->cancellation.request_sequence &&
         left->cancellation.source_index == right->cancellation.source_index &&
         left->cancellation.reason == right->cancellation.reason &&
         outcome_equal(&left->outcome, &right->outcome);
}

static bool task_record_equal(
    const w_seed_task_lifecycle0_task_record *left,
    const w_seed_task_lifecycle0_task_record *right) {
  return left != NULL && right != NULL && left->task_id == right->task_id &&
         left->lexical_index == right->lexical_index &&
         left->generation == right->generation && left->state == right->state &&
         left->cancellation_requested == right->cancellation_requested &&
         left->settled_before_cancellation ==
             right->settled_before_cancellation &&
         left->reserved == right->reserved &&
         left->transition_count == right->transition_count &&
         left->cancellation.generation == right->cancellation.generation &&
         left->cancellation.request_sequence ==
             right->cancellation.request_sequence &&
         left->cancellation.source_index == right->cancellation.source_index &&
         left->cancellation.reason == right->cancellation.reason &&
         outcome_equal(&left->candidate, &right->candidate) &&
         outcome_equal(&left->outcome, &right->outcome);
}

static bool event_equal(const w_seed_task_lifecycle0_event *left,
                        const w_seed_task_lifecycle0_event *right) {
  return left != NULL && right != NULL && left->kind == right->kind &&
         left->sequence == right->sequence &&
         left->target_index == right->target_index &&
         left->generation == right->generation &&
         left->source_index == right->source_index &&
         left->reason == right->reason &&
         outcome_equal(&left->outcome, &right->outcome) &&
         left->snapshot.generation == right->snapshot.generation &&
         left->snapshot.request_sequence == right->snapshot.request_sequence &&
         left->snapshot.source_index == right->snapshot.source_index &&
         left->snapshot.reason == right->snapshot.reason;
}

static bool event_is_zero(const w_seed_task_lifecycle0_event *event) {
  return event != NULL && event->kind == W_SEED_TASK_LIFECYCLE0_EVENT_NONE &&
         event->sequence == 0u && event->target_index == 0u &&
         event->generation == 0u && event->source_index == 0u &&
         event->reason == W_SEED_TASK_LIFECYCLE0_CANCEL_REASON_NONE &&
         outcome_valid(&event->outcome) &&
         event->outcome.kind == W_SEED_TASK_LIFECYCLE0_OUTCOME_NONE &&
         event->snapshot.generation == 0u &&
         event->snapshot.request_sequence == 0u &&
         event->snapshot.source_index == 0u &&
         event->snapshot.reason == W_SEED_TASK_LIFECYCLE0_CANCEL_REASON_NONE;
}

static bool result_matches_state(
    const w_seed_task_lifecycle_view *transaction,
    const w_seed_task_lifecycle0_state *state,
    const w_seed_task_lifecycle0_result *result) {
  if (transaction == NULL || state == NULL || result == NULL ||
      !schema_valid(result->schema, transaction->expected_schema,
                    transaction->schema_size) ||
      !bytes_equal(result->schema, transaction->schema, sizeof(result->schema)) ||
      result->scope_generation != transaction->scope_generation ||
      result->task_count != transaction->task_count ||
      result->event_count != transaction->event_count ||
      result->transition_count != state->transition_count ||
      result->primary_error_task != state->primary_error_task ||
      !scope_record_equal(&result->scope, &state->scope))
    return false;
  for (uint32_t task = 0u; task < transaction->task_count; task += 1u)
    if (!task_record_equal(&result->tasks[task], &state->tasks[task])) return false;
  {
    const w_seed_task_lifecycle0_task_record empty = {0};
    for (uint32_t task = transaction->task_count;
         task < W_SEED_TASK_LIFECYCLE0_MAX_TASKS; task += 1u)
      if (!bytes_equal(&result->tasks[task], &empty, sizeof(empty))) return false;
  }
  for (uint32_t event = 0u; event < transaction->event_count; event += 1u)
    if (!event_equal(&result->trace[event], &transaction->events[event]))
      return false;
  for (uint32_t event = transaction->event_count;
       event < W_SEED_TASK_LIFECYCLE0_MAX_EVENTS; event += 1u)
    if (!event_is_zero(&result->trace[event])) return false;
  return result->transaction_digest == transaction_digest(transaction);
}

static bool measurement_matches_state(
    const w_seed_task_lifecycle_view *transaction,
    const w_seed_task_lifecycle0_state *state,
    const w_seed_task_lifecycle0_measurement *measurement) {
  if (transaction == NULL || state == NULL || measurement == NULL ||
      !schema_valid(measurement->schema, transaction->expected_schema,
                    transaction->schema_size) ||
      !bytes_equal(measurement->schema, transaction->schema,
                   sizeof(measurement->schema)) ||
      measurement->scope_generation != transaction->scope_generation ||
      measurement->task_count != transaction->task_count ||
      measurement->event_count != transaction->event_count ||
      measurement->transition_count != state->transition_count ||
      measurement->primary_error_task != state->primary_error_task ||
      !scope_record_equal(&measurement->scope, &state->scope))
    return false;
  for (uint32_t task = 0u; task < transaction->task_count; task += 1u)
    if (!task_record_equal(&measurement->tasks[task], &state->tasks[task]))
      return false;
  {
    const w_seed_task_lifecycle0_task_record empty = {0};
    for (uint32_t task = transaction->task_count;
         task < W_SEED_TASK_LIFECYCLE0_MAX_TASKS; task += 1u)
      if (!bytes_equal(&measurement->tasks[task], &empty, sizeof(empty)))
        return false;
  }
  return measurement->transaction_digest == transaction_digest(transaction);
}

static w_seed_task_lifecycle_view lifecycle0_view(
    const w_seed_task_lifecycle0_transaction *transaction) {
  return (w_seed_task_lifecycle_view){
      transaction == NULL ? NULL : transaction->schema,
      W_SEED_TASK_LIFECYCLE0_SCHEMA_VERSION,
      sizeof(W_SEED_TASK_LIFECYCLE0_SCHEMA_VERSION),
      transaction == NULL ? 0u : transaction->scope_generation,
      transaction == NULL ? 0u : transaction->task_count,
      transaction == NULL ? NULL : transaction->tasks,
      transaction == NULL ? 0u : transaction->event_count,
      transaction == NULL ? NULL : transaction->events};
}

static w_seed_task_lifecycle_view lifecycle1_view(
    const w_seed_task_lifecycle1_transaction *transaction) {
  return (w_seed_task_lifecycle_view){
      transaction == NULL ? NULL : transaction->schema,
      W_SEED_TASK_LIFECYCLE1_SCHEMA_VERSION,
      sizeof(W_SEED_TASK_LIFECYCLE1_SCHEMA_VERSION),
      transaction == NULL ? 0u : transaction->scope_generation,
      transaction == NULL ? 0u : transaction->task_count,
      transaction == NULL ? NULL : transaction->tasks,
      transaction == NULL ? 0u : transaction->event_count,
      transaction == NULL ? NULL : transaction->events};
}

static bool output_aliases_transaction(
    const w_seed_task_lifecycle0_transaction *transaction, const void *output,
    size_t output_size) {
  w_seed_task_lifecycle0_memory_range transaction_range;
  w_seed_task_lifecycle0_memory_range output_range;
  return !memory_range_make(transaction, 1u, sizeof(*transaction),
                            &transaction_range) ||
         !memory_range_make(output, 1u, output_size, &output_range) ||
         memory_ranges_overlap(transaction_range, output_range);
}

w_seed_task_lifecycle0_status w_seed_task_lifecycle0_run(
    const w_seed_task_lifecycle0_transaction *transaction,
    w_seed_task_lifecycle0_result *result) {
  if (transaction == NULL || result == NULL)
    return W_SEED_TASK_LIFECYCLE0_INVALID_ARGUMENT;
  if (output_aliases_transaction(transaction, result, sizeof(*result)))
    return W_SEED_TASK_LIFECYCLE0_ALIAS;
  if (transaction->task_count > W_SEED_TASK_LIFECYCLE0_MAX_TASKS ||
      transaction->event_count > W_SEED_TASK_LIFECYCLE0_MAX_EVENTS)
    return W_SEED_TASK_LIFECYCLE0_CAPACITY;
  w_seed_task_lifecycle0_task_record
      task_storage[W_SEED_TASK_LIFECYCLE0_MAX_TASKS] = {0};
  w_seed_task_lifecycle0_state state = {0u, W_SEED_TASK_LIFECYCLE0_NONE,
                                        {0}, task_storage,
                                        W_SEED_TASK_LIFECYCLE0_MAX_TASKS};
  const w_seed_task_lifecycle_view view = lifecycle0_view(transaction);
  const w_seed_task_lifecycle0_status status =
      reduce_transaction(&view, &state);
  if (status != W_SEED_TASK_LIFECYCLE0_OK) return status;
  result_from_state(&view, &state, result);
  return W_SEED_TASK_LIFECYCLE0_OK;
}

w_seed_task_lifecycle0_status w_seed_task_lifecycle0_measure(
    const w_seed_task_lifecycle0_transaction *transaction,
    w_seed_task_lifecycle0_measurement *measurement) {
  if (transaction == NULL || measurement == NULL)
    return W_SEED_TASK_LIFECYCLE0_INVALID_ARGUMENT;
  if (output_aliases_transaction(transaction, measurement, sizeof(*measurement)))
    return W_SEED_TASK_LIFECYCLE0_ALIAS;
  if (transaction->task_count > W_SEED_TASK_LIFECYCLE0_MAX_TASKS ||
      transaction->event_count > W_SEED_TASK_LIFECYCLE0_MAX_EVENTS)
    return W_SEED_TASK_LIFECYCLE0_CAPACITY;
  w_seed_task_lifecycle0_task_record
      task_storage[W_SEED_TASK_LIFECYCLE0_MAX_TASKS] = {0};
  w_seed_task_lifecycle0_state state = {0u, W_SEED_TASK_LIFECYCLE0_NONE,
                                        {0}, task_storage,
                                        W_SEED_TASK_LIFECYCLE0_MAX_TASKS};
  const w_seed_task_lifecycle_view view = lifecycle0_view(transaction);
  const w_seed_task_lifecycle0_status status =
      reduce_transaction(&view, &state);
  if (status != W_SEED_TASK_LIFECYCLE0_OK) return status;
  measurement_from_state(&view, &state, measurement);
  return W_SEED_TASK_LIFECYCLE0_OK;
}

bool w_seed_task_lifecycle0_verify(
  const w_seed_task_lifecycle0_transaction *transaction,
    const w_seed_task_lifecycle0_result *result) {
  if (transaction == NULL || result == NULL) return false;
  if (transaction->task_count > W_SEED_TASK_LIFECYCLE0_MAX_TASKS ||
      transaction->event_count > W_SEED_TASK_LIFECYCLE0_MAX_EVENTS)
    return false;
  w_seed_task_lifecycle0_task_record
      task_storage[W_SEED_TASK_LIFECYCLE0_MAX_TASKS] = {0};
  w_seed_task_lifecycle0_state state = {0u, W_SEED_TASK_LIFECYCLE0_NONE,
                                        {0}, task_storage,
                                        W_SEED_TASK_LIFECYCLE0_MAX_TASKS};
  const w_seed_task_lifecycle_view view = lifecycle0_view(transaction);
  return reduce_transaction(&view, &state) ==
             W_SEED_TASK_LIFECYCLE0_OK &&
         result_matches_state(&view, &state, result);
}

bool w_seed_task_lifecycle0_verify_measurement(
  const w_seed_task_lifecycle0_transaction *transaction,
    const w_seed_task_lifecycle0_measurement *measurement) {
  if (transaction == NULL || measurement == NULL) return false;
  if (transaction->task_count > W_SEED_TASK_LIFECYCLE0_MAX_TASKS ||
      transaction->event_count > W_SEED_TASK_LIFECYCLE0_MAX_EVENTS)
    return false;
  w_seed_task_lifecycle0_task_record
      task_storage[W_SEED_TASK_LIFECYCLE0_MAX_TASKS] = {0};
  w_seed_task_lifecycle0_state state = {0u, W_SEED_TASK_LIFECYCLE0_NONE,
                                        {0}, task_storage,
                                        W_SEED_TASK_LIFECYCLE0_MAX_TASKS};
  const w_seed_task_lifecycle_view view = lifecycle0_view(transaction);
  if (reduce_transaction(&view, &state) !=
      W_SEED_TASK_LIFECYCLE0_OK)
    return false;
  return measurement_matches_state(&view, &state, measurement);
}

static bool ranges_pairwise_disjoint(
    const w_seed_task_lifecycle0_memory_range *ranges, size_t count) {
  if (ranges == NULL) return false;
  for (size_t left = 0u; left < count; left += 1u)
    for (size_t right = left + 1u; right < count; right += 1u)
      if (memory_ranges_overlap(ranges[left], ranges[right])) return false;
  return true;
}

static bool lifecycle1_count_bytes_valid(uint32_t count,
                                         size_t element_size) {
  return element_size != 0u &&
         (uintmax_t)count <= (uintmax_t)SIZE_MAX / (uintmax_t)element_size;
}

static bool lifecycle1_ranges_valid(
    const w_seed_task_lifecycle1_transaction *transaction,
    w_seed_task_lifecycle1_workspace workspace,
    const w_seed_task_lifecycle1_output *output,
    const w_seed_task_lifecycle1_counts *counts,
    const w_seed_task_lifecycle1_result *result,
    const w_seed_task_lifecycle1_output *output_descriptor) {
  if (transaction == NULL || result == NULL) return false;
  w_seed_task_lifecycle0_memory_range ranges[9];
  size_t count = 0u;
#define ADD_RANGE(pointer, elements, type)                                      \
  do {                                                                          \
    if (!memory_range_make((pointer), (elements), sizeof(type), &ranges[count])) \
      return false;                                                              \
    count += 1u;                                                                \
  } while (0)
  ADD_RANGE(transaction, 1u, *transaction);
  ADD_RANGE(transaction->tasks, transaction->task_count,
            w_seed_task_lifecycle0_task_spec);
  ADD_RANGE(transaction->events, transaction->event_count,
            w_seed_task_lifecycle0_event);
  ADD_RANGE(workspace.tasks, transaction->task_count,
            w_seed_task_lifecycle0_task_record);
  if (output != NULL) {
    ADD_RANGE(output->tasks, transaction->task_count,
              w_seed_task_lifecycle0_task_record);
    ADD_RANGE(output->trace, transaction->event_count,
              w_seed_task_lifecycle0_event);
  }
  if (counts != NULL) ADD_RANGE(counts, 1u, *counts);
  ADD_RANGE(result, 1u, *result);
  if (output_descriptor != NULL)
    ADD_RANGE(output_descriptor, 1u, *output_descriptor);
#undef ADD_RANGE
  return ranges_pairwise_disjoint(ranges, count);
}

static void lifecycle1_result_from_state(
    const w_seed_task_lifecycle_view *transaction,
    const w_seed_task_lifecycle0_state *state,
    w_seed_task_lifecycle1_result *result) {
  zero_bytes(result, sizeof(*result));
  copy_bytes(result->schema, W_SEED_TASK_LIFECYCLE1_SCHEMA_VERSION,
             sizeof(result->schema));
  result->scope_generation = transaction->scope_generation;
  result->task_count = transaction->task_count;
  result->event_count = transaction->event_count;
  result->transition_count = state->transition_count;
  result->primary_error_task = state->primary_error_task;
  copy_bytes(&result->scope, &state->scope, sizeof(result->scope));
  result->transaction_digest = transaction_digest(transaction);
}

static bool lifecycle1_result_matches_state(
    const w_seed_task_lifecycle_view *transaction,
    const w_seed_task_lifecycle0_state *state,
    const w_seed_task_lifecycle1_result *result) {
  return transaction != NULL && state != NULL && result != NULL &&
         schema_valid(result->schema, W_SEED_TASK_LIFECYCLE1_SCHEMA_VERSION,
                      sizeof(W_SEED_TASK_LIFECYCLE1_SCHEMA_VERSION)) &&
         result->scope_generation == transaction->scope_generation &&
         result->task_count == transaction->task_count &&
         result->event_count == transaction->event_count &&
         result->transition_count == state->transition_count &&
         result->primary_error_task == state->primary_error_task &&
         scope_record_equal(&result->scope, &state->scope) &&
         result->transaction_digest == transaction_digest(transaction);
}

w_seed_task_lifecycle0_status w_seed_task_lifecycle1_measure(
    const w_seed_task_lifecycle1_transaction *transaction,
    w_seed_task_lifecycle1_workspace workspace,
    w_seed_task_lifecycle1_counts *counts,
    w_seed_task_lifecycle1_result *result) {
  if (transaction == NULL || counts == NULL || result == NULL)
    return W_SEED_TASK_LIFECYCLE0_INVALID_ARGUMENT;
  if ((size_t)transaction->task_count > workspace.task_capacity ||
      !lifecycle1_count_bytes_valid(
          transaction->task_count,
          sizeof(w_seed_task_lifecycle0_task_record)) ||
      !lifecycle1_count_bytes_valid(transaction->event_count,
                                    sizeof(w_seed_task_lifecycle0_event)))
    return W_SEED_TASK_LIFECYCLE0_CAPACITY;
  const w_seed_task_lifecycle_view view = lifecycle1_view(transaction);
  w_seed_task_lifecycle0_status status;
  if (!transaction_shape_valid(&view, &status)) return status;
  if (!lifecycle1_ranges_valid(transaction, workspace, NULL, counts, result,
                               NULL))
    return W_SEED_TASK_LIFECYCLE0_ALIAS;
  w_seed_task_lifecycle0_state state = {
      0u, W_SEED_TASK_LIFECYCLE0_NONE, {0}, workspace.tasks,
      workspace.task_capacity};
  status = reduce_transaction(&view, &state);
  if (status != W_SEED_TASK_LIFECYCLE0_OK) return status;
  const w_seed_task_lifecycle1_counts counts_candidate = {
      transaction->task_count, transaction->event_count};
  w_seed_task_lifecycle1_result result_candidate;
  lifecycle1_result_from_state(&view, &state, &result_candidate);
  *counts = counts_candidate;
  *result = result_candidate;
  return W_SEED_TASK_LIFECYCLE0_OK;
}

w_seed_task_lifecycle0_status w_seed_task_lifecycle1_run(
    const w_seed_task_lifecycle1_transaction *transaction,
    w_seed_task_lifecycle1_workspace workspace,
    w_seed_task_lifecycle1_output output,
    w_seed_task_lifecycle1_result *result) {
  if (transaction == NULL || result == NULL)
    return W_SEED_TASK_LIFECYCLE0_INVALID_ARGUMENT;
  if ((size_t)transaction->task_count > workspace.task_capacity ||
      (size_t)transaction->task_count > output.task_capacity ||
      (size_t)transaction->event_count > output.event_capacity ||
      !lifecycle1_count_bytes_valid(
          transaction->task_count,
          sizeof(w_seed_task_lifecycle0_task_record)) ||
      !lifecycle1_count_bytes_valid(transaction->event_count,
                                    sizeof(w_seed_task_lifecycle0_event)))
    return W_SEED_TASK_LIFECYCLE0_CAPACITY;
  const w_seed_task_lifecycle_view view = lifecycle1_view(transaction);
  w_seed_task_lifecycle0_status status;
  if (!transaction_shape_valid(&view, &status)) return status;
  if (!lifecycle1_ranges_valid(transaction, workspace, &output, NULL, result,
                               NULL))
    return W_SEED_TASK_LIFECYCLE0_ALIAS;
  w_seed_task_lifecycle0_state state = {
      0u, W_SEED_TASK_LIFECYCLE0_NONE, {0}, workspace.tasks,
      workspace.task_capacity};
  status = reduce_transaction(&view, &state);
  if (status != W_SEED_TASK_LIFECYCLE0_OK) return status;
  w_seed_task_lifecycle1_result result_candidate;
  lifecycle1_result_from_state(&view, &state, &result_candidate);
  for (uint32_t task = 0u; task < transaction->task_count; task += 1u)
    output.tasks[task] = state.tasks[task];
  for (uint32_t event = 0u; event < transaction->event_count; event += 1u)
    output.trace[event] = transaction->events[event];
  *result = result_candidate;
  return W_SEED_TASK_LIFECYCLE0_OK;
}

bool w_seed_task_lifecycle1_verify(
    const w_seed_task_lifecycle1_transaction *transaction,
    w_seed_task_lifecycle1_workspace workspace,
    const w_seed_task_lifecycle1_output *output,
    const w_seed_task_lifecycle1_result *result) {
  if (transaction == NULL || output == NULL || result == NULL ||
      (size_t)transaction->task_count > workspace.task_capacity ||
      (size_t)transaction->task_count > output->task_capacity ||
      (size_t)transaction->event_count > output->event_capacity ||
      !lifecycle1_count_bytes_valid(
          transaction->task_count,
          sizeof(w_seed_task_lifecycle0_task_record)) ||
      !lifecycle1_count_bytes_valid(transaction->event_count,
                                    sizeof(w_seed_task_lifecycle0_event)))
    return false;
  const w_seed_task_lifecycle_view view = lifecycle1_view(transaction);
  w_seed_task_lifecycle0_status status;
  if (!transaction_shape_valid(&view, &status) ||
      !lifecycle1_ranges_valid(transaction, workspace, output, NULL, result,
                               output))
    return false;
  w_seed_task_lifecycle0_state state = {
      0u, W_SEED_TASK_LIFECYCLE0_NONE, {0}, workspace.tasks,
      workspace.task_capacity};
  if (reduce_transaction(&view, &state) != W_SEED_TASK_LIFECYCLE0_OK ||
      !lifecycle1_result_matches_state(&view, &state, result))
    return false;
  for (uint32_t task = 0u; task < transaction->task_count; task += 1u)
    if (!task_record_equal(&output->tasks[task], &state.tasks[task]))
      return false;
  for (uint32_t event = 0u; event < transaction->event_count; event += 1u)
    if (!event_equal(&output->trace[event], &transaction->events[event]))
      return false;
  return true;
}
