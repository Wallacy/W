#include "w_seed_parallel_selection1.h"

#include "w_seed_sha256.h"

#include <limits.h>
#include <string.h>

typedef struct {
  uintptr_t start;
  uintptr_t end;
  bool active;
} selection1_range;

typedef struct {
  uint32_t root_function_index;
  size_t task_count;
  w_seed_hir0_call_placement_kind placement;
  w_seed_frontend_domain_mode domain_mode;
  uint32_t domain_capabilities;
  uint8_t hir_semantic_digest[32];
} selection1_summary;

enum { W_PARSEL1_INPUT_RANGE_CAPACITY = 32u };

static bool range_make(const void *pointer, size_t count, size_t element_size,
                       selection1_range *range) {
  if (range == NULL) return false;
  *range = (selection1_range){0u, 0u, false};
  if (count == 0u) return true;
  if (pointer == NULL || element_size == 0u || count > SIZE_MAX / element_size)
    return false;
  const size_t bytes = count * element_size;
  const uintptr_t start = (uintptr_t)pointer;
  if (start > UINTPTR_MAX - bytes) return false;
  *range = (selection1_range){start, start + bytes, true};
  return true;
}

static bool ranges_overlap(selection1_range left, selection1_range right) {
  return left.active && right.active && left.start < right.end &&
         right.start < left.end;
}

static bool add_range(selection1_range *ranges, size_t capacity, size_t *count,
                      const void *pointer, size_t elements,
                      size_t element_size) {
  if (ranges == NULL || count == NULL || *count >= capacity) return false;
  if (!range_make(pointer, elements, element_size, &ranges[*count]))
    return false;
  *count += 1u;
  return true;
}

static bool append_hir_ranges(const w_seed_hir0_program *program,
                              selection1_range *ranges, size_t capacity,
                              size_t *count) {
  if (program == NULL || ranges == NULL || count == NULL) return false;
#define W_PARSEL1_ADD(pointer, number)                                        \
  do {                                                                         \
    if (!add_range(ranges, capacity, count, (pointer), (number),              \
                   sizeof(*(pointer))))                                       \
      return false;                                                            \
  } while (0)
  W_PARSEL1_ADD(program, 1u);
  W_PARSEL1_ADD(program->modules, program->module_capacity);
  W_PARSEL1_ADD(program->identities, program->identity_capacity);
  W_PARSEL1_ADD(program->types, program->type_capacity);
  W_PARSEL1_ADD(program->enums, program->enum_capacity);
  W_PARSEL1_ADD(program->enum_cases, program->enum_case_capacity);
  W_PARSEL1_ADD(program->enum_case_parameters,
                program->enum_case_parameter_capacity);
  W_PARSEL1_ADD(program->enum_subset_members,
                program->enum_subset_member_capacity);
  W_PARSEL1_ADD(program->functions, program->function_capacity);
  W_PARSEL1_ADD(program->parameters, program->parameter_capacity);
  W_PARSEL1_ADD(program->blocks, program->block_capacity);
  W_PARSEL1_ADD(program->block_arguments, program->block_argument_capacity);
  W_PARSEL1_ADD(program->edge_arguments, program->edge_argument_capacity);
  W_PARSEL1_ADD(program->switch_edges, program->switch_edge_capacity);
  W_PARSEL1_ADD(program->switch_captures, program->switch_capture_capacity);
  W_PARSEL1_ADD(program->instructions, program->instruction_capacity);
  W_PARSEL1_ADD(program->bindings, program->binding_capacity);
  W_PARSEL1_ADD(program->calls, program->call_capacity);
  W_PARSEL1_ADD(program->host_parameters, program->host_parameter_capacity);
  W_PARSEL1_ADD(program->arguments, program->argument_capacity);
  W_PARSEL1_ADD(program->enum_payloads, program->enum_payload_capacity);
  W_PARSEL1_ADD(program->requirements, program->requirement_capacity);
  W_PARSEL1_ADD(program->values, program->value_capacity);
  W_PARSEL1_ADD(program->interpolation_segments,
                program->interpolation_segment_capacity);
  W_PARSEL1_ADD(program->terminators, program->terminator_capacity);
  W_PARSEL1_ADD(program->entries, program->entry_capacity);
  W_PARSEL1_ADD(program->text_bytes, program->text_byte_capacity);
  W_PARSEL1_ADD(program->value_bytes, program->value_byte_capacity);
  W_PARSEL1_ADD(program->receipt, program->receipt_capacity);
  W_PARSEL1_ADD(program->external_modules, program->external_module_capacity);
  W_PARSEL1_ADD(program->external_symbols, program->external_symbol_capacity);
#undef W_PARSEL1_ADD
  return true;
}

