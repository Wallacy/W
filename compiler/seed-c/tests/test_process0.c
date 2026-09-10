#include "w_seed_process0.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition)) {                                                        \
      (void)fprintf(stderr, "process0 check failed: %s (%s:%d)\n",         \
                    #condition, __FILE__, __LINE__);                          \
      return false;                                                            \
    }                                                                          \
  } while (0)

typedef struct {
  w_seed_process0_root root;
  w_seed_process0_arguments arguments;
  w_seed_process0_context context;
} process0_fixture;

static bool view_equals(w_seed_process0_native_view view,
                        w_seed_process0_encoding encoding, const void *data,
                        size_t length, const w_seed_process0_root *root,
                        uint64_t generation) {
  return view.encoding == encoding && view.data == data &&
         view.length == length && view.root == root &&
         view.generation == generation;
}

static bool test_posix_bytes_and_lifecycle(void) {
  static const uint8_t invalid_utf8[] = {0xffu, 0x80u, 0xc3u};
  static const uint8_t ordinary[] = {'a', 'l', 'p', 'h', 'a'};
  const w_seed_process0_native_arg items[] = {
      {W_SEED_PROCESS0_ENCODING_POSIX_BYTES, ordinary, sizeof(ordinary)},
      {W_SEED_PROCESS0_ENCODING_POSIX_BYTES, NULL, 0u},
      {W_SEED_PROCESS0_ENCODING_POSIX_BYTES, invalid_utf8,
       sizeof(invalid_utf8)},
  };
  const w_seed_process0_arg_vector vector = {
      W_SEED_PROCESS0_ENCODING_POSIX_BYTES, items,
      sizeof(items) / sizeof(items[0])};
  process0_fixture fixture = {0};
  CHECK(w_seed_process0_root_init(&vector, &fixture.root, &fixture.arguments,
                                 &fixture.context) ==
        W_SEED_PROCESS0_OK);
  CHECK(w_seed_process0_arguments_count(&fixture.arguments).status ==
        W_SEED_PROCESS0_OK);
  CHECK(w_seed_process0_arguments_count(&fixture.arguments).count == 3u);

  const w_seed_process0_get_result first =
      w_seed_process0_arguments_get(&fixture.arguments, 0u);
  CHECK(first.status == W_SEED_PROCESS0_OK &&
        view_equals(first.view, W_SEED_PROCESS0_ENCODING_POSIX_BYTES, ordinary,
                    sizeof(ordinary), &fixture.root, 1u));
  const w_seed_process0_get_result empty =
      w_seed_process0_arguments_get(&fixture.arguments, 1u);
  CHECK(empty.status == W_SEED_PROCESS0_OK &&
        view_equals(empty.view, W_SEED_PROCESS0_ENCODING_POSIX_BYTES, NULL, 0u,
                    &fixture.root, 1u));
  const w_seed_process0_get_result invalid =
      w_seed_process0_arguments_get(&fixture.arguments, 2u);
  CHECK(invalid.status == W_SEED_PROCESS0_OK &&
        view_equals(invalid.view, W_SEED_PROCESS0_ENCODING_POSIX_BYTES,
                    invalid_utf8, sizeof(invalid_utf8), &fixture.root, 1u));
  CHECK(w_seed_process0_arguments_get(&fixture.arguments, 3u).status ==
        W_SEED_PROCESS0_OUT_OF_RANGE);

  const w_seed_process0_contains_result found_invalid =
      w_seed_process0_arguments_contains_native(
          &fixture.arguments,
          (w_seed_process0_native_arg){W_SEED_PROCESS0_ENCODING_POSIX_BYTES,
                                       invalid_utf8, sizeof(invalid_utf8)});
  CHECK(found_invalid.status == W_SEED_PROCESS0_OK && found_invalid.found);
  const w_seed_process0_contains_result found_empty =
      w_seed_process0_arguments_contains_native(
          &fixture.arguments,
          (w_seed_process0_native_arg){W_SEED_PROCESS0_ENCODING_POSIX_BYTES,
                                       NULL, 0u});
  CHECK(found_empty.status == W_SEED_PROCESS0_OK && found_empty.found);
  static const uint8_t different_case[] = {'A', 'L', 'P', 'H', 'A'};
  CHECK(!w_seed_process0_arguments_contains_native(
             &fixture.arguments,
             (w_seed_process0_native_arg){W_SEED_PROCESS0_ENCODING_POSIX_BYTES,
                                          different_case,
                                          sizeof(different_case)})
             .found);
  static const uint8_t prefix[] = {'a', 'l', 'p'};
  CHECK(!w_seed_process0_arguments_contains_native(
             &fixture.arguments,
             (w_seed_process0_native_arg){W_SEED_PROCESS0_ENCODING_POSIX_BYTES,
                                          prefix, sizeof(prefix)})
             .found);
  CHECK(w_seed_process0_arguments_contains_native(
            &fixture.arguments,
            (w_seed_process0_native_arg){
                W_SEED_PROCESS0_ENCODING_WINDOWS_UTF16, different_case,
                sizeof(different_case)})
            .status == W_SEED_PROCESS0_INVALID);

  const w_seed_process0_arguments stale_alias = fixture.arguments;
  uint8_t root_before_busy[sizeof(fixture.root)];
  (void)memcpy(root_before_busy, &fixture.root, sizeof(root_before_busy));
  CHECK(w_seed_process0_root_finalize(&fixture.root) ==
        W_SEED_PROCESS0_BUSY);
  CHECK(memcmp(&fixture.root, root_before_busy, sizeof(root_before_busy)) == 0);
  CHECK(w_seed_process0_arguments_drop(&fixture.arguments) ==
        W_SEED_PROCESS0_OK);
  CHECK(w_seed_process0_arguments_count(&fixture.arguments).status ==
        W_SEED_PROCESS0_INACTIVE);
  CHECK(w_seed_process0_arguments_count(&stale_alias).status ==
        W_SEED_PROCESS0_INACTIVE);
  CHECK(w_seed_process0_arguments_drop(&fixture.arguments) ==
        W_SEED_PROCESS0_INACTIVE);
  CHECK(w_seed_process0_root_finalize(&fixture.root) == W_SEED_PROCESS0_BUSY);
  CHECK(w_seed_process0_context_drop(&fixture.context) == W_SEED_PROCESS0_OK);
  CHECK(w_seed_process0_context_drop(&fixture.context) ==
        W_SEED_PROCESS0_INACTIVE);
  CHECK(w_seed_process0_root_finalize(&fixture.root) == W_SEED_PROCESS0_OK);
  const w_seed_process0_native_arg overlapping_items[] = {
      {W_SEED_PROCESS0_ENCODING_POSIX_BYTES, ordinary, 3u},
      {W_SEED_PROCESS0_ENCODING_POSIX_BYTES, ordinary + 1u, 3u},
  };
  const w_seed_process0_arg_vector overlapping_vector = {
      W_SEED_PROCESS0_ENCODING_POSIX_BYTES, overlapping_items, 2u};
  process0_fixture overlapping_fixture = {0};
  CHECK(w_seed_process0_root_init(
            &overlapping_vector, &overlapping_fixture.root,
            &overlapping_fixture.arguments, &overlapping_fixture.context) ==
        W_SEED_PROCESS0_OK);
  const w_seed_process0_get_result overlapping_value =
      w_seed_process0_arguments_get(&overlapping_fixture.arguments, 1u);
  CHECK(overlapping_value.status == W_SEED_PROCESS0_OK &&
        overlapping_value.view.data == ordinary + 1u &&
        overlapping_value.view.length == 3u);
  CHECK(w_seed_process0_arguments_drop(&overlapping_fixture.arguments) ==
        W_SEED_PROCESS0_OK);
  CHECK(w_seed_process0_context_drop(&overlapping_fixture.context) ==
        W_SEED_PROCESS0_OK);
  CHECK(w_seed_process0_root_finalize(&overlapping_fixture.root) ==
        W_SEED_PROCESS0_OK);
  CHECK(!fixture.root.live && fixture.root.generation == 1u);
  return true;
}

