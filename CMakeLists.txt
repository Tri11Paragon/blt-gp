cmake_minimum_required(VERSION 3.25)

macro(sanitizers target_name)
    if (${ENABLE_ADDRSAN} MATCHES ON)
        target_compile_options(${target_name} PRIVATE -fsanitize=address)
        target_link_options(${target_name} PRIVATE -fsanitize=address)
    endif ()

    if (${ENABLE_UBSAN} MATCHES ON)
        target_compile_options(${target_name} PRIVATE -fsanitize=undefined)
        target_link_options(${target_name} PRIVATE -fsanitize=undefined)
    endif ()

    if (${ENABLE_TSAN} MATCHES ON)
        target_compile_options(${target_name} PRIVATE -fsanitize=thread)
        target_link_options(${target_name} PRIVATE -fsanitize=thread)
    endif ()
endmacro()

macro(compile_options target_name)
    if (NOT ${MOLD} STREQUAL MOLD-NOTFOUND)
        target_compile_options(${target_name} PUBLIC -fuse-ld=mold)
    endif ()

    target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic -Wno-comment ${BLT_COMPILE_OPTIONS})
    target_link_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic -Wno-comment ${BLT_LINK_OPTIONS})
    sanitizers(${target_name})
endmacro()

project(blt-gp VERSION 0.5.40)

include(CTest)

option(ENABLE_ADDRSAN "Enable the address sanitizer" OFF)
option(ENABLE_UBSAN "Enable the ub sanitizer" OFF)
option(ENABLE_TSAN "Enable the thread data race sanitizer" OFF)
option(BUILD_EXAMPLES "Build example programs. This will build with CTest" OFF)
option(BUILD_GP_TESTS "Build test programs." OFF)
option(DEBUG_LEVEL "Enable debug features which prints extra information to the console, might slow processing down. [0, 3)" 0)
option(BLT_GP_DEBUG_CHECK_TREES "Enable checking of trees after every operation" OFF)
option(BLT_GP_DEBUG_TRACK_ALLOCATIONS "Track total allocations. Can be accessed with blt::gp::tracker" OFF)
option(ENABLE_COVERAGE "Enable code coverage instrumentation" OFF)

set(CMAKE_CXX_STANDARD 17)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

if (ENABLE_COVERAGE)
    message(STATUS "Enabling Coverage Compiler Commands")
    list(APPEND BLT_COMPILE_OPTIONS "--coverage")
    list(APPEND BLT_LINK_OPTIONS "--coverage")
endif ()

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    list(APPEND BLT_COMPILE_OPTIONS "-O3")
    list(APPEND BLT_COMPILE_OPTIONS "-g")
endif ()

if (NOT ${CMAKE_BUILD_TYPE})
    set(CMAKE_BUILD_TYPE Release)
endif ()

if (NOT TARGET BLT)
    add_subdirectory(lib/blt)
endif ()

include_directories(include/ include/blt/gp/)
file(GLOB_RECURSE PROJECT_BUILD_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_library(blt-gp ${PROJECT_BUILD_FILES})

compile_options(blt-gp)

find_program(MOLD "mold")

if (NOT ${MOLD} STREQUAL MOLD-NOTFOUND)
    target_compile_options(blt-gp PUBLIC -fuse-ld=mold)
endif ()

target_include_directories(blt-gp PUBLIC include/ include/blt/gp/)

target_link_libraries(blt-gp PRIVATE BLT Threads::Threads)
target_compile_definitions(blt-gp PRIVATE BLT_DEBUG_LEVEL=${DEBUG_LEVEL})

if (${TRACK_ALLOCATIONS})
    target_compile_definitions(blt-gp PRIVATE BLT_TRACK_ALLOCATIONS=1)
endif ()

macro(blt_add_project name source type )

    project(${name}-${type})

    add_executable(${name}-${type} ${source})

    target_link_libraries(${name}-${type} PRIVATE BLT blt-gp Threads::Threads)

    compile_options(${name}-${type})
    target_compile_definitions(${name}-${type} PRIVATE BLT_DEBUG_LEVEL=${DEBUG_LEVEL})

    if (${TRACK_ALLOCATIONS})
        target_compile_definitions(${name}-${type} PRIVATE BLT_TRACK_ALLOCATIONS=1)
    endif ()

    add_test(NAME ${name}-${type} COMMAND ${name}-${type} ${ARGN})

    set_property(TEST ${name}-${type} PROPERTY FAIL_REGULAR_EXPRESSION "FAIL;ERROR;FATAL;exception")

    project(blt-gp)
endmacro()

if (${BUILD_EXAMPLES})

endif ()

if (${BUILD_GP_TESTS})
    blt_add_project(gp-stack tests/stack.cpp test)
    blt_add_project(gp-tree-types tests/tree_types.cpp test)
    blt_add_project(gp-tree-execution tests/tree_execution.cpp test)
    blt_add_project(gp-tree-builders tests/tree_builders.cpp test)
    blt_add_project(gp-tree-mutation tests/tree_mutation.cpp test)
    blt_add_project(gp-tree tests/tree.cpp test)
    blt_add_project(gp-util-traits tests/util/traits.cpp test)
    blt_add_project(gp-function-indirection tests/function_indirection.cpp test 10)

endif ()