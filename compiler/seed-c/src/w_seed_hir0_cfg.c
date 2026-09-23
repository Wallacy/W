#include "w_seed_hir0_cfg.h"

#include <stdint.h>

static bool records_available(const void *records, size_t count,
                              size_t capacity) {
  return count <= capacity && (count == 0u || records != NULL);
}

static bool range_valid(size_t first, size_t count, size_t total) {
  return first <= total && count <= total - first;
}

typedef struct {
  const void *records;
  size_t capacity;
  size_t record_size;
} memory_region;

static bool address_ranges_overlap(const void *left, size_t left_size,
                                   const void *right, size_t right_size) {
  if (left_size == 0u || right_size == 0u || left == NULL || right == NULL)
    return false;
  const uintptr_t left_start = (uintptr_t)left;
  const uintptr_t right_start = (uintptr_t)right;
  if (left_size > UINTPTR_MAX - left_start ||
      right_size > UINTPTR_MAX - right_start)
    return true;
  const uintptr_t left_end = left_start + left_size;
  const uintptr_t right_end = right_start + right_size;
  return left_start < right_end && right_start < left_end;
}

static bool output_overlaps_program_storage(
    const w_seed_hir0_program *program,
    const w_seed_hir0_cfg_analysis *out) {
  if (address_ranges_overlap(out, sizeof(*out), program, sizeof(*program)))
    return true;

  const memory_region regions[] = {
      {program->modules, program->module_capacity,
       sizeof(*program->modules)},
      {program->identities, program->identity_capacity,
       sizeof(*program->identities)},
      {program->types, program->type_capacity, sizeof(*program->types)},
      {program->enums, program->enum_capacity, sizeof(*program->enums)},
      {program->enum_cases, program->enum_case_capacity,
       sizeof(*program->enum_cases)},
      {program->enum_case_parameters, program->enum_case_parameter_capacity,
       sizeof(*program->enum_case_parameters)},
      {program->enum_subset_members, program->enum_subset_member_capacity,
       sizeof(*program->enum_subset_members)},
      {program->functions, program->function_capacity,
       sizeof(*program->functions)},
      {program->parameters, program->parameter_capacity,
       sizeof(*program->parameters)},
      {program->blocks, program->block_capacity, sizeof(*program->blocks)},
      {program->block_arguments, program->block_argument_capacity,
       sizeof(*program->block_arguments)},
      {program->edge_arguments, program->edge_argument_capacity,
       sizeof(*program->edge_arguments)},
      {program->switch_edges, program->switch_edge_capacity,
       sizeof(*program->switch_edges)},
      {program->switch_captures, program->switch_capture_capacity,
       sizeof(*program->switch_captures)},
      {program->instructions, program->instruction_capacity,
       sizeof(*program->instructions)},
      {program->bindings, program->binding_capacity,
       sizeof(*program->bindings)},
      {program->calls, program->call_capacity, sizeof(*program->calls)},
      {program->host_parameters, program->host_parameter_capacity,
       sizeof(*program->host_parameters)},
      {program->arguments, program->argument_capacity,
       sizeof(*program->arguments)},
      {program->enum_payloads, program->enum_payload_capacity,
       sizeof(*program->enum_payloads)},
      {program->requirements, program->requirement_capacity,
       sizeof(*program->requirements)},
      {program->values, program->value_capacity, sizeof(*program->values)},
      {program->interpolation_segments, program->interpolation_segment_capacity,
       sizeof(*program->interpolation_segments)},
      {program->terminators, program->terminator_capacity,
       sizeof(*program->terminators)},
      {program->entries, program->entry_capacity, sizeof(*program->entries)},
      {program->text_bytes, program->text_byte_capacity,
       sizeof(*program->text_bytes)},
      {program->value_bytes, program->value_byte_capacity,
       sizeof(*program->value_bytes)},
      {program->receipt, program->receipt_capacity,
       sizeof(*program->receipt)},
      {program->external_modules, program->external_module_capacity,
       sizeof(*program->external_modules)},
      {program->external_symbols, program->external_symbol_capacity,
       sizeof(*program->external_symbols)},
      {program->cleanups, program->cleanup_capacity,
       sizeof(*program->cleanups)},
  };
  for (size_t region = 0u;
       region < sizeof(regions) / sizeof(regions[0]); region += 1u) {
    if (regions[region].records == NULL || regions[region].capacity == 0u)
      continue;
    if (regions[region].capacity > SIZE_MAX / regions[region].record_size ||
        address_ranges_overlap(out, sizeof(*out), regions[region].records,
                               regions[region].capacity *
                                   regions[region].record_size))
      return true;
  }
  return false;
}

