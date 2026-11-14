# orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Every source's thin wrapper built by nvcc, one object library a wrapper named wrap_<source>, with the include path and settings
# src/build_engine.sh uses for the engine's sources. harness.py cu build configures and builds this.

cmake_minimum_required(VERSION 3.18)

get_filename_component(ORIOR_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.." ABSOLUTE)
set(ORIOR_CU "${ORIOR_ROOT}/src/cu")
# the second tree of sources, the includes at the top of the tree, and the folder its wrappers and suites stand in
set(ORIOR_INCLUDES "${ORIOR_ROOT}/includes")
set(ORIOR_INCLUDES_UNIT "${ORIOR_ROOT}/utils/test/includes/unit")

# nvcc's host compiler on Windows is MSVC's cl.exe, which harness.py names, and nvcc finds its headers and libraries from it
if(ORIOR_CU_HOST)
    set(CMAKE_CUDA_HOST_COMPILER "${ORIOR_CU_HOST}")
    set(CMAKE_C_COMPILER "${ORIOR_CU_HOST}")
    set(CMAKE_CXX_COMPILER "${ORIOR_CU_HOST}")
endif()
# nvcc from the toolkit's standard folder on Linux, where its bin folder is not on the PATH
if(NOT CMAKE_CUDA_COMPILER AND NOT WIN32)
    file(GLOB ORIOR_CUDA_CANDIDATES "/usr/local/cuda*/bin/nvcc")
    if(ORIOR_CUDA_CANDIDATES)
        list(GET ORIOR_CUDA_CANDIDATES 0 CMAKE_CUDA_COMPILER)
    endif()
endif()
project(orior_cu_unit LANGUAGES C CXX CUDA)

# C++17, as the sources build, and on Windows C++20: nvcc's MSVC front end accepts the designated initializers of MMgr's headers from C++20,
# where GCC accepts them as an extension
if(WIN32)
    set(CMAKE_CUDA_STANDARD 20)
else()
    set(CMAKE_CUDA_STANDARD 17)
endif()
set(CMAKE_CUDA_STANDARD_REQUIRED ON)
if(NOT DEFINED CMAKE_CUDA_ARCHITECTURES)
    set(CMAKE_CUDA_ARCHITECTURES native)
endif()

# Every folder under src/cu and includes with a header, but MMgr's and embedded_types', which have a library's own layout and are
# reached from their include and src folders; the top of the tree, which a wrapper names its source from
file(GLOB_RECURSE ORIOR_CU_HEADERS "${ORIOR_CU}/*.h" "${ORIOR_CU}/*.cuh" "${ORIOR_CU}/*.hpp"
     "${ORIOR_INCLUDES}/*.h" "${ORIOR_INCLUDES}/*.cuh" "${ORIOR_INCLUDES}/*.hpp")
set(ORIOR_CU_INCLUDES "${ORIOR_ROOT}" "${ORIOR_CU}" "${ORIOR_INCLUDES}")
foreach(header ${ORIOR_CU_HEADERS})
    if(NOT header MATCHES "/includes/(mmgr|embedded_types)/")
        get_filename_component(folder "${header}" DIRECTORY)
        list(APPEND ORIOR_CU_INCLUDES "${folder}")
    endif()
endforeach()
list(REMOVE_DUPLICATES ORIOR_CU_INCLUDES)
list(APPEND ORIOR_CU_INCLUDES "${ORIOR_INCLUDES}/mmgr/include" "${ORIOR_INCLUDES}/mmgr/src" "${ORIOR_INCLUDES}/embedded_types/include")

set(ORIOR_CU_DEFINES BODY_OVERLAP_BUILD_DLL=1 HEAVIEST_MATCHING_BUILD_DLL=1 SHIFT_AGREEMENT_BUILD_DLL=1
    MMGR_ENABLE_DMA=0 MMGR_ENABLE_EXTRAM=0 MMGR_ALIGN_BYTES=32 MMGR_MOVABLE_READ_MS=10u MMGR_MOVABLE_QUIET_READINGS=2u)
set(ORIOR_CU_OPTIONS -fmad=false)
# the CUDA toolkit's headers refuse MSVC's traditional preprocessor
if(MSVC)
    list(APPEND ORIOR_CU_OPTIONS "SHELL:-Xcompiler /Zc:preprocessor")
endif()

file(GLOB_RECURSE ORIOR_CU_WRAPPERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*_wrap.cu" "${ORIOR_INCLUDES_UNIT}/*_wrap.cu")
foreach(wrapper ${ORIOR_CU_WRAPPERS})
    get_filename_component(name "${wrapper}" NAME)
    string(REGEX REPLACE "_wrap\\.cu$" "" name "${name}")
    add_library(wrap_${name} OBJECT "${wrapper}")
    target_include_directories(wrap_${name} PRIVATE ${ORIOR_CU_INCLUDES})
    target_compile_definitions(wrap_${name} PRIVATE ${ORIOR_CU_DEFINES})
    target_compile_options(wrap_${name} PRIVATE ${ORIOR_CU_OPTIONS})
endforeach()

# Unity from the clone harness.py makes under build/unity, built as C with C11's noreturn; a suite compiled as C++ uses the noreturn
# form Unity's header chooses for C++
set(ORIOR_UNITY "${ORIOR_ROOT}/build/unity/src")
add_library(cu_unity STATIC "${ORIOR_UNITY}/unity.c")
target_include_directories(cu_unity PUBLIC "${ORIOR_UNITY}")
target_compile_definitions(cu_unity PUBLIC UNITY_INCLUDE_DOUBLE)
target_compile_definitions(cu_unity PRIVATE UNITY_NORETURN=_Noreturn)
set_property(TARGET cu_unity PROPERTY C_STANDARD 11)

# A suite is a folder test_<source> holding test_<source>.cu, which includes the source's wrapper, and the generated unity_runner.cu
file(GLOB_RECURSE ORIOR_CU_SUITES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/test_*.cu" "${ORIOR_INCLUDES_UNIT}/test_*.cu")
foreach(suite ${ORIOR_CU_SUITES})
    get_filename_component(name "${suite}" NAME_WE)
    get_filename_component(folder "${suite}" DIRECTORY)
    add_executable(${name} "${suite}" "${folder}/unity_runner.cu")
    target_include_directories(${name} PRIVATE ${ORIOR_CU_INCLUDES} "${CMAKE_CURRENT_SOURCE_DIR}" "${ORIOR_INCLUDES_UNIT}")
    target_compile_definitions(${name} PRIVATE ${ORIOR_CU_DEFINES})
    target_compile_options(${name} PRIVATE ${ORIOR_CU_OPTIONS})
    target_link_libraries(${name} PRIVATE cu_unity)
    set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/suites")
endforeach()
