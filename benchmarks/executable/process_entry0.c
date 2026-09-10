/* Private PROCESS_ENTRY0 handler baseline; the shared C harness supplies the
 * root-scoped adapters and owns process startup/finalization. */

#include <stdint.h>

extern int32_t w_seed_process_entry0_context_drop(void *context);
extern int32_t w_seed_process_entry0_arguments_drop(void *arguments);

int32_t w_seed_process_entry0_handler(void *arguments, void *context) {
  const int32_t context_status =
      w_seed_process_entry0_context_drop(context);
  if (context_status != 0) __builtin_trap();

  const int32_t arguments_status =
      w_seed_process_entry0_arguments_drop(arguments);
  if (arguments_status != 0) __builtin_trap();

  return 0;
}
