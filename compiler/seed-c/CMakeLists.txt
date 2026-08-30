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
  src/w_seed_module_scan.c
  src/w_seed_ephemeral_graph.c
  src/w_seed_ephemeral_provider.c
  src/w_seed_ephemeral_provider_linux.c
  src/w_seed_ephemeral_provider_windows.c
  src/w_seed_ephemeral_driver.c
  src/w_seed_formatter.c
  src/w_seed_sha256.c
  src/w_seed_diagnostic.c
  src/w_seed_ephemeral_check.c
  src/w_seed_frontend.c
  src/w_seed_constir.c
  src/w_seed_generic_validation.c
  src/w_seed_hlo0.c
  src/w_seed_hlo1.c
  src/w_seed_hir0.c
  src/w_seed_run0.c
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

add_executable(w_seed_module_scan_tests
  tests/test_module_scan.c
)
target_link_libraries(w_seed_module_scan_tests PRIVATE w_seed_source)

add_executable(w_seed_ephemeral_graph_tests
  tests/test_ephemeral_graph.c
)
target_link_libraries(w_seed_ephemeral_graph_tests PRIVATE w_seed_source)

add_executable(w_seed_ephemeral_provider_tests
  tests/test_ephemeral_provider.c
)
target_link_libraries(w_seed_ephemeral_provider_tests PRIVATE w_seed_source)

add_executable(w_seed_ephemeral_provider_adapter_tests
  tests/test_ephemeral_provider_linux.c
)
target_link_libraries(w_seed_ephemeral_provider_adapter_tests PRIVATE w_seed_source)

add_executable(w_seed_ephemeral_provider_windows_tests
  tests/test_ephemeral_provider_windows.c
)
target_link_libraries(w_seed_ephemeral_provider_windows_tests PRIVATE w_seed_source)

if(WIN32)
  target_link_libraries(w_seed_source PUBLIC ntdll)
endif()

add_executable(w_seed_ephemeral_driver_tests
  tests/test_ephemeral_driver.c
)
target_link_libraries(w_seed_ephemeral_driver_tests PRIVATE w_seed_source)

add_executable(w_seed_ephemeral_driver_linux_tests
  tests/test_ephemeral_driver_linux.c
)
target_link_libraries(w_seed_ephemeral_driver_linux_tests PRIVATE w_seed_source)

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

add_executable(w_seed_check_driver
  tests/check_driver.c
  cli/check.c
  cli/check_host.c
  cli/io.c
  cli/check_pipeline.c
  cli/check_retry.c
  cli/check_storage.c
)
target_link_libraries(w_seed_check_driver PRIVATE w_seed_source)

add_executable(w_seed_check_storage_tests
  tests/test_check_storage.c
  cli/check_storage.c
)
target_include_directories(w_seed_check_storage_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/cli
)
target_link_libraries(w_seed_check_storage_tests PRIVATE w_seed_source)

add_executable(w_seed_check_retry_tests
  tests/test_check_retry.c
  cli/check_retry.c
  cli/check_storage.c
)
target_include_directories(w_seed_check_retry_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/cli
)
target_link_libraries(w_seed_check_retry_tests PRIVATE w_seed_source)

add_executable(w_seed_check_pipeline_tests
  tests/test_check_pipeline.c
  cli/check_pipeline.c
  cli/check_retry.c
  cli/check_storage.c
)
target_include_directories(w_seed_check_pipeline_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/cli
)
target_link_libraries(w_seed_check_pipeline_tests PRIVATE w_seed_source)

add_executable(w_seed_check_host_tests
  tests/test_check_host.c
  cli/check_host.c
)
target_include_directories(w_seed_check_host_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/cli
)
target_link_libraries(w_seed_check_host_tests PRIVATE w_seed_source)

add_executable(w
  cli/w.c
  cli/check.c
  cli/check_host.c
  cli/check_pipeline.c
  cli/check_retry.c
  cli/check_storage.c
  cli/io.c
)
target_include_directories(w PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/cli
)
target_link_libraries(w PRIVATE w_seed_source)

add_executable(w_seed_constir_probe
  tests/constir_probe.c
)
target_link_libraries(w_seed_constir_probe PRIVATE w_seed_source)

add_executable(w_seed_constir_tests
  tests/test_constir.c
)
target_link_libraries(w_seed_constir_tests PRIVATE w_seed_source)

add_executable(w_seed_generic_validation_tests
  tests/test_generic_validation.c
)
target_link_libraries(w_seed_generic_validation_tests PRIVATE w_seed_source)

add_executable(w_seed_frontend_tests
  tests/test_frontend.c
)
target_link_libraries(w_seed_frontend_tests PRIVATE w_seed_source)

