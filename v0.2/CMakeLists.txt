cmake_minimum_required(VERSION 3.20)
project(sgn_c_engine LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 默认使用 Release 模式
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# AVX2 + FMA + AVX-VNNI 支持
add_compile_options(-mavx2 -mavxvnni -mfma -O3)

# OpenMP 支持（col2im 使用 #pragma omp parallel for）
find_package(OpenMP REQUIRED)

find_package(pybind11 CONFIG REQUIRED)

# ============================================================
# HC 扩展模块（sgn_c_engine）
# ============================================================
pybind11_add_module(sgn_c_engine
    # C 源文件
    src/hc8_net.c
    src/hc16_net.c
    src/hc16ms.c
    src/hc4_pshufb.c
    src/col2im_c.c
    # pybind11 绑定
    src/pysgn_net.cpp
    src/pysgn_hc16.cpp
    src/pysgn_hc16ms.cpp
    src/pysgn_hc4_pshufb.cpp
    src/pysgn_col2im.cpp
    # C++ 封装
    src/col2im.cpp
    src/col2im_bindings.cpp
    src/hc8_coproduct_bindings.cpp
)

target_include_directories(sgn_c_engine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# C 文件强制 C 编译器编译
set_source_files_properties(
    src/hc8_net.c
    src/hc16_net.c
    src/hc16ms.c
    src/hc4_pshufb.c
    src/col2im_c.c
    PROPERTIES LANGUAGE C
)

# SGN_PC_EXTENSION 宏（C 文件依赖此宏）
target_compile_definitions(sgn_c_engine PRIVATE SGN_PC_EXTENSION=1)

# OpenMP 链接
target_link_libraries(sgn_c_engine PRIVATE OpenMP::OpenMP_CXX)

# 输出到 build 目录
set_target_properties(sgn_c_engine PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)