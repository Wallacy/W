#include "w_seed_parallel_invocation1.h"

#include "w_seed_scalar_evaluator0.h"
#include "w_seed_sha256.h"

#include <limits.h>
#include <string.h>

typedef struct {
  uintptr_t start;
  uintptr_t end;
  bool active;
} parinv1_range;

typedef struct {
  size_t task_count;
  size_t argument_count;
  uint32_t root_function_index;
  uint8_t hir_semantic_digest[32];
  uint8_t selection_semantic_digest[32];
} parinv1_summary;

enum {
  W_PARINV1_PRODUCER_RANGE_CAPACITY = 35u,
  W_PARINV1_COMPLETE_RANGE_CAPACITY = 39u,
};

static bool range_make(const void *pointer, size_t count, size_t element_size,
                       parinv1_range *range) {
  if (range == NULL) return false;
  *range = (parinv1_range){0u, 0u, false};
  if (count == 0u) return true;
  if (pointer == NULL || element_size == 0u || count > SIZE_MAX / element_size)
    return false;
  const size_t bytes = count * element_size;
  const uintptr_t start = (uintptr_t)pointer;
  if (start > UINTPTR_MAX - bytes) return false;
  *range = (parinv1_range){start, start + bytes, true};
  return true;
}

static bool ranges_overlap(parinv1_range left, parinv1_range right) {
  return left.active && right.active && left.start < right.end &&
         right.start < left.end;
}

static bool add_range(parinv1_range *ranges, size_t capacity, size_t *count,
                      const void *pointer, size_t elements,
                      size_t element_size) {
  if (ranges == NULL || count == NULL || *count >= capacity) return false;
  if (!range_make(pointer, elements, element_size, &ranges[*count]))
    return false;
  *count += 1u;
  return true;
}

static bool append_hir_ranges(const w_seed_hir0_program *program,
                              parinv1_range *ranges, size_t capacity,
                              size_t *count) {
  if (program == NULL || ranges == NULL || count == NULL) return false;
#define W_PARINV1_ADD(pointer, number)                                        \
  do {                                                                         \
    if (!add_range(ranges, capacity, count, (pointer), (number),              \
                   sizeof(*(pointer))))                                       \
      return false;                                                            \
  } while (0)
  W_PARINV1_ADD(program, 1u);
  W_PARINV1_ADD(program->modules, program->module_capacity);
  W_PARINV1_ADD(program->identities, program->identity_capacity);
  W_PARINV1_ADD(program->types, program->type_capacity);
  W_PARINV1_ADD(program->enums, program->enum_capacity);
  W_PARINV1_ADD(program->enum_cases, program->enum_case_capacity);
  W_PARINV1_ADD(program->enum_case_parameters,
                program->enum_case_parameter_capacity);
  W_PARINV1_ADD(program->enum_subset_members,
                program->enum_subset_member_capacity);
  W_PARINV1_ADD(program->functions, program->function_capacity);
  W_PARINV1_ADD(program->parameters, program->parameter_capacity);
  W_PARINV1_ADD(program->blocks, program->block_capacity);
  W_PARINV1_ADD(program->block_arguments, program->block_argument_capacity);
  W_PARINV1_ADD(program->edge_arguments, program->edge_argument_capacity);
  W_PARINV1_ADD(program->switch_edges, program->switch_edge_capacity);
  W_PARINV1_ADD(program->switch_captures, program->switch_capture_capacity);
  W_PARINV1_ADD(program->instructions, program->instruction_capacity);
  W_PARINV1_ADD(program->bindings, program->binding_capacity);
  W_PARINV1_ADD(program->calls, program->call_capacity);
  W_PARINV1_ADD(program->host_parameters, program->host_parameter_capacity);
  W_PARINV1_ADD(program->arguments, program->argument_capacity);
  W_PARINV1_ADD(program->enum_payloads, program->enum_payload_capacity);
  W_PARINV1_ADD(program->requirements, program->requirement_capacity);
  W_PARINV1_ADD(program->values, program->value_capacity);
  W_PARINV1_ADD(program->interpolation_segments,
                program->interpolation_segment_capacity);
  W_PARINV1_ADD(program->terminators, program->terminator_capacity);
  W_PARINV1_ADD(program->entries, program->entry_capacity);
  W_PARINV1_ADD(program->text_bytes, program->text_byte_capacity);
  W_PARINV1_ADD(program->value_bytes, program->value_byte_capacity);
  W_PARINV1_ADD(program->receipt, program->receipt_capacity);
  W_PARINV1_ADD(program->external_modules, program->external_module_capacity);
  W_PARINV1_ADD(program->external_symbols, program->external_symbol_capacity);
#undef W_PARINV1_ADD
  return true;
}

