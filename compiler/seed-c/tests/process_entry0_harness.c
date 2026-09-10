#include "w_seed_process0.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#endif

enum {
  PROCESS_ENTRY0_MAX_SELECTED_ARGUMENTS = 32,
  PROCESS_ENTRY0_FAILURE = 11,
  PROCESS_ENTRY0_HANDLER_FAILURE = 12,
};

typedef int32_t (*process_entry0_handler)(void *arguments, void *context);

extern int32_t w_seed_process_entry0_handler(void *arguments, void *context);

static w_seed_process0_arguments *expected_arguments;
static w_seed_process0_context *expected_context;
static const char *selected_fault;
static size_t drop_count;
static w_seed_process0_owner_kind drop_order[2];

static bool fault_is(const char *name) {
  return selected_fault != NULL && name != NULL &&
         strcmp(selected_fault, name) == 0;
}

/* These two functions are private ABI adapters for this gate. They compare
 * the opaque address before the typed cast, then forward to PROCESS0. */
int32_t w_seed_process_entry0_context_drop(void *opaque_context) {
  if (opaque_context != (void *)expected_context || expected_context == NULL ||
      drop_count != 0u || fault_is("wrong-context"))
    return (int32_t)W_SEED_PROCESS0_INVALID;
  drop_order[drop_count] = W_SEED_PROCESS0_OWNER_CONTEXT;
  drop_count += 1u;
  if (fault_is("missing") || fault_is("noop-success")) return 0;
  return (int32_t)w_seed_process0_context_drop(expected_context);
}

int32_t w_seed_process_entry0_arguments_drop(void *opaque_arguments) {
  if (opaque_arguments != (void *)expected_arguments ||
      expected_arguments == NULL || drop_count != 1u ||
      drop_order[0] != W_SEED_PROCESS0_OWNER_CONTEXT ||
      fault_is("wrong-arguments"))
    return (int32_t)W_SEED_PROCESS0_INVALID;
  drop_order[drop_count] = W_SEED_PROCESS0_OWNER_ARGUMENTS;
  drop_count += 1u;
  if (fault_is("noop-success")) return 0;
  return (int32_t)w_seed_process0_arguments_drop(expected_arguments);
}

static bool owners_live(const w_seed_process0_root *root,
                        const w_seed_process0_arguments *arguments,
                        const w_seed_process0_context *context) {
  return root != NULL && arguments != NULL && context != NULL && root->live &&
         root->arguments_live && root->context_live && arguments->live &&
         context->live;
}

static bool owners_released(const w_seed_process0_root *root,
                            const w_seed_process0_arguments *arguments,
                            const w_seed_process0_context *context) {
  return root != NULL && arguments != NULL && context != NULL &&
         !root->arguments_live && !root->context_live && !arguments->live &&
         !context->live;
}

#if defined(_WIN32)
static void suppress_fault_dialogs(void) {
  (void)SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX |
                     SEM_NOOPENFILEERRORBOX);
}
#endif

int main(int argc, char **argv) {
#if defined(_WIN32)
  suppress_fault_dialogs();
#endif
  if (argc < 1 || argv == NULL || argv[0] == NULL ||
      argc - 1 > PROCESS_ENTRY0_MAX_SELECTED_ARGUMENTS)
    return PROCESS_ENTRY0_FAILURE;

  selected_fault = getenv("W_SEED_PROCESS_ENTRY0_FAULT");
  w_seed_process0_native_arg selected_items[
      PROCESS_ENTRY0_MAX_SELECTED_ARGUMENTS];
  const size_t selected_count = (size_t)(argc - 1);
  for (size_t index = 0u; index < selected_count; index += 1u) {
    if (argv[index + 1u] == NULL) return PROCESS_ENTRY0_FAILURE;
    selected_items[index] = (w_seed_process0_native_arg){
        W_SEED_PROCESS0_ENCODING_POSIX_BYTES, argv[index + 1u],
        strlen(argv[index + 1u])};
  }
  const w_seed_process0_arg_vector vector = {
      W_SEED_PROCESS0_ENCODING_POSIX_BYTES,
      selected_count == 0u ? NULL : selected_items,
      selected_count};
  w_seed_process0_root root = {0};
  w_seed_process0_arguments arguments = {0};
  w_seed_process0_context context = {0};
  if (w_seed_process0_root_init(&vector, &root, &arguments, &context) !=
          W_SEED_PROCESS0_OK ||
      !owners_live(&root, &arguments, &context))
    return PROCESS_ENTRY0_FAILURE;

  expected_arguments = &arguments;
  expected_context = &context;
  drop_count = 0u;
  const process_entry0_handler handler = w_seed_process_entry0_handler;
  if (fault_is("stale-generation")) context.generation += 1u;
  void *handler_arguments = (void *)expected_arguments;
  void *handler_context = (void *)expected_context;
  if (fault_is("reversed-arguments")) {
    handler_arguments = (void *)expected_context;
    handler_context = (void *)expected_arguments;
  }
  const int32_t handler_status =
      handler(handler_arguments, handler_context);
  if (handler_status != 0 || !owners_released(&root, &arguments, &context) ||
      drop_count != 2u || drop_order[0] != W_SEED_PROCESS0_OWNER_CONTEXT ||
      drop_order[1] != W_SEED_PROCESS0_OWNER_ARGUMENTS)
    return handler_status == 0 ? PROCESS_ENTRY0_FAILURE
                               : PROCESS_ENTRY0_HANDLER_FAILURE;

  if (w_seed_process0_root_finalize(&root) != W_SEED_PROCESS0_OK ||
      root.live || root.arguments_live || root.context_live)
    return PROCESS_ENTRY0_FAILURE;
  return 0;
}
