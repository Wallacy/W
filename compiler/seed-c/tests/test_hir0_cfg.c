#include "w_seed_hir0_cfg.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TEST_MAX_BLOCKS (W_SEED_HIR0_CFG_MAX_BLOCKS + 1u)
#define CFG_ORACLE_MAX_BLOCKS 6u
#define CFG_ORACLE_EXHAUSTIVE_BLOCKS 4u

typedef struct {
  w_seed_hir0_program program;
  w_seed_hir0_function functions[2];
  w_seed_hir0_block blocks[TEST_MAX_BLOCKS];
  w_seed_hir0_terminator terminators[TEST_MAX_BLOCKS];
} cfg_fixture;

typedef union {
  max_align_t alignment;
  w_seed_hir0_terminator terminators[TEST_MAX_BLOCKS];
  w_seed_hir0_cfg_analysis analysis;
} cfg_alias_storage;

typedef struct {
  size_t header;
  uint64_t members;
  uint64_t latches;
} cfg_oracle_loop;

typedef struct {
  uint64_t reachable;
  uint64_t successors[CFG_ORACLE_MAX_BLOCKS];
  uint64_t dominators[CFG_ORACLE_MAX_BLOCKS];
  uint64_t backedge_targets[CFG_ORACLE_MAX_BLOCKS];
  size_t loop_count;
  cfg_oracle_loop loops[CFG_ORACLE_MAX_BLOCKS];
  bool residual_cycle;
  bool irreducible_scc;
  bool crossing_simple_cycles;
  bool bad_loop_header;
  bool too_many_loops;
  bool non_laminar_loops;
} cfg_oracle_result;

typedef struct {
  size_t graph_count;
  size_t accepted_count;
  size_t irreducible_rejections;
  size_t crossing_cycle_rejections;
  size_t nested_loop_acceptances;
} cfg_oracle_stats;

#define CHECK(condition)                                                     \
  do {                                                                       \
    if (!(condition)) {                                                      \
      (void)fprintf(stderr, "hir0 cfg check failed: %s (%s:%d)\n", #condition, \
                    __FILE__, __LINE__);                                    \
      return false;                                                          \
    }                                                                        \
  } while (0)

static void initialize_function(cfg_fixture *fixture, size_t function_index,
                                uint32_t first_block, uint32_t block_count) {
  fixture->functions[function_index].first_block = first_block;
  fixture->functions[function_index].block_count = block_count;
  fixture->program.functions = fixture->functions;
  fixture->program.function_count = function_index + 1u;
  fixture->program.function_capacity = 2u;
}

static void initialize_fixture(cfg_fixture *fixture, size_t block_count) {
  (void)memset(fixture, 0, sizeof(*fixture));
  initialize_function(fixture, 0u, 0u, (uint32_t)block_count);
  fixture->program.blocks = fixture->blocks;
  fixture->program.block_count = block_count;
  fixture->program.block_capacity = TEST_MAX_BLOCKS;
  fixture->program.terminators = fixture->terminators;
  fixture->program.terminator_count = block_count;
  fixture->program.terminator_capacity = TEST_MAX_BLOCKS;
  for (size_t block = 0u; block < block_count; block += 1u) {
    fixture->blocks[block].owner_function = 0u;
    fixture->blocks[block].ordinal = (uint32_t)block;
    fixture->blocks[block].terminator_index = (uint32_t)block;
    fixture->blocks[block].next_block = W_SEED_HIR0_NONE;
    fixture->terminators[block].owner_block = (uint32_t)block;
    fixture->terminators[block].kind = W_SEED_HIR0_TERMINATOR_RETURN_UNIT;
    fixture->terminators[block].ordinal = 0u;
    fixture->terminators[block].target_block = W_SEED_HIR0_NONE;
    fixture->terminators[block].else_block = W_SEED_HIR0_NONE;
    fixture->terminators[block].third_block = W_SEED_HIR0_NONE;
    fixture->terminators[block].first_edge_argument = W_SEED_HIR0_NONE;
  }
}

static void set_jump(cfg_fixture *fixture, size_t source, uint32_t target) {
  w_seed_hir0_terminator *term = &fixture->terminators[source];
  term->kind = W_SEED_HIR0_TERMINATOR_JUMP;
  term->target_block = target;
  term->else_block = W_SEED_HIR0_NONE;
}

static void set_branch(cfg_fixture *fixture, size_t source, uint32_t target,
                       uint32_t else_block) {
  w_seed_hir0_terminator *term = &fixture->terminators[source];
  term->kind = W_SEED_HIR0_TERMINATOR_BRANCH;
  term->target_block = target;
  term->else_block = else_block;
}

