cmake_minimum_required(VERSION 3.16)

project(MiniServe
    VERSION 0.1.0
    LANGUAGES CXX
)

# ============================================================
# C++ Configuration
# ============================================================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


# ============================================================
# Dependencies
# ============================================================

find_package(PkgConfig REQUIRED)

pkg_check_modules(ONNXRUNTIME REQUIRED libonnxruntime)


# ============================================================
# Common Configuration
# ============================================================

set(MINISERVE_INCLUDE_DIR
    ${PROJECT_SOURCE_DIR}/include
)

set(MINISERVE_SOURCES
    src/runtime/batch.cpp
    src/runtime/batch_scheduler.cpp
    src/runtime/onnxruntime_backend.cpp
)


# ============================================================
# MiniServe Server
# ============================================================

add_executable(minisrv
    src/main.cpp
    ${MINISERVE_SOURCES}
    src/server/http_server.cpp
)

target_include_directories(minisrv
    PRIVATE
        ${MINISERVE_INCLUDE_DIR}
        ${ONNXRUNTIME_INCLUDE_DIRS}
)

target_link_directories(minisrv
    PRIVATE
        ${ONNXRUNTIME_LIBRARY_DIRS}
)

target_link_libraries(minisrv
    PRIVATE
        ${ONNXRUNTIME_LIBRARIES}
)


# ============================================================
# Benchmark
# ============================================================

add_executable(benchmark
    tests/benchmark.cpp
    ${MINISERVE_SOURCES}
)

target_include_directories(benchmark
    PRIVATE
        ${MINISERVE_INCLUDE_DIR}
        ${ONNXRUNTIME_INCLUDE_DIRS}
)

target_link_directories(benchmark
    PRIVATE
        ${ONNXRUNTIME_LIBRARY_DIRS}
)

target_link_libraries(benchmark
    PRIVATE
        ${ONNXRUNTIME_LIBRARIES}
)


# ============================================================
# HTTP Benchmark
# ============================================================

add_executable(http_benchmark
    tests/http_benchmark.cpp
)

target_include_directories(http_benchmark
    PRIVATE
        ${MINISERVE_INCLUDE_DIR}
)


# ============================================================
# Compiler Warnings
# ============================================================

if (MSVC)

    target_compile_options(minisrv PRIVATE /W4)
    target_compile_options(benchmark PRIVATE /W4)
    target_compile_options(http_benchmark PRIVATE /W4)

else()

    target_compile_options(minisrv PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

    target_compile_options(benchmark PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

    target_compile_options(http_benchmark PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

endif()
