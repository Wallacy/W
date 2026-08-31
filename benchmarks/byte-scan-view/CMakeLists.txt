cmake_minimum_required(VERSION 3.21)
project(bmd3_byte_scan_view C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(byte_scan_view byte_scan_view.c)