static bool test_empty_vector_and_overflow(void) {
  const w_seed_process0_arg_vector empty_vector = {
      W_SEED_PROCESS0_ENCODING_POSIX_BYTES, NULL, 0u};
  process0_fixture empty = {0};
  CHECK(w_seed_process0_root_init(&empty_vector, &empty.root, &empty.arguments,
                                 &empty.context) == W_SEED_PROCESS0_OK);
  CHECK(w_seed_process0_arguments_count(&empty.arguments).status ==
        W_SEED_PROCESS0_OK &&
        w_seed_process0_arguments_count(&empty.arguments).count == 0u);
  CHECK(w_seed_process0_arguments_get(&empty.arguments, 0u).status ==
        W_SEED_PROCESS0_OUT_OF_RANGE);
  CHECK(w_seed_process0_arguments_contains_native(
            &empty.arguments,
            (w_seed_process0_native_arg){W_SEED_PROCESS0_ENCODING_POSIX_BYTES,
                                         NULL, 0u})
            .status == W_SEED_PROCESS0_OK);
  CHECK(!w_seed_process0_arguments_contains_native(
             &empty.arguments,
             (w_seed_process0_native_arg){W_SEED_PROCESS0_ENCODING_POSIX_BYTES,
                                          NULL, 0u})
             .found);
  CHECK(w_seed_process0_arguments_drop(&empty.arguments) ==
        W_SEED_PROCESS0_OK);
  CHECK(w_seed_process0_context_drop(&empty.context) == W_SEED_PROCESS0_OK);
  CHECK(w_seed_process0_root_finalize(&empty.root) == W_SEED_PROCESS0_OK);

  const w_seed_process0_native_arg item = {
      W_SEED_PROCESS0_ENCODING_POSIX_BYTES, NULL, 0u};
  const w_seed_process0_arg_vector count_overflow = {
      W_SEED_PROCESS0_ENCODING_POSIX_BYTES, &item, SIZE_MAX};
  process0_fixture count_fixture = {0};
  CHECK(w_seed_process0_root_init(&count_overflow, &count_fixture.root,
                                 &count_fixture.arguments,
                                 &count_fixture.context) ==
        W_SEED_PROCESS0_INVALID);
  const w_seed_process0_native_arg utf16_overflow = {
      W_SEED_PROCESS0_ENCODING_WINDOWS_UTF16, NULL, SIZE_MAX};
  const w_seed_process0_arg_vector utf16_vector = {
      W_SEED_PROCESS0_ENCODING_WINDOWS_UTF16, &utf16_overflow, 1u};
  process0_fixture utf16_fixture = {0};
  CHECK(w_seed_process0_root_init(&utf16_vector, &utf16_fixture.root,
                                 &utf16_fixture.arguments,
                                 &utf16_fixture.context) ==
        W_SEED_PROCESS0_INVALID);
  return true;
}

