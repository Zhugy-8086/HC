cmake_minimum_required(VERSION 3.10)
project(SGN VERSION 2.0.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)

# Include directory
include_directories(${CMAKE_SOURCE_DIR}/sgn/include)

# Core C modules (always built)
set(SGN_CORE_SOURCES
    sgn/src/hc.c
    sgn/src/hc8.c
    sgn/src/hc16.c
    sgn/src/hc32.c
    sgn/src/hc64.c
    sgn/src/dc.c
    sgn/src/hc_simd.c
)

# Extension modules (C++ with C linkage)
set(SGN_EXT_SOURCES
    sgn/src/hpdc_core.cpp
    sgn/src/hpdc_sandbox.cpp
    sgn/src/hpdc_trie.cpp
    sgn/src/hpdc_engine.cpp
    sgn/src/hpdc_storage.cpp
    sgn/src/hpdc_network.cpp
    sgn/src/hpdc_plugin.cpp
    sgn/src/sgn_abi_dynamic.cpp
)

# Static library: core
add_library(sgn_core STATIC ${SGN_CORE_SOURCES})

# Static library: extensions
add_library(sgn_ext STATIC ${SGN_EXT_SOURCES})
target_link_libraries(sgn_ext PUBLIC sgn_core)

# Full library (all modules)
add_library(sgn STATIC ${SGN_CORE_SOURCES} ${SGN_EXT_SOURCES})

# Test executable
add_executable(test_sgn tests/test_sgn.c)
target_link_libraries(test_sgn sgn_core)

# Install
install(TARGETS sgn_core sgn_ext sgn ARCHIVE DESTINATION lib)
install(DIRECTORY sgn/include/ DESTINATION include)
