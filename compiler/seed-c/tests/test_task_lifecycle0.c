#include "w_seed_task_lifecycle0.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition)) {                                                         \
      fprintf(stderr, "check failed at line %d: %s\n", __LINE__, #condition); \
      return false;                                                            \
    }                                                                          \
  } while (0)

static w_seed_task_lifecycle0_outcome outcome_none(void) {
  return (w_seed_task_lifecycle0_outcome){0};
}

static w_seed_task_lifecycle0_outcome outcome_success(int64_t value) {
  w_seed_task_lifecycle0_outcome outcome = outcome_none();
  outcome.kind = W_SEED_TASK_LIFECYCLE0_OUTCOME_SUCCESS;
  outcome.success_value = value;
  return outcome;
}

static w_seed_task_lifecycle0_outcome outcome_error(
    w_seed_task_lifecycle0_error_code code) {
  w_seed_task_lifecycle0_outcome outcome = outcome_none();
  outcome.kind = W_SEED_TASK_LIFECYCLE0_OUTCOME_ERROR;
  outcome.error_code = code;
  return outcome;
}

static w_seed_task_lifecycle0_event event_make(
    w_seed_task_lifecycle0_event_kind kind, uint32_t sequence,
    uint32_t target_index, uint32_t generation) {
  w_seed_task_lifecycle0_event event = {0};
  event.kind = kind;
  event.sequence = sequence;
  event.target_index = target_index;
  event.generation = generation;
  event.source_index = W_SEED_TASK_LIFECYCLE0_NONE;
  return event;
}

static void append_event(w_seed_task_lifecycle0_transaction *transaction,
                         w_seed_task_lifecycle0_event event) {
  event.sequence = transaction->event_count;
  transaction->events[transaction->event_count] = event;
  transaction->event_count += 1u;
}

static void append_task(w_seed_task_lifecycle0_transaction *transaction,
                        w_seed_task_lifecycle0_event_kind kind,
                        uint32_t task, uint32_t generation) {
  append_event(transaction,
               event_make(kind, transaction->event_count, task, generation));
}

static void append_scope(w_seed_task_lifecycle0_transaction *transaction,
                         w_seed_task_lifecycle0_event_kind kind,
                         uint32_t generation) {
  append_event(transaction,
               event_make(kind, transaction->event_count,
                          W_SEED_TASK_LIFECYCLE0_NONE, generation));
}

static w_seed_task_lifecycle0_transaction transaction_start(
    uint32_t scope_generation, uint32_t task_count) {
  w_seed_task_lifecycle0_transaction transaction = {0};
  memcpy(transaction.schema, W_SEED_TASK_LIFECYCLE0_SCHEMA_VERSION,
         sizeof(transaction.schema));
  transaction.scope_generation = scope_generation;
  transaction.task_count = task_count;
  for (uint32_t task = 0u; task < W_SEED_TASK_LIFECYCLE0_MAX_TASKS; task += 1u) {
    transaction.tasks[task].task_id = task;
    transaction.tasks[task].lexical_index = task;
    transaction.tasks[task].generation = scope_generation + task + 1u;
    transaction.tasks[task].body_outcome = outcome_success(0);
  }
  append_scope(&transaction, W_SEED_TASK_LIFECYCLE0_EVENT_SCOPE_OPEN,
               scope_generation);
  return transaction;
}

static void append_reservations_and_publications(
    w_seed_task_lifecycle0_transaction *transaction) {
  for (uint32_t task = 0u; task < transaction->task_count; task += 1u)
    append_task(transaction, W_SEED_TASK_LIFECYCLE0_EVENT_TASK_RESERVED, task,
                transaction->tasks[task].generation);
  for (uint32_t task = 0u; task < transaction->task_count; task += 1u)
    append_task(transaction, W_SEED_TASK_LIFECYCLE0_EVENT_TASK_PUBLISHED, task,
                transaction->tasks[task].generation);
}

