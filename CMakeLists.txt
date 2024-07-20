cmake_minimum_required(VERSION 3.25)
project(blt-gp VERSION 0.0.90)

include(CTest)

option(ENABLE_ADDRSAN "Enable the address sanitizer" OFF)
option(ENABLE_UBSAN "Enable the ub sanitizer" OFF)
option(ENABLE_TSAN "Enable the thread data race sanitizer" OFF)
option(BUILD_EXAMPLES "Build example programs. This is a single executable" OFF)
option(DEBUG_LEVEL "Enable debug features which prints extra information to the console, might slow processing down. [0, 3)" 0)

set(CMAKE_CXX_STANDARD 17)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

if (NOT TARGET BLT)
    add_subdirectory(lib/blt)
endif()

include_directories(include/)
file(GLOB_RECURSE PROJECT_BUILD_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_library(blt-gp ${PROJECT_BUILD_FILES})

target_compile_options(blt-gp PRIVATE -Wall -Wextra -Werror -Wpedantic -Wno-comment)
target_link_options(blt-gp PRIVATE -Wall -Wextra -Werror -Wpedantic -Wno-comment)

target_include_directories(blt-gp PUBLIC include/)

target_link_libraries(blt-gp PRIVATE BLT Threads::Threads)
target_compile_definitions(blt-gp PRIVATE BLT_DEBUG_LEVEL=${DEBUG_LEVEL})

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

if (${BUILD_EXAMPLES})
    macro(blt_add_example name source)

        project(${name}-example)

        add_executable(${name}-example ${source})

        target_link_libraries(${name}-example PRIVATE BLT blt-gp Threads::Threads)

        target_compile_options(${name}-example PRIVATE -Wall -Wextra -Wpedantic -Wno-comment)
        target_link_options(${name}-example PRIVATE -Wall -Wextra -Wpedantic -Wno-comment)
        target_compile_definitions(${name}-example PRIVATE BLT_DEBUG_LEVEL=${DEBUG_LEVEL})

        if (${ENABLE_ADDRSAN} MATCHES ON)
            target_compile_options(${name}-example PRIVATE -fsanitize=address)
            target_link_options(${name}-example PRIVATE -fsanitize=address)
        endif ()

        if (${ENABLE_UBSAN} MATCHES ON)
            target_compile_options(${name}-example PRIVATE -fsanitize=undefined)
            target_link_options(${name}-example PRIVATE -fsanitize=undefined)
        endif ()

        if (${ENABLE_TSAN} MATCHES ON)
            target_compile_options(${name}-example PRIVATE -fsanitize=thread)
            target_link_options(${name}-example PRIVATE -fsanitize=thread)
        endif ()

        add_test(NAME ${name} COMMAND ${name}-example)

        project(blt-gp)
    endmacro()

    blt_add_example(blt-gp1 examples/gp_test_1.cpp)
    blt_add_example(blt-gp2 examples/gp_test_2.cpp)
    blt_add_example(blt-gp3 examples/gp_test_3.cpp)
    blt_add_example(blt-gp4 examples/gp_test_4.cpp)
    blt_add_example(blt-gp5 examples/gp_test_5.cpp)
    blt_add_example(blt-gp6 examples/gp_test_6.cpp)
    blt_add_example(blt-gp7 examples/gp_test_7.cpp)
    blt_add_example(blt-SR-playground examples/pg_symbolic_regression.cpp)

endif ()