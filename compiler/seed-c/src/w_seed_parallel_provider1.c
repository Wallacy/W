#include "w_seed_parallel_provider1.h"

#include "w_seed_parallel_provider0_platform.h"
#include "w_seed_scalar_evaluator0.h"
#include "w_seed_sha256.h"

#include <limits.h>
#include <string.h>

typedef struct {
  uintptr_t start;
  uintptr_t end;
  bool active;
} provider1_range;

enum { W_PROVIDER1_INPUT_RANGE_CAPACITY = 40u };

static bool range_make(const void *pointer, size_t count, size_t element_size,
                       provider1_range *range) {
  if (range == NULL) return false;
  *range = (provider1_range){0u, 0u, false};
  if (count == 0u) return true;
  if (pointer == NULL || element_size == 0u || count > SIZE_MAX / element_size)
    return false;
  const size_t bytes = count * element_size;
  const uintptr_t start = (uintptr_t)pointer;
  if (start > UINTPTR_MAX - bytes) return false;
  *range = (provider1_range){start, start + bytes, true};
  return true;
}

static bool ranges_overlap(provider1_range left, provider1_range right) {
  return left.active && right.active && left.start < right.end &&
         right.start < left.end;
}

static bool add_range(provider1_range *ranges, size_t capacity, size_t *count,
                      const void *pointer, size_t elements,
                      size_t element_size) {
  if (ranges == NULL || count == NULL || *count >= capacity) return false;
  if (!range_make(pointer, elements, element_size, &ranges[*count]))
    return false;
  *count += 1u;
  return true;
}

static bool append_hir_ranges(const w_seed_hir0_program *program,
                              provider1_range *ranges, size_t capacity,
                              size_t *count) {
  if (program == NULL || ranges == NULL || count == NULL) return false;
#define W_PROVIDER1_ADD(pointer, number)                                      \
  do {                                                                         \
    if (!add_range(ranges, capacity, count, (pointer), (number),              \
                   sizeof(*(pointer))))                                       \
      return false;                                                            \
  } while (0)
  W_PROVIDER1_ADD(program, 1u);
  W_PROVIDER1_ADD(program->modules, program->module_capacity);
  W_PROVIDER1_ADD(program->identities, program->identity_capacity);
  W_PROVIDER1_ADD(program->types, program->type_capacity);
  W_PROVIDER1_ADD(program->enums, program->enum_capacity);
  W_PROVIDER1_ADD(program->enum_cases, program->enum_case_capacity);
  W_PROVIDER1_ADD(program->enum_case_parameters,
                  program->enum_case_parameter_capacity);
  W_PROVIDER1_ADD(program->enum_subset_members,
                  program->enum_subset_member_capacity);
  W_PROVIDER1_ADD(program->functions, program->function_capacity);
  W_PROVIDER1_ADD(program->parameters, program->parameter_capacity);
  W_PROVIDER1_ADD(program->blocks, program->block_capacity);
  W_PROVIDER1_ADD(program->block_arguments, program->block_argument_capacity);
  W_PROVIDER1_ADD(program->edge_arguments, program->edge_argument_capacity);
  W_PROVIDER1_ADD(program->switch_edges, program->switch_edge_capacity);
  W_PROVIDER1_ADD(program->switch_captures, program->switch_capture_capacity);
  W_PROVIDER1_ADD(program->instructions, program->instruction_capacity);
  W_PROVIDER1_ADD(program->bindings, program->binding_capacity);
  W_PROVIDER1_ADD(program->calls, program->call_capacity);
  W_PROVIDER1_ADD(program->host_parameters, program->host_parameter_capacity);
  W_PROVIDER1_ADD(program->arguments, program->argument_capacity);
  W_PROVIDER1_ADD(program->enum_payloads, program->enum_payload_capacity);
  W_PROVIDER1_ADD(program->requirements, program->requirement_capacity);
  W_PROVIDER1_ADD(program->values, program->value_capacity);
  W_PROVIDER1_ADD(program->interpolation_segments,
                  program->interpolation_segment_capacity);
  W_PROVIDER1_ADD(program->terminators, program->terminator_capacity);
  W_PROVIDER1_ADD(program->entries, program->entry_capacity);
  W_PROVIDER1_ADD(program->text_bytes, program->text_byte_capacity);
  W_PROVIDER1_ADD(program->value_bytes, program->value_byte_capacity);
  W_PROVIDER1_ADD(program->receipt, program->receipt_capacity);
  W_PROVIDER1_ADD(program->external_modules, program->external_module_capacity);
  W_PROVIDER1_ADD(program->external_symbols, program->external_symbol_capacity);
#undef W_PROVIDER1_ADD
  return true;
}

