cmake_minimum_required(VERSION 3.25)
project(blt-gp VERSION 0.1.27)

include(CTest)

option(ENABLE_ADDRSAN "Enable the address sanitizer" OFF)
option(ENABLE_UBSAN "Enable the ub sanitizer" OFF)
option(ENABLE_TSAN "Enable the thread data race sanitizer" OFF)
option(BUILD_EXAMPLES "Build example programs. This will build with CTest" OFF)
option(BUILD_GP_TESTS "Build test programs." OFF)
option(DEBUG_LEVEL "Enable debug features which prints extra information to the console, might slow processing down. [0, 3)" 0)
option(TRACK_ALLOCATIONS "Track total allocations. Can be accessed with blt::gp::tracker" OFF)

set(CMAKE_CXX_STANDARD 17)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

if (NOT TARGET BLT)
    add_subdirectory(lib/blt)
endif ()

include_directories(include/)
file(GLOB_RECURSE PROJECT_BUILD_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_library(blt-gp ${PROJECT_BUILD_FILES})

target_compile_options(blt-gp PRIVATE -Wall -Wextra -Wpedantic -Wno-comment)
target_link_options(blt-gp PRIVATE -Wall -Wextra -Wpedantic -Wno-comment)

find_program(MOLD "mold")

if (NOT ${MOLD} STREQUAL MOLD-NOTFOUND)
    target_compile_options(blt-gp PUBLIC -fuse-ld=mold)
endif ()

target_include_directories(blt-gp PUBLIC include/)

target_link_libraries(blt-gp PRIVATE BLT Threads::Threads)
target_compile_definitions(blt-gp PRIVATE BLT_DEBUG_LEVEL=${DEBUG_LEVEL})

if (${TRACK_ALLOCATIONS})
    target_compile_definitions(blt-gp PRIVATE BLT_TRACK_ALLOCATIONS=1)
endif ()

if (${ENABLE_ADDRSAN} MATCHES ON)
    target_compile_options(blt-gp PRIVATE -fsanitize=address)
    target_link_options(blt-gp PRIVATE -fsanitize=address)
endif ()

if (${ENABLE_UBSAN} MATCHES ON)
    target_compile_options(blt-gp PRIVATE -fsanitize=undefined)
    target_link_options(blt-gp PRIVATE -fsanitize=undefined)
endif ()

if (${ENABLE_TSAN} MATCHES ON)
    target_compile_options(blt-gp PRIVATE -fsanitize=thread)
    target_link_options(blt-gp PRIVATE -fsanitize=thread)
endif ()

macro(blt_add_project name source type)

    project(${name}-${type})

    #add_compile_options(-fuse-ld=mold)

    add_executable(${name}-${type} ${source})

    target_link_libraries(${name}-${type} PRIVATE BLT blt-gp Threads::Threads)

    target_compile_options(${name}-${type} PRIVATE -Wall -Wextra -Wpedantic -Wno-comment)
    target_link_options(${name}-${type} PRIVATE -Wall -Wextra -Wpedantic -Wno-comment)
    target_compile_definitions(${name}-${type} PRIVATE BLT_DEBUG_LEVEL=${DEBUG_LEVEL})

    if (${TRACK_ALLOCATIONS})
        target_compile_definitions(${name}-${type} PRIVATE BLT_TRACK_ALLOCATIONS=1)
    endif ()

    if (${ENABLE_ADDRSAN} MATCHES ON)
        target_compile_options(${name}-${type} PRIVATE -fsanitize=address)
        target_link_options(${name}-${type} PRIVATE -fsanitize=address)
    endif ()

    if (${ENABLE_UBSAN} MATCHES ON)
        target_compile_options(${name}-${type} PRIVATE -fsanitize=undefined)
        target_link_options(${name}-${type} PRIVATE -fsanitize=undefined)
    endif ()

    if (${ENABLE_TSAN} MATCHES ON)
        target_compile_options(${name}-${type} PRIVATE -fsanitize=thread)
        target_link_options(${name}-${type} PRIVATE -fsanitize=thread)
    endif ()

    add_test(NAME ${name} COMMAND ${name}-${type})

    #    set (passRegex "Pass" "Passed" "PASS" "PASSED")
    #    set (failRegex "WARN" "FAIL" "ERROR" "FATAL")
    set(failRegex "\\[WARN\\]" "FAIL" "ERROR" "FATAL" "exception")

    #    set_property (TEST ${name} PROPERTY PASS_REGULAR_EXPRESSION "${passRegex}")
    set_property(TEST ${name} PROPERTY FAIL_REGULAR_EXPRESSION "${failRegex}")

    project(blt-gp)
endmacro()

if (${BUILD_EXAMPLES})

    blt_add_project(blt-symbolic-regression examples/symbolic_regression.cpp example)
    blt_add_project(blt-rice-classification examples/rice_classification.cpp example)

endif ()

if (${BUILD_GP_TESTS})

    blt_add_project(blt-stack tests/stack_tests.cpp test)
    blt_add_project(blt-eval tests/evaluation_tests.cpp test)
    blt_add_project(blt-order tests/order_tests.cpp test)
    #blt_add_project(blt-destructor tests/destructor_test.cpp test)
    blt_add_project(blt-gp1 tests/gp_test_1.cpp test)
    blt_add_project(blt-gp2 tests/gp_test_2.cpp test)
    blt_add_project(blt-gp3 tests/gp_test_3.cpp test)
    blt_add_project(blt-gp4 tests/gp_test_4.cpp test)
    blt_add_project(blt-gp5 tests/gp_test_5.cpp test)
    blt_add_project(blt-gp6 tests/gp_test_6.cpp test)
    blt_add_project(blt-gp7 tests/gp_test_7.cpp test)

endif ()