static bool append_producer_ranges(
    const w_seed_hir0_program *hir_program,
    const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection1_program *selection,
    const w_seed_parallel_selection1_result *selection_result,
    parinv1_range *ranges, size_t capacity, size_t *count) {
  return add_range(ranges, capacity, count, hir_result, 1u,
                   sizeof(*hir_result)) &&
         append_hir_ranges(hir_program, ranges, capacity, count) &&
         add_range(ranges, capacity, count, selection, 1u,
                   sizeof(*selection)) &&
         add_range(ranges, capacity, count, selection->tasks,
                   selection->task_capacity, sizeof(*selection->tasks)) &&
         add_range(ranges, capacity, count, selection_result, 1u,
                   sizeof(*selection_result));
}

static bool outputs_alias_producers(
    const w_seed_hir0_program *hir_program,
    const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection1_program *selection,
    const w_seed_parallel_selection1_result *selection_result,
    const w_seed_parallel_invocation1_output *output,
    const w_seed_parallel_invocation1_result *result) {
  if (output == NULL || result == NULL) return true;
  parinv1_range outputs[4];
  if (!range_make(output, 1u, sizeof(*output), &outputs[0]) ||
      !range_make(output->tasks, output->task_capacity, sizeof(*output->tasks),
                  &outputs[1]) ||
      !range_make(output->arguments, output->argument_capacity,
                  sizeof(*output->arguments), &outputs[2]) ||
      !range_make(result, 1u, sizeof(*result), &outputs[3]))
    return true;
  for (size_t left = 0u; left < 4u; left += 1u)
    for (size_t right = left + 1u; right < 4u; right += 1u)
      if (ranges_overlap(outputs[left], outputs[right])) return true;
  parinv1_range inputs[W_PARINV1_PRODUCER_RANGE_CAPACITY];
  size_t input_count = 0u;
  if (!append_producer_ranges(hir_program, hir_result, selection,
                              selection_result, inputs,
                              W_PARINV1_PRODUCER_RANGE_CAPACITY, &input_count))
    return true;
  for (size_t output_index = 0u; output_index < 4u; output_index += 1u)
    for (size_t input_index = 0u; input_index < input_count; input_index += 1u)
      if (ranges_overlap(outputs[output_index], inputs[input_index]))
        return true;
  return false;
}

static bool measure_aliases_producers(
    const w_seed_hir0_program *hir_program,
    const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection1_program *selection,
    const w_seed_parallel_selection1_result *selection_result,
    const w_seed_parallel_invocation1_counts *counts,
    const w_seed_parallel_invocation1_result *result) {
  parinv1_range outputs[2];
  if (counts == NULL || result == NULL ||
      !range_make(counts, 1u, sizeof(*counts), &outputs[0]) ||
      !range_make(result, 1u, sizeof(*result), &outputs[1]) ||
      ranges_overlap(outputs[0], outputs[1]))
    return true;
  parinv1_range inputs[W_PARINV1_PRODUCER_RANGE_CAPACITY];
  size_t input_count = 0u;
  if (!append_producer_ranges(hir_program, hir_result, selection,
                              selection_result, inputs,
                              W_PARINV1_PRODUCER_RANGE_CAPACITY, &input_count))
    return true;
  for (size_t output_index = 0u; output_index < 2u; output_index += 1u)
    for (size_t input_index = 0u; input_index < input_count; input_index += 1u)
      if (ranges_overlap(outputs[output_index], inputs[input_index]))
        return true;
  return false;
}

static bool invocation_aliases_producers(
    const w_seed_hir0_program *hir_program,
    const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection1_program *selection,
    const w_seed_parallel_selection1_result *selection_result,
    const w_seed_parallel_invocation1_program *invocation,
    const w_seed_parallel_invocation1_result *result) {
  if (invocation == NULL || result == NULL) return true;
  parinv1_range outputs[4];
  if (!range_make(invocation, 1u, sizeof(*invocation), &outputs[0]) ||
      !range_make(invocation->tasks, invocation->task_capacity,
                  sizeof(*invocation->tasks), &outputs[1]) ||
      !range_make(invocation->arguments, invocation->argument_capacity,
                  sizeof(*invocation->arguments), &outputs[2]) ||
      !range_make(result, 1u, sizeof(*result), &outputs[3]))
    return true;
  for (size_t left = 0u; left < 4u; left += 1u)
    for (size_t right = left + 1u; right < 4u; right += 1u)
      if (ranges_overlap(outputs[left], outputs[right])) return true;
  parinv1_range inputs[W_PARINV1_PRODUCER_RANGE_CAPACITY];
  size_t input_count = 0u;
  if (!append_producer_ranges(hir_program, hir_result, selection,
                              selection_result, inputs,
                              W_PARINV1_PRODUCER_RANGE_CAPACITY, &input_count))
    return true;
  for (size_t output_index = 0u; output_index < 4u; output_index += 1u)
    for (size_t input_index = 0u; input_index < input_count; input_index += 1u)
      if (ranges_overlap(outputs[output_index], inputs[input_index]))
        return true;
  return false;
}

static bool range_valid_or_empty(uint32_t first, uint32_t count,
                                 size_t total) {
  if (count == 0u) return first == W_SEED_HIR0_NONE;
  return first != W_SEED_HIR0_NONE && first <= total &&
         count <= total - first;
}

static bool i64_type(const w_seed_hir0_program *program, uint32_t type_index) {
  return program != NULL && type_index < program->type_count &&
         program->types[type_index].kind == W_SEED_HIR0_TYPE_I64;
}

static const w_seed_hir0_argument *argument_for_ordinal(
    const w_seed_hir0_program *program, const w_seed_hir0_call *call,
    uint32_t ordinal, size_t *matches) {
  if (program == NULL || call == NULL || matches == NULL) return NULL;
  const w_seed_hir0_argument *found = NULL;
  *matches = 0u;
  for (size_t index = 0u; index < call->argument_count; index += 1u) {
    const w_seed_hir0_argument *argument =
        &program->arguments[(size_t)call->first_argument + index];
    if (argument->parameter_ordinal != ordinal) continue;
    found = argument;
    *matches += 1u;
  }
  return found;
}

static bool validate_task(const w_seed_hir0_program *program,
                          const w_seed_parallel_selection1_task *selected,
                          size_t *argument_count) {
  if (program == NULL || selected == NULL || argument_count == NULL ||
      selected->call_index >= program->call_count ||
      selected->function_index >= program->function_count)
    return false;
  const w_seed_hir0_call *call = &program->calls[selected->call_index];
  const w_seed_hir0_function *function =
      &program->functions[selected->function_index];
  if (call->execution_kind !=
          W_SEED_HIR0_CALL_STRUCTURED_ASYNC_PARALLEL_DOMAIN_DISPATCH ||
      call->argument_count != function->parameter_count ||
      !range_valid_or_empty(call->first_argument, call->argument_count,
                            program->argument_count) ||
      !range_valid_or_empty(function->first_parameter,
                            function->parameter_count,
                            program->parameter_count) ||
      !i64_type(program, call->result_type) ||
      !i64_type(program, function->return_type) || function->is_async ||
      function->is_const || function->is_throws || function->is_unsafe ||
      function->has_borrow_clause || function->is_anonymous_entry ||
      function->block_count != 1u)
    return false;
  for (uint32_t ordinal = 0u; ordinal < call->argument_count; ordinal += 1u) {
    size_t matches = 0u;
    const w_seed_hir0_argument *argument =
        argument_for_ordinal(program, call, ordinal, &matches);
    const size_t parameter_index = (size_t)function->first_parameter + ordinal;
    if (matches != 1u || argument == NULL ||
        argument->owner_call != selected->call_index ||
        argument->value_index >= program->value_count ||
        !i64_type(program, argument->type_index) ||
        parameter_index >= program->parameter_count ||
        !i64_type(program, program->parameters[parameter_index].type_index) ||
        argument->type_index != program->parameters[parameter_index].type_index)
      return false;
  }
  int64_t proof_value = 0;
  size_t budget = W_SEED_PARALLEL_INVOCATION1_STEP_BUDGET;
  if (!w_seed_scalar_evaluator0_evaluate_call(
          program, selected->call_index, &budget, &proof_value))
    return false;
  *argument_count = call->argument_count;
  return true;
}

static bool derive_summary(
    const w_seed_hir0_program *hir_program,
    const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection1_program *selection,
    parinv1_summary *summary) {
  if (hir_program == NULL || hir_result == NULL || selection == NULL ||
      summary == NULL || selection->task_count == 0u ||
      selection->task_count > UINT32_MAX)
    return false;
  size_t arguments = 0u;
  for (size_t task = 0u; task < selection->task_count; task += 1u) {
    size_t task_arguments = 0u;
    if (!validate_task(hir_program, &selection->tasks[task], &task_arguments) ||
        task_arguments > UINT32_MAX || arguments > UINT32_MAX - task_arguments)
      return false;
    arguments += task_arguments;
  }
  *summary = (parinv1_summary){selection->task_count, arguments,
                              selection->root_function_index, {0}, {0}};
  (void)memcpy(summary->hir_semantic_digest, hir_result->semantic_digest,
               sizeof(summary->hir_semantic_digest));
  (void)memcpy(summary->selection_semantic_digest,
               selection->semantic_digest,
               sizeof(summary->selection_semantic_digest));
  return true;
}

static void emit_records(
    const w_seed_hir0_program *hir_program,
    const w_seed_parallel_selection1_program *selection,
    w_seed_parallel_invocation1_task *tasks,
    w_seed_parallel_invocation1_argument *arguments) {
  size_t argument_cursor = 0u;
  for (size_t task = 0u; task < selection->task_count; task += 1u) {
    const w_seed_parallel_selection1_task *selected = &selection->tasks[task];
    const w_seed_hir0_call *call = &hir_program->calls[selected->call_index];
    const w_seed_hir0_function *function =
        &hir_program->functions[selected->function_index];
    tasks[task] = (w_seed_parallel_invocation1_task){
        selected->call_index,
        selected->function_index,
        call->argument_count == 0u ? W_SEED_HIR0_NONE
                                   : (uint32_t)argument_cursor,
        call->argument_count};
    for (uint32_t ordinal = 0u; ordinal < call->argument_count; ordinal += 1u) {
      size_t matches = 0u;
      const w_seed_hir0_argument *argument =
          argument_for_ordinal(hir_program, call, ordinal, &matches);
      const uint32_t parameter_index = function->first_parameter + ordinal;
      arguments[argument_cursor] = (w_seed_parallel_invocation1_argument){
          (uint32_t)task, ordinal, parameter_index, argument->value_index,
          argument->type_index};
      argument_cursor += 1u;
    }
  }
}