static bool append_input_ranges(const w_seed_parallel_provider1_input *input,
                                provider1_range *ranges, size_t capacity,
                                size_t *count, bool include_descriptor) {
  if (input == NULL || ranges == NULL || count == NULL) return false;
  if (include_descriptor &&
      !add_range(ranges, capacity, count, input, 1u, sizeof(*input)))
    return false;
  return add_range(ranges, capacity, count, input->hir_result, 1u,
                   sizeof(*input->hir_result)) &&
         append_hir_ranges(input->hir_program, ranges, capacity, count) &&
         add_range(ranges, capacity, count, input->selection, 1u,
                   sizeof(*input->selection)) &&
         add_range(ranges, capacity, count, input->selection->tasks,
                   input->selection->task_capacity,
                   sizeof(*input->selection->tasks)) &&
         add_range(ranges, capacity, count, input->selection_result, 1u,
                   sizeof(*input->selection_result)) &&
         add_range(ranges, capacity, count, input->invocation, 1u,
                   sizeof(*input->invocation)) &&
         add_range(ranges, capacity, count, input->invocation->tasks,
                   input->invocation->task_capacity,
                   sizeof(*input->invocation->tasks)) &&
         add_range(ranges, capacity, count, input->invocation->arguments,
                   input->invocation->argument_capacity,
                   sizeof(*input->invocation->arguments)) &&
         add_range(ranges, capacity, count, input->invocation_result, 1u,
                   sizeof(*input->invocation_result));
}

static bool input_valid(const w_seed_parallel_provider1_input *input) {
  return input != NULL && input->hir_program != NULL &&
         input->hir_result != NULL && input->selection != NULL &&
         input->selection_result != NULL && input->invocation != NULL &&
         input->invocation_result != NULL &&
         (input->provider_capacity == 1u || input->provider_capacity == 2u) &&
         input->selection->task_count != 0u &&
         input->selection->task_count <= UINT32_MAX &&
         w_seed_parallel_invocation1_verify(
             input->hir_program, input->hir_result, input->selection,
             input->selection_result, input->invocation,
             input->invocation_result);
}

static bool outputs_alias_inputs(
    const w_seed_parallel_provider1_input *input,
    const w_seed_parallel_provider1_output *output,
    const w_seed_parallel_provider1_result *result,
    const w_seed_parallel_provider1_receipt *receipt) {
  if (input == NULL || output == NULL || result == NULL || receipt == NULL)
    return true;
  provider1_range outputs[5];
  if (!range_make(output, 1u, sizeof(*output), &outputs[0]) ||
      !range_make(output->outcomes, output->outcome_capacity,
                  sizeof(*output->outcomes), &outputs[1]) ||
      !range_make(output->workspace_values, output->workspace_value_capacity,
                  sizeof(*output->workspace_values), &outputs[2]) ||
      !range_make(result, 1u, sizeof(*result), &outputs[3]) ||
      !range_make(receipt, 1u, sizeof(*receipt), &outputs[4]))
    return true;
  for (size_t left = 0u; left < 5u; left += 1u)
    for (size_t right = left + 1u; right < 5u; right += 1u)
      if (ranges_overlap(outputs[left], outputs[right])) return true;
  provider1_range inputs[W_PROVIDER1_INPUT_RANGE_CAPACITY];
  size_t count = 0u;
  if (!append_input_ranges(input, inputs, W_PROVIDER1_INPUT_RANGE_CAPACITY,
                           &count, true))
    return true;
  for (size_t output_index = 0u; output_index < 5u; output_index += 1u)
    for (size_t input_index = 0u; input_index < count; input_index += 1u)
      if (ranges_overlap(outputs[output_index], inputs[input_index]))
        return true;
  return false;
}