static void initialize_offset_fixture(cfg_fixture *fixture,
                                      size_t block_count) {
  initialize_fixture(fixture, block_count + 1u);
  initialize_function(fixture, 0u, 1u, (uint32_t)block_count);
  initialize_function(fixture, 1u, 0u, 1u);
  fixture->blocks[0].owner_function = 1u;
  fixture->blocks[0].ordinal = 0u;
  for (size_t block = 0u; block < block_count; block += 1u) {
    fixture->blocks[block + 1u].owner_function = 0u;
    fixture->blocks[block + 1u].ordinal = (uint32_t)block;
  }
}

static uint64_t oracle_bit(size_t ordinal) {
  return UINT64_C(1) << ordinal;
}

static size_t oracle_take_first_bit(uint64_t *bits) {
  for (size_t bit = 0u; bit < CFG_ORACLE_MAX_BLOCKS; bit += 1u) {
    if ((*bits & oracle_bit(bit)) == 0u) continue;
    *bits &= ~oracle_bit(bit);
    return bit;
  }
  return CFG_ORACLE_MAX_BLOCKS;
}

static uint64_t oracle_reachable_from(const uint64_t *successors,
                                     size_t block_count, size_t start,
                                     size_t blocked) {
  if (start >= block_count || start == blocked) return 0u;
  size_t queue[CFG_ORACLE_MAX_BLOCKS] = {0u};
  size_t queue_count = 1u;
  size_t queue_cursor = 0u;
  uint64_t reachable = oracle_bit(start);
  queue[0] = start;
  while (queue_cursor < queue_count) {
    const size_t source = queue[queue_cursor++];
    uint64_t targets = successors[source];
    while (targets != 0u) {
      const size_t target = oracle_take_first_bit(&targets);
      const uint64_t target_bit = oracle_bit(target);
      if (target == blocked || (reachable & target_bit) != 0u) continue;
      reachable |= target_bit;
      queue[queue_count++] = target;
    }
  }
  return reachable;
}

static bool oracle_dfs_cycle(const uint64_t *successors, uint64_t reachable,
                             size_t block_count, size_t source,
                             uint8_t *colors,
                             const uint64_t *removed_backedges) {
  colors[source] = 1u;
  uint64_t targets = successors[source] & reachable;
  if (removed_backedges != NULL) targets &= ~removed_backedges[source];
  while (targets != 0u) {
    const size_t target = oracle_take_first_bit(&targets);
    if (colors[target] == 1u) return true;
    if (colors[target] == 0u &&
        oracle_dfs_cycle(successors, reachable, block_count, target, colors,
                         removed_backedges))
      return true;
  }
  colors[source] = 2u;
  (void)block_count;
  return false;
}

static bool oracle_has_cycle(const uint64_t *successors, uint64_t reachable,
                             size_t block_count,
                             const uint64_t *removed_backedges) {
  uint8_t colors[CFG_ORACLE_MAX_BLOCKS] = {0u};
  for (size_t block = 0u; block < block_count; block += 1u) {
    if ((reachable & oracle_bit(block)) != 0u && colors[block] == 0u &&
        oracle_dfs_cycle(successors, reachable, block_count, block, colors,
                         removed_backedges))
      return true;
  }
  return false;
}

static bool oracle_has_irreducible_scc(const uint64_t *successors,
                                       uint64_t reachable,
                                       size_t block_count) {
  uint64_t from[CFG_ORACLE_MAX_BLOCKS] = {0u};
  uint64_t assigned = 0u;
  for (size_t source = 0u; source < block_count; source += 1u) {
    from[source] = oracle_reachable_from(successors, block_count, source,
                                         block_count);
  }

  for (size_t representative = 0u; representative < block_count;
       representative += 1u) {
    const uint64_t representative_bit = oracle_bit(representative);
    if ((reachable & representative_bit) == 0u ||
        (assigned & representative_bit) != 0u)
      continue;
    uint64_t component = 0u;
    for (size_t block = 0u; block < block_count; block += 1u) {
      const uint64_t block_bit = oracle_bit(block);
      if ((from[representative] & block_bit) != 0u &&
          (from[block] & representative_bit) != 0u)
        component |= block_bit;
    }
    assigned |= component;
    const bool cyclic = (component & (component - UINT64_C(1))) != 0u ||
                        (successors[representative] & representative_bit) !=
                            0u;
    if (!cyclic) continue;

    uint64_t entry_targets =
        (component & oracle_bit(0u)) != 0u ? oracle_bit(0u) : 0u;
    for (size_t source = 0u; source < block_count; source += 1u) {
      const uint64_t source_bit = oracle_bit(source);
      if ((reachable & source_bit) == 0u ||
          (component & source_bit) != 0u)
        continue;
      entry_targets |= successors[source] & component;
    }
    if ((entry_targets & (entry_targets - UINT64_C(1))) != 0u) return true;
  }
  return false;
}

