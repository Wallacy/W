#include "w_seed_parallel_provider0.h"

#include "w_seed_parallel_provider0_platform.h"
#include "w_seed_scalar_evaluator0.h"
#include "w_seed_sha256.h"

#include <limits.h>
#include <string.h>

typedef struct {
  uintptr_t start;
  uintptr_t end;
  bool active;
} provider_range;

static bool range_make(const void *pointer, size_t count, size_t element_size,
                       provider_range *range) {
  if (range == NULL) return false;
  *range = (provider_range){0u, 0u, false};
  if (count == 0u) return true;
  if (pointer == NULL || element_size == 0u || count > SIZE_MAX / element_size)
    return false;
  const size_t bytes = count * element_size;
  const uintptr_t start = (uintptr_t)pointer;
  if (start > UINTPTR_MAX - bytes) return false;
  *range = (provider_range){start, start + bytes, true};
  return true;
}

static bool ranges_overlap(provider_range left, provider_range right) {
  return left.active && right.active && left.start < right.end &&
         right.start < left.end;
}

static bool add_input_range(provider_range *ranges, size_t capacity,
                            size_t *count, const void *pointer,
                            size_t element_count, size_t element_size) {
  if (ranges == NULL || count == NULL || *count >= capacity) return false;
  provider_range next;
  if (!range_make(pointer, element_count, element_size, &next)) return false;
  ranges[*count] = next;
  *count += 1u;
  return true;
}

static bool outputs_alias_input(
    const w_seed_parallel_provider0_input *input,
    const w_seed_parallel_provider0_outcomes *outcomes,
    const w_seed_parallel_provider0_receipt *receipt) {
  if (input == NULL || input->program == NULL || input->hir_result == NULL ||
      input->selection == NULL || outcomes == NULL || receipt == NULL)
    return true;
  provider_range output_ranges[2];
  if (!range_make(outcomes, 1u, sizeof(*outcomes), &output_ranges[0]) ||
      !range_make(receipt, 1u, sizeof(*receipt), &output_ranges[1]) ||
      ranges_overlap(output_ranges[0], output_ranges[1]))
    return true;

  provider_range inputs[40];
  size_t count = 0u;
#define W_PROVIDER0_ADD(pointer, number)                                      \
  do {                                                                         \
    if (!add_input_range(inputs, sizeof(inputs) / sizeof(inputs[0]), &count,   \
                         (pointer), (number), sizeof(*(pointer))))             \
      return true;                                                              \
  } while (0)
  W_PROVIDER0_ADD(input, 1u);
  W_PROVIDER0_ADD(input->program, 1u);
  W_PROVIDER0_ADD(input->hir_result, 1u);
  W_PROVIDER0_ADD(input->selection, 1u);
  W_PROVIDER0_ADD(input->invocation, 1u);
  const w_seed_hir0_program *program = input->program;
  W_PROVIDER0_ADD(program->modules, program->module_capacity);
  W_PROVIDER0_ADD(program->identities, program->identity_capacity);
  W_PROVIDER0_ADD(program->types, program->type_capacity);
  W_PROVIDER0_ADD(program->enums, program->enum_capacity);
  W_PROVIDER0_ADD(program->enum_cases, program->enum_case_capacity);
  W_PROVIDER0_ADD(program->enum_case_parameters,
                  program->enum_case_parameter_capacity);
  W_PROVIDER0_ADD(program->enum_subset_members,
                  program->enum_subset_member_capacity);
  W_PROVIDER0_ADD(program->functions, program->function_capacity);
  W_PROVIDER0_ADD(program->parameters, program->parameter_capacity);
  W_PROVIDER0_ADD(program->blocks, program->block_capacity);
  W_PROVIDER0_ADD(program->block_arguments, program->block_argument_capacity);
  W_PROVIDER0_ADD(program->edge_arguments, program->edge_argument_capacity);
  W_PROVIDER0_ADD(program->switch_edges, program->switch_edge_capacity);
  W_PROVIDER0_ADD(program->switch_captures, program->switch_capture_capacity);
  W_PROVIDER0_ADD(program->instructions, program->instruction_capacity);
  W_PROVIDER0_ADD(program->bindings, program->binding_capacity);
  W_PROVIDER0_ADD(program->calls, program->call_capacity);
  W_PROVIDER0_ADD(program->host_parameters, program->host_parameter_capacity);
  W_PROVIDER0_ADD(program->arguments, program->argument_capacity);
  W_PROVIDER0_ADD(program->enum_payloads, program->enum_payload_capacity);
  W_PROVIDER0_ADD(program->requirements, program->requirement_capacity);
  W_PROVIDER0_ADD(program->values, program->value_capacity);
  W_PROVIDER0_ADD(program->interpolation_segments,
                  program->interpolation_segment_capacity);
  W_PROVIDER0_ADD(program->terminators, program->terminator_capacity);
  W_PROVIDER0_ADD(program->entries, program->entry_capacity);
  W_PROVIDER0_ADD(program->text_bytes, program->text_byte_capacity);
  W_PROVIDER0_ADD(program->value_bytes, program->value_byte_capacity);
  W_PROVIDER0_ADD(program->receipt, program->receipt_capacity);
  W_PROVIDER0_ADD(program->external_modules,
                  program->external_module_capacity);
  W_PROVIDER0_ADD(program->external_symbols,
                  program->external_symbol_capacity);
#undef W_PROVIDER0_ADD
  for (size_t output = 0u; output < 2u; output += 1u)
    for (size_t index = 0u; index < count; index += 1u)
      if (ranges_overlap(output_ranges[output], inputs[index])) return true;
  return false;
}