static void sha_u32(w_seed_sha256_state *state, uint32_t value) {
  const uint8_t bytes[4] = {(uint8_t)value, (uint8_t)(value >> 8u),
                            (uint8_t)(value >> 16u),
                            (uint8_t)(value >> 24u)};
  w_seed_sha256_update(state, bytes, sizeof(bytes));
}

static void seal(const parinv1_summary *summary,
                 const w_seed_parallel_invocation1_task *tasks,
                 const w_seed_parallel_invocation1_argument *arguments,
                 uint8_t digest[32]) {
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  w_seed_sha256_update(
      &state, (const uint8_t *)W_SEED_PARALLEL_INVOCATION1_SCHEMA_VERSION,
      sizeof(W_SEED_PARALLEL_INVOCATION1_SCHEMA_VERSION) - 1u);
  w_seed_sha256_update(&state, summary->hir_semantic_digest,
                       sizeof(summary->hir_semantic_digest));
  w_seed_sha256_update(&state, summary->selection_semantic_digest,
                       sizeof(summary->selection_semantic_digest));
  sha_u32(&state, summary->root_function_index);
  sha_u32(&state, (uint32_t)summary->task_count);
  sha_u32(&state, (uint32_t)summary->argument_count);
  for (size_t index = 0u; index < summary->task_count; index += 1u) {
    sha_u32(&state, tasks[index].call_index);
    sha_u32(&state, tasks[index].function_index);
    sha_u32(&state, tasks[index].first_argument);
    sha_u32(&state, tasks[index].argument_count);
  }
  for (size_t index = 0u; index < summary->argument_count; index += 1u) {
    sha_u32(&state, arguments[index].owner_task);
    sha_u32(&state, arguments[index].parameter_ordinal);
    sha_u32(&state, arguments[index].parameter_index);
    sha_u32(&state, arguments[index].value_index);
    sha_u32(&state, arguments[index].type_index);
  }
  w_seed_sha256_final(&state, digest);
}

static void make_result(const parinv1_summary *summary,
                        const w_seed_parallel_invocation1_task *tasks,
                        const w_seed_parallel_invocation1_argument *arguments,
                        bool written,
                        w_seed_parallel_invocation1_result *result) {
  (void)memset(result, 0, sizeof(*result));
  result->status = W_SEED_PARALLEL_INVOCATION1_OK;
  result->required = (w_seed_parallel_invocation1_counts){
      summary->task_count, summary->argument_count};
  result->written = written ? result->required
                            : (w_seed_parallel_invocation1_counts){0u, 0u};
  (void)memcpy(result->schema, W_SEED_PARALLEL_INVOCATION1_SCHEMA_VERSION,
               sizeof(result->schema));
  result->root_function_index = summary->root_function_index;
  (void)memcpy(result->hir_semantic_digest, summary->hir_semantic_digest,
               sizeof(result->hir_semantic_digest));
  (void)memcpy(result->selection_semantic_digest,
               summary->selection_semantic_digest,
               sizeof(result->selection_semantic_digest));
  if (written) seal(summary, tasks, arguments, result->semantic_digest);
}

w_seed_parallel_invocation1_status w_seed_parallel_invocation1_measure(
    const w_seed_hir0_program *hir_program,
    const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection1_program *selection,
    const w_seed_parallel_selection1_result *selection_result,
    w_seed_parallel_invocation1_counts *counts,
    w_seed_parallel_invocation1_result *result) {
  if (hir_program == NULL || hir_result == NULL || selection == NULL ||
      selection_result == NULL || counts == NULL || result == NULL ||
      !w_seed_parallel_selection1_verify(hir_program, hir_result, selection,
                                         selection_result))
    return W_SEED_PARALLEL_INVOCATION1_INVALID;
  parinv1_summary summary;
  if (!derive_summary(hir_program, hir_result, selection, &summary))
    return W_SEED_PARALLEL_INVOCATION1_UNSUPPORTED;
  if (measure_aliases_producers(hir_program, hir_result, selection,
                                selection_result, counts, result))
    return W_SEED_PARALLEL_INVOCATION1_ALIAS;
  w_seed_parallel_invocation1_result candidate;
  make_result(&summary, NULL, NULL, false, &candidate);
  *counts = candidate.required;
  *result = candidate;
  return W_SEED_PARALLEL_INVOCATION1_OK;
}

