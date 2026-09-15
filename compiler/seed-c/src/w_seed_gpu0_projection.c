#include "w_seed_gpu0_projection.h"

#include <limits.h>
#include <string.h>

typedef struct {
  uintptr_t begin;
  uintptr_t end;
  bool active;
} gpu0_projection_range;

static bool projection_range_make(const void *pointer, size_t count,
                                  size_t element_size,
                                  gpu0_projection_range *range) {
  if (range == NULL || element_size == 0u) return false;
  *range = (gpu0_projection_range){0u, 0u, false};
  if (count == 0u) return true;
  if (pointer == NULL || count > SIZE_MAX / element_size) return false;
  const size_t bytes = count * element_size;
  const uintptr_t begin = (uintptr_t)pointer;
  if (bytes > (size_t)UINTPTR_MAX ||
      begin > UINTPTR_MAX - (uintptr_t)bytes)
    return false;
  *range = (gpu0_projection_range){begin, begin + (uintptr_t)bytes, true};
  return true;
}

static bool projection_ranges_overlap(gpu0_projection_range left,
                                      gpu0_projection_range right) {
  return left.active && right.active && left.begin < right.end &&
         right.begin < left.end;
}

static bool projection_append_range(gpu0_projection_range *ranges,
                                    size_t capacity, size_t *count,
                                    const void *pointer, size_t elements,
                                    size_t element_size) {
  if (ranges == NULL || count == NULL || *count >= capacity) return false;
  gpu0_projection_range range;
  if (!projection_range_make(pointer, elements, element_size, &range))
    return false;
  ranges[*count] = range;
  *count += 1u;
  return true;
}

static bool projection_add_size(size_t left, size_t right, size_t *result) {
  if (result == NULL || right > SIZE_MAX - left) return false;
  *result = left + right;
  return true;
}

static bool projection_destinations_free(
    const w_seed_gpu_module_program *module_program,
    const w_seed_gpu_module_result *module_result,
    const w_seed_gpu0_projection_output *output,
    const w_seed_gpu0_program *program) {
  gpu0_projection_range sources[8];
  gpu0_projection_range destinations[8];
  size_t source_count = 0u;
  size_t destination_count = 0u;
  if (!projection_append_range(sources, 8u, &source_count, module_program, 1u,
                               sizeof(*module_program)) ||
      !projection_append_range(sources, 8u, &source_count, module_result, 1u,
                               sizeof(*module_result)) ||
      !projection_append_range(sources, 8u, &source_count,
                               module_program->modules,
                               module_program->module_capacity,
                               sizeof(*module_program->modules)) ||
      !projection_append_range(sources, 8u, &source_count,
                               module_program->kernels,
                               module_program->kernel_capacity,
                               sizeof(*module_program->kernels)) ||
      !projection_append_range(sources, 8u, &source_count, module_program->text,
                               module_program->text_capacity, sizeof(uint8_t)) ||
      !projection_append_range(sources, 8u, &source_count,
                               module_program->frontend_receipt,
                               module_program->frontend_receipt_capacity,
                               sizeof(uint8_t)) ||
      !projection_append_range(destinations, 8u, &destination_count, output,
                               1u, sizeof(*output)) ||
      !projection_append_range(destinations, 8u, &destination_count, program,
                               1u, sizeof(*program)) ||
      !projection_append_range(destinations, 8u, &destination_count,
                               output->functions,
                               output->function_capacity,
                               sizeof(*output->functions)) ||
      !projection_append_range(destinations, 8u, &destination_count,
                               output->operations,
                               output->operation_capacity,
                               sizeof(*output->operations)) ||
      !projection_append_range(destinations, 8u, &destination_count,
                               output->text, output->text_capacity,
                               sizeof(*output->text)))
    return false;

  for (size_t left = 0u; left < destination_count; left += 1u) {
    for (size_t right = left + 1u; right < destination_count; right += 1u)
      if (projection_ranges_overlap(destinations[left], destinations[right]))
        return false;
    for (size_t source = 0u; source < source_count; source += 1u)
      if (projection_ranges_overlap(destinations[left], sources[source]))
        return false;
  }
  return true;
}

static bool projection_text(const w_seed_gpu_module_program *program,
                            size_t offset, size_t bytes, const char **text) {
  if (program == NULL || text == NULL || offset > program->text_bytes ||
      bytes > program->text_bytes - offset)
    return false;
  *text = bytes == 0u ? NULL : (const char *)(program->text + offset);
  return true;
}

