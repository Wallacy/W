#include "w_seed_hir0_cfg.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TEST_MAX_BLOCKS (W_SEED_HIR0_CFG_MAX_BLOCKS + 1u)

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
  if (!test_irreducible_two_entry_cycle_rejected()) return 1;
  if (!test_non_nested_overlapping_cycles_rejected()) return 1;
  if (!test_output_alias_is_rejected_without_mutation()) return 1;
  if (!test_capacity_and_all_or_nothing()) return 1;
  (void)puts("hir0 cfg tests: ok");
  return 0;
}