static void append_task_completion(
    w_seed_task_lifecycle0_transaction *transaction, uint32_t task,
    bool suspended_once) {
  const uint32_t generation = transaction->tasks[task].generation;
  append_task(transaction, W_SEED_TASK_LIFECYCLE0_EVENT_TASK_ACTIVE, task,
              generation);
  if (suspended_once) {
    append_task(transaction, W_SEED_TASK_LIFECYCLE0_EVENT_TASK_SUSPENDED, task,
                generation);
    append_task(transaction, W_SEED_TASK_LIFECYCLE0_EVENT_TASK_READY, task,
                generation);
    append_task(transaction, W_SEED_TASK_LIFECYCLE0_EVENT_TASK_ACTIVE, task,
                generation);
  }
  w_seed_task_lifecycle0_event settled = event_make(
      W_SEED_TASK_LIFECYCLE0_EVENT_TASK_BODY_SETTLED,
      transaction->event_count, task, generation);
  settled.outcome = transaction->tasks[task].body_outcome;
  append_event(transaction, settled);
  append_task(transaction, W_SEED_TASK_LIFECYCLE0_EVENT_TASK_CLEANUP, task,
              generation);
  w_seed_task_lifecycle0_event committed = event_make(
      W_SEED_TASK_LIFECYCLE0_EVENT_TASK_OUTCOME_COMMITTED,
      transaction->event_count, task, generation);
  committed.outcome = transaction->tasks[task].body_outcome;
  append_event(transaction, committed);
}

static void append_task_join_release(
    w_seed_task_lifecycle0_transaction *transaction, uint32_t task) {
  const uint32_t generation = transaction->tasks[task].generation;
  append_task(transaction, W_SEED_TASK_LIFECYCLE0_EVENT_TASK_JOINED, task,
              generation);
  append_task(transaction, W_SEED_TASK_LIFECYCLE0_EVENT_TASK_RELEASED, task,
              generation);
}

static w_seed_task_lifecycle0_transaction valid_one_success(void) {
  w_seed_task_lifecycle0_transaction transaction = transaction_start(41u, 1u);
  transaction.tasks[0].body_outcome = outcome_success(7);
  append_reservations_and_publications(&transaction);
  append_task_completion(&transaction, 0u, false);
  append_task_join_release(&transaction, 0u);
  append_scope(&transaction, W_SEED_TASK_LIFECYCLE0_EVENT_SCOPE_DRAINING,
               transaction.scope_generation);
  append_scope(&transaction,
               W_SEED_TASK_LIFECYCLE0_EVENT_SCOPE_CHILDREN_DRAINED,
               transaction.scope_generation);
  {
    w_seed_task_lifecycle0_event committed = event_make(
        W_SEED_TASK_LIFECYCLE0_EVENT_SCOPE_OUTCOME_COMMITTED,
        transaction.event_count, W_SEED_TASK_LIFECYCLE0_NONE,
        transaction.scope_generation);
    committed.outcome = outcome_success(7);
    append_event(&transaction, committed);
  }
  append_scope(&transaction, W_SEED_TASK_LIFECYCLE0_EVENT_SCOPE_JOINED,
               transaction.scope_generation);
  return transaction;
}

static w_seed_task_lifecycle0_transaction valid_two_success(void) {
  w_seed_task_lifecycle0_transaction transaction = transaction_start(45u, 2u);
  transaction.tasks[0].body_outcome = outcome_success(3);
  transaction.tasks[1].body_outcome = outcome_success(5);
  append_reservations_and_publications(&transaction);
  append_task_completion(&transaction, 0u, false);
  append_task_completion(&transaction, 1u, false);
  append_task_join_release(&transaction, 0u);
  append_task_join_release(&transaction, 1u);
  append_scope(&transaction, W_SEED_TASK_LIFECYCLE0_EVENT_SCOPE_DRAINING,
               transaction.scope_generation);
  append_scope(&transaction,
               W_SEED_TASK_LIFECYCLE0_EVENT_SCOPE_CHILDREN_DRAINED,
               transaction.scope_generation);
  {
    w_seed_task_lifecycle0_event committed = event_make(
        W_SEED_TASK_LIFECYCLE0_EVENT_SCOPE_OUTCOME_COMMITTED,
        transaction.event_count, W_SEED_TASK_LIFECYCLE0_NONE,
        transaction.scope_generation);
    committed.outcome = outcome_success(8);
    append_event(&transaction, committed);
  }
  append_scope(&transaction, W_SEED_TASK_LIFECYCLE0_EVENT_SCOPE_JOINED,
               transaction.scope_generation);
  return transaction;
}