static void oracle_collect_simple_cycles(const uint64_t *successors,
                                         uint64_t reachable,
                                         size_t block_count, size_t start,
                                         size_t current, uint64_t visited,
                                         bool *cycle_vertex_sets) {
  uint64_t targets = successors[current] & reachable;
  while (targets != 0u) {
    const size_t target = oracle_take_first_bit(&targets);
    if (target == start) {
      cycle_vertex_sets[visited] = true;
    } else if (target > start &&
               (visited & oracle_bit(target)) == 0u) {
      oracle_collect_simple_cycles(successors, reachable, block_count, start,
                                   target, visited | oracle_bit(target),
                                   cycle_vertex_sets);
    }
  }
  (void)block_count;
}

static bool oracle_has_crossing_simple_cycles(const uint64_t *successors,
                                              uint64_t reachable,
                                              size_t block_count) {
  bool cycle_vertex_sets[UINT64_C(1) << CFG_ORACLE_MAX_BLOCKS] = {false};
  for (size_t start = 0u; start < block_count; start += 1u) {
    if ((reachable & oracle_bit(start)) != 0u)
      oracle_collect_simple_cycles(successors, reachable, block_count, start,
                                   start, oracle_bit(start),
                                   cycle_vertex_sets);
  }
  const size_t set_count = (size_t)1u << block_count;
  for (size_t left = 1u; left < set_count; left += 1u) {
    if (!cycle_vertex_sets[left]) continue;
    for (size_t right = left + 1u; right < set_count; right += 1u) {
      if (!cycle_vertex_sets[right]) continue;
      const size_t overlap = left & right;
      if (overlap != 0u && (left & ~right) != 0u &&
          (right & ~left) != 0u)
        return true;
    }
  }
  return false;
}

static void oracle_read_successors(const cfg_fixture *fixture,
                                   size_t block_count,
                                   uint64_t *successors) {
  for (size_t source = 0u; source < block_count; source += 1u) {
    const w_seed_hir0_terminator *term = &fixture->terminators[source + 1u];
    if (term->kind == W_SEED_HIR0_TERMINATOR_JUMP) {
      successors[source] |= oracle_bit((size_t)term->target_block - 1u);
    } else if (term->kind == W_SEED_HIR0_TERMINATOR_BRANCH) {
      successors[source] |= oracle_bit((size_t)term->target_block - 1u);
      successors[source] |= oracle_bit((size_t)term->else_block - 1u);
    }
  }
}

static void oracle_analyze(const uint64_t *successors, size_t block_count,
                           cfg_oracle_result *result) {
  (void)memset(result, 0, sizeof(*result));
  for (size_t block = 0u; block < block_count; block += 1u)
    result->successors[block] = successors[block];
  result->reachable = oracle_reachable_from(successors, block_count, 0u,
                                            block_count);
  result->irreducible_scc = oracle_has_irreducible_scc(
      successors, result->reachable, block_count);
  result->crossing_simple_cycles = oracle_has_crossing_simple_cycles(
      successors, result->reachable, block_count);

  for (size_t dominator = 0u; dominator < block_count; dominator += 1u) {
    const uint64_t without_dominator = oracle_reachable_from(
        successors, block_count, 0u, dominator);
    for (size_t block = 0u; block < block_count; block += 1u) {
      const uint64_t block_bit = oracle_bit(block);
      if ((result->reachable & block_bit) != 0u &&
          (block == dominator ||
           (without_dominator & block_bit) == 0u))
        result->dominators[block] |= oracle_bit(dominator);
    }
  }

  for (size_t source = 0u; source < block_count; source += 1u) {
    for (size_t target = 0u; target < block_count; target += 1u) {
      if ((successors[source] & oracle_bit(target)) != 0u &&
          (result->dominators[source] & oracle_bit(target)) != 0u)
        result->backedge_targets[source] |= oracle_bit(target);
    }
  }
  result->residual_cycle = oracle_has_cycle(
      successors, result->reachable, block_count,
      result->backedge_targets);

  for (size_t header = 0u; header < block_count; header += 1u) {
    const uint64_t header_bit = oracle_bit(header);
    uint64_t latches = 0u;
    for (size_t source = 0u; source < block_count; source += 1u) {
      if ((result->backedge_targets[source] & header_bit) != 0u)
        latches |= oracle_bit(source);
    }
    if (latches == 0u) continue;

    cfg_oracle_loop *loop = &result->loops[result->loop_count++];
    loop->header = header;
    loop->latches = latches;
    loop->members = header_bit | latches;
    size_t queue[CFG_ORACLE_MAX_BLOCKS] = {0u};
    size_t queue_count = 0u;
    size_t queue_cursor = 0u;
    for (size_t source = 0u; source < block_count; source += 1u) {
      if ((latches & oracle_bit(source)) != 0u && source != header)
        queue[queue_count++] = source;
    }
    while (queue_cursor < queue_count) {
      const size_t block = queue[queue_cursor++];
      const uint64_t block_bit = oracle_bit(block);
      for (size_t predecessor = 0u; predecessor < block_count;
           predecessor += 1u) {
        const uint64_t predecessor_bit = oracle_bit(predecessor);
        if ((result->reachable & predecessor_bit) == 0u ||
            (successors[predecessor] & block_bit) == 0u ||
            (loop->members & predecessor_bit) != 0u)
          continue;
        loop->members |= predecessor_bit;
        if (predecessor != header)
          queue[queue_count++] = predecessor;
      }
    }
    for (size_t block = 0u; block < block_count; block += 1u) {
      if ((loop->members & oracle_bit(block)) != 0u &&
          (result->dominators[block] & header_bit) == 0u)
        result->bad_loop_header = true;
    }
    if (result->loop_count > W_SEED_HIR0_CFG_MAX_LOOPS)
      result->too_many_loops = true;
  }

  for (size_t left = 0u; left < result->loop_count; left += 1u) {
    for (size_t right = left + 1u; right < result->loop_count;
         right += 1u) {
      const uint64_t left_members = result->loops[left].members;
      const uint64_t right_members = result->loops[right].members;
      const uint64_t overlap = left_members & right_members;
      if (overlap != 0u && (left_members & ~right_members) != 0u &&
          (right_members & ~left_members) != 0u)
        result->non_laminar_loops = true;
    }
  }
}

