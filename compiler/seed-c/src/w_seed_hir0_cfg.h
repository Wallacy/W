#ifndef W_SEED_HIR0_CFG_H
#define W_SEED_HIR0_CFG_H

#include "w_seed_hir0.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* This internal seed analysis has an explicit implementation bound. It is
 * not a language-level limit. */
#define W_SEED_HIR0_CFG_MAX_BLOCKS 64u
#define W_SEED_HIR0_CFG_MAX_LOOPS 2u

typedef struct {
  uint32_t header_block;
  /* Local block-ordinal bitsets, relative to the analyzed function. */
  uint64_t member_blocks;
  uint64_t backedge_sources;
} w_seed_hir0_cfg_loop;

typedef struct {
  uint32_t function_index;
  uint32_t first_block;
  size_t block_count;
  /* All bitsets use function-local block ordinals. Unreachable blocks have
   * no dominators and do not appear in reachable_blocks. */
  uint64_t reachable_blocks;
  uint64_t successors[W_SEED_HIR0_CFG_MAX_BLOCKS];
  uint64_t dominators[W_SEED_HIR0_CFG_MAX_BLOCKS];
  uint64_t backedge_targets[W_SEED_HIR0_CFG_MAX_BLOCKS];
  size_t loop_count;
  w_seed_hir0_cfg_loop loops[W_SEED_HIR0_CFG_MAX_LOOPS];
} w_seed_hir0_cfg_analysis;

/* Analyze the function's branch/jump/return graph without changing HIR.
 * `out` must not overlap `program` or its backing records. The result is
 * published only after every check succeeds. No heap storage is used.
 * Unreachable blocks are reported, not rejected. Other terminator families,
 * typed edge arguments, carrier tuples, and source-label provenance are not
 * interpreted here. */
bool w_seed_hir0_cfg_analyze(const w_seed_hir0_program *program,
                             size_t function_index,
                             w_seed_hir0_cfg_analysis *out);

/* Query arguments are global HIR block indices. Dominance is true only for
 * reachable blocks. */
bool w_seed_hir0_cfg_is_reachable(const w_seed_hir0_cfg_analysis *analysis,
                                  uint32_t block_index);
bool w_seed_hir0_cfg_dominates(const w_seed_hir0_cfg_analysis *analysis,
                               uint32_t dominator_block,
                               uint32_t block_index);
bool w_seed_hir0_cfg_is_backedge(const w_seed_hir0_cfg_analysis *analysis,
                                 uint32_t source_block,
                                 uint32_t target_block);
bool w_seed_hir0_cfg_loop_for_header(
    const w_seed_hir0_cfg_analysis *analysis, uint32_t header_block,
    w_seed_hir0_cfg_loop *out);
bool w_seed_hir0_cfg_is_in_loop(const w_seed_hir0_cfg_analysis *analysis,
                                uint32_t header_block,
                                uint32_t block_index);

#endif