static bool local_ordinal(size_t first_block, size_t block_count,
                          uint32_t global_block, size_t *ordinal_out) {
  if ((size_t)global_block < first_block) return false;
  const size_t ordinal = (size_t)global_block - first_block;
  if (ordinal >= block_count) return false;
  if (ordinal_out != NULL) *ordinal_out = ordinal;
  return true;
}

static bool cfg_shape_valid(const w_seed_hir0_cfg_analysis *analysis) {
  return analysis != NULL && analysis->block_count != 0u &&
         analysis->block_count <= W_SEED_HIR0_CFG_MAX_BLOCKS &&
         analysis->loop_count <= W_SEED_HIR0_CFG_MAX_LOOPS;
}

static uint64_t ordinal_bit(size_t ordinal) {
  return UINT64_C(1) << ordinal;
}

static size_t bit_count(uint64_t bits) {
  size_t count = 0u;
  while (bits != 0u) {
    bits &= bits - UINT64_C(1);
    count += 1u;
  }
  return count;
}

static bool add_target(const w_seed_hir0_program *program,
                       size_t function_index, size_t first_block,
                       size_t block_count, uint32_t target_block,
                       uint64_t *successors) {
  size_t target_ordinal = 0u;
  if (target_block == W_SEED_HIR0_NONE ||
      !local_ordinal(first_block, block_count, target_block,
                     &target_ordinal) ||
      target_block >= program->block_count ||
      program->blocks[target_block].owner_function != function_index)
    return false;
  *successors |= ordinal_bit(target_ordinal);
  return true;
}

static bool graph_is_acyclic_without_backedges(
    const w_seed_hir0_cfg_analysis *analysis) {
  size_t indegree[W_SEED_HIR0_CFG_MAX_BLOCKS] = {0u};
  size_t queue[W_SEED_HIR0_CFG_MAX_BLOCKS] = {0u};
  size_t queue_count = 0u;
  size_t queue_cursor = 0u;
  const uint64_t reachable = analysis->reachable_blocks;

  for (size_t source = 0u; source < analysis->block_count; source += 1u) {
    const uint64_t edges = analysis->successors[source] & reachable &
                           ~analysis->backedge_targets[source];
    for (size_t target = 0u; target < analysis->block_count; target += 1u) {
      if ((edges & ordinal_bit(target)) != 0u) indegree[target] += 1u;
    }
  }
  for (size_t block = 0u; block < analysis->block_count; block += 1u) {
    if ((reachable & ordinal_bit(block)) != 0u && indegree[block] == 0u)
      queue[queue_count++] = block;
  }

  while (queue_cursor < queue_count) {
    const size_t source = queue[queue_cursor++];
    const uint64_t edges = analysis->successors[source] & reachable &
                           ~analysis->backedge_targets[source];
    for (size_t target = 0u; target < analysis->block_count; target += 1u) {
      if ((edges & ordinal_bit(target)) == 0u) continue;
      if (indegree[target] == 0u) return false;
      indegree[target] -= 1u;
      if (indegree[target] == 0u) queue[queue_count++] = target;
    }
  }
  return queue_count == bit_count(reachable);
}