typedef struct {
  const w_seed_hir0_program *program;
  const w_seed_parallel_invocation0_plan *invocation;
  uint32_t task_index;
} provider_invocation_context;

static bool provider_invoke_hir(void *raw, int64_t *value) {
  const provider_invocation_context *context =
      (const provider_invocation_context *)raw;
  if (context == NULL || value == NULL ||
      context->task_index >= context->invocation->task_count)
    return false;
  size_t budget = W_SEED_PARALLEL_INVOCATION0_STEP_BUDGET;
  return w_seed_scalar_evaluator0_evaluate_call(
      context->program,
      context->invocation->tasks[context->task_index].call_index, &budget,
      value);
}

static void sha_u32(w_seed_sha256_state *state, uint32_t value) {
  const uint8_t bytes[4] = {(uint8_t)value, (uint8_t)(value >> 8u),
                            (uint8_t)(value >> 16u),
                            (uint8_t)(value >> 24u)};
  w_seed_sha256_update(state, bytes, sizeof(bytes));
}

static void sha_i64(w_seed_sha256_state *state, int64_t value) {
  const uint64_t bits = (uint64_t)value;
  uint8_t bytes[8];
  for (size_t index = 0u; index < sizeof(bytes); index += 1u)
    bytes[index] = (uint8_t)(bits >> (index * 8u));
  w_seed_sha256_update(state, bytes, sizeof(bytes));
}

static void seal_outcomes(w_seed_parallel_provider0_outcomes *outcomes) {
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  w_seed_sha256_update(
      &state, (const uint8_t *)W_SEED_PARALLEL_PROVIDER0_SCHEMA_VERSION,
      sizeof(W_SEED_PARALLEL_PROVIDER0_SCHEMA_VERSION) - 1u);
  w_seed_sha256_update(&state, outcomes->hir_semantic_digest,
                       sizeof(outcomes->hir_semantic_digest));
  sha_u32(&state, outcomes->task_count);
  for (size_t index = 0u; index < outcomes->task_count; index += 1u) {
    sha_u32(&state, outcomes->function_indices[index]);
    sha_i64(&state, outcomes->values[index]);
  }
  w_seed_sha256_final(&state, outcomes->outcome_digest);
}

static bool outcomes_equal(const w_seed_parallel_provider0_outcomes *left,
                           const w_seed_parallel_provider0_outcomes *right) {
  if (left == NULL || right == NULL ||
      memcmp(left->schema, right->schema, sizeof(left->schema)) != 0 ||
      left->task_count != right->task_count ||
      memcmp(left->hir_semantic_digest, right->hir_semantic_digest,
             sizeof(left->hir_semantic_digest)) != 0 ||
      memcmp(left->outcome_digest, right->outcome_digest,
             sizeof(left->outcome_digest)) != 0)
    return false;
  for (size_t index = 0u; index < W_SEED_PARALLEL_PROVIDER0_MAX_TASKS;
       index += 1u)
    if (left->function_indices[index] != right->function_indices[index] ||
        left->values[index] != right->values[index])
      return false;
  return true;
}

bool w_seed_parallel_provider0_verify_outcomes(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection0 *selection,
    const w_seed_parallel_provider0_outcomes *outcomes) {
  if (program == NULL || hir_result == NULL || selection == NULL ||
      outcomes == NULL ||
      !w_seed_parallel_selection0_verify(program, hir_result, selection) ||
      outcomes->task_count != selection->task_count ||
      outcomes->task_count == 0u ||
      outcomes->task_count > W_SEED_PARALLEL_PROVIDER0_MAX_TASKS ||
      memcmp(outcomes->schema, W_SEED_PARALLEL_PROVIDER0_SCHEMA_VERSION,
             sizeof(outcomes->schema)) != 0 ||
      memcmp(outcomes->hir_semantic_digest, hir_result->semantic_digest,
             sizeof(outcomes->hir_semantic_digest)) != 0)
    return false;
  for (size_t index = 0u; index < outcomes->task_count; index += 1u)
    if (outcomes->function_indices[index] !=
        selection->task_function_indices[index])
      return false;
  for (size_t index = outcomes->task_count;
       index < W_SEED_PARALLEL_PROVIDER0_MAX_TASKS; index += 1u)
    if (outcomes->function_indices[index] != 0u || outcomes->values[index] != 0)
      return false;
  w_seed_parallel_provider0_outcomes expected = *outcomes;
  (void)memset(expected.outcome_digest, 0, sizeof(expected.outcome_digest));
  seal_outcomes(&expected);
  return outcomes_equal(outcomes, &expected);
}

