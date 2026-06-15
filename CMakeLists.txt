# Copyright 2026 The Espresso Authors
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.25)

project(espresso VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PEABERRY_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../peaberry" CACHE PATH
    "Path to the peaberry library source tree")

if(NOT EXISTS "${PEABERRY_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "peaberry not found at ${PEABERRY_SOURCE_DIR}. "
        "Clone peaberry as a sibling directory or set -DPEABERRY_SOURCE_DIR=...")
endif()

set(PEABERRY_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(PEABERRY_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(PEABERRY_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)

add_subdirectory(${PEABERRY_SOURCE_DIR} ${CMAKE_BINARY_DIR}/peaberry)

set(PEABERRY_SHADER_DIR "${CMAKE_BINARY_DIR}/peaberry/shaders" CACHE PATH
    "Compiled SPIR-V output directory from the peaberry build")

add_subdirectory(examples)
add_subdirectory(benchmarks)