w_seed_parallel_invocation1_status w_seed_parallel_invocation1_run(
    const w_seed_hir0_program *hir_program,
    const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection1_program *selection,
    const w_seed_parallel_selection1_result *selection_result,
    const w_seed_parallel_invocation1_output *output,
    w_seed_parallel_invocation1_result *result) {
  if (hir_program == NULL || hir_result == NULL || selection == NULL ||
      selection_result == NULL || output == NULL || result == NULL ||
      !w_seed_parallel_selection1_verify(hir_program, hir_result, selection,
                                         selection_result))
    return W_SEED_PARALLEL_INVOCATION1_INVALID;
  parinv1_summary summary;
  if (!derive_summary(hir_program, hir_result, selection, &summary))
    return W_SEED_PARALLEL_INVOCATION1_UNSUPPORTED;
  if (output->task_capacity < summary.task_count ||
      output->argument_capacity < summary.argument_count ||
      (summary.task_count != 0u && output->tasks == NULL) ||
      (summary.argument_count != 0u && output->arguments == NULL))
    return W_SEED_PARALLEL_INVOCATION1_CAPACITY;
  if (outputs_alias_producers(hir_program, hir_result, selection,
                              selection_result, output, result))
    return W_SEED_PARALLEL_INVOCATION1_ALIAS;
  emit_records(hir_program, selection, output->tasks, output->arguments);
  w_seed_parallel_invocation1_result candidate;
  make_result(&summary, output->tasks, output->arguments, true, &candidate);
  *result = candidate;
  return W_SEED_PARALLEL_INVOCATION1_OK;
}

bool w_seed_parallel_invocation1_program_from_output(
    const w_seed_parallel_invocation1_output *output,
    const w_seed_parallel_invocation1_result *result,
    w_seed_parallel_invocation1_program *program) {
  if (output == NULL || result == NULL || program == NULL ||
      result->status != W_SEED_PARALLEL_INVOCATION1_OK ||
      result->required.tasks == 0u ||
      result->required.tasks != result->written.tasks ||
      result->required.arguments != result->written.arguments ||
      result->written.tasks > output->task_capacity ||
      result->written.arguments > output->argument_capacity ||
      result->written.tasks > UINT32_MAX ||
      result->written.arguments > UINT32_MAX || output->tasks == NULL ||
      (result->written.arguments != 0u && output->arguments == NULL) ||
      memcmp(result->schema, W_SEED_PARALLEL_INVOCATION1_SCHEMA_VERSION,
             sizeof(result->schema)) != 0)
    return false;
  parinv1_range sources[4];
  if (!range_make(output, 1u, sizeof(*output), &sources[0]) ||
      !range_make(output->tasks, output->task_capacity, sizeof(*output->tasks),
                  &sources[1]) ||
      !range_make(output->arguments, output->argument_capacity,
                  sizeof(*output->arguments), &sources[2]) ||
      !range_make(result, 1u, sizeof(*result), &sources[3]))
    return false;
  for (size_t left = 0u; left < 4u; left += 1u)
    for (size_t right = left + 1u; right < 4u; right += 1u)
      if (ranges_overlap(sources[left], sources[right])) return false;
  parinv1_range destination;
  if (!range_make(program, 1u, sizeof(*program), &destination)) return false;
  for (size_t index = 0u; index < 4u; index += 1u)
    if (ranges_overlap(destination, sources[index])) return false;
  w_seed_parallel_invocation1_program candidate = {
      output->tasks,
      result->written.tasks,
      output->task_capacity,
      output->arguments,
      result->written.arguments,
      output->argument_capacity,
      result->root_function_index,
      {0},
      {0},
      {0}};
  (void)memcpy(candidate.hir_semantic_digest, result->hir_semantic_digest,
               sizeof(candidate.hir_semantic_digest));
  (void)memcpy(candidate.selection_semantic_digest,
               result->selection_semantic_digest,
               sizeof(candidate.selection_semantic_digest));
  (void)memcpy(candidate.semantic_digest, result->semantic_digest,
               sizeof(candidate.semantic_digest));
  *program = candidate;
  return true;
}

