cmake_minimum_required(VERSION 3.20)

project(w_seed_source_reader C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(w_seed_source STATIC
  src/w_seed_source.c
  src/w_seed_foreign.c
  src/w_seed_unicode.c
  src/w_seed_unicode_data.c
  src/w_seed_lexer.c
  src/w_seed_parser.c
  src/w_seed_formatter.c
  src/w_seed_sha256.c
  src/w_seed_diagnostic.c
  src/w_seed_frontend.c
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

add_executable(w_seed_parser_tests
  tests/test_parser.c
)
target_link_libraries(w_seed_parser_tests PRIVATE w_seed_source)

add_executable(w_seed_parser_probe
  tests/parser_probe.c
)
target_link_libraries(w_seed_parser_probe PRIVATE w_seed_source)

add_executable(w_seed_formatter_probe
  tests/formatter_probe.c
)
target_link_libraries(w_seed_formatter_probe PRIVATE w_seed_source)

add_executable(w_seed_formatter_tests
  tests/test_formatter.c
)
target_link_libraries(w_seed_formatter_tests PRIVATE w_seed_source)

add_executable(w_seed_diagnostic_probe
  tests/diagnostic_probe.c
)
target_link_libraries(w_seed_diagnostic_probe PRIVATE w_seed_source)

add_executable(w_seed_diagnostic_tests
  tests/test_diagnostic.c
)
target_link_libraries(w_seed_diagnostic_tests PRIVATE w_seed_source)

add_executable(w_seed_diagnostic_mapping_probe
  tests/diagnostic_mapping_probe.c
)
target_link_libraries(w_seed_diagnostic_mapping_probe PRIVATE w_seed_source)

add_executable(w_seed_foreign_tests
  tests/test_foreign.c
)
target_link_libraries(w_seed_foreign_tests PRIVATE w_seed_source)

add_executable(w_seed_foreign_probe
  tests/foreign_probe.c
)
target_link_libraries(w_seed_foreign_probe PRIVATE w_seed_source)

add_executable(w_seed_frontend_probe
  tests/frontend_probe.c
)
target_link_libraries(w_seed_frontend_probe PRIVATE w_seed_source)

add_executable(w_seed_frontend_tests
  tests/test_frontend.c
)
target_link_libraries(w_seed_frontend_tests PRIVATE w_seed_source)

if(MSVC)
  target_compile_options(w_seed_source_tests PRIVATE /W4 /WX)
  target_compile_options(w_seed_source_probe PRIVATE /W4 /WX)
  target_compile_options(w_seed_lexer_tests PRIVATE /W4 /WX)
  target_compile_options(w_seed_lexer_probe PRIVATE /W4 /WX)
  target_compile_options(w_seed_unicode_tests PRIVATE /W4 /WX)
  target_compile_options(w_seed_parser_tests PRIVATE /W4 /WX)
  target_compile_options(w_seed_parser_probe PRIVATE /W4 /WX)
  target_compile_options(w_seed_formatter_probe PRIVATE /W4 /WX)
  target_compile_options(w_seed_formatter_tests PRIVATE /W4 /WX)
  target_compile_options(w_seed_diagnostic_probe PRIVATE /W4 /WX)
  target_compile_options(w_seed_diagnostic_tests PRIVATE /W4 /WX)
  target_compile_options(w_seed_diagnostic_mapping_probe PRIVATE /W4 /WX)
  target_compile_options(w_seed_foreign_tests PRIVATE /W4 /WX)
  target_compile_options(w_seed_foreign_probe PRIVATE /W4 /WX)
  target_compile_options(w_seed_frontend_probe PRIVATE /W4 /WX)
  target_compile_options(w_seed_frontend_tests PRIVATE /W4 /WX)
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
  target_compile_options(w_seed_parser_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wshadow
    -Werror
  )
  target_compile_options(w_seed_parser_probe PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wshadow
    -Werror
  )
  target_compile_options(w_seed_formatter_probe PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wshadow
    -Werror
  )
  target_compile_options(w_seed_formatter_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wshadow
    -Werror
  )
  target_compile_options(w_seed_diagnostic_probe PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wshadow
    -Werror
  )
  target_compile_options(w_seed_diagnostic_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wshadow
    -Werror
  )
  target_compile_options(w_seed_diagnostic_mapping_probe PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wshadow
    -Werror
  )
  target_compile_options(w_seed_foreign_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wshadow
    -Werror
  )
  target_compile_options(w_seed_foreign_probe PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wshadow
    -Werror
  )
  target_compile_options(w_seed_frontend_probe PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wshadow
    -Werror
  )
  target_compile_options(w_seed_frontend_tests PRIVATE
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
add_test(NAME w_seed_parser_unit COMMAND w_seed_parser_tests)
add_test(NAME w_seed_formatter_unit COMMAND w_seed_formatter_tests)
add_test(NAME w_seed_diagnostic_unit COMMAND w_seed_diagnostic_tests)
add_test(NAME w_seed_foreign_unit COMMAND w_seed_foreign_tests)
add_test(NAME w_seed_frontend_unit COMMAND w_seed_frontend_tests)
