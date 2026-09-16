#include "parallel_panic_boundary1_witness.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32) && defined(_WIN64)

#include <windows.h>

#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition)) {                                                        \
      (void)fprintf(stderr, "panic boundary check failed: %s (%s:%d)\n",     \
                    #condition, __FILE__, __LINE__);                          \
      return false;                                                            \
    }                                                                          \
  } while (0)

static w_seed_parallel_panic_boundary1_witness witness;

static w_seed_parallel_panic_boundary1_input make_input(
    const char *helper_path, w_seed_parallel_panic_boundary1_fault fault,
    uint64_t invocation_nonce) {
  return (w_seed_parallel_panic_boundary1_input){
      .typed_input = &witness.typed_input,
      .helper_path = helper_path,
      .expected_panic_code = W_SEED_PARALLEL_PLATFORM1_PANIC_EXPLICIT,
      .timeout_ms = 0u,
      .invocation_nonce = invocation_nonce,
      .helper_fault = fault};
}

static bool test_valid_boundary(const char *helper_path) {
  CHECK(w_seed_parallel_panic_boundary1_witness_init(&witness));
  const w_seed_parallel_panic_boundary1_input input =
      make_input(helper_path, W_SEED_PARALLEL_PANIC_BOUNDARY1_FAULT_NONE,
                 0x0123456789abcdefULL);
  w_seed_parallel_panic_boundary1_receipt receipt;
  (void)memset(&receipt, 0xa5, sizeof(receipt));
  const w_seed_parallel_panic_boundary1_status boundary_status =
      w_seed_parallel_panic_boundary1_run(
          &input, &(w_seed_parallel_panic_boundary1_output){&receipt, 1u});
  CHECK(boundary_status == W_SEED_PARALLEL_PANIC_BOUNDARY1_OK);
  CHECK(w_seed_parallel_panic_boundary1_verify(&input, &receipt));
  CHECK(receipt.protocol_version ==
            W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_PROTOCOL_VERSION &&
        receipt.frame_type == W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_TYPE_PANIC &&
        receipt.frame_length == W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_BYTES &&
        receipt.invocation_nonce == input.invocation_nonce &&
        receipt.sequence == 1u && receipt.wire_status ==
            W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_STATUS_PANIC &&
        receipt.semantic_result_published == false &&
        receipt.child_pid != 0u && receipt.child_was_live_at_teardown &&
        receipt.child_terminated && receipt.child_joined &&
        receipt.handles_closed &&
        receipt.child_exit_code ==
            W_SEED_PARALLEL_PANIC_BOUNDARY1_TERMINATION_EXIT_CODE &&
        receipt.panic_source_index == 1u &&
        receipt.panic_source_lexical_index == witness.tasks[1].lexical_index &&
        receipt.panic_source_call_index == witness.tasks[1].call_index &&
        receipt.panic_code == W_SEED_PARALLEL_PLATFORM1_PANIC_EXPLICIT &&
        receipt.started_count == 2u && receipt.settled_count == 2u &&
        receipt.canceled_before_start_count == 0u &&
        receipt.maximum_active == 2u && receipt.cancellation_source_index == 1u &&
        receipt.cancellation_requested &&
        receipt.panic_receipt_source_index == 1u && receipt.panic_requested);

  w_seed_parallel_panic_boundary1_receipt forged = receipt;
  forged.provenance_digest[0] ^= 1u;
  CHECK(!w_seed_parallel_panic_boundary1_verify(&input, &forged));
  forged = receipt;
  forged.wire_provenance_digest[0] ^= 1u;
  CHECK(!w_seed_parallel_panic_boundary1_verify(&input, &forged));
  forged = receipt;
  forged.panic_code = W_SEED_PARALLEL_PLATFORM1_PANIC_INTERNAL_CONTRACT;
  CHECK(!w_seed_parallel_panic_boundary1_verify(&input, &forged));
  return true;
}

