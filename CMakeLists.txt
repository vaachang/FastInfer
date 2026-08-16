cmake_minimum_required(VERSION 3.16)

project(MiniServe
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(minisrv
    src/main.cpp
    src/core/thread_pool.cpp
    src/runtime/batch.cpp
    src/runtime/batch_scheduler.cpp
    src/runtime/fake_backend.cpp
)

target_include_directories(minisrv
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
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