static bool oracle_accepts(const cfg_oracle_result *result) {
  return !result->residual_cycle && !result->irreducible_scc &&
         !result->bad_loop_header && !result->too_many_loops &&
         !result->non_laminar_loops;
}

static size_t terminal_choice_count(size_t block_count) {
  return 1u + block_count + block_count * (block_count - 1u);
}

static void configure_graph_from_code(cfg_fixture *fixture,
                                      size_t block_count, size_t graph_code) {
  initialize_offset_fixture(fixture, block_count);
  const size_t choices = terminal_choice_count(block_count);
  for (size_t source = 0u; source < block_count; source += 1u) {
    const size_t choice = graph_code % choices;
    graph_code /= choices;
    if (choice == 0u) continue;
    if (choice <= block_count) {
      set_jump(fixture, source + 1u, (uint32_t)choice);
      continue;
    }

    size_t branch_choice = choice - (block_count + 1u);
    bool configured = false;
    for (size_t first = 0u; first < block_count; first += 1u) {
      for (size_t second = 0u; second < block_count; second += 1u) {
        if (first == second) continue;
        if (branch_choice == 0u) {
          set_branch(fixture, source + 1u, (uint32_t)(first + 1u),
                     (uint32_t)(second + 1u));
          configured = true;
          break;
        }
        branch_choice -= 1u;
      }
      if (configured) break;
    }
  }
}

static bool compare_analysis_to_oracle(
    const w_seed_hir0_cfg_analysis *analysis,
    const cfg_oracle_result *expected, size_t block_count) {
  CHECK(analysis->first_block == 1u);
  CHECK(analysis->block_count == block_count);
  CHECK(analysis->reachable_blocks == expected->reachable);
  CHECK(analysis->loop_count == expected->loop_count);
  for (size_t source = 0u; source < block_count; source += 1u) {
    CHECK(analysis->successors[source] == expected->successors[source]);
    CHECK(analysis->dominators[source] == expected->dominators[source]);
    CHECK(analysis->backedge_targets[source] ==
          expected->backedge_targets[source]);
    CHECK(w_seed_hir0_cfg_is_reachable(analysis, (uint32_t)(source + 1u)) ==
          ((expected->reachable & oracle_bit(source)) != 0u));
    for (size_t target = 0u; target < block_count; target += 1u) {
      const uint32_t global_source = (uint32_t)(source + 1u);
      const uint32_t global_target = (uint32_t)(target + 1u);
      CHECK(w_seed_hir0_cfg_dominates(analysis, global_source,
                                     global_target) ==
            ((expected->dominators[target] & oracle_bit(source)) != 0u));
      CHECK(w_seed_hir0_cfg_is_backedge(analysis, global_source,
                                       global_target) ==
            ((expected->backedge_targets[source] & oracle_bit(target)) !=
             0u));
      for (size_t loop = 0u; loop < expected->loop_count; loop += 1u) {
        CHECK(w_seed_hir0_cfg_is_in_loop(
                  analysis, (uint32_t)(expected->loops[loop].header + 1u),
                  global_target) ==
              ((expected->loops[loop].members & oracle_bit(target)) != 0u));
      }
    }
  }
  for (size_t loop = 0u; loop < expected->loop_count; loop += 1u) {
    w_seed_hir0_cfg_loop actual = {0};
    CHECK(w_seed_hir0_cfg_loop_for_header(
        analysis, (uint32_t)(expected->loops[loop].header + 1u), &actual));
    CHECK(actual.header_block == expected->loops[loop].header + 1u);
    CHECK(actual.member_blocks == expected->loops[loop].members);
    CHECK(actual.backedge_sources == expected->loops[loop].latches);
  }
  return true;
}