w_seed_parallel_provider0_status w_seed_parallel_provider0_execute(
    const w_seed_parallel_provider0_input *input,
    w_seed_parallel_provider0_outcomes *outcomes,
    w_seed_parallel_provider0_receipt *receipt) {
  if (input == NULL || outcomes == NULL || receipt == NULL ||
      input->program == NULL || input->hir_result == NULL ||
      input->selection == NULL || input->invocation == NULL ||
      input->provider_capacity == 0u ||
      input->provider_capacity > W_SEED_PARALLEL_PROVIDER0_MAX_CAPACITY ||
      !w_seed_parallel_selection0_verify(
          input->program, input->hir_result, input->selection) ||
      !w_seed_parallel_invocation0_verify(
          input->program, input->hir_result, input->selection,
          input->invocation))
    return W_SEED_PARALLEL_PROVIDER0_INVALID;
  if (outputs_alias_input(input, outcomes, receipt))
    return W_SEED_PARALLEL_PROVIDER0_INVALID;

  int64_t values[W_SEED_PARALLEL_PROVIDER0_MAX_TASKS] = {0};
  provider_invocation_context
      contexts[W_SEED_PARALLEL_PROVIDER0_MAX_TASKS];
  w_seed_parallel_provider0_internal_job
      jobs[W_SEED_PARALLEL_PROVIDER0_MAX_TASKS];
  for (size_t index = 0u; index < input->selection->task_count; index += 1u) {
    contexts[index] = (provider_invocation_context){
        input->program, input->invocation, (uint32_t)index};
    jobs[index] = (w_seed_parallel_provider0_internal_job){
        provider_invoke_hir, &contexts[index]};
  }
  uint32_t started = 0u;
  uint32_t completed = 0u;
  uint32_t maximum_active_workers = 0u;
  w_seed_parallel_provider0_kind provider_kind =
      W_SEED_PARALLEL_PROVIDER0_KIND_NONE;
  const w_seed_parallel_provider0_platform_status platform_status =
      w_seed_parallel_provider0_platform_execute(
          jobs, input->selection->task_count, input->provider_capacity,
          values, &started, &completed, &maximum_active_workers,
          &provider_kind);
  if (platform_status == W_SEED_PARALLEL_PROVIDER0_PLATFORM_UNSUPPORTED)
    return W_SEED_PARALLEL_PROVIDER0_UNSUPPORTED;
  if (platform_status == W_SEED_PARALLEL_PROVIDER0_PLATFORM_TASK_FAILURE)
    return W_SEED_PARALLEL_PROVIDER0_TASK_FAILURE;
  if (platform_status != W_SEED_PARALLEL_PROVIDER0_PLATFORM_OK)
    return W_SEED_PARALLEL_PROVIDER0_PROVIDER_FAILURE;
  if (started != input->selection->task_count ||
      completed != input->selection->task_count ||
      maximum_active_workers == 0u ||
      maximum_active_workers > input->provider_capacity ||
      provider_kind == W_SEED_PARALLEL_PROVIDER0_KIND_NONE)
    return W_SEED_PARALLEL_PROVIDER0_PROVIDER_FAILURE;

  w_seed_parallel_provider0_outcomes outcome_candidate;
  (void)memset(&outcome_candidate, 0, sizeof(outcome_candidate));
  (void)memcpy(outcome_candidate.schema,
               W_SEED_PARALLEL_PROVIDER0_SCHEMA_VERSION,
               sizeof(outcome_candidate.schema));
  outcome_candidate.task_count = input->selection->task_count;
  (void)memcpy(outcome_candidate.hir_semantic_digest,
               input->hir_result->semantic_digest,
               sizeof(outcome_candidate.hir_semantic_digest));
  for (size_t index = 0u; index < input->selection->task_count; index += 1u) {
    outcome_candidate.function_indices[index] =
        input->selection->task_function_indices[index];
    outcome_candidate.values[index] = values[index];
  }
  seal_outcomes(&outcome_candidate);
  if (!w_seed_parallel_provider0_verify_outcomes(
          input->program, input->hir_result, input->selection,
          &outcome_candidate))
    return W_SEED_PARALLEL_PROVIDER0_PROVIDER_FAILURE;

  w_seed_parallel_provider0_receipt receipt_candidate;
  (void)memset(&receipt_candidate, 0, sizeof(receipt_candidate));
  receipt_candidate.provider_kind = provider_kind;
  receipt_candidate.provider_capacity = input->provider_capacity;
  receipt_candidate.task_count = input->selection->task_count;
  receipt_candidate.started_count = started;
  receipt_candidate.completed_count = completed;
  receipt_candidate.maximum_active_workers = maximum_active_workers;
  receipt_candidate.overlap_observed = maximum_active_workers > 1u;
  *outcomes = outcome_candidate;
  *receipt = receipt_candidate;
  return W_SEED_PARALLEL_PROVIDER0_OK;
}