add_executable(w_seed_hlo0_tests
  tests/test_hlo0.c
)
target_link_libraries(w_seed_hlo0_tests PRIVATE w_seed_source)

add_executable(w_seed_hlo0_gate
  tests/hlo0_gate.c
)
target_link_libraries(w_seed_hlo0_gate PRIVATE w_seed_source)

add_executable(w_seed_hlo1_tests
  tests/test_hlo1.c
)
target_link_libraries(w_seed_hlo1_tests PRIVATE w_seed_source)

add_executable(w_seed_hlo1_gate
  tests/hlo1_gate.c
)
target_link_libraries(w_seed_hlo1_gate PRIVATE w_seed_source)

add_executable(w_seed_hir0_tests
  tests/test_hir0.c
)
target_link_libraries(w_seed_hir0_tests PRIVATE w_seed_source)

add_executable(w_seed_run0_tests
  tests/test_run0.c
)
target_link_libraries(w_seed_run0_tests PRIVATE w_seed_source)

add_executable(w_seed_run0_gate
  tests/run0_gate.c
  cli/io.c
)
target_include_directories(w_seed_run0_gate PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/cli
)
target_link_libraries(w_seed_run0_gate PRIVATE w_seed_source)

add_executable(w_seed_cli_io_tests
  tests/test_cli_io.c
  cli/io.c
)
target_include_directories(w_seed_cli_io_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/cli
)
target_link_libraries(w_seed_cli_io_tests PRIVATE w_seed_source)