static bool test_windows_units_and_kind(void) {
  static const uint16_t lone_surrogate[] = {0xd800u, (uint16_t)'X'};
  static const uint16_t ordinary[] = {(uint16_t)'A', (uint16_t)'B'};
  const w_seed_process0_native_arg items[] = {
      {W_SEED_PROCESS0_ENCODING_WINDOWS_UTF16, lone_surrogate,
       sizeof(lone_surrogate) / sizeof(lone_surrogate[0])},
      {W_SEED_PROCESS0_ENCODING_WINDOWS_UTF16, ordinary,
       sizeof(ordinary) / sizeof(ordinary[0])},
      {W_SEED_PROCESS0_ENCODING_WINDOWS_UTF16, NULL, 0u},
  };
  const w_seed_process0_arg_vector vector = {
      W_SEED_PROCESS0_ENCODING_WINDOWS_UTF16, items,
      sizeof(items) / sizeof(items[0])};
  process0_fixture fixture = {0};
  CHECK(w_seed_process0_root_init(&vector, &fixture.root, &fixture.arguments,
                                 &fixture.context) ==
        W_SEED_PROCESS0_OK);
  const w_seed_process0_get_result lone =
      w_seed_process0_arguments_get(&fixture.arguments, 0u);
  CHECK(lone.status == W_SEED_PROCESS0_OK &&
        view_equals(lone.view, W_SEED_PROCESS0_ENCODING_WINDOWS_UTF16,
                    lone_surrogate, 2u, &fixture.root, 1u));
  CHECK(w_seed_process0_arguments_contains_native(
            &fixture.arguments,
            (w_seed_process0_native_arg){W_SEED_PROCESS0_ENCODING_WINDOWS_UTF16,
                                         lone_surrogate, 2u})
            .found);
  CHECK(w_seed_process0_arguments_get(&fixture.arguments, 2u).view.length == 0u);
  const w_seed_process0_owner_kind kind_before = fixture.arguments.kind;
  fixture.arguments.kind = W_SEED_PROCESS0_OWNER_CONTEXT;
  CHECK(w_seed_process0_arguments_count(&fixture.arguments).status ==
        W_SEED_PROCESS0_INVALID);
  fixture.arguments.kind = kind_before;
  CHECK(w_seed_process0_arguments_drop(&fixture.arguments) ==
        W_SEED_PROCESS0_OK);
  CHECK(w_seed_process0_context_drop(&fixture.context) == W_SEED_PROCESS0_OK);
  CHECK(w_seed_process0_root_finalize(&fixture.root) == W_SEED_PROCESS0_OK);
  return true;
}