static bool build_natural_loops(w_seed_hir0_cfg_analysis *analysis) {
  for (size_t header = 0u; header < analysis->block_count; header += 1u) {
    const uint64_t header_bit = ordinal_bit(header);
    uint64_t latch_sources = 0u;
    for (size_t source = 0u; source < analysis->block_count; source += 1u) {
      if ((analysis->backedge_targets[source] & header_bit) != 0u)
        latch_sources |= ordinal_bit(source);
    }
    if (latch_sources == 0u) continue;
    if (analysis->loop_count >= W_SEED_HIR0_CFG_MAX_LOOPS) return false;

    w_seed_hir0_cfg_loop *loop = &analysis->loops[analysis->loop_count];
    loop->header_block = analysis->first_block + (uint32_t)header;
    loop->backedge_sources = latch_sources;
    loop->member_blocks = header_bit | latch_sources;

    size_t worklist[W_SEED_HIR0_CFG_MAX_BLOCKS] = {0u};
    size_t worklist_count = 0u;
    for (size_t source = 0u; source < analysis->block_count; source += 1u) {
      if ((latch_sources & ordinal_bit(source)) != 0u && source != header)
        worklist[worklist_count++] = source;
    }
    size_t worklist_cursor = 0u;
    while (worklist_cursor < worklist_count) {
      const size_t block = worklist[worklist_cursor++];
      const uint64_t block_bit = ordinal_bit(block);
      for (size_t predecessor = 0u;
           predecessor < analysis->block_count; predecessor += 1u) {
        const uint64_t predecessor_bit = ordinal_bit(predecessor);
        if ((analysis->reachable_blocks & predecessor_bit) == 0u ||
            (analysis->successors[predecessor] & block_bit) == 0u ||
            (loop->member_blocks & predecessor_bit) != 0u)
          continue;
        loop->member_blocks |= predecessor_bit;
        if (predecessor != header)
          worklist[worklist_count++] = predecessor;
      }
    }

    /* A natural loop header must dominate every block pulled into its
     * reverse-reachable body. A violation is a multi-entry cycle. */
    for (size_t block = 0u; block < analysis->block_count; block += 1u) {
      if ((loop->member_blocks & ordinal_bit(block)) != 0u &&
          (analysis->dominators[block] & header_bit) == 0u)
        return false;
    }
    analysis->loop_count += 1u;
  }

  for (size_t left = 0u; left < analysis->loop_count; left += 1u) {
    for (size_t right = left + 1u; right < analysis->loop_count;
         right += 1u) {
      const uint64_t left_members = analysis->loops[left].member_blocks;
      const uint64_t right_members = analysis->loops[right].member_blocks;
      const uint64_t overlap = left_members & right_members;
      if (overlap != 0u && (left_members & ~right_members) != 0u &&
          (right_members & ~left_members) != 0u)
        return false;
    }
  }
  return true;
}

