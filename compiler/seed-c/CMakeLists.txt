cmake_minimum_required(VERSION 3.20)

project(w_seed_source_reader C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(w_seed_source STATIC
  src/w_seed_source.c
  src/w_seed_unicode.c
  src/w_seed_unicode_data.c
  src/w_seed_lexer.c
)

target_include_directories(w_seed_source
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(MSVC)
  target_compile_options(w_seed_source PRIVATE /W4 /WX)
else()
  target_compile_options(w_seed_source PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wshadow
    -Werror
  )
endif()

add_executable(w_seed_source_tests
  tests/test_source.c
)
target_link_libraries(w_seed_source_tests PRIVATE w_seed_source)

add_executable(w_seed_source_probe
  tests/source_probe.c
)
target_link_libraries(w_seed_source_probe PRIVATE w_seed_source)

add_executable(w_seed_lexer_tests
  tests/test_lexer.c
)
target_link_libraries(w_seed_lexer_tests PRIVATE w_seed_source)

add_executable(w_seed_lexer_probe
  tests/lexer_probe.c
)
target_link_libraries(w_seed_lexer_probe PRIVATE w_seed_source)

add_executable(w_seed_unicode_tests
  tests/test_unicode.c
)
target_link_libraries(w_seed_unicode_tests PRIVATE w_seed_source)

if(MSVC)
  target_compile_options(w_seed_source_tests PRIVATE /W4 /WX)
  target_compile_options(w_seed_source_probe PRIVATE /W4 /WX)
  target_compile_options(w_seed_lexer_tests PRIVATE /W4 /WX)
  target_compile_options(w_seed_lexer_probe PRIVATE /W4 /WX)
  target_compile_options(w_seed_unicode_tests PRIVATE /W4 /WX)
else()
  target_compile_options(w_seed_source_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wshadow
    -Werror
  )
  target_compile_options(w_seed_source_probe PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wshadow
    -Werror
  )
  target_compile_options(w_seed_lexer_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wshadow
    -Werror
  )
  target_compile_options(w_seed_lexer_probe PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wshadow
    -Werror
  )
  target_compile_options(w_seed_unicode_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wshadow
    -Werror
  )
endif()

enable_testing()
add_test(NAME w_seed_source_unit COMMAND w_seed_source_tests)
add_test(NAME w_seed_lexer_unit COMMAND w_seed_lexer_tests)
add_test(NAME w_seed_unicode_unit COMMAND w_seed_unicode_tests)