bool w_seed_gpu0_program_from_gpu_module(
    const w_seed_gpu_module_program *module_program,
    const w_seed_gpu_module_result *module_result, size_t module_index,
    size_t kernel_ordinal, const w_seed_gpu0_projection_output *output,
    w_seed_gpu0_program *program) {
  if (module_program == NULL || module_result == NULL || output == NULL ||
      program == NULL || output->functions == NULL ||
      output->operations == NULL ||
      output->text == NULL ||
      output->function_capacity < W_SEED_GPU0_EVIDENCE_FUNCTION_CAPACITY ||
      output->operation_capacity < W_SEED_GPU0_EVIDENCE_OPERATION_CAPACITY ||
      !w_seed_gpu_module_verify(module_program, module_result) ||
      !projection_destinations_free(module_program, module_result, output,
                                     program) ||
      module_index >= module_program->module_count)
    return false;

  const w_seed_gpu_module_record *module =
      &module_program->modules[module_index];
  if (kernel_ordinal >= module->kernel_count ||
      module->first_kernel > SIZE_MAX - kernel_ordinal)
    return false;
  const size_t kernel_index = module->first_kernel + kernel_ordinal;
  if (kernel_index >= module_program->kernel_count ||
      module_program->kernels[kernel_index].payload < INT32_MIN ||
      module_program->kernels[kernel_index].payload > INT32_MAX)
    return false;
  const w_seed_gpu_module_kernel *kernel =
      &module_program->kernels[kernel_index];

  const char *host_name = NULL;
  const char *interface_name = NULL;
  const char *implementation_name = NULL;
  if (!projection_text(module_program, module->kernel_contract_name_offset,
                       module->kernel_contract_name_bytes, &host_name) ||
      !projection_text(module_program, kernel->label_offset, kernel->label_bytes,
                       &interface_name) ||
      !projection_text(module_program, kernel->function_name_offset,
                       kernel->function_name_bytes, &implementation_name) ||
      host_name == NULL || interface_name == NULL || implementation_name == NULL)
    return false;

  const int32_t payload = (int32_t)kernel->payload;
  size_t interface_offset = 0u;
  size_t implementation_offset = 0u;
  size_t text_bytes = 0u;
  if (!projection_add_size(module->kernel_contract_name_bytes, kernel->label_bytes,
                           &implementation_offset) ||
      !projection_add_size(implementation_offset,
                           kernel->function_name_bytes, &text_bytes) ||
      text_bytes > output->text_capacity)
    return false;
  interface_offset = module->kernel_contract_name_bytes;
  const w_seed_gpu0_range host_result = {
      W_SEED_GPU0_ADDRESS_HOST, 0u, W_SEED_GPU0_RESULT_BYTES,
      W_SEED_GPU0_RESULT_BYTES};
  const w_seed_gpu0_range device_result = {
      W_SEED_GPU0_ADDRESS_DEVICE, 0u, W_SEED_GPU0_RESULT_BYTES,
      W_SEED_GPU0_RESULT_BYTES};
  const w_seed_gpu0_range empty = {0};
  const w_seed_gpu0_function functions[
      W_SEED_GPU0_EVIDENCE_FUNCTION_CAPACITY] = {
      {.name = host_name,
       .name_length = module->kernel_contract_name_bytes,
       .interface_name = NULL,
       .interface_name_length = 0u,
       .role = W_SEED_GPU0_FUNCTION_HOST_ROOT,
       .first_operation = 0u,
       .operation_count = 6u,
       .parameter_count = 0u,
       .effects = W_SEED_GPU0_EFFECT_NONE},
      {.name = implementation_name,
       .name_length = kernel->function_name_bytes,
       .interface_name = interface_name,
       .interface_name_length = kernel->label_bytes,
       .role = W_SEED_GPU0_FUNCTION_DEVICE_KERNEL,
       .first_operation = 6u,
       .operation_count = 1u,
       .parameter_count = 1u,
       .effects = W_SEED_GPU0_EFFECT_NONE},
  };
  const w_seed_gpu0_operation operations[
      W_SEED_GPU0_EVIDENCE_OPERATION_CAPACITY] = {
      {.kind = W_SEED_GPU0_OPERATION_ALLOCATE_DEVICE_RESULT,
       .source = empty,
       .destination = device_result,
       .function_index = W_SEED_GPU0_NONE,
       .dependency_operation = W_SEED_GPU0_NONE,
       .i32_value = 0},
      {.kind = W_SEED_GPU0_OPERATION_COPY_HOST_TO_DEVICE,
       .source = host_result,
       .destination = device_result,
       .function_index = W_SEED_GPU0_NONE,
       .dependency_operation = W_SEED_GPU0_NONE,
       .i32_value = 0},
      {.kind = W_SEED_GPU0_OPERATION_LAUNCH,
       .source = empty,
       .destination = empty,
       .function_index = 1u,
       .dependency_operation = W_SEED_GPU0_NONE,
       .i32_value = 0},
      {.kind = W_SEED_GPU0_OPERATION_JOIN,
       .source = empty,
       .destination = empty,
       .function_index = W_SEED_GPU0_NONE,
       .dependency_operation = 2u,
       .i32_value = 0},
      {.kind = W_SEED_GPU0_OPERATION_COPY_DEVICE_TO_HOST,
       .source = device_result,
       .destination = host_result,
       .function_index = W_SEED_GPU0_NONE,
       .dependency_operation = W_SEED_GPU0_NONE,
       .i32_value = 0},
      {.kind = W_SEED_GPU0_OPERATION_VERIFY_RESULT,
       .source = host_result,
       .destination = empty,
       .function_index = W_SEED_GPU0_NONE,
       .dependency_operation = W_SEED_GPU0_NONE,
       .i32_value = payload},
      {.kind = W_SEED_GPU0_OPERATION_STORE_I32,
       .source = empty,
       .destination = device_result,
       .function_index = W_SEED_GPU0_NONE,
       .dependency_operation = W_SEED_GPU0_NONE,
       .i32_value = payload},
  };
  const w_seed_gpu0_program staged = {
      .functions = functions,
      .function_count = W_SEED_GPU0_EVIDENCE_FUNCTION_CAPACITY,
      .function_capacity = W_SEED_GPU0_EVIDENCE_FUNCTION_CAPACITY,
      .operations = operations,
      .operation_count = W_SEED_GPU0_EVIDENCE_OPERATION_CAPACITY,
      .operation_capacity = W_SEED_GPU0_EVIDENCE_OPERATION_CAPACITY,
      .host_result_capacity_bytes = W_SEED_GPU0_RESULT_BYTES,
      .device_result_capacity_bytes = W_SEED_GPU0_RESULT_BYTES};
  w_seed_gpu0_measurement measurement;
  if (w_seed_gpu0_measure(&staged, &measurement) != W_SEED_GPU0_OK)
    return false;

  /* All validation and artifact measurement above are source-only. Commit the
   * copied identities and records only after the final failure point. */
  (void)memcpy(output->text, host_name, module->kernel_contract_name_bytes);
  (void)memcpy(output->text + interface_offset, interface_name,
               kernel->label_bytes);
  (void)memcpy(output->text + implementation_offset, implementation_name,
               kernel->function_name_bytes);
  w_seed_gpu0_function projected_functions[
      W_SEED_GPU0_EVIDENCE_FUNCTION_CAPACITY];
  (void)memcpy(projected_functions, functions, sizeof(projected_functions));
  projected_functions[0].name = output->text;
  projected_functions[1].name = output->text + implementation_offset;
  projected_functions[1].interface_name = output->text + interface_offset;
  (void)memcpy(output->functions, projected_functions,
               sizeof(projected_functions));
  (void)memcpy(output->operations, operations, sizeof(operations));
  const w_seed_gpu0_program candidate = {
      .functions = output->functions,
      .function_count = W_SEED_GPU0_EVIDENCE_FUNCTION_CAPACITY,
      .function_capacity = W_SEED_GPU0_EVIDENCE_FUNCTION_CAPACITY,
      .operations = output->operations,
      .operation_count = W_SEED_GPU0_EVIDENCE_OPERATION_CAPACITY,
      .operation_capacity = W_SEED_GPU0_EVIDENCE_OPERATION_CAPACITY,
      .host_result_capacity_bytes = W_SEED_GPU0_RESULT_BYTES,
      .device_result_capacity_bytes = W_SEED_GPU0_RESULT_BYTES};
  *program = candidate;
  return true;
}