static bool test_normal_lifecycle_and_measurement(void) {
  const w_seed_task_lifecycle0_transaction transaction = valid_one_success();
  w_seed_task_lifecycle0_result result = {0};
  w_seed_task_lifecycle0_measurement measurement = {0};
  CHECK(w_seed_task_lifecycle0_run(&transaction, &result) ==
        W_SEED_TASK_LIFECYCLE0_OK);
  CHECK(w_seed_task_lifecycle0_measure(&transaction, &measurement) ==
        W_SEED_TASK_LIFECYCLE0_OK);
  CHECK(w_seed_task_lifecycle0_verify(&transaction, &result));
  CHECK(w_seed_task_lifecycle0_verify_measurement(&transaction, &measurement));
  CHECK(result.scope.state == W_SEED_TASK_LIFECYCLE0_SCOPE_JOINED);
  CHECK(result.tasks[0].state == W_SEED_TASK_LIFECYCLE0_TASK_RELEASED);
  CHECK(result.tasks[0].outcome.kind ==
        W_SEED_TASK_LIFECYCLE0_OUTCOME_SUCCESS);
  CHECK(result.tasks[0].outcome.success_value == 7);
  CHECK(result.scope.outcome.kind == W_SEED_TASK_LIFECYCLE0_OUTCOME_SUCCESS);
  CHECK(result.scope.outcome.success_value == 7);
  CHECK(result.transition_count == transaction.event_count);
  CHECK(measurement.transaction_digest == result.transaction_digest);
  CHECK(memcmp(&measurement.scope, &result.scope, sizeof(measurement.scope)) ==
        0);
  return true;
}

static bool test_suspended_path_and_lexical_error_selection(void) {
  w_seed_task_lifecycle0_transaction transaction = transaction_start(50u, 2u);
  transaction.tasks[0].body_outcome =
      outcome_error(W_SEED_TASK_LIFECYCLE0_ERROR_SCOPE);
  transaction.tasks[1].body_outcome =
      outcome_error(W_SEED_TASK_LIFECYCLE0_ERROR_BODY);
  append_reservations_and_publications(&transaction);
  append_task_completion(&transaction, 1u, true);
  append_task_completion(&transaction, 0u, false);
  {
    w_seed_task_lifecycle0_event cancel = event_make(
        W_SEED_TASK_LIFECYCLE0_EVENT_SCOPE_CANCELLATION_REQUESTED,
        transaction.event_count, W_SEED_TASK_LIFECYCLE0_NONE,
        transaction.scope_generation);
    cancel.source_index = 1u;
    cancel.reason = W_SEED_TASK_LIFECYCLE0_CANCEL_REASON_ERROR_FAIL_FAST;
    cancel.snapshot.generation = transaction.scope_generation;
    cancel.snapshot.request_sequence = transaction.event_count;
    cancel.snapshot.source_index = cancel.source_index;
    cancel.snapshot.reason = cancel.reason;
    append_event(&transaction, cancel);
  }
  append_task_join_release(&transaction, 0u);
  append_task_join_release(&transaction, 1u);
  append_scope(&transaction, W_SEED_TASK_LIFECYCLE0_EVENT_SCOPE_DRAINING,
               transaction.scope_generation);
  append_scope(&transaction,
               W_SEED_TASK_LIFECYCLE0_EVENT_SCOPE_CHILDREN_DRAINED,
               transaction.scope_generation);
  {
    w_seed_task_lifecycle0_event committed = event_make(
        W_SEED_TASK_LIFECYCLE0_EVENT_SCOPE_OUTCOME_COMMITTED,
        transaction.event_count, W_SEED_TASK_LIFECYCLE0_NONE,
        transaction.scope_generation);
    committed.outcome =
        outcome_error(W_SEED_TASK_LIFECYCLE0_ERROR_SCOPE);
    append_event(&transaction, committed);
  }
  append_scope(&transaction, W_SEED_TASK_LIFECYCLE0_EVENT_SCOPE_RETAINED,
               transaction.scope_generation);

  w_seed_task_lifecycle0_result result = {0};
  CHECK(w_seed_task_lifecycle0_run(&transaction, &result) ==
        W_SEED_TASK_LIFECYCLE0_OK);
  CHECK(result.scope.state == W_SEED_TASK_LIFECYCLE0_SCOPE_RETAINED);
  CHECK(result.primary_error_task == 0u);
  CHECK(result.scope.primary_error_task == 0u);
  CHECK(result.scope.outcome.kind == W_SEED_TASK_LIFECYCLE0_OUTCOME_ERROR);
  CHECK(result.scope.outcome.error_code ==
        W_SEED_TASK_LIFECYCLE0_ERROR_SCOPE);
  CHECK(result.tasks[0].outcome.error_code ==
        W_SEED_TASK_LIFECYCLE0_ERROR_SCOPE);
  CHECK(result.tasks[1].outcome.error_code ==
        W_SEED_TASK_LIFECYCLE0_ERROR_BODY);
  CHECK(result.tasks[1].transition_count == 11u);
  return true;
}