bool w_seed_parallel_invocation1_verify(
    const w_seed_hir0_program *hir_program,
    const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection1_program *selection,
    const w_seed_parallel_selection1_result *selection_result,
    const w_seed_parallel_invocation1_program *invocation,
    const w_seed_parallel_invocation1_result *result) {
  if (hir_program == NULL || hir_result == NULL || selection == NULL ||
      selection_result == NULL || invocation == NULL || result == NULL ||
      result->status != W_SEED_PARALLEL_INVOCATION1_OK ||
      !w_seed_parallel_selection1_verify(hir_program, hir_result, selection,
                                         selection_result) ||
      invocation->tasks == NULL || invocation->task_count == 0u ||
      invocation->task_count > invocation->task_capacity ||
      invocation->argument_count > invocation->argument_capacity ||
      (invocation->argument_count != 0u && invocation->arguments == NULL) ||
      invocation->task_count > UINT32_MAX ||
      invocation->argument_count > UINT32_MAX ||
      result->required.tasks != invocation->task_count ||
      result->written.tasks != invocation->task_count ||
      result->required.arguments != invocation->argument_count ||
      result->written.arguments != invocation->argument_count ||
      memcmp(result->schema, W_SEED_PARALLEL_INVOCATION1_SCHEMA_VERSION,
             sizeof(result->schema)) != 0 ||
      invocation_aliases_producers(hir_program, hir_result, selection,
                                   selection_result, invocation, result))
    return false;
  parinv1_summary summary;
  if (!derive_summary(hir_program, hir_result, selection, &summary) ||
      invocation->task_count != summary.task_count ||
      invocation->argument_count != summary.argument_count ||
      invocation->root_function_index != summary.root_function_index ||
      result->root_function_index != summary.root_function_index ||
      memcmp(invocation->hir_semantic_digest, summary.hir_semantic_digest,
             sizeof(invocation->hir_semantic_digest)) != 0 ||
      memcmp(result->hir_semantic_digest, summary.hir_semantic_digest,
             sizeof(result->hir_semantic_digest)) != 0 ||
      memcmp(invocation->selection_semantic_digest,
             summary.selection_semantic_digest,
             sizeof(invocation->selection_semantic_digest)) != 0 ||
      memcmp(result->selection_semantic_digest,
             summary.selection_semantic_digest,
             sizeof(result->selection_semantic_digest)) != 0)
    return false;
  size_t argument_cursor = 0u;
  for (size_t task = 0u; task < summary.task_count; task += 1u) {
    const w_seed_parallel_selection1_task *selected = &selection->tasks[task];
    const w_seed_hir0_call *call = &hir_program->calls[selected->call_index];
    const w_seed_hir0_function *function =
        &hir_program->functions[selected->function_index];
    const w_seed_parallel_invocation1_task expected = {
        selected->call_index,
        selected->function_index,
        call->argument_count == 0u ? W_SEED_HIR0_NONE
                                   : (uint32_t)argument_cursor,
        call->argument_count};
    if (memcmp(&invocation->tasks[task], &expected, sizeof(expected)) != 0)
      return false;
    for (uint32_t ordinal = 0u; ordinal < call->argument_count; ordinal += 1u) {
      size_t matches = 0u;
      const w_seed_hir0_argument *argument =
          argument_for_ordinal(hir_program, call, ordinal, &matches);
      if (argument == NULL || matches != 1u) return false;
      const w_seed_parallel_invocation1_argument expected_argument = {
          (uint32_t)task, ordinal, function->first_parameter + ordinal,
          argument->value_index, argument->type_index};
      if (memcmp(&invocation->arguments[argument_cursor], &expected_argument,
                 sizeof(expected_argument)) != 0)
        return false;
      argument_cursor += 1u;
    }
  }
  if (argument_cursor != summary.argument_count) return false;
  uint8_t expected_digest[32];
  seal(&summary, invocation->tasks, invocation->arguments, expected_digest);
  return memcmp(invocation->semantic_digest, expected_digest,
                sizeof(expected_digest)) == 0 &&
         memcmp(result->semantic_digest, expected_digest,
                sizeof(expected_digest)) == 0;
}