static bool outputs_alias_inputs(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection1_output *output,
    const w_seed_parallel_selection1_result *result) {
  if (program == NULL || hir_result == NULL || output == NULL || result == NULL)
    return true;
  selection1_range outputs[3];
  if (!range_make(output, 1u, sizeof(*output), &outputs[0]) ||
      !range_make(output->tasks, output->task_capacity, sizeof(*output->tasks),
                  &outputs[1]) ||
      !range_make(result, 1u, sizeof(*result), &outputs[2]))
    return true;
  for (size_t left = 0u; left < 3u; left += 1u)
    for (size_t right = left + 1u; right < 3u; right += 1u)
      if (ranges_overlap(outputs[left], outputs[right])) return true;
  selection1_range inputs[W_PARSEL1_INPUT_RANGE_CAPACITY];
  size_t count = 0u;
  if (!add_range(inputs, W_PARSEL1_INPUT_RANGE_CAPACITY, &count, hir_result, 1u,
                 sizeof(*hir_result)) ||
      !append_hir_ranges(program, inputs, W_PARSEL1_INPUT_RANGE_CAPACITY,
                         &count))
    return true;
  for (size_t output_index = 0u; output_index < 3u; output_index += 1u)
    for (size_t input_index = 0u; input_index < count; input_index += 1u)
      if (ranges_overlap(outputs[output_index], inputs[input_index]))
        return true;
  return false;
}

static bool measure_outputs_alias_inputs(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection1_counts *counts,
    const w_seed_parallel_selection1_result *result) {
  selection1_range count_range;
  selection1_range result_range;
  if (counts == NULL || result == NULL ||
      !range_make(counts, 1u, sizeof(*counts), &count_range) ||
      !range_make(result, 1u, sizeof(*result), &result_range) ||
      ranges_overlap(count_range, result_range))
    return true;
  selection1_range inputs[W_PARSEL1_INPUT_RANGE_CAPACITY];
  size_t count = 0u;
  if (!add_range(inputs, W_PARSEL1_INPUT_RANGE_CAPACITY, &count, hir_result, 1u,
                 sizeof(*hir_result)) ||
      !append_hir_ranges(program, inputs, W_PARSEL1_INPUT_RANGE_CAPACITY,
                         &count))
    return true;
  for (size_t index = 0u; index < count; index += 1u)
    if (ranges_overlap(count_range, inputs[index]) ||
        ranges_overlap(result_range, inputs[index]))
      return true;
  return false;
}

static bool selection_aliases_inputs(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection1_program *selection,
    const w_seed_parallel_selection1_result *result) {
  if (program == NULL || hir_result == NULL || selection == NULL ||
      result == NULL)
    return true;
  selection1_range outputs[3];
  if (!range_make(selection, 1u, sizeof(*selection), &outputs[0]) ||
      !range_make(selection->tasks, selection->task_capacity,
                  sizeof(*selection->tasks), &outputs[1]) ||
      !range_make(result, 1u, sizeof(*result), &outputs[2]))
    return true;
  for (size_t left = 0u; left < 3u; left += 1u)
    for (size_t right = left + 1u; right < 3u; right += 1u)
      if (ranges_overlap(outputs[left], outputs[right])) return true;
  selection1_range inputs[W_PARSEL1_INPUT_RANGE_CAPACITY];
  size_t count = 0u;
  if (!add_range(inputs, W_PARSEL1_INPUT_RANGE_CAPACITY, &count, hir_result, 1u,
                 sizeof(*hir_result)) ||
      !append_hir_ranges(program, inputs, W_PARSEL1_INPUT_RANGE_CAPACITY,
                         &count))
    return true;
  for (size_t output_index = 0u; output_index < 3u; output_index += 1u)
    for (size_t input_index = 0u; input_index < count; input_index += 1u)
      if (ranges_overlap(outputs[output_index], inputs[input_index]))
        return true;
  return false;
}