static w_seed_task_lifecycle0_transaction valid_canceled(
    bool settle_before_cancel) {
  w_seed_task_lifecycle0_transaction transaction = transaction_start(60u, 1u);
  transaction.tasks[0].body_outcome = outcome_success(19);
  append_reservations_and_publications(&transaction);
  append_task(&transaction, W_SEED_TASK_LIFECYCLE0_EVENT_TASK_READY, 0u,
              transaction.tasks[0].generation);
  if (settle_before_cancel) {
    append_task(&transaction, W_SEED_TASK_LIFECYCLE0_EVENT_TASK_ACTIVE, 0u,
                transaction.tasks[0].generation);
    {
      w_seed_task_lifecycle0_event settled = event_make(
          W_SEED_TASK_LIFECYCLE0_EVENT_TASK_BODY_SETTLED,
          transaction.event_count, 0u, transaction.tasks[0].generation);
      settled.outcome = transaction.tasks[0].body_outcome;
      append_event(&transaction, settled);
    }
  }
  {
    w_seed_task_lifecycle0_event cancel = event_make(
        W_SEED_TASK_LIFECYCLE0_EVENT_SCOPE_CANCELLATION_REQUESTED,
        transaction.event_count, W_SEED_TASK_LIFECYCLE0_NONE,
        transaction.scope_generation);
    cancel.reason = W_SEED_TASK_LIFECYCLE0_CANCEL_REASON_USER_REQUEST;
    cancel.snapshot.generation = transaction.scope_generation;
    cancel.snapshot.request_sequence = transaction.event_count;
    cancel.snapshot.source_index = W_SEED_TASK_LIFECYCLE0_NONE;
    cancel.snapshot.reason = cancel.reason;
    append_event(&transaction, cancel);
  }
  if (!settle_before_cancel) {
    append_task(&transaction, W_SEED_TASK_LIFECYCLE0_EVENT_TASK_ACTIVE, 0u,
                transaction.tasks[0].generation);
    {
      w_seed_task_lifecycle0_event settled = event_make(
          W_SEED_TASK_LIFECYCLE0_EVENT_TASK_BODY_SETTLED,
          transaction.event_count, 0u, transaction.tasks[0].generation);
      settled.outcome = transaction.tasks[0].body_outcome;
      append_event(&transaction, settled);
    }
  }
  append_task(&transaction, W_SEED_TASK_LIFECYCLE0_EVENT_TASK_CLEANUP, 0u,
              transaction.tasks[0].generation);
  {
    w_seed_task_lifecycle0_event committed = event_make(
        W_SEED_TASK_LIFECYCLE0_EVENT_TASK_OUTCOME_COMMITTED,
        transaction.event_count, 0u, transaction.tasks[0].generation);
    if (settle_before_cancel)
      committed.outcome = transaction.tasks[0].body_outcome;
    else {
      committed.outcome.kind = W_SEED_TASK_LIFECYCLE0_OUTCOME_CANCELED;
    committed.outcome.canceled.generation = transaction.scope_generation;
    committed.outcome.canceled.request_sequence =
        settle_before_cancel ? 6u : 4u;
      committed.outcome.canceled.source_index =
          W_SEED_TASK_LIFECYCLE0_NONE;
      committed.outcome.canceled.reason =
          W_SEED_TASK_LIFECYCLE0_CANCEL_REASON_USER_REQUEST;
    }
    append_event(&transaction, committed);
  }
  append_task_join_release(&transaction, 0u);
  append_scope(&transaction, W_SEED_TASK_LIFECYCLE0_EVENT_SCOPE_DRAINING,
               transaction.scope_generation);
  append_scope(&transaction,
               W_SEED_TASK_LIFECYCLE0_EVENT_SCOPE_CHILDREN_DRAINED,
               transaction.scope_generation);
  {
    w_seed_task_lifecycle0_event committed = event_make(
        W_SEED_TASK_LIFECYCLE0_EVENT_SCOPE_OUTCOME_COMMITTED,
        transaction.event_count, W_SEED_TASK_LIFECYCLE0_NONE,
        transaction.scope_generation);
    committed.outcome.kind = W_SEED_TASK_LIFECYCLE0_OUTCOME_CANCELED;
    committed.outcome.canceled.generation = transaction.scope_generation;
    committed.outcome.canceled.request_sequence =
        settle_before_cancel ? 6u : 4u;
    committed.outcome.canceled.source_index = W_SEED_TASK_LIFECYCLE0_NONE;
    committed.outcome.canceled.reason =
        W_SEED_TASK_LIFECYCLE0_CANCEL_REASON_USER_REQUEST;
    append_event(&transaction, committed);
  }
  append_scope(&transaction, W_SEED_TASK_LIFECYCLE0_EVENT_SCOPE_JOINED,
               transaction.scope_generation);
  return transaction;
}