if(MSVC)
  target_compile_options(w_seed_source_tests PRIVATE /W4 /WX)
  target_compile_options(w_seed_source_probe PRIVATE /W4 /WX)
  target_compile_options(w_seed_lexer_tests PRIVATE /W4 /WX)
  target_compile_options(w_seed_lexer_probe PRIVATE /W4 /WX)
  target_compile_options(w_seed_unicode_tests PRIVATE /W4 /WX)
  target_compile_options(w_seed_parser_tests PRIVATE /W4 /WX)
  target_compile_options(w_seed_parser_probe PRIVATE /W4 /WX)
  target_compile_options(w_seed_module_scan_tests PRIVATE /W4 /WX)
  target_compile_options(w_seed_ephemeral_graph_tests PRIVATE /W4 /WX)
  target_compile_options(w_seed_ephemeral_provider_tests PRIVATE /W4 /WX)
  target_compile_options(w_seed_ephemeral_provider_adapter_tests PRIVATE /W4 /WX)
  target_compile_options(w_seed_ephemeral_provider_windows_tests PRIVATE /W4 /WX)
  target_compile_options(w_seed_ephemeral_driver_tests PRIVATE /W4 /WX)
  target_compile_options(w_seed_ephemeral_driver_linux_tests PRIVATE /W4 /WX)
  target_compile_options(w_seed_formatter_probe PRIVATE /W4 /WX)
  target_compile_options(w_seed_formatter_tests PRIVATE /W4 /WX)
  target_compile_options(w_seed_diagnostic_probe PRIVATE /W4 /WX)
  target_compile_options(w_seed_diagnostic_tests PRIVATE /W4 /WX)
  target_compile_options(w_seed_diagnostic_mapping_probe PRIVATE /W4 /WX)
  target_compile_options(w_seed_foreign_tests PRIVATE /W4 /WX)
  target_compile_options(w_seed_foreign_probe PRIVATE /W4 /WX)
  target_compile_options(w_seed_frontend_probe PRIVATE /W4 /WX)
  target_compile_options(w_seed_check_driver PRIVATE /W4 /WX)
  target_compile_options(w_seed_check_storage_tests PRIVATE /W4 /WX)
  target_compile_options(w_seed_check_retry_tests PRIVATE /W4 /WX)
  target_compile_options(w_seed_check_pipeline_tests PRIVATE /W4 /WX)
  target_compile_options(w_seed_check_host_tests PRIVATE /W4 /WX)
  target_compile_options(w PRIVATE /W4 /WX)
  target_compile_options(w_seed_frontend_tests PRIVATE /W4 /WX)
  target_compile_options(w_seed_hlo0_tests PRIVATE /W4 /WX)
  target_compile_options(w_seed_hlo0_gate PRIVATE /W4 /WX)
  target_compile_options(w_seed_hlo1_tests PRIVATE /W4 /WX)
  target_compile_options(w_seed_hlo1_gate PRIVATE /W4 /WX)
  target_compile_options(w_seed_hir0_tests PRIVATE /W4 /WX)
  target_compile_options(w_seed_run0_tests PRIVATE /W4 /WX)
  target_compile_options(w_seed_run0_gate PRIVATE /W4 /WX)
  target_compile_options(w_seed_cli_io_tests PRIVATE /W4 /WX)
  target_compile_options(w_seed_constir_probe PRIVATE /W4 /WX)
  target_compile_options(w_seed_constir_tests PRIVATE /W4 /WX)
  target_compile_options(w_seed_generic_validation_tests PRIVATE /W4 /WX)
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
  target_compile_options(w_seed_module_scan_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wshadow
    -Werror
  )
  target_compile_options(w_seed_ephemeral_provider_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wshadow
    -Werror
  )
  target_compile_options(w_seed_ephemeral_provider_adapter_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wshadow
    -Werror
  )
  target_compile_options(w_seed_ephemeral_provider_windows_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wshadow
    -Werror
  )
  target_compile_options(w_seed_ephemeral_driver_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wshadow
    -Werror
  )
  target_compile_options(w_seed_ephemeral_driver_linux_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wshadow
    -Werror
  )
  target_compile_options(w_seed_ephemeral_graph_tests PRIVATE
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
  target_compile_options(w_seed_check_driver PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wshadow
    -Werror
  )
  target_compile_options(w_seed_check_storage_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wshadow
    -Werror
  )
  target_compile_options(w_seed_check_retry_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wshadow
    -Werror
  )
  target_compile_options(w_seed_check_pipeline_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wshadow
    -Werror
  )
  target_compile_options(w_seed_check_host_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wshadow
    -Werror
  )
  target_compile_options(w PRIVATE
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
  target_compile_options(w_seed_constir_probe PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wshadow
    -Werror
  )
  target_compile_options(w_seed_constir_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wshadow
    -Werror
  )
  target_compile_options(w_seed_generic_validation_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wshadow
    -Werror
  )
  target_compile_options(w_seed_hlo0_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wshadow
    -Werror
  )
  target_compile_options(w_seed_hlo0_gate PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wshadow
    -Werror
  )
  target_compile_options(w_seed_hlo1_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wshadow
    -Werror
  )
  target_compile_options(w_seed_hlo1_gate PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wshadow
    -Werror
  )
  target_compile_options(w_seed_hir0_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wshadow
    -Werror
  )
  target_compile_options(w_seed_run0_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wshadow
    -Werror
  )
  target_compile_options(w_seed_run0_gate PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wshadow
    -Werror
  )
  target_compile_options(w_seed_cli_io_tests PRIVATE
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
add_test(NAME w_seed_module_scan_unit COMMAND w_seed_module_scan_tests)
add_test(NAME w_seed_ephemeral_graph_unit COMMAND w_seed_ephemeral_graph_tests)
add_test(NAME w_seed_ephemeral_provider_core_unit COMMAND w_seed_ephemeral_provider_tests)
add_test(NAME w_seed_ephemeral_provider_adapter_unit COMMAND w_seed_ephemeral_provider_adapter_tests)
add_test(NAME w_seed_ephemeral_provider_windows_unit COMMAND w_seed_ephemeral_provider_windows_tests)
add_test(NAME w_seed_ephemeral_driver_unit COMMAND w_seed_ephemeral_driver_tests)
add_test(NAME w_seed_ephemeral_driver_linux_unit COMMAND w_seed_ephemeral_driver_linux_tests)
add_test(NAME w_seed_formatter_unit COMMAND w_seed_formatter_tests)
add_test(NAME w_seed_diagnostic_unit COMMAND w_seed_diagnostic_tests)
add_test(NAME w_seed_foreign_unit COMMAND w_seed_foreign_tests)
add_test(NAME w_seed_frontend_unit COMMAND w_seed_frontend_tests)
add_test(NAME w_seed_hlo0_unit COMMAND w_seed_hlo0_tests)
add_test(NAME w_seed_hlo1_unit COMMAND w_seed_hlo1_tests)
add_test(NAME w_seed_hir0_unit COMMAND w_seed_hir0_tests)
add_test(NAME w_seed_run0_unit COMMAND w_seed_run0_tests)
add_test(NAME w_seed_cli_io_unit COMMAND w_seed_cli_io_tests)
add_test(NAME w_seed_check_storage_unit COMMAND w_seed_check_storage_tests)
add_test(NAME w_seed_check_retry_unit COMMAND w_seed_check_retry_tests)
add_test(NAME w_seed_check_pipeline_unit COMMAND w_seed_check_pipeline_tests)
add_test(NAME w_seed_check_host_unit COMMAND w_seed_check_host_tests)
add_test(NAME w_seed_constir_unit COMMAND w_seed_constir_tests)
add_test(NAME w_seed_generic_validation_unit COMMAND w_seed_generic_validation_tests)