bool w_seed_hir0_cfg_analyze(const w_seed_hir0_program *program,
                             size_t function_index,
                             w_seed_hir0_cfg_analysis *out) {
  if (program == NULL || out == NULL || function_index > UINT32_MAX ||
      !records_available(program->functions, program->function_count,
                         program->function_capacity) ||
      !records_available(program->blocks, program->block_count,
                         program->block_capacity) ||
      !records_available(program->terminators, program->terminator_count,
                         program->terminator_capacity) ||
      !records_available(program->instructions, program->instruction_count,
                         program->instruction_capacity) ||
      function_index >= program->function_count)
    return false;
  if (output_overlaps_program_storage(program, out)) return false;

  const w_seed_hir0_function *function = &program->functions[function_index];
  const size_t first_block = function->first_block;
  const size_t block_count = function->block_count;
  if (block_count == 0u || block_count > W_SEED_HIR0_CFG_MAX_BLOCKS ||
      !range_valid(first_block, block_count, program->block_count) ||
      first_block + block_count - 1u > UINT32_MAX)
    return false;

  w_seed_hir0_cfg_analysis result = {0};
  result.function_index = (uint32_t)function_index;
  result.first_block = function->first_block;
  result.block_count = block_count;

  for (size_t ordinal = 0u; ordinal < block_count; ordinal += 1u) {
    const size_t block_index = first_block + ordinal;
    const w_seed_hir0_block *block = &program->blocks[block_index];
    if (block->owner_function != function_index || block->ordinal != ordinal ||
        block->next_block != W_SEED_HIR0_NONE ||
        !range_valid(block->first_instruction, block->instruction_count,
                     program->instruction_count) ||
        block->terminator_index >= program->terminator_count)
      return false;

    const w_seed_hir0_terminator *terminator =
        &program->terminators[block->terminator_index];
    if (terminator->owner_block != block_index ||
        terminator->ordinal != block->instruction_count ||
        terminator->third_block != W_SEED_HIR0_NONE)
      return false;

    uint64_t successors = 0u;
    switch (terminator->kind) {
      case W_SEED_HIR0_TERMINATOR_BRANCH:
        if (terminator->target_block == terminator->else_block ||
            terminator->first_edge_argument != W_SEED_HIR0_NONE ||
            terminator->edge_argument_count != 0u ||
            !add_target(program, function_index, first_block, block_count,
                        terminator->target_block, &successors) ||
            !add_target(program, function_index, first_block, block_count,
                        terminator->else_block, &successors))
          return false;
        break;
      case W_SEED_HIR0_TERMINATOR_JUMP:
        if (terminator->else_block != W_SEED_HIR0_NONE ||
            !add_target(program, function_index, first_block, block_count,
                        terminator->target_block, &successors))
          return false;
        break;
      case W_SEED_HIR0_TERMINATOR_RETURN_UNIT:
      case W_SEED_HIR0_TERMINATOR_RETURN_VALUE:
        if (terminator->target_block != W_SEED_HIR0_NONE ||
            terminator->else_block != W_SEED_HIR0_NONE ||
            terminator->first_edge_argument != W_SEED_HIR0_NONE ||
            terminator->edge_argument_count != 0u)
          return false;
        break;
      default:
        return false;
    }
    result.successors[ordinal] = successors;
  }

  size_t worklist[W_SEED_HIR0_CFG_MAX_BLOCKS] = {0u};
  size_t worklist_count = 1u;
  size_t worklist_cursor = 0u;
  worklist[0] = 0u;
  result.reachable_blocks = ordinal_bit(0u);
  while (worklist_cursor < worklist_count) {
    const size_t source = worklist[worklist_cursor++];
    uint64_t targets = result.successors[source];
    for (size_t target = 0u; target < block_count; target += 1u) {
      const uint64_t target_bit = ordinal_bit(target);
      if ((targets & target_bit) == 0u ||
          (result.reachable_blocks & target_bit) != 0u)
        continue;
      result.reachable_blocks |= target_bit;
      worklist[worklist_count++] = target;
    }
  }

  for (size_t block = 0u; block < block_count; block += 1u) {
    if ((result.reachable_blocks & ordinal_bit(block)) != 0u)
      result.dominators[block] = result.reachable_blocks;
  }
  result.dominators[0] = ordinal_bit(0u);
  bool changed = true;
  while (changed) {
    changed = false;
    for (size_t block = 1u; block < block_count; block += 1u) {
      const uint64_t block_bit = ordinal_bit(block);
      if ((result.reachable_blocks & block_bit) == 0u) continue;

      uint64_t common_dominators = result.reachable_blocks;
      bool has_predecessor = false;
      for (size_t predecessor = 0u; predecessor < block_count;
           predecessor += 1u) {
        const uint64_t predecessor_bit = ordinal_bit(predecessor);
        if ((result.reachable_blocks & predecessor_bit) == 0u ||
            (result.successors[predecessor] & block_bit) == 0u)
          continue;
        common_dominators &= result.dominators[predecessor];
        has_predecessor = true;
      }
      if (!has_predecessor) return false;
      const uint64_t updated = common_dominators | block_bit;
      if (updated != result.dominators[block]) {
        result.dominators[block] = updated;
        changed = true;
      }
    }
  }

  for (size_t source = 0u; source < block_count; source += 1u) {
    const uint64_t source_bit = ordinal_bit(source);
    if ((result.reachable_blocks & source_bit) == 0u) continue;
    for (size_t target = 0u; target < block_count; target += 1u) {
      const uint64_t target_bit = ordinal_bit(target);
      if ((result.successors[source] & target_bit) != 0u &&
          (result.dominators[source] & target_bit) != 0u)
        result.backedge_targets[source] |= target_bit;
    }
  }

  if (!graph_is_acyclic_without_backedges(&result) ||
      !build_natural_loops(&result))
    return false;

  *out = result;
  return true;
}