static bool test_init_is_all_or_nothing(void) {
  static const uint8_t bytes[] = {'x', 0xffu};

  process0_fixture malformed = {0};
  (void)memset(&malformed.arguments, 0xa1, sizeof(malformed.arguments));
  (void)memset(&malformed.context, 0xa2, sizeof(malformed.context));
  uint8_t malformed_before[sizeof(malformed)];
  (void)memcpy(malformed_before, &malformed, sizeof(malformed_before));
  const w_seed_process0_native_arg malformed_item = {
      W_SEED_PROCESS0_ENCODING_NONE, bytes, sizeof(bytes)};
  const w_seed_process0_arg_vector mixed_vector = {
      W_SEED_PROCESS0_ENCODING_POSIX_BYTES, &malformed_item, 1u};
  CHECK(w_seed_process0_root_init(
            &mixed_vector, &malformed.root, &malformed.arguments,
            &malformed.context) == W_SEED_PROCESS0_INVALID);
  CHECK(memcmp(&malformed, malformed_before, sizeof(malformed_before)) == 0);

  process0_fixture overlap = {0};
  (void)memset(&overlap.arguments, 0xb1, sizeof(overlap.arguments));
  (void)memset(&overlap.context, 0xb2, sizeof(overlap.context));
  uint8_t overlap_before[sizeof(overlap)];
  (void)memcpy(overlap_before, &overlap, sizeof(overlap_before));
  CHECK(w_seed_process0_root_init(
            (const w_seed_process0_arg_vector *)(const void *)&overlap.root,
            &overlap.root, &overlap.arguments, &overlap.context) ==
        W_SEED_PROCESS0_OVERLAP);
  CHECK(memcmp(&overlap, overlap_before, sizeof(overlap_before)) == 0);

  process0_fixture backing_overlap = {0};
  (void)memset(&backing_overlap.arguments, 0xc1,
               sizeof(backing_overlap.arguments));
  (void)memset(&backing_overlap.context, 0xc2,
               sizeof(backing_overlap.context));
  const w_seed_process0_native_arg overlapping_item = {
      W_SEED_PROCESS0_ENCODING_POSIX_BYTES, &backing_overlap.root,
      sizeof(backing_overlap.root)};
  const w_seed_process0_arg_vector overlapping_vector = {
      W_SEED_PROCESS0_ENCODING_POSIX_BYTES, &overlapping_item, 1u};
  uint8_t backing_overlap_before[sizeof(backing_overlap)];
  (void)memcpy(backing_overlap_before, &backing_overlap,
               sizeof(backing_overlap_before));
  CHECK(w_seed_process0_root_init(
            &overlapping_vector, &backing_overlap.root,
            &backing_overlap.arguments, &backing_overlap.context) ==
        W_SEED_PROCESS0_OVERLAP);
  CHECK(memcmp(&backing_overlap, backing_overlap_before,
               sizeof(backing_overlap_before)) == 0);

  process0_fixture invalid_encoding = {0};
  const w_seed_process0_native_arg invalid_item = {
      W_SEED_PROCESS0_ENCODING_WINDOWS_UTF16, bytes, sizeof(bytes)};
  const w_seed_process0_arg_vector invalid_vector = {
      W_SEED_PROCESS0_ENCODING_POSIX_BYTES, &invalid_item, 1u};
  uint8_t invalid_before[sizeof(invalid_encoding)];
  (void)memcpy(invalid_before, &invalid_encoding, sizeof(invalid_before));
  CHECK(w_seed_process0_root_init(
            &invalid_vector, &invalid_encoding.root,
            &invalid_encoding.arguments, &invalid_encoding.context) ==
        W_SEED_PROCESS0_INVALID);
  CHECK(memcmp(&invalid_encoding, invalid_before, sizeof(invalid_before)) == 0);
  return true;
}