static bool text_is(const w_seed_hir0_program *program, w_seed_hir0_text text,
                    const char *literal) {
  if (program == NULL || literal == NULL) return false;
  const size_t bytes = strlen(literal);
  return text.count == bytes && text.offset <= program->text_byte_count &&
         text.count <= program->text_byte_count - text.offset &&
         (bytes == 0u ||
          memcmp(program->text_bytes + text.offset, literal, bytes) == 0);
}

static bool derive_task(const w_seed_hir0_program *program, size_t call_index,
                        w_seed_parallel_selection1_task *task) {
  if (program == NULL || task == NULL || call_index >= program->call_count ||
      call_index > UINT32_MAX)
    return false;
  const w_seed_hir0_call *call = &program->calls[call_index];
  if (call->execution_kind !=
          W_SEED_HIR0_CALL_STRUCTURED_ASYNC_PARALLEL_DOMAIN_DISPATCH ||
      call->placement != W_SEED_HIR0_CALL_PLACEMENT_PARALLEL_DOMAIN ||
      call->domain_mode != W_SEED_FRONTEND_DOMAIN_MODE_CONCURRENT ||
      call->domain_capabilities != W_SEED_FRONTEND_DOMAIN_CAPABILITY_PARALLEL ||
      !text_is(program, call->domain_identity,
               W_SEED_FRONTEND_DOMAIN_IDENTITY) ||
      call->callee_identity >= program->identity_count ||
      call->owner_instruction >= program->instruction_count ||
      call->owner_instruction + 1u >= program->instruction_count)
    return false;
  const w_seed_hir0_identity *identity =
      &program->identities[call->callee_identity];
  const w_seed_hir0_instruction *launch_instruction =
      &program->instructions[call->owner_instruction + 1u];
  if (identity->kind != W_SEED_HIR0_IDENTITY_FUNCTION ||
      identity->target_index >= program->function_count ||
      launch_instruction->kind != W_SEED_HIR0_INSTRUCTION_BINDING ||
      launch_instruction->binding_index >= program->binding_count)
    return false;
  const uint32_t launch_index = launch_instruction->binding_index;
  const w_seed_hir0_binding *launch = &program->bindings[launch_index];
  if (launch->task_role != W_SEED_HIR0_TASK_ROLE_LAUNCH ||
      launch->task_peer_binding >= program->binding_count)
    return false;
  const uint32_t join_index = launch->task_peer_binding;
  const w_seed_hir0_binding *join = &program->bindings[join_index];
  if (join->task_role != W_SEED_HIR0_TASK_ROLE_AWAIT_RESULT ||
      join->task_peer_binding != launch_index)
    return false;
  *task = (w_seed_parallel_selection1_task){
      (uint32_t)call_index, identity->target_index, call->owner_instruction,
      launch_index, join_index};
  return true;
}

static bool derive_summary(const w_seed_hir0_program *program,
                           const w_seed_hir0_result *hir_result,
                           selection1_summary *summary) {
  if (program == NULL || hir_result == NULL || summary == NULL ||
      program->entry_count != 1u ||
      program->entries[0].target_function >= program->function_count)
    return false;
  size_t tasks = 0u;
  for (size_t index = 0u; index < program->call_count; index += 1u) {
    const w_seed_hir0_call_execution_kind kind =
        program->calls[index].execution_kind;
    if (kind == W_SEED_HIR0_CALL_STRUCTURED_ASYNC_PARALLEL_DOMAIN_DISPATCH) {
      w_seed_parallel_selection1_task ignored;
      if (!derive_task(program, index, &ignored)) return false;
      tasks += 1u;
      if (tasks > UINT32_MAX) return false;
    } else if (kind != W_SEED_HIR0_CALL_DIRECT)
      return false;
  }
  if (tasks == 0u) return false;
  *summary = (selection1_summary){
      program->entries[0].target_function,
      tasks,
      W_SEED_HIR0_CALL_PLACEMENT_PARALLEL_DOMAIN,
      W_SEED_FRONTEND_DOMAIN_MODE_CONCURRENT,
      W_SEED_FRONTEND_DOMAIN_CAPABILITY_PARALLEL,
      {0}};
  (void)memcpy(summary->hir_semantic_digest, hir_result->semantic_digest,
               sizeof(summary->hir_semantic_digest));
  return true;
}