static bool test_exhaustive_small_cfgs_against_oracle(void) {
  cfg_oracle_stats stats = {0};
  for (size_t block_count = 1u;
       block_count <= CFG_ORACLE_EXHAUSTIVE_BLOCKS;
       block_count += 1u) {
    const size_t choices = terminal_choice_count(block_count);
    size_t graph_count = 1u;
    for (size_t block = 0u; block < block_count; block += 1u)
      graph_count *= choices;

    for (size_t graph_code = 0u; graph_code < graph_count;
         graph_code += 1u) {
      cfg_fixture fixture;
      cfg_oracle_result expected = {0};
      uint64_t successors[CFG_ORACLE_MAX_BLOCKS] = {0u};
      configure_graph_from_code(&fixture, block_count, graph_code);
      oracle_read_successors(&fixture, block_count, successors);
      oracle_analyze(successors, block_count, &expected);
      stats.graph_count += 1u;
      const bool accepted = oracle_accepts(&expected);
      stats.accepted_count += accepted ? 1u : 0u;
      stats.irreducible_rejections +=
          !accepted && expected.irreducible_scc ? 1u : 0u;
      stats.crossing_cycle_rejections +=
          !accepted && expected.crossing_simple_cycles ? 1u : 0u;
      if (accepted && expected.loop_count == 2u) {
        const uint64_t first = expected.loops[0].members;
        const uint64_t second = expected.loops[1].members;
        if ((first & ~second) == 0u || (second & ~first) == 0u)
          stats.nested_loop_acceptances += 1u;
      }

      w_seed_hir0_cfg_analysis actual;
      uint8_t before[sizeof(actual)];
      (void)memset(&actual, 0xA5, sizeof(actual));
      (void)memcpy(before, &actual, sizeof(actual));
      const bool analyzed =
          w_seed_hir0_cfg_analyze(&fixture.program, 0u, &actual);
      if (analyzed != accepted) {
        (void)fprintf(stderr,
                       "oracle mismatch: blocks=%llu graph=%llu analyzed=%d "
                       "accepted=%d residual=%d irreducible=%d "
                       "crossing=%d bad-header=%d too-many=%d non-laminar=%d\n",
                       (unsigned long long)block_count,
                       (unsigned long long)graph_code, analyzed, accepted,
                       expected.residual_cycle, expected.irreducible_scc,
                       expected.crossing_simple_cycles,
                       expected.bad_loop_header, expected.too_many_loops,
                       expected.non_laminar_loops);
        return false;
      }
      if (!accepted) {
        CHECK(memcmp(before, &actual, sizeof(actual)) == 0);
        continue;
      }
      CHECK(compare_analysis_to_oracle(&actual, &expected, block_count));
    }
  }

  CHECK(stats.graph_count == 84548u);
  CHECK(stats.accepted_count > 0u);
  CHECK(stats.irreducible_rejections > 0u);
  CHECK(stats.crossing_cycle_rejections > 0u);
  CHECK(stats.nested_loop_acceptances > 0u);
  return true;
}

static bool test_nested_loop_depth_boundary(void) {
  for (size_t depth = 2u; depth <= 3u; depth += 1u) {
    cfg_fixture fixture;
    cfg_oracle_result expected = {0};
    uint64_t successors[CFG_ORACLE_MAX_BLOCKS] = {0u};
    initialize_offset_fixture(&fixture, 6u);
    set_branch(&fixture, 1u, 2u, 6u);
    set_branch(&fixture, 2u, 3u, 4u);
    if (depth == 2u)
      set_jump(&fixture, 3u, 4u);
    else
      set_branch(&fixture, 3u, 3u, 4u);
    set_branch(&fixture, 4u, 2u, 5u);
    set_branch(&fixture, 5u, 1u, 6u);

    oracle_read_successors(&fixture, 6u, successors);
    oracle_analyze(successors, 6u, &expected);
    CHECK(expected.loop_count == depth);
    CHECK(expected.too_many_loops == (depth > W_SEED_HIR0_CFG_MAX_LOOPS));
    CHECK(oracle_accepts(&expected) ==
          (depth <= W_SEED_HIR0_CFG_MAX_LOOPS));

    w_seed_hir0_cfg_analysis actual;
    uint8_t before[sizeof(actual)];
    (void)memset(&actual, 0xA5, sizeof(actual));
    (void)memcpy(before, &actual, sizeof(actual));
    const bool analyzed =
        w_seed_hir0_cfg_analyze(&fixture.program, 0u, &actual);
    CHECK(analyzed == (depth <= W_SEED_HIR0_CFG_MAX_LOOPS));
    if (analyzed) {
      CHECK(compare_analysis_to_oracle(&actual, &expected, 6u));
    } else {
      CHECK(memcmp(before, &actual, sizeof(actual)) == 0);
    }
  }
  return true;
}