static bool test_transactional_rejections(const char *helper_path) {
  CHECK(w_seed_parallel_panic_boundary1_witness_init(&witness));
  const uint64_t nonce = 0x0011223344556677ULL;
  const w_seed_parallel_panic_boundary1_input valid_input =
      make_input(helper_path, W_SEED_PARALLEL_PANIC_BOUNDARY1_FAULT_NONE, nonce);
  w_seed_parallel_panic_boundary1_receipt receipt;
  (void)memset(&receipt, 0x3c, sizeof(receipt));
  const w_seed_parallel_panic_boundary1_receipt before = receipt;
  CHECK(w_seed_parallel_panic_boundary1_run(
            &valid_input,
            &(w_seed_parallel_panic_boundary1_output){&receipt, 0u}) ==
        W_SEED_PARALLEL_PANIC_BOUNDARY1_CAPACITY);
  CHECK(memcmp(&receipt, &before, sizeof(receipt)) == 0);

  CHECK(w_seed_parallel_panic_boundary1_run(
            &valid_input,
            &(w_seed_parallel_panic_boundary1_output){
                (w_seed_parallel_panic_boundary1_receipt *)&witness.typed_input,
                1u}) == W_SEED_PARALLEL_PANIC_BOUNDARY1_ALIAS);
  CHECK(memcmp(&receipt, &before, sizeof(receipt)) == 0);

  uint8_t hir_record_before[sizeof(witness.hir_instructions[0])];
  (void)memcpy(hir_record_before, &witness.hir_instructions[0],
               sizeof(hir_record_before));
  CHECK(w_seed_parallel_panic_boundary1_run(
            &valid_input,
            &(w_seed_parallel_panic_boundary1_output){
                (w_seed_parallel_panic_boundary1_receipt *)&witness
                    .hir_instructions[0],
                1u}) == W_SEED_PARALLEL_PANIC_BOUNDARY1_ALIAS);
  CHECK(memcmp(hir_record_before, &witness.hir_instructions[0],
               sizeof(hir_record_before)) == 0);

  uint8_t hir_text_before[sizeof(witness.hir_text)];
  (void)memcpy(hir_text_before, witness.hir_text, sizeof(hir_text_before));
  CHECK(w_seed_parallel_panic_boundary1_run(
            &valid_input,
            &(w_seed_parallel_panic_boundary1_output){
                (w_seed_parallel_panic_boundary1_receipt *)witness.hir_text,
                1u}) == W_SEED_PARALLEL_PANIC_BOUNDARY1_ALIAS);
  CHECK(memcmp(hir_text_before, witness.hir_text,
               sizeof(hir_text_before)) == 0);

  w_seed_parallel_local_provider1_authority forged_authority = witness.authority;
  forged_authority.receipt.contract_digest[0] ^= 1u;
  w_seed_parallel_typed_binding1_input forged_authority_typed =
      witness.typed_input;
  forged_authority_typed.provider_authority = &forged_authority;
  w_seed_parallel_panic_boundary1_input forged_authority_input = valid_input;
  forged_authority_input.typed_input = &forged_authority_typed;
  (void)memset(&receipt, 0x4d, sizeof(receipt));
  const w_seed_parallel_panic_boundary1_receipt authority_before = receipt;
  CHECK(w_seed_parallel_panic_boundary1_run(
            &forged_authority_input,
            &(w_seed_parallel_panic_boundary1_output){&receipt, 1u}) ==
        W_SEED_PARALLEL_PANIC_BOUNDARY1_AUTHORITY);
  CHECK(memcmp(&receipt, &authority_before, sizeof(receipt)) == 0);

  w_seed_hir0_result forged_hir = witness.hir_result;
  forged_hir.semantic_digest[0] ^= 1u;
  w_seed_parallel_typed_binding1_input forged_hir_typed = witness.typed_input;
  forged_hir_typed.hir_result = &forged_hir;
  w_seed_parallel_panic_boundary1_input forged_hir_input = valid_input;
  forged_hir_input.typed_input = &forged_hir_typed;
  (void)memset(&receipt, 0x51, sizeof(receipt));
  const w_seed_parallel_panic_boundary1_receipt hir_before = receipt;
  CHECK(w_seed_parallel_panic_boundary1_run(
            &forged_hir_input,
            &(w_seed_parallel_panic_boundary1_output){&receipt, 1u}) ==
        W_SEED_PARALLEL_PANIC_BOUNDARY1_HIR);
  CHECK(memcmp(&receipt, &hir_before, sizeof(receipt)) == 0);

  w_seed_parallel_typed_binding1_input unsupported_typed = witness.typed_input;
  unsupported_typed.provider.target =
      W_SEED_PARALLEL_TYPED_BINDING1_TARGET_NONE;
  w_seed_parallel_panic_boundary1_input unsupported_input = valid_input;
  unsupported_input.typed_input = &unsupported_typed;
  (void)memset(&receipt, 0x62, sizeof(receipt));
  const w_seed_parallel_panic_boundary1_receipt unsupported_before = receipt;
  CHECK(w_seed_parallel_panic_boundary1_run(
            &unsupported_input,
            &(w_seed_parallel_panic_boundary1_output){&receipt, 1u}) ==
        W_SEED_PARALLEL_PANIC_BOUNDARY1_UNSUPPORTED);
  CHECK(memcmp(&receipt, &unsupported_before, sizeof(receipt)) == 0);

  w_seed_parallel_panic_boundary1_input zero_nonce = valid_input;
  zero_nonce.invocation_nonce = 0u;
  (void)memset(&receipt, 0x73, sizeof(receipt));
  const w_seed_parallel_panic_boundary1_receipt nonce_before = receipt;
  CHECK(w_seed_parallel_panic_boundary1_run(
            &zero_nonce,
            &(w_seed_parallel_panic_boundary1_output){&receipt, 1u}) ==
        W_SEED_PARALLEL_PANIC_BOUNDARY1_INVALID);
  CHECK(memcmp(&receipt, &nonce_before, sizeof(receipt)) == 0);
  return true;
}