static void sha_u32(w_seed_sha256_state *state, uint32_t value) {
  const uint8_t bytes[4] = {(uint8_t)value, (uint8_t)(value >> 8u),
                            (uint8_t)(value >> 16u),
                            (uint8_t)(value >> 24u)};
  w_seed_sha256_update(state, bytes, sizeof(bytes));
}

static void seal(const selection1_summary *summary,
                 const w_seed_parallel_selection1_task *tasks,
                 uint8_t digest[32]) {
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  w_seed_sha256_update(
      &state, (const uint8_t *)W_SEED_PARALLEL_SELECTION1_SCHEMA_VERSION,
      sizeof(W_SEED_PARALLEL_SELECTION1_SCHEMA_VERSION) - 1u);
  w_seed_sha256_update(&state, summary->hir_semantic_digest,
                       sizeof(summary->hir_semantic_digest));
  sha_u32(&state, summary->root_function_index);
  sha_u32(&state, (uint32_t)summary->placement);
  w_seed_sha256_update(&state,
                       (const uint8_t *)W_SEED_FRONTEND_DOMAIN_IDENTITY,
                       sizeof(W_SEED_FRONTEND_DOMAIN_IDENTITY) - 1u);
  sha_u32(&state, (uint32_t)summary->domain_mode);
  sha_u32(&state, summary->domain_capabilities);
  /* derive_summary bounds this semantic cardinality to UINT32_MAX. Encoding it
   * as u32 keeps the digest identical on 32-bit and 64-bit compiler hosts. */
  sha_u32(&state, (uint32_t)summary->task_count);
  for (size_t index = 0u; index < summary->task_count; index += 1u) {
    sha_u32(&state, tasks[index].call_index);
    sha_u32(&state, tasks[index].function_index);
    sha_u32(&state, tasks[index].call_instruction);
    sha_u32(&state, tasks[index].launch_binding);
    sha_u32(&state, tasks[index].join_binding);
  }
  w_seed_sha256_final(&state, digest);
}

static void emit_tasks(const w_seed_hir0_program *program,
                       w_seed_parallel_selection1_task *tasks) {
  size_t written = 0u;
  for (size_t index = 0u; index < program->call_count; index += 1u) {
    if (program->calls[index].execution_kind !=
        W_SEED_HIR0_CALL_STRUCTURED_ASYNC_PARALLEL_DOMAIN_DISPATCH)
      continue;
    const w_seed_hir0_call *call = &program->calls[index];
    const w_seed_hir0_identity *identity =
        &program->identities[call->callee_identity];
    const uint32_t launch_index =
        program->instructions[call->owner_instruction + 1u].binding_index;
    tasks[written] = (w_seed_parallel_selection1_task){
        (uint32_t)index, identity->target_index, call->owner_instruction,
        launch_index, program->bindings[launch_index].task_peer_binding};
    written += 1u;
  }
}

static void make_result(const selection1_summary *summary,
                        const w_seed_parallel_selection1_task *tasks,
                        bool written,
                        w_seed_parallel_selection1_result *result) {
  (void)memset(result, 0, sizeof(*result));
  result->status = W_SEED_PARALLEL_SELECTION1_OK;
  result->required.tasks = summary->task_count;
  result->written.tasks = written ? summary->task_count : 0u;
  (void)memcpy(result->schema, W_SEED_PARALLEL_SELECTION1_SCHEMA_VERSION,
               sizeof(result->schema));
  result->root_function_index = summary->root_function_index;
  result->placement = summary->placement;
  (void)memcpy(result->domain_identity, W_SEED_FRONTEND_DOMAIN_IDENTITY,
               sizeof(result->domain_identity));
  result->domain_mode = summary->domain_mode;
  result->domain_capabilities = summary->domain_capabilities;
  (void)memcpy(result->hir_semantic_digest, summary->hir_semantic_digest,
               sizeof(result->hir_semantic_digest));
  if (written) seal(summary, tasks, result->semantic_digest);
}