static bool test_cancellation_race_and_monotonicity(void) {
  w_seed_task_lifecycle0_transaction settled = valid_canceled(true);
  w_seed_task_lifecycle0_result settled_result = {0};
  CHECK(w_seed_task_lifecycle0_run(&settled, &settled_result) ==
        W_SEED_TASK_LIFECYCLE0_OK);
  CHECK(settled_result.tasks[0].settled_before_cancellation != 0u);
  CHECK(settled_result.tasks[0].outcome.kind ==
        W_SEED_TASK_LIFECYCLE0_OUTCOME_SUCCESS);

  w_seed_task_lifecycle0_transaction canceled = valid_canceled(false);
  w_seed_task_lifecycle0_result canceled_result = {0};
  CHECK(w_seed_task_lifecycle0_run(&canceled, &canceled_result) ==
        W_SEED_TASK_LIFECYCLE0_OK);
  CHECK(canceled_result.tasks[0].settled_before_cancellation == 0u);
  CHECK(canceled_result.tasks[0].outcome.kind ==
        W_SEED_TASK_LIFECYCLE0_OUTCOME_CANCELED);
  CHECK(canceled_result.tasks[0].outcome.canceled.request_sequence == 4u);

  w_seed_task_lifecycle0_transaction duplicate = canceled;
  w_seed_task_lifecycle0_event duplicate_cancel = duplicate.events[4];
  duplicate_cancel.sequence = duplicate.event_count;
  duplicate.events[duplicate.event_count] = duplicate_cancel;
  duplicate.event_count += 1u;
  w_seed_task_lifecycle0_result sentinel;
  memset(&sentinel, 0xA5, sizeof(sentinel));
  const w_seed_task_lifecycle0_result before = sentinel;
  CHECK(w_seed_task_lifecycle0_run(&duplicate, &sentinel) ==
        W_SEED_TASK_LIFECYCLE0_NONMONOTONIC_CANCELLATION);
  CHECK(memcmp(&sentinel, &before, sizeof(sentinel)) == 0);
  return true;
}