static bool expect_failure_preserves_output(cfg_fixture *fixture) {
  w_seed_hir0_cfg_analysis result;
  uint8_t before[sizeof(result)];
  (void)memset(&result, 0xA5, sizeof(result));
  (void)memcpy(before, &result, sizeof(result));
  const bool analyzed = w_seed_hir0_cfg_analyze(&fixture->program, 0u,
                                                &result);
  return !analyzed && memcmp(before, &result, sizeof(result)) == 0;
}

static bool test_straight_line(void) {
  cfg_fixture fixture;
  initialize_fixture(&fixture, 3u);
  set_jump(&fixture, 0u, 1u);
  set_jump(&fixture, 1u, 2u);

  w_seed_hir0_cfg_analysis analysis = {0};
  CHECK(w_seed_hir0_cfg_analyze(&fixture.program, 0u, &analysis));
  CHECK(analysis.block_count == 3u && analysis.loop_count == 0u);
  CHECK(w_seed_hir0_cfg_is_reachable(&analysis, 0u));
  CHECK(w_seed_hir0_cfg_is_reachable(&analysis, 2u));
  CHECK(w_seed_hir0_cfg_dominates(&analysis, 0u, 2u));
  CHECK(w_seed_hir0_cfg_dominates(&analysis, 1u, 2u));
  CHECK(!w_seed_hir0_cfg_dominates(&analysis, 2u, 1u));
  return true;
}

static bool test_one_natural_loop(void) {
  cfg_fixture fixture;
  initialize_fixture(&fixture, 4u);
  set_jump(&fixture, 0u, 1u);
  set_branch(&fixture, 1u, 2u, 3u);
  set_jump(&fixture, 2u, 1u);

  w_seed_hir0_cfg_analysis analysis = {0};
  w_seed_hir0_cfg_loop loop = {0};
  CHECK(w_seed_hir0_cfg_analyze(&fixture.program, 0u, &analysis));
  CHECK(analysis.loop_count == 1u);
  CHECK(w_seed_hir0_cfg_loop_for_header(&analysis, 1u, &loop));
  CHECK(loop.header_block == 1u && loop.member_blocks == UINT64_C(0x6) &&
        loop.backedge_sources == UINT64_C(0x4));
  CHECK(w_seed_hir0_cfg_is_backedge(&analysis, 2u, 1u));
  CHECK(w_seed_hir0_cfg_is_in_loop(&analysis, 1u, 2u));
  CHECK(!w_seed_hir0_cfg_is_backedge(&analysis, 1u, 2u));
  return true;
}

static bool test_two_nested_loops(void) {
  cfg_fixture fixture;
  initialize_fixture(&fixture, 6u);
  set_jump(&fixture, 0u, 1u);
  set_branch(&fixture, 1u, 2u, 5u);
  set_branch(&fixture, 2u, 3u, 4u);
  set_jump(&fixture, 3u, 2u);
  set_jump(&fixture, 4u, 1u);

  w_seed_hir0_cfg_analysis analysis = {0};
  w_seed_hir0_cfg_loop outer = {0};
  w_seed_hir0_cfg_loop inner = {0};
  CHECK(w_seed_hir0_cfg_analyze(&fixture.program, 0u, &analysis));
  CHECK(analysis.loop_count == 2u);
  CHECK(w_seed_hir0_cfg_loop_for_header(&analysis, 1u, &outer));
  CHECK(w_seed_hir0_cfg_loop_for_header(&analysis, 2u, &inner));
  CHECK(outer.member_blocks == UINT64_C(0x1E));
  CHECK(inner.member_blocks == UINT64_C(0x0C));
  CHECK((inner.member_blocks & ~outer.member_blocks) == 0u);
  CHECK(w_seed_hir0_cfg_is_backedge(&analysis, 3u, 2u));
  CHECK(w_seed_hir0_cfg_is_backedge(&analysis, 4u, 1u));
  CHECK(w_seed_hir0_cfg_dominates(&analysis, 1u, 4u));
  return true;
}