w_seed_parallel_selection1_status w_seed_parallel_selection1_measure(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    w_seed_parallel_selection1_counts *counts,
    w_seed_parallel_selection1_result *result) {
  if (program == NULL || hir_result == NULL || counts == NULL || result == NULL)
    return W_SEED_PARALLEL_SELECTION1_INVALID;
  if (!w_seed_hir0_verify(program, hir_result))
    return W_SEED_PARALLEL_SELECTION1_INVALID;
  selection1_summary summary;
  if (!derive_summary(program, hir_result, &summary))
    return W_SEED_PARALLEL_SELECTION1_UNSUPPORTED;
  if (measure_outputs_alias_inputs(program, hir_result, counts, result))
    return W_SEED_PARALLEL_SELECTION1_ALIAS;
  w_seed_parallel_selection1_result candidate;
  make_result(&summary, NULL, false, &candidate);
  *counts = candidate.required;
  *result = candidate;
  return W_SEED_PARALLEL_SELECTION1_OK;
}

w_seed_parallel_selection1_status w_seed_parallel_selection1_run(
    const w_seed_hir0_program *program, const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection1_output *output,
    w_seed_parallel_selection1_result *result) {
  if (program == NULL || hir_result == NULL || output == NULL || result == NULL)
    return W_SEED_PARALLEL_SELECTION1_INVALID;
  if (!w_seed_hir0_verify(program, hir_result))
    return W_SEED_PARALLEL_SELECTION1_INVALID;
  selection1_summary summary;
  if (!derive_summary(program, hir_result, &summary))
    return W_SEED_PARALLEL_SELECTION1_UNSUPPORTED;
  if (output->task_capacity < summary.task_count ||
      (summary.task_count != 0u && output->tasks == NULL))
    return W_SEED_PARALLEL_SELECTION1_CAPACITY;
  if (outputs_alias_inputs(program, hir_result, output, result))
    return W_SEED_PARALLEL_SELECTION1_ALIAS;
  /* All derivation and fallible checks finish before caller storage changes. */
  w_seed_parallel_selection1_task *tasks = output->tasks;
  emit_tasks(program, tasks);
  w_seed_parallel_selection1_result candidate;
  make_result(&summary, tasks, true, &candidate);
  *result = candidate;
  return W_SEED_PARALLEL_SELECTION1_OK;
}

bool w_seed_parallel_selection1_program_from_output(
    const w_seed_parallel_selection1_output *output,
    const w_seed_parallel_selection1_result *result,
    w_seed_parallel_selection1_program *program) {
  if (output == NULL || result == NULL || program == NULL ||
      result->status != W_SEED_PARALLEL_SELECTION1_OK ||
      result->required.tasks == 0u ||
      result->required.tasks != result->written.tasks ||
      result->written.tasks > output->task_capacity || output->tasks == NULL ||
      memcmp(result->schema, W_SEED_PARALLEL_SELECTION1_SCHEMA_VERSION,
             sizeof(result->schema)) != 0 ||
      result->placement != W_SEED_HIR0_CALL_PLACEMENT_PARALLEL_DOMAIN ||
      memcmp(result->domain_identity, W_SEED_FRONTEND_DOMAIN_IDENTITY,
             sizeof(result->domain_identity)) != 0 ||
      result->domain_mode != W_SEED_FRONTEND_DOMAIN_MODE_CONCURRENT ||
      result->domain_capabilities !=
          W_SEED_FRONTEND_DOMAIN_CAPABILITY_PARALLEL)
    return false;
  selection1_range destination;
  selection1_range source;
  if (!range_make(program, 1u, sizeof(*program), &destination) ||
      !range_make(output, 1u, sizeof(*output), &source) ||
      ranges_overlap(destination, source) ||
      !range_make(output->tasks, output->task_capacity,
                  sizeof(*output->tasks), &source) ||
      ranges_overlap(destination, source) ||
      !range_make(result, 1u, sizeof(*result), &source) ||
      ranges_overlap(destination, source))
    return false;
  w_seed_parallel_selection1_program candidate = {
      output->tasks,
      result->written.tasks,
      output->task_capacity,
      result->root_function_index,
      result->placement,
      {0},
      result->domain_mode,
      result->domain_capabilities,
      {0},
      {0}};
  (void)memcpy(candidate.domain_identity, result->domain_identity,
               sizeof(candidate.domain_identity));
  (void)memcpy(candidate.hir_semantic_digest, result->hir_semantic_digest,
               sizeof(candidate.hir_semantic_digest));
  (void)memcpy(candidate.semantic_digest, result->semantic_digest,
               sizeof(candidate.semantic_digest));
  *program = candidate;
  return true;
}