static bool test_private_panic_transaction(void) {
  CHECK(w_seed_parallel_panic_boundary1_witness_init(&witness));
  w_seed_parallel_typed_binding1_panic_signal signal;
  (void)memset(&signal, 0x5a, sizeof(signal));
  CHECK(w_seed_parallel_typed_binding1_panic_run(&witness.typed_input,
                                                 &signal) ==
        W_SEED_PARALLEL_TYPED_BINDING1_OK);
  CHECK(signal.source_index == 1u && signal.lexical_index == witness.tasks[1].lexical_index &&
        signal.call_index == witness.tasks[1].call_index &&
        signal.panic_code == W_SEED_PARALLEL_PLATFORM1_PANIC_EXPLICIT &&
        !signal.semantic_result_published && signal.started_count == 2u &&
        signal.settled_count == 2u && signal.maximum_active == 2u &&
        signal.cancellation_requested && signal.panic_requested);
  CHECK(witness.provider_context.calls[0] == 1u &&
        witness.provider_context.calls[1] == 1u);

  w_seed_parallel_typed_binding1_input context_alias = witness.typed_input;
  context_alias.provider_job.context = &signal;
  w_seed_parallel_typed_binding1_panic_signal signal_before;
  (void)memset(&signal_before, 0x27, sizeof(signal_before));
  signal = signal_before;
  CHECK(w_seed_parallel_typed_binding1_panic_run(&context_alias, &signal) ==
        W_SEED_PARALLEL_TYPED_BINDING1_ALIAS);
  CHECK(memcmp(&signal, &signal_before, sizeof(signal)) == 0);

  struct {
    w_seed_parallel_panic_boundary1_provider_context context;
    uint8_t tail[sizeof(signal) + 8u];
  } interior_context = {0};
  interior_context.context = witness.provider_context;
  (void)memset(interior_context.context.calls, 0,
               sizeof(interior_context.context.calls));
  w_seed_parallel_typed_binding1_input interior_context_input =
      witness.typed_input;
  interior_context_input.provider_job.context = &interior_context.context;
  interior_context_input.provider_job.context_bytes =
      sizeof(interior_context);
  w_seed_parallel_typed_binding1_panic_signal *interior_signal =
      (w_seed_parallel_typed_binding1_panic_signal *)(interior_context.tail + 1u);
  w_seed_parallel_typed_binding1_panic_signal interior_signal_before;
  (void)memset(interior_context.tail, 0x38, sizeof(interior_context.tail));
  (void)memcpy(&interior_signal_before, interior_signal,
               sizeof(interior_signal_before));
  CHECK(w_seed_parallel_typed_binding1_panic_run(&interior_context_input,
                                                 interior_signal) ==
        W_SEED_PARALLEL_TYPED_BINDING1_ALIAS);
  CHECK(memcmp(&interior_signal_before, interior_signal,
               sizeof(interior_signal_before)) == 0);

  (void)memset(&signal_before, 0x27, sizeof(signal_before));
  signal = signal_before;
  witness.provider_context.requested[1] =
      (w_seed_parallel_platform1_completion){
          .kind = W_SEED_PARALLEL_PLATFORM1_COMPLETION_PANIC};
  CHECK(w_seed_parallel_typed_binding1_panic_run(&witness.typed_input, &signal) !=
            W_SEED_PARALLEL_TYPED_BINDING1_OK &&
        memcmp(&signal, &signal_before, sizeof(signal)) == 0);
  return true;
}