static bool test_two_sibling_loops(void) {
  cfg_fixture fixture;
  initialize_fixture(&fixture, 6u);
  set_jump(&fixture, 0u, 1u);
  set_branch(&fixture, 1u, 2u, 3u);
  set_branch(&fixture, 2u, 1u, 3u);
  set_branch(&fixture, 3u, 4u, 5u);
  set_branch(&fixture, 4u, 3u, 5u);

  w_seed_hir0_cfg_analysis analysis = {0};
  w_seed_hir0_cfg_loop first = {0};
  w_seed_hir0_cfg_loop second = {0};
  CHECK(w_seed_hir0_cfg_analyze(&fixture.program, 0u, &analysis));
  CHECK(analysis.loop_count == 2u);
  CHECK(w_seed_hir0_cfg_loop_for_header(&analysis, 1u, &first));
  CHECK(w_seed_hir0_cfg_loop_for_header(&analysis, 3u, &second));
  CHECK(first.member_blocks == UINT64_C(0x6));
  CHECK(second.member_blocks == UINT64_C(0x18));
  CHECK((first.member_blocks & second.member_blocks) == 0u);
  return true;
}

static bool test_continue_backedges_share_header(void) {
  cfg_fixture fixture;
  initialize_fixture(&fixture, 6u);
  set_jump(&fixture, 0u, 1u);
  set_branch(&fixture, 1u, 2u, 5u);
  set_branch(&fixture, 2u, 1u, 3u);
  set_branch(&fixture, 3u, 1u, 5u);

  w_seed_hir0_cfg_analysis analysis = {0};
  w_seed_hir0_cfg_loop loop = {0};
  CHECK(w_seed_hir0_cfg_analyze(&fixture.program, 0u, &analysis));
  CHECK(analysis.loop_count == 1u);
  CHECK(w_seed_hir0_cfg_loop_for_header(&analysis, 1u, &loop));
  CHECK(loop.member_blocks == UINT64_C(0x0E));
  CHECK(loop.backedge_sources == UINT64_C(0x0C));
  CHECK(w_seed_hir0_cfg_is_backedge(&analysis, 2u, 1u));
  CHECK(w_seed_hir0_cfg_is_backedge(&analysis, 3u, 1u));
  return true;
}

static bool test_wrong_owner_ranges_and_targets(void) {
  cfg_fixture fixture;
  initialize_fixture(&fixture, 2u);
  fixture.blocks[1].owner_function = 1u;
  CHECK(expect_failure_preserves_output(&fixture));

  initialize_fixture(&fixture, 2u);
  fixture.blocks[0].first_instruction = 1u;
  fixture.blocks[0].instruction_count = 1u;
  CHECK(expect_failure_preserves_output(&fixture));

  initialize_fixture(&fixture, 2u);
  fixture.blocks[0].terminator_index = 2u;
  CHECK(expect_failure_preserves_output(&fixture));

  initialize_fixture(&fixture, 2u);
  fixture.terminators[0].owner_block = 1u;
  CHECK(expect_failure_preserves_output(&fixture));

  initialize_fixture(&fixture, 2u);
  set_branch(&fixture, 0u, 1u, 9u);
  CHECK(expect_failure_preserves_output(&fixture));

  initialize_fixture(&fixture, 3u);
  initialize_function(&fixture, 0u, 0u, 2u);
  initialize_function(&fixture, 1u, 2u, 1u);
  fixture.blocks[2].owner_function = 1u;
  set_branch(&fixture, 0u, 2u, 1u);
  CHECK(expect_failure_preserves_output(&fixture));
  return true;
}

static bool test_unreachable_blocks_are_reported(void) {
  cfg_fixture fixture;
  initialize_fixture(&fixture, 4u);
  set_jump(&fixture, 0u, 1u);
  set_jump(&fixture, 2u, 3u);

  w_seed_hir0_cfg_analysis analysis = {0};
  CHECK(w_seed_hir0_cfg_analyze(&fixture.program, 0u, &analysis));
  CHECK(w_seed_hir0_cfg_is_reachable(&analysis, 0u));
  CHECK(w_seed_hir0_cfg_is_reachable(&analysis, 1u));
  CHECK(!w_seed_hir0_cfg_is_reachable(&analysis, 2u));
  CHECK(!w_seed_hir0_cfg_is_reachable(&analysis, 3u));
  CHECK(!w_seed_hir0_cfg_dominates(&analysis, 2u, 2u));
  return true;
}