static bool test_stale_generation_after_reinit(void) {
  static const uint8_t byte = 'r';
  const w_seed_process0_native_arg item = {
      W_SEED_PROCESS0_ENCODING_POSIX_BYTES, &byte, 1u};
  const w_seed_process0_arg_vector vector = {
      W_SEED_PROCESS0_ENCODING_POSIX_BYTES, &item, 1u};
  process0_fixture fixture = {0};
  CHECK(w_seed_process0_root_init(&vector, &fixture.root, &fixture.arguments,
                                 &fixture.context) ==
        W_SEED_PROCESS0_OK);
  w_seed_process0_arguments stale = fixture.arguments;
  CHECK(w_seed_process0_arguments_drop(&fixture.arguments) ==
        W_SEED_PROCESS0_OK);
  CHECK(w_seed_process0_context_drop(&fixture.context) == W_SEED_PROCESS0_OK);
  CHECK(w_seed_process0_root_finalize(&fixture.root) == W_SEED_PROCESS0_OK);
  CHECK(w_seed_process0_root_init(&vector, &fixture.root, &fixture.arguments,
                                 &fixture.context) ==
        W_SEED_PROCESS0_OK);
  /* First exercise the copied-alias barrier, then isolate the generation
   * barrier on the real address-stable owner record. */
  CHECK(w_seed_process0_arguments_count(&stale).status ==
        W_SEED_PROCESS0_INACTIVE);
  fixture.arguments.generation = 1u;
  CHECK(w_seed_process0_arguments_count(&fixture.arguments).status ==
        W_SEED_PROCESS0_INACTIVE);
  fixture.arguments.generation = 2u;
  CHECK(fixture.root.generation == 2u);
  CHECK(w_seed_process0_arguments_drop(&fixture.arguments) ==
        W_SEED_PROCESS0_OK);
  CHECK(w_seed_process0_context_drop(&fixture.context) == W_SEED_PROCESS0_OK);
  CHECK(w_seed_process0_root_finalize(&fixture.root) == W_SEED_PROCESS0_OK);
  fixture.arguments.generation = UINT64_MAX;
  fixture.context.generation = UINT64_MAX;
  fixture.root.generation = UINT64_MAX;
  uint8_t exhausted_before[sizeof(fixture)];
  (void)memcpy(exhausted_before, &fixture, sizeof(exhausted_before));
  CHECK(w_seed_process0_root_init(&vector, &fixture.root, &fixture.arguments,
                                 &fixture.context) ==
        W_SEED_PROCESS0_INVALID);
  CHECK(memcmp(&fixture, exhausted_before, sizeof(exhausted_before)) == 0);
  return true;
}

static bool test_wrapper_destination_and_finalize_guards(void) {
  static const uint8_t byte = 'g';
  const w_seed_process0_native_arg item = {
      W_SEED_PROCESS0_ENCODING_POSIX_BYTES, &byte, 1u};
  const w_seed_process0_arg_vector vector = {
      W_SEED_PROCESS0_ENCODING_POSIX_BYTES, &item, 1u};
  process0_fixture first = {0};
  process0_fixture second = {0};
  CHECK(w_seed_process0_root_init(&vector, &first.root, &first.arguments,
                                 &first.context) == W_SEED_PROCESS0_OK);
  uint8_t first_before[sizeof(first)];
  uint8_t second_before[sizeof(second)];
  (void)memcpy(first_before, &first, sizeof(first_before));
  (void)memcpy(second_before, &second, sizeof(second_before));
  CHECK(w_seed_process0_root_init(&vector, &second.root, &first.arguments,
                                 &second.context) == W_SEED_PROCESS0_BUSY);
  CHECK(memcmp(&first, first_before, sizeof(first_before)) == 0 &&
        memcmp(&second, second_before, sizeof(second_before)) == 0);

  CHECK(w_seed_process0_arguments_drop(&first.arguments) ==
        W_SEED_PROCESS0_OK);
  (void)memcpy(first_before, &first, sizeof(first_before));
  (void)memcpy(second_before, &second, sizeof(second_before));
  CHECK(w_seed_process0_root_init(&vector, &second.root, &first.arguments,
                                 &second.context) == W_SEED_PROCESS0_INVALID);
  CHECK(memcmp(&first, first_before, sizeof(first_before)) == 0 &&
        memcmp(&second, second_before, sizeof(second_before)) == 0);
  CHECK(w_seed_process0_context_drop(&first.context) == W_SEED_PROCESS0_OK);
  CHECK(w_seed_process0_root_finalize(&first.root) == W_SEED_PROCESS0_OK);

  process0_fixture released = {0};
  CHECK(w_seed_process0_root_init(&vector, &released.root,
                                 &released.arguments, &released.context) ==
        W_SEED_PROCESS0_OK);
  CHECK(w_seed_process0_arguments_drop(&released.arguments) ==
        W_SEED_PROCESS0_OK);
  CHECK(w_seed_process0_context_drop(&released.context) == W_SEED_PROCESS0_OK);
  uint8_t released_before[sizeof(released.root)];
  (void)memcpy(released_before, &released.root, sizeof(released_before));
  const uint64_t generation_before = released.arguments.generation;
  released.arguments.generation = generation_before + 1u;
  CHECK(w_seed_process0_root_finalize(&released.root) ==
        W_SEED_PROCESS0_INVALID);
  CHECK(memcmp(&released.root, released_before, sizeof(released_before)) == 0);
  released.arguments.generation = generation_before;
  const w_seed_process0_owner_kind kind_before = released.arguments.kind;
  released.arguments.kind = W_SEED_PROCESS0_OWNER_CONTEXT;
  CHECK(w_seed_process0_root_finalize(&released.root) ==
        W_SEED_PROCESS0_INVALID);
  CHECK(memcmp(&released.root, released_before, sizeof(released_before)) == 0);
  released.arguments.kind = kind_before;
  CHECK(w_seed_process0_root_finalize(&released.root) == W_SEED_PROCESS0_OK);

  process0_fixture live_flag = {0};
  CHECK(w_seed_process0_root_init(&vector, &live_flag.root,
                                 &live_flag.arguments, &live_flag.context) ==
        W_SEED_PROCESS0_OK);
  CHECK(w_seed_process0_context_drop(&live_flag.context) == W_SEED_PROCESS0_OK);
  live_flag.root.arguments_live = false;
  uint8_t live_flag_before[sizeof(live_flag.root)];
  (void)memcpy(live_flag_before, &live_flag.root, sizeof(live_flag_before));
  CHECK(w_seed_process0_root_finalize(&live_flag.root) ==
        W_SEED_PROCESS0_BUSY);
  CHECK(memcmp(&live_flag.root, live_flag_before, sizeof(live_flag_before)) ==
        0);
  live_flag.root.arguments_live = true;
  CHECK(w_seed_process0_arguments_drop(&live_flag.arguments) ==
        W_SEED_PROCESS0_OK);
  CHECK(w_seed_process0_root_finalize(&live_flag.root) == W_SEED_PROCESS0_OK);
  return true;
}