static bool test_normal_error_route(void) {
  CHECK(w_seed_parallel_panic_boundary1_witness_init(&witness));
  w_seed_parallel_typed_binding1_counts counts;
  w_seed_parallel_typed_binding1_result measured;
  CHECK(w_seed_parallel_typed_binding1_measure(&witness.typed_input, &counts,
                                               &measured) ==
        W_SEED_PARALLEL_TYPED_BINDING1_OK);
  witness.provider_context.requested[1] =
      (w_seed_parallel_platform1_completion){
          .kind = W_SEED_PARALLEL_PLATFORM1_COMPLETION_ERROR,
          .error_code = 17u,
          .error_case_ordinal = measured.error_identity.case_ordinal};
  w_seed_parallel_platform1_completion completions[2];
  w_seed_parallel_platform1_receipt upstream;
  w_seed_parallel_provider0_kind provider_kind =
      W_SEED_PARALLEL_PROVIDER0_KIND_NONE;
  w_seed_parallel_typed_binding1_record records[2];
  w_seed_parallel_typed_binding1_result result;
  const w_seed_parallel_typed_binding1_workspace workspace = {
      completions, 2u, &upstream, &provider_kind};
  const w_seed_parallel_typed_binding1_output output = {records, 2u};
  CHECK(w_seed_parallel_typed_binding1_run(&witness.typed_input, &workspace,
                                           &output, &result) ==
            W_SEED_PARALLEL_TYPED_BINDING1_OK &&
        w_seed_parallel_typed_binding1_verify(&witness.typed_input, &workspace,
                                              &output, &result) &&
        records[0].outcome ==
            W_SEED_PARALLEL_TYPED_BINDING1_OUTCOME_SUCCESS &&
        records[1].outcome == W_SEED_PARALLEL_TYPED_BINDING1_OUTCOME_ERROR &&
        upstream.cancellation_requested && upstream.cancellation_source_index ==
            1u);
  return true;
}

