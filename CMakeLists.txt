cmake_minimum_required(VERSION 3.16)

project(MiniServe
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)

pkg_check_modules(ONNXRUNTIME REQUIRED libonnxruntime)

add_executable(minisrv
    src/main.cpp
    src/core/thread_pool.cpp
    src/runtime/batch.cpp
    src/runtime/batch_scheduler.cpp
    src/runtime/fake_backend.cpp
    src/runtime/onnxruntime_backend.cpp
)

target_include_directories(minisrv
    PRIVATE
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

target_include_directories(minisrv
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

add_executable(ort_test
    src/runtime/ort_test.cpp
)

target_include_directories(ort_test
    PRIVATE
    ${ONNXRUNTIME_INCLUDE_DIRS}
)

target_link_directories(ort_test
    PRIVATE
    ${ONNXRUNTIME_LIBRARY_DIRS}
)

target_link_libraries(ort_test
    PRIVATE
    ${ONNXRUNTIME_LIBRARIES}
)

add_executable(ort_infer_test
    src/runtime/ort_infer_test.cpp
)

target_include_directories(ort_infer_test
    PRIVATE
    ${ONNXRUNTIME_INCLUDE_DIRS}
)

target_link_directories(ort_infer_test
    PRIVATE
    ${ONNXRUNTIME_LIBRARY_DIRS}
)

target_link_libraries(ort_infer_test
    PRIVATE
    ${ONNXRUNTIME_LIBRARIES}
)

add_executable(benchmark
    tests/benchmark.cpp
    src/core/thread_pool.cpp
    src/runtime/batch.cpp
    src/runtime/batch_scheduler.cpp
    src/runtime/onnxruntime_backend.cpp
)

target_include_directories(benchmark
    PRIVATE
    ${PROJECT_SOURCE_DIR}/include
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

if (MSVC)
    target_compile_options(minisrv PRIVATE /W4)
else()
    target_compile_options(minisrv PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()