bool w_seed_parallel_selection1_verify(
    const w_seed_hir0_program *hir_program,
    const w_seed_hir0_result *hir_result,
    const w_seed_parallel_selection1_program *selection,
    const w_seed_parallel_selection1_result *result) {
  if (hir_program == NULL || hir_result == NULL || selection == NULL ||
      result == NULL || result->status != W_SEED_PARALLEL_SELECTION1_OK ||
      !w_seed_hir0_verify(hir_program, hir_result) ||
      selection->tasks == NULL || selection->task_count == 0u ||
      selection->task_count > selection->task_capacity ||
      result->required.tasks != selection->task_count ||
      result->written.tasks != selection->task_count ||
      memcmp(result->schema, W_SEED_PARALLEL_SELECTION1_SCHEMA_VERSION,
             sizeof(result->schema)) != 0 ||
      selection_aliases_inputs(hir_program, hir_result, selection, result))
    return false;
  selection1_summary summary;
  if (!derive_summary(hir_program, hir_result, &summary) ||
      summary.task_count != selection->task_count ||
      selection->root_function_index != summary.root_function_index ||
      selection->placement != summary.placement ||
      selection->domain_mode != summary.domain_mode ||
      selection->domain_capabilities != summary.domain_capabilities ||
      memcmp(selection->domain_identity, W_SEED_FRONTEND_DOMAIN_IDENTITY,
             sizeof(selection->domain_identity)) != 0 ||
      memcmp(selection->hir_semantic_digest, summary.hir_semantic_digest,
             sizeof(selection->hir_semantic_digest)) != 0)
    return false;
  for (size_t ordinal = 0u; ordinal < summary.task_count; ordinal += 1u) {
    size_t found = 0u;
    w_seed_parallel_selection1_task expected;
    bool present = false;
    for (size_t index = 0u; index < hir_program->call_count; index += 1u) {
      if (hir_program->calls[index].execution_kind !=
          W_SEED_HIR0_CALL_STRUCTURED_ASYNC_PARALLEL_DOMAIN_DISPATCH)
        continue;
      if (found == ordinal) {
        if (!derive_task(hir_program, index, &expected)) return false;
        present = true;
        break;
      }
      found += 1u;
    }
    if (!present ||
        memcmp(&selection->tasks[ordinal], &expected, sizeof(expected)) != 0)
      return false;
  }
  uint8_t expected_digest[32];
  seal(&summary, selection->tasks, expected_digest);
  return memcmp(selection->semantic_digest, expected_digest,
                sizeof(expected_digest)) == 0 &&
         memcmp(result->semantic_digest, expected_digest,
                sizeof(expected_digest)) == 0 &&
         memcmp(result->hir_semantic_digest, summary.hir_semantic_digest,
                sizeof(result->hir_semantic_digest)) == 0 &&
         result->root_function_index == summary.root_function_index &&
         result->placement == summary.placement &&
         result->domain_mode == summary.domain_mode &&
         result->domain_capabilities == summary.domain_capabilities &&
         memcmp(result->domain_identity, W_SEED_FRONTEND_DOMAIN_IDENTITY,
                sizeof(result->domain_identity)) == 0;
}