static bool test_helper_faults(const char *helper_path) {
  static const struct {
    w_seed_parallel_panic_boundary1_fault fault;
    w_seed_parallel_panic_boundary1_status expected;
  } CASES[] = {
      {W_SEED_PARALLEL_PANIC_BOUNDARY1_FAULT_NON_PANIC,
       W_SEED_PARALLEL_PANIC_BOUNDARY1_NON_PANIC},
      {W_SEED_PARALLEL_PANIC_BOUNDARY1_FAULT_MALFORMED,
       W_SEED_PARALLEL_PANIC_BOUNDARY1_PROTOCOL},
      {W_SEED_PARALLEL_PANIC_BOUNDARY1_FAULT_TRAILING_OUTPUT,
       W_SEED_PARALLEL_PANIC_BOUNDARY1_PROTOCOL},
      {W_SEED_PARALLEL_PANIC_BOUNDARY1_FAULT_STALE_NONCE,
       W_SEED_PARALLEL_PANIC_BOUNDARY1_PROTOCOL},
      {W_SEED_PARALLEL_PANIC_BOUNDARY1_FAULT_STALE_DIGEST,
       W_SEED_PARALLEL_PANIC_BOUNDARY1_PROTOCOL},
      {W_SEED_PARALLEL_PANIC_BOUNDARY1_FAULT_STALE_GENERATION,
       W_SEED_PARALLEL_PANIC_BOUNDARY1_PROTOCOL},
      {W_SEED_PARALLEL_PANIC_BOUNDARY1_FAULT_EARLY_EXIT,
       W_SEED_PARALLEL_PANIC_BOUNDARY1_EARLY_EXIT},
      {W_SEED_PARALLEL_PANIC_BOUNDARY1_FAULT_PARTIAL_OUTPUT,
       W_SEED_PARALLEL_PANIC_BOUNDARY1_PARTIAL},
      {W_SEED_PARALLEL_PANIC_BOUNDARY1_FAULT_TIMEOUT,
       W_SEED_PARALLEL_PANIC_BOUNDARY1_TIMEOUT},
  };
  for (size_t index = 0u; index < sizeof(CASES) / sizeof(CASES[0]);
       index += 1u) {
    CHECK(w_seed_parallel_panic_boundary1_witness_init(&witness));
    w_seed_parallel_panic_boundary1_input input = make_input(
        helper_path, CASES[index].fault, 0x1000000000000000ULL + index + 1u);
    input.timeout_ms =
        CASES[index].fault == W_SEED_PARALLEL_PANIC_BOUNDARY1_FAULT_TIMEOUT
            ? 250u
            : W_SEED_PARALLEL_PANIC_BOUNDARY1_DEFAULT_TIMEOUT_MS;
    w_seed_parallel_panic_boundary1_receipt receipt;
    (void)memset(&receipt, 0x8b, sizeof(receipt));
    const w_seed_parallel_panic_boundary1_receipt before = receipt;
    const w_seed_parallel_panic_boundary1_status status =
        w_seed_parallel_panic_boundary1_run(
            &input,
            &(w_seed_parallel_panic_boundary1_output){&receipt, 1u});
    if (status != CASES[index].expected) {
      (void)fprintf(stderr,
                    "panic boundary fault case %u returned %u, expected %u\n",
                    (unsigned int)index, (unsigned int)status,
                    (unsigned int)CASES[index].expected);
      return false;
    }
    CHECK(memcmp(&receipt, &before, sizeof(receipt)) == 0);
  }
  return true;
}

static bool test_encoder_rejections(void) {
  CHECK(w_seed_parallel_panic_boundary1_witness_init(&witness));
  w_seed_parallel_typed_binding1_counts counts;
  w_seed_parallel_typed_binding1_result measured;
  CHECK(w_seed_parallel_typed_binding1_measure(&witness.typed_input, &counts,
                                               &measured) ==
        W_SEED_PARALLEL_TYPED_BINDING1_OK);
  w_seed_parallel_typed_binding1_panic_signal signal;
  CHECK(w_seed_parallel_typed_binding1_panic_run(&witness.typed_input, &signal) ==
        W_SEED_PARALLEL_TYPED_BINDING1_OK);
  uint8_t wire[W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_BYTES];
  (void)memset(wire, 0x4a, sizeof(wire));
  uint8_t wire_before[W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_BYTES];
  (void)memset(wire_before, 0x4a, sizeof(wire_before));
  signal.semantic_result_published = true;
  CHECK(!w_seed_parallel_panic_boundary1_encode_wire(
      &witness.typed_input, &measured, &signal, 7u, 9u, wire));
  CHECK(memcmp(wire, wire_before, sizeof(wire)) == 0);
  signal.semantic_result_published = false;
  signal.panic_code = W_SEED_PARALLEL_PLATFORM1_PANIC_NONE;
  CHECK(!w_seed_parallel_panic_boundary1_encode_wire(
      &witness.typed_input, &measured, &signal, 7u, 9u, wire));
  CHECK(memcmp(wire, wire_before, sizeof(wire)) == 0);
  return true;
}

int main(int argc, char **argv) {
  if (argc != 2 || argv == NULL || argv[1] == NULL) return 2;
  if (!test_valid_boundary(argv[1]) ||
      !test_transactional_rejections(argv[1]) ||
      !test_private_panic_transaction() || !test_normal_error_route() ||
      !test_helper_faults(argv[1]) || !test_encoder_rejections())
    return 1;
  return 0;
}

#else

int main(void) { return 77; }

#endif