static bool test_adversarial_rejection_and_exact_snapshots(void) {
  const w_seed_task_lifecycle0_transaction valid = valid_one_success();
  w_seed_task_lifecycle0_result sentinel;
  memset(&sentinel, 0x5A, sizeof(sentinel));

  w_seed_task_lifecycle0_transaction stale = valid;
  stale.events[2].generation += 1u;
  const w_seed_task_lifecycle0_result stale_before = sentinel;
  CHECK(w_seed_task_lifecycle0_run(&stale, &sentinel) ==
        W_SEED_TASK_LIFECYCLE0_STALE_GENERATION);
  CHECK(memcmp(&sentinel, &stale_before, sizeof(sentinel)) == 0);

  w_seed_task_lifecycle0_transaction orphan = valid;
  orphan.events[2].target_index = orphan.task_count;
  CHECK(w_seed_task_lifecycle0_run(&orphan, &sentinel) ==
        W_SEED_TASK_LIFECYCLE0_ORPHAN);
  CHECK(memcmp(&sentinel, &stale_before, sizeof(sentinel)) == 0);

  w_seed_task_lifecycle0_transaction reordered = valid_two_success();
  reordered.events[1].target_index = 1u;
  reordered.events[1].generation = reordered.tasks[1].generation;
  w_seed_task_lifecycle0_status reordered_status =
      w_seed_task_lifecycle0_run(&reordered, &sentinel);
  CHECK(reordered_status == W_SEED_TASK_LIFECYCLE0_REORDERED);
  CHECK(memcmp(&sentinel, &stale_before, sizeof(sentinel)) == 0);

  w_seed_task_lifecycle0_transaction join_reordered = valid_two_success();
  for (uint32_t event = 0u; event < join_reordered.event_count; event += 1u)
    if (join_reordered.events[event].kind ==
        W_SEED_TASK_LIFECYCLE0_EVENT_TASK_JOINED) {
      join_reordered.events[event].target_index = 1u;
      join_reordered.events[event].generation =
          join_reordered.tasks[1].generation;
      break;
    }
  CHECK(w_seed_task_lifecycle0_run(&join_reordered, &sentinel) ==
        W_SEED_TASK_LIFECYCLE0_JOIN_ORDER);
  CHECK(memcmp(&sentinel, &stale_before, sizeof(sentinel)) == 0);

  w_seed_task_lifecycle0_transaction release_reordered = valid_two_success();
  for (uint32_t event = 0u; event < release_reordered.event_count; event += 1u)
    if (release_reordered.events[event].kind ==
        W_SEED_TASK_LIFECYCLE0_EVENT_TASK_RELEASED) {
      release_reordered.events[event].target_index = 1u;
      release_reordered.events[event].generation =
          release_reordered.tasks[1].generation;
      break;
    }
  CHECK(w_seed_task_lifecycle0_run(&release_reordered, &sentinel) ==
        W_SEED_TASK_LIFECYCLE0_RELEASE_ORDER);
  CHECK(memcmp(&sentinel, &stale_before, sizeof(sentinel)) == 0);

  w_seed_task_lifecycle0_transaction out_of_order = valid;
  out_of_order.events[3].kind = W_SEED_TASK_LIFECYCLE0_EVENT_TASK_CLEANUP;
  CHECK(w_seed_task_lifecycle0_run(&out_of_order, &sentinel) ==
        W_SEED_TASK_LIFECYCLE0_OUT_OF_ORDER);
  CHECK(memcmp(&sentinel, &stale_before, sizeof(sentinel)) == 0);

  w_seed_task_lifecycle0_transaction missing = valid;
  missing.event_count -= 1u;
  CHECK(w_seed_task_lifecycle0_run(&missing, &sentinel) ==
        W_SEED_TASK_LIFECYCLE0_MISSING);
  CHECK(memcmp(&sentinel, &stale_before, sizeof(sentinel)) == 0);

  w_seed_task_lifecycle0_transaction too_many = valid;
  too_many.task_count = W_SEED_TASK_LIFECYCLE0_MAX_TASKS + 1u;
  CHECK(w_seed_task_lifecycle0_run(&too_many, &sentinel) ==
        W_SEED_TASK_LIFECYCLE0_CAPACITY);
  CHECK(memcmp(&sentinel, &stale_before, sizeof(sentinel)) == 0);

  w_seed_task_lifecycle0_result result = {0};
  CHECK(w_seed_task_lifecycle0_run(&valid, &result) ==
        W_SEED_TASK_LIFECYCLE0_OK);
  w_seed_task_lifecycle0_result forged = result;
  forged.tasks[0].outcome.success_value += 1;
  CHECK(!w_seed_task_lifecycle0_verify(&valid, &forged));
  forged = result;
  forged.trace[4].outcome.success_value += 1;
  CHECK(!w_seed_task_lifecycle0_verify(&valid, &forged));

  w_seed_task_lifecycle0_transaction malformed = valid;
  malformed.tasks[0].body_outcome.kind =
      W_SEED_TASK_LIFECYCLE0_OUTCOME_CANCELED;
  CHECK(w_seed_task_lifecycle0_run(&malformed, &sentinel) ==
        W_SEED_TASK_LIFECYCLE0_INVALID_OUTCOME);
  CHECK(memcmp(&sentinel, &stale_before, sizeof(sentinel)) == 0);

  w_seed_task_lifecycle0_measurement measurement;
  memset(&measurement, 0xC3, sizeof(measurement));
  const w_seed_task_lifecycle0_measurement measurement_before = measurement;
  CHECK(w_seed_task_lifecycle0_measure(&out_of_order, &measurement) ==
        W_SEED_TASK_LIFECYCLE0_OUT_OF_ORDER);
  CHECK(memcmp(&measurement, &measurement_before, sizeof(measurement)) == 0);

  CHECK(w_seed_task_lifecycle0_run(
            &valid, (w_seed_task_lifecycle0_result *)(void *)&valid) ==
        W_SEED_TASK_LIFECYCLE0_ALIAS);
  return true;
}

int main(void) {
  const bool ok = test_normal_lifecycle_and_measurement() &&
                  test_suspended_path_and_lexical_error_selection() &&
                  test_cancellation_race_and_monotonicity() &&
                  test_adversarial_rejection_and_exact_snapshots();
  if (!ok) return 1;
  puts("task_lifecycle0 tests: ok");
  return 0;
}