static bool measure_aliases_inputs(
    const w_seed_parallel_provider1_input *input,
    const w_seed_parallel_provider1_counts *counts,
    const w_seed_parallel_provider1_result *result) {
  provider1_range outputs[2];
  if (counts == NULL || result == NULL ||
      !range_make(counts, 1u, sizeof(*counts), &outputs[0]) ||
      !range_make(result, 1u, sizeof(*result), &outputs[1]) ||
      ranges_overlap(outputs[0], outputs[1]))
    return true;
  provider1_range inputs[W_PROVIDER1_INPUT_RANGE_CAPACITY];
  size_t count = 0u;
  if (!append_input_ranges(input, inputs, W_PROVIDER1_INPUT_RANGE_CAPACITY,
                           &count, true))
    return true;
  for (size_t output_index = 0u; output_index < 2u; output_index += 1u)
    for (size_t input_index = 0u; input_index < count; input_index += 1u)
      if (ranges_overlap(outputs[output_index], inputs[input_index]))
        return true;
  return false;
}

static bool verify_aliases_inputs(
    const w_seed_parallel_provider1_input *input,
    const w_seed_parallel_provider1_outcome *outcomes, size_t outcome_count,
    const w_seed_parallel_provider1_result *result) {
  provider1_range outputs[2];
  if (outcomes == NULL || result == NULL ||
      !range_make(outcomes, outcome_count, sizeof(*outcomes), &outputs[0]) ||
      !range_make(result, 1u, sizeof(*result), &outputs[1]) ||
      ranges_overlap(outputs[0], outputs[1]))
    return true;
  provider1_range inputs[W_PROVIDER1_INPUT_RANGE_CAPACITY];
  size_t count = 0u;
  if (!append_input_ranges(input, inputs, W_PROVIDER1_INPUT_RANGE_CAPACITY,
                           &count, true))
    return true;
  for (size_t output_index = 0u; output_index < 2u; output_index += 1u)
    for (size_t input_index = 0u; input_index < count; input_index += 1u)
      if (ranges_overlap(outputs[output_index], inputs[input_index]))
        return true;
  return false;
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

static void seal_outcomes(
    const w_seed_parallel_provider1_input *input,
    const w_seed_parallel_provider1_outcome *outcomes, size_t outcome_count,
    uint8_t digest[32]) {
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  w_seed_sha256_update(
      &state, (const uint8_t *)W_SEED_PARALLEL_PROVIDER1_SCHEMA_VERSION,
      sizeof(W_SEED_PARALLEL_PROVIDER1_SCHEMA_VERSION) - 1u);
  w_seed_sha256_update(&state, input->hir_result->semantic_digest, 32u);
  w_seed_sha256_update(&state, input->selection->semantic_digest, 32u);
  w_seed_sha256_update(&state, input->invocation->semantic_digest, 32u);
  sha_u32(&state, (uint32_t)outcome_count);
  for (size_t index = 0u; index < outcome_count; index += 1u) {
    sha_u32(&state, outcomes[index].function_index);
    sha_i64(&state, outcomes[index].value);
  }
  w_seed_sha256_final(&state, digest);
}

static void make_result(const w_seed_parallel_provider1_input *input,
                        bool written,
                        const w_seed_parallel_provider1_outcome *outcomes,
                        w_seed_parallel_provider1_result *result) {
  const size_t tasks = input->selection->task_count;
  (void)memset(result, 0, sizeof(*result));
  result->status = W_SEED_PARALLEL_PROVIDER1_OK;
  result->required = (w_seed_parallel_provider1_counts){tasks, tasks};
  result->written = written ? result->required
                            : (w_seed_parallel_provider1_counts){0u, 0u};
  (void)memcpy(result->schema, W_SEED_PARALLEL_PROVIDER1_SCHEMA_VERSION,
               sizeof(result->schema));
  result->task_count = (uint32_t)tasks;
  (void)memcpy(result->hir_semantic_digest,
               input->hir_result->semantic_digest,
               sizeof(result->hir_semantic_digest));
  (void)memcpy(result->selection_semantic_digest,
               input->selection->semantic_digest,
               sizeof(result->selection_semantic_digest));
  (void)memcpy(result->invocation_semantic_digest,
               input->invocation->semantic_digest,
               sizeof(result->invocation_semantic_digest));
  if (written) seal_outcomes(input, outcomes, tasks, result->outcome_digest);
}

typedef struct {
  const w_seed_parallel_provider1_input *input;
} provider1_context;

static bool invoke_task(void *raw, size_t task_index, int64_t *value) {
  const provider1_context *context = (const provider1_context *)raw;
  if (context == NULL || context->input == NULL || value == NULL ||
      task_index >= context->input->invocation->task_count)
    return false;
  size_t budget = W_SEED_PARALLEL_INVOCATION1_STEP_BUDGET;
  return w_seed_scalar_evaluator0_evaluate_call(
      context->input->hir_program,
      context->input->invocation->tasks[task_index].call_index, &budget,
      value);
}

w_seed_parallel_provider1_status w_seed_parallel_provider1_measure(
    const w_seed_parallel_provider1_input *input,
    w_seed_parallel_provider1_counts *counts,
    w_seed_parallel_provider1_result *result) {
  if (!input_valid(input) || counts == NULL || result == NULL)
    return W_SEED_PARALLEL_PROVIDER1_INVALID;
  if (measure_aliases_inputs(input, counts, result))
    return W_SEED_PARALLEL_PROVIDER1_ALIAS;
  w_seed_parallel_provider1_result candidate;
  make_result(input, false, NULL, &candidate);
  *counts = candidate.required;
  *result = candidate;
  return W_SEED_PARALLEL_PROVIDER1_OK;
}

w_seed_parallel_provider1_status w_seed_parallel_provider1_execute(
    const w_seed_parallel_provider1_input *input,
    const w_seed_parallel_provider1_output *output,
    w_seed_parallel_provider1_result *result,
    w_seed_parallel_provider1_receipt *receipt) {
  if (!input_valid(input) || output == NULL || result == NULL || receipt == NULL)
    return W_SEED_PARALLEL_PROVIDER1_INVALID;
  const size_t tasks = input->selection->task_count;
  if (output->outcome_capacity < tasks ||
      output->workspace_value_capacity < tasks || output->outcomes == NULL ||
      output->workspace_values == NULL)
    return W_SEED_PARALLEL_PROVIDER1_CAPACITY;
  if (outputs_alias_inputs(input, output, result, receipt))
    return W_SEED_PARALLEL_PROVIDER1_ALIAS;

  provider1_context context = {input};
  const w_seed_parallel_provider0_internal_job job = {invoke_task, &context};
  uint32_t started = 0u;
  uint32_t completed = 0u;
  uint32_t maximum_active = 0u;
  w_seed_parallel_provider0_kind platform_kind =
      W_SEED_PARALLEL_PROVIDER0_KIND_NONE;
  const w_seed_parallel_provider0_platform_status platform_status =
      w_seed_parallel_provider0_platform_execute(
          &job, tasks, input->provider_capacity, output->workspace_values,
          &started, &completed, &maximum_active, &platform_kind);
  if (platform_status == W_SEED_PARALLEL_PROVIDER0_PLATFORM_UNSUPPORTED)
    return W_SEED_PARALLEL_PROVIDER1_UNSUPPORTED;
  if (platform_status == W_SEED_PARALLEL_PROVIDER0_PLATFORM_TASK_FAILURE)
    return W_SEED_PARALLEL_PROVIDER1_TASK_FAILURE;
  if (platform_status != W_SEED_PARALLEL_PROVIDER0_PLATFORM_OK ||
      platform_kind != W_SEED_PARALLEL_PROVIDER0_KIND_WINDOWS_KERNEL32 ||
      started != tasks || completed != tasks || maximum_active == 0u ||
      maximum_active > input->provider_capacity)
    return W_SEED_PARALLEL_PROVIDER1_PROVIDER_FAILURE;

  w_seed_parallel_provider1_result result_candidate;
  w_seed_parallel_provider1_outcome outcome_for_digest;
  w_seed_sha256_state digest_state;
  w_seed_sha256_init(&digest_state);
  w_seed_sha256_update(
      &digest_state,
      (const uint8_t *)W_SEED_PARALLEL_PROVIDER1_SCHEMA_VERSION,
      sizeof(W_SEED_PARALLEL_PROVIDER1_SCHEMA_VERSION) - 1u);
  w_seed_sha256_update(&digest_state, input->hir_result->semantic_digest, 32u);
  w_seed_sha256_update(&digest_state, input->selection->semantic_digest, 32u);
  w_seed_sha256_update(&digest_state, input->invocation->semantic_digest, 32u);
  sha_u32(&digest_state, (uint32_t)tasks);
  for (size_t index = 0u; index < tasks; index += 1u) {
    outcome_for_digest = (w_seed_parallel_provider1_outcome){
        input->invocation->tasks[index].function_index,
        output->workspace_values[index]};
    sha_u32(&digest_state, outcome_for_digest.function_index);
    sha_i64(&digest_state, outcome_for_digest.value);
  }
  make_result(input, false, NULL, &result_candidate);
  result_candidate.written = result_candidate.required;
  w_seed_sha256_final(&digest_state, result_candidate.outcome_digest);

  w_seed_parallel_provider1_receipt receipt_candidate;
  (void)memset(&receipt_candidate, 0, sizeof(receipt_candidate));
  receipt_candidate.provider_kind =
      W_SEED_PARALLEL_PROVIDER1_KIND_WINDOWS_KERNEL32;
  receipt_candidate.provider_capacity = input->provider_capacity;
  receipt_candidate.task_count = (uint32_t)tasks;
  receipt_candidate.started_count = started;
  receipt_candidate.completed_count = completed;
  receipt_candidate.maximum_active_workers = maximum_active;
  receipt_candidate.overlap_observed = maximum_active > 1u;

  for (size_t index = 0u; index < tasks; index += 1u)
    output->outcomes[index] = (w_seed_parallel_provider1_outcome){
        input->invocation->tasks[index].function_index,
        output->workspace_values[index]};
  *result = result_candidate;
  *receipt = receipt_candidate;
  return W_SEED_PARALLEL_PROVIDER1_OK;
}

bool w_seed_parallel_provider1_verify_outcomes(
    const w_seed_parallel_provider1_input *input,
    const w_seed_parallel_provider1_outcome *outcomes, size_t outcome_count,
    const w_seed_parallel_provider1_result *result) {
  if (!input_valid(input) || outcomes == NULL || result == NULL ||
      result->status != W_SEED_PARALLEL_PROVIDER1_OK ||
      outcome_count != input->selection->task_count ||
      result->required.outcomes != outcome_count ||
      result->required.workspace_values != outcome_count ||
      result->written.outcomes != outcome_count ||
      result->written.workspace_values != outcome_count ||
      result->task_count != outcome_count ||
      memcmp(result->schema, W_SEED_PARALLEL_PROVIDER1_SCHEMA_VERSION,
             sizeof(result->schema)) != 0 ||
      memcmp(result->hir_semantic_digest,
             input->hir_result->semantic_digest,
             sizeof(result->hir_semantic_digest)) != 0 ||
      memcmp(result->selection_semantic_digest,
             input->selection->semantic_digest,
             sizeof(result->selection_semantic_digest)) != 0 ||
      memcmp(result->invocation_semantic_digest,
             input->invocation->semantic_digest,
             sizeof(result->invocation_semantic_digest)) != 0 ||
      verify_aliases_inputs(input, outcomes, outcome_count, result))
    return false;
  for (size_t index = 0u; index < outcome_count; index += 1u) {
    int64_t expected = 0;
    size_t budget = W_SEED_PARALLEL_INVOCATION1_STEP_BUDGET;
    if (outcomes[index].function_index !=
            input->invocation->tasks[index].function_index ||
        !w_seed_scalar_evaluator0_evaluate_call(
            input->hir_program, input->invocation->tasks[index].call_index,
            &budget, &expected) ||
        outcomes[index].value != expected)
      return false;
  }
  uint8_t expected_digest[32];
  seal_outcomes(input, outcomes, outcome_count, expected_digest);
  return memcmp(result->outcome_digest, expected_digest,
                sizeof(expected_digest)) == 0;
}
