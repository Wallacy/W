#define WIN32_LEAN_AND_MEAN

#include "parallel_panic_boundary1_witness.h"

#if defined(_WIN32) && defined(_WIN64)

#include <windows.h>

#include <limits.h>
#include <string.h>
#include <wchar.h>

static bool parse_u64(const wchar_t *text, uint64_t *value) {
  if (text == NULL || value == NULL || *text == L'\0') return false;
  uint64_t parsed = 0u;
  for (size_t index = 0u; text[index] != L'\0'; index += 1u) {
    if (text[index] < L'0' || text[index] > L'9' ||
        parsed > (UINT64_MAX - (uint64_t)(text[index] - L'0')) / 10u)
      return false;
    parsed = parsed * 10u + (uint64_t)(text[index] - L'0');
  }
  *value = parsed;
  return true;
}

static bool option_value(const wchar_t *argument, const wchar_t *prefix,
                         uint64_t *value) {
  if (argument == NULL || prefix == NULL || value == NULL) return false;
  const size_t prefix_length = wcslen(prefix);
  return wcsncmp(argument, prefix, prefix_length) == 0 &&
         parse_u64(argument + prefix_length, value);
}

static void keep_alive(void) {
  for (;;) (void)Sleep(1000u);
}

static bool write_frame(HANDLE write_handle,
                        const uint8_t frame[W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_BYTES]) {
  if (write_handle == NULL || frame == NULL) return false;
  DWORD written = 0u;
  return WriteFile(write_handle, frame,
                   W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_BYTES, &written, NULL) !=
             FALSE &&
         written == W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_BYTES;
}

static bool write_partial(HANDLE write_handle,
                          const uint8_t frame[W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_BYTES]) {
  if (write_handle == NULL || frame == NULL) return false;
  DWORD written = 0u;
  const DWORD partial = W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_BYTES / 2u;
  return WriteFile(write_handle, frame, partial, &written, NULL) != FALSE &&
         written == partial;
}

static int helper_main(HANDLE write_handle,
                       w_seed_parallel_panic_boundary1_fault fault,
                       uint64_t invocation_nonce) {
  if (fault == W_SEED_PARALLEL_PANIC_BOUNDARY1_FAULT_EARLY_EXIT)
    {
      ExitProcess(0xe2u);
      return 0xe2;
    }
  static w_seed_parallel_panic_boundary1_witness witness;
  if (!w_seed_parallel_panic_boundary1_witness_init(&witness)) return 0xe3;
  w_seed_parallel_typed_binding1_counts counts;
  w_seed_parallel_typed_binding1_result measured;
  if (w_seed_parallel_typed_binding1_measure(&witness.typed_input, &counts,
                                             &measured) !=
          W_SEED_PARALLEL_TYPED_BINDING1_OK ||
      counts.completions != 2u || counts.records != 2u ||
      measured.written.completions != 0u || measured.written.records != 0u)
    return 0xe4;
  w_seed_parallel_typed_binding1_panic_signal signal;
  (void)memset(&signal, 0x5a, sizeof(signal));
  const w_seed_parallel_typed_binding1_status panic_status =
      w_seed_parallel_typed_binding1_panic_run(&witness.typed_input, &signal);
  if (panic_status != W_SEED_PARALLEL_TYPED_BINDING1_OK ||
      witness.provider_context.calls[0] != 1u ||
      witness.provider_context.calls[1] != 1u)
    return 0xe5;
  uint8_t frame[W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_BYTES];
  if (!w_seed_parallel_panic_boundary1_encode_wire(
          &witness.typed_input, &measured, &signal, GetCurrentProcessId(),
          invocation_nonce, frame))
    return 0xe6;
  switch (fault) {
    case W_SEED_PARALLEL_PANIC_BOUNDARY1_FAULT_NONE:
      if (!write_frame(write_handle, frame)) return 0xe7;
      break;
    case W_SEED_PARALLEL_PANIC_BOUNDARY1_FAULT_PARTIAL_OUTPUT:
      if (!write_partial(write_handle, frame)) return 0xe8;
      break;
    case W_SEED_PARALLEL_PANIC_BOUNDARY1_FAULT_MALFORMED:
      frame[W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_RESERVED_OFFSET] = 1u;
      if (!write_frame(write_handle, frame)) return 0xe9;
      break;
    case W_SEED_PARALLEL_PANIC_BOUNDARY1_FAULT_NON_PANIC:
      frame[W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_STATUS_OFFSET] = 0u;
      frame[W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_STATUS_OFFSET + 1u] = 0u;
      frame[W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_STATUS_OFFSET + 2u] = 0u;
      frame[W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_STATUS_OFFSET + 3u] = 0u;
      if (!write_frame(write_handle, frame)) return 0xea;
      break;
    case W_SEED_PARALLEL_PANIC_BOUNDARY1_FAULT_TIMEOUT:
      keep_alive();
      break;
    case W_SEED_PARALLEL_PANIC_BOUNDARY1_FAULT_TRAILING_OUTPUT:
      if (!write_frame(write_handle, frame)) return 0xeb;
      {
        const uint8_t trailing = 0x7fu;
        DWORD written = 0u;
        if (WriteFile(write_handle, &trailing, 1u, &written, NULL) == FALSE ||
            written != 1u)
          return 0xec;
      }
      break;
    case W_SEED_PARALLEL_PANIC_BOUNDARY1_FAULT_STALE_NONCE:
      frame[W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_NONCE_OFFSET] ^= 1u;
      if (!write_frame(write_handle, frame)) return 0xed;
      break;
    case W_SEED_PARALLEL_PANIC_BOUNDARY1_FAULT_STALE_DIGEST:
      frame[W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_HIR_DIGEST_OFFSET] ^= 1u;
      if (!write_frame(write_handle, frame)) return 0xee;
      break;
    case W_SEED_PARALLEL_PANIC_BOUNDARY1_FAULT_STALE_GENERATION:
      frame[W_SEED_PARALLEL_PANIC_BOUNDARY1_WIRE_GENERATION_OFFSET] ^= 1u;
      if (!write_frame(write_handle, frame)) return 0xef;
      break;
    case W_SEED_PARALLEL_PANIC_BOUNDARY1_FAULT_EARLY_EXIT:
      return 0xf0;
    default:
      return 0xf1;
  }
  keep_alive();
  return 0xef;
}

int wmain(int argc, wchar_t **argv) {
  if (argc != 4 || argv == NULL || argv[0] == NULL || argv[0][0] == L'\0')
    return 0xf0;
  uint64_t write_value = 0u;
  uint64_t fault_value = 0u;
  uint64_t nonce_value = 0u;
  if (!option_value(argv[1], L"--w-seed-write-handle=", &write_value) ||
      !option_value(argv[2], L"--w-seed-fault=", &fault_value) ||
      !option_value(argv[3], L"--w-seed-invocation-nonce=", &nonce_value) ||
      write_value == 0u || write_value > (uint64_t)UINTPTR_MAX ||
      fault_value >
          (uint64_t)W_SEED_PARALLEL_PANIC_BOUNDARY1_FAULT_STALE_GENERATION ||
      nonce_value == 0u)
    return 0xf1;
  const int result = helper_main(
      (HANDLE)(uintptr_t)write_value,
      (w_seed_parallel_panic_boundary1_fault)fault_value, nonce_value);
  return result;
}

#else

int main(void) { return 0; }

#endif