static bool value_aliases_complete_inputs(
    const w_seed_hir0_program *hir_program,
    const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection1_program *selection,
    const w_seed_parallel_selection1_result *selection_result,
    const w_seed_parallel_invocation1_program *invocation,
    const w_seed_parallel_invocation1_result *result, const int64_t *value) {
  parinv1_range value_range;
  if (!range_make(value, 1u, sizeof(*value), &value_range)) return true;
  parinv1_range inputs[W_PARINV1_COMPLETE_RANGE_CAPACITY];
  size_t count = 0u;
  if (!append_producer_ranges(hir_program, hir_result, selection,
                              selection_result, inputs,
                              W_PARINV1_COMPLETE_RANGE_CAPACITY, &count) ||
      !add_range(inputs, W_PARINV1_COMPLETE_RANGE_CAPACITY, &count, invocation,
                 1u, sizeof(*invocation)) ||
      !add_range(inputs, W_PARINV1_COMPLETE_RANGE_CAPACITY, &count,
                 invocation->tasks, invocation->task_capacity,
                 sizeof(*invocation->tasks)) ||
      !add_range(inputs, W_PARINV1_COMPLETE_RANGE_CAPACITY, &count,
                 invocation->arguments, invocation->argument_capacity,
                 sizeof(*invocation->arguments)) ||
      !add_range(inputs, W_PARINV1_COMPLETE_RANGE_CAPACITY, &count, result, 1u,
                 sizeof(*result)))
    return true;
  for (size_t index = 0u; index < count; index += 1u)
    if (ranges_overlap(value_range, inputs[index])) return true;
  return false;
}

w_seed_parallel_invocation1_status w_seed_parallel_invocation1_evaluate_task(
    const w_seed_hir0_program *hir_program,
    const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection1_program *selection,
    const w_seed_parallel_selection1_result *selection_result,
    const w_seed_parallel_invocation1_program *invocation,
    const w_seed_parallel_invocation1_result *result, uint32_t task_index,
    int64_t *value) {
  if (value == NULL ||
      !w_seed_parallel_invocation1_verify(
          hir_program, hir_result, selection, selection_result, invocation,
          result) ||
      task_index >= invocation->task_count ||
      value_aliases_complete_inputs(hir_program, hir_result, selection,
                                    selection_result, invocation, result,
                                    value))
    return W_SEED_PARALLEL_INVOCATION1_INVALID;
  int64_t candidate = 0;
  size_t budget = W_SEED_PARALLEL_INVOCATION1_STEP_BUDGET;
  if (!w_seed_scalar_evaluator0_evaluate_call(
          hir_program, invocation->tasks[task_index].call_index, &budget,
          &candidate))
    return W_SEED_PARALLEL_INVOCATION1_EVALUATION_FAILURE;
  *value = candidate;
  return W_SEED_PARALLEL_INVOCATION1_OK;
}