bool w_seed_hir0_cfg_is_reachable(const w_seed_hir0_cfg_analysis *analysis,
                                  uint32_t block_index) {
  size_t ordinal = 0u;
  return cfg_shape_valid(analysis) &&
         local_ordinal(analysis->first_block, analysis->block_count,
                       block_index, &ordinal) &&
         (analysis->reachable_blocks & ordinal_bit(ordinal)) != 0u;
}

bool w_seed_hir0_cfg_dominates(const w_seed_hir0_cfg_analysis *analysis,
                               uint32_t dominator_block,
                               uint32_t block_index) {
  size_t dominator = 0u;
  size_t block = 0u;
  return cfg_shape_valid(analysis) &&
         local_ordinal(analysis->first_block, analysis->block_count,
                       dominator_block, &dominator) &&
         local_ordinal(analysis->first_block, analysis->block_count,
                       block_index, &block) &&
         (analysis->reachable_blocks & ordinal_bit(dominator)) != 0u &&
         (analysis->reachable_blocks & ordinal_bit(block)) != 0u &&
         (analysis->dominators[block] & ordinal_bit(dominator)) != 0u;
}

bool w_seed_hir0_cfg_is_backedge(const w_seed_hir0_cfg_analysis *analysis,
                                 uint32_t source_block,
                                 uint32_t target_block) {
  size_t source = 0u;
  size_t target = 0u;
  return cfg_shape_valid(analysis) &&
         local_ordinal(analysis->first_block, analysis->block_count,
                       source_block, &source) &&
         local_ordinal(analysis->first_block, analysis->block_count,
                       target_block, &target) &&
         (analysis->backedge_targets[source] & ordinal_bit(target)) != 0u;
}

bool w_seed_hir0_cfg_loop_for_header(
    const w_seed_hir0_cfg_analysis *analysis, uint32_t header_block,
    w_seed_hir0_cfg_loop *out) {
  if (!cfg_shape_valid(analysis) || out == NULL) return false;
  for (size_t loop_index = 0u; loop_index < analysis->loop_count;
       loop_index += 1u) {
    if (analysis->loops[loop_index].header_block == header_block) {
      *out = analysis->loops[loop_index];
      return true;
    }
  }
  return false;
}

bool w_seed_hir0_cfg_is_in_loop(const w_seed_hir0_cfg_analysis *analysis,
                                uint32_t header_block,
                                uint32_t block_index) {
  size_t ordinal = 0u;
  w_seed_hir0_cfg_loop loop = {0};
  return cfg_shape_valid(analysis) &&
         local_ordinal(analysis->first_block, analysis->block_count,
                       block_index, &ordinal) &&
         w_seed_hir0_cfg_loop_for_header(analysis, header_block, &loop) &&
         (loop.member_blocks & ordinal_bit(ordinal)) != 0u;
}