static bool test_selected_argv(int argc, char **argv) {
  CHECK(argc == 4 && strcmp(argv[1], "--selected-argv") == 0 &&
        strcmp(argv[2], "alpha") == 0 && strcmp(argv[3], "payload") == 0);
  /* The test deliberately selects indices 2 and 3. It does not assign a W
   * meaning to argv[0], and it does not claim a native Windows UTF-16 path. */
  const w_seed_process0_native_arg selected[] = {
      {W_SEED_PROCESS0_ENCODING_POSIX_BYTES, argv[2], strlen(argv[2])},
      {W_SEED_PROCESS0_ENCODING_POSIX_BYTES, argv[3], strlen(argv[3])},
  };
  const w_seed_process0_arg_vector vector = {
      W_SEED_PROCESS0_ENCODING_POSIX_BYTES, selected, 2u};
  process0_fixture fixture = {0};
  CHECK(w_seed_process0_root_init(&vector, &fixture.root, &fixture.arguments,
                                 &fixture.context) ==
        W_SEED_PROCESS0_OK);
  CHECK(w_seed_process0_arguments_count(&fixture.arguments).count == 2u);
  for (size_t index = 0u; index < 2u; index += 1u) {
    const w_seed_process0_get_result value =
        w_seed_process0_arguments_get(&fixture.arguments, index);
    CHECK(value.status == W_SEED_PROCESS0_OK &&
          value.view.data == selected[index].data &&
          value.view.length == selected[index].length);
  }
  CHECK(w_seed_process0_arguments_drop(&fixture.arguments) ==
        W_SEED_PROCESS0_OK);
  CHECK(w_seed_process0_context_drop(&fixture.context) == W_SEED_PROCESS0_OK);
  CHECK(w_seed_process0_root_finalize(&fixture.root) == W_SEED_PROCESS0_OK);
  return true;
}

int main(int argc, char **argv) {
  if (!test_posix_bytes_and_lifecycle() ||
      !test_empty_vector_and_overflow() || !test_windows_units_and_kind() ||
      !test_init_is_all_or_nothing() ||
      !test_stale_generation_after_reinit() ||
      !test_wrapper_destination_and_finalize_guards())
    return 1;
  if (argc == 1) {
    (void)puts("w_seed_process0_tests: ok");
    return 0;
  }
  if (argc != 4 || !test_selected_argv(argc, argv)) return 1;
  (void)puts("w_seed_process0_tests: ok");
  return 0;
}