static bool test_unreachable_predecessor_does_not_block_reachable_dag(void) {
  cfg_fixture fixture;
  cfg_oracle_result expected = {0};
  uint64_t successors[CFG_ORACLE_MAX_BLOCKS] = {0u};
  initialize_offset_fixture(&fixture, 2u);
  set_jump(&fixture, 2u, 1u);
  oracle_read_successors(&fixture, 2u, successors);
  oracle_analyze(successors, 2u, &expected);

  w_seed_hir0_cfg_analysis analysis = {0};
  CHECK(oracle_accepts(&expected));
  CHECK(expected.reachable == oracle_bit(0u));
  CHECK(w_seed_hir0_cfg_analyze(&fixture.program, 0u, &analysis));
  CHECK(compare_analysis_to_oracle(&analysis, &expected, 2u));
  return true;
}

static bool test_irreducible_two_entry_cycle_rejected(void) {
  cfg_fixture fixture;
  initialize_fixture(&fixture, 5u);
  set_branch(&fixture, 0u, 1u, 2u);
  set_jump(&fixture, 1u, 3u);
  set_jump(&fixture, 2u, 3u);
  set_branch(&fixture, 3u, 1u, 4u);
  CHECK(expect_failure_preserves_output(&fixture));
  return true;
}

static bool test_non_nested_overlapping_cycles_rejected(void) {
  cfg_fixture fixture;
  initialize_fixture(&fixture, 6u);
  set_branch(&fixture, 0u, 1u, 2u);
  set_jump(&fixture, 1u, 3u);
  set_jump(&fixture, 2u, 4u);
  set_branch(&fixture, 3u, 2u, 5u);
  set_branch(&fixture, 4u, 1u, 5u);
  CHECK(expect_failure_preserves_output(&fixture));
  return true;
}

static bool test_output_alias_is_rejected_without_mutation(void) {
  cfg_fixture fixture;
  cfg_alias_storage alias;
  initialize_fixture(&fixture, 2u);
  set_jump(&fixture, 0u, 1u);
  (void)memcpy(alias.terminators, fixture.terminators,
               sizeof(fixture.terminators));
  fixture.program.terminators = alias.terminators;
  uint8_t before[sizeof(alias.terminators)];
  (void)memcpy(before, alias.terminators, sizeof(before));
  CHECK(!w_seed_hir0_cfg_analyze(&fixture.program, 0u, &alias.analysis));
  CHECK(memcmp(before, alias.terminators, sizeof(before)) == 0);
  return true;
}

static bool test_capacity_and_all_or_nothing(void) {
  cfg_fixture fixture;
  initialize_fixture(&fixture, TEST_MAX_BLOCKS);
  CHECK(expect_failure_preserves_output(&fixture));

  initialize_fixture(&fixture, 4u);
  set_branch(&fixture, 0u, 0u, 1u);
  set_branch(&fixture, 1u, 1u, 2u);
  set_branch(&fixture, 2u, 2u, 3u);
  CHECK(expect_failure_preserves_output(&fixture));

  initialize_fixture(&fixture, W_SEED_HIR0_CFG_MAX_BLOCKS);
  for (size_t block = 0u; block + 1u < W_SEED_HIR0_CFG_MAX_BLOCKS;
       block += 1u)
    set_jump(&fixture, block, (uint32_t)(block + 1u));
  w_seed_hir0_cfg_analysis analysis = {0};
  CHECK(w_seed_hir0_cfg_analyze(&fixture.program, 0u, &analysis));
  CHECK(analysis.block_count == W_SEED_HIR0_CFG_MAX_BLOCKS);
  CHECK(analysis.reachable_blocks == UINT64_MAX);
  CHECK(w_seed_hir0_cfg_dominates(&analysis, 0u, 63u));
  return true;
}

int main(void) {
  if (!test_straight_line()) return 1;
  if (!test_one_natural_loop()) return 1;
  if (!test_two_nested_loops()) return 1;
  if (!test_two_sibling_loops()) return 1;
  if (!test_continue_backedges_share_header()) return 1;
  if (!test_wrong_owner_ranges_and_targets()) return 1;
  if (!test_unreachable_blocks_are_reported()) return 1;
  if (!test_unreachable_predecessor_does_not_block_reachable_dag()) return 1;
  if (!test_irreducible_two_entry_cycle_rejected()) return 1;
  if (!test_non_nested_overlapping_cycles_rejected()) return 1;
  if (!test_exhaustive_small_cfgs_against_oracle()) return 1;
  if (!test_nested_loop_depth_boundary()) return 1;
  if (!test_output_alias_is_rejected_without_mutation()) return 1;
  if (!test_capacity_and_all_or_nothing()) return 1;
  (void)puts("hir0 cfg tests: ok");
  return 0;
}
