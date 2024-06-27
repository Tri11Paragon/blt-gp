cmake_minimum_required(VERSION 3.25)
project(blt-gp VERSION 0.0.35)

include(CTest)

option(ENABLE_ADDRSAN "Enable the address sanitizer" OFF)
option(ENABLE_UBSAN "Enable the ub sanitizer" OFF)
option(ENABLE_TSAN "Enable the thread data race sanitizer" OFF)
option(BUILD_EXAMPLES "Build example programs. This is a single executable" OFF)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(lib/blt)

include_directories(include/)
file(GLOB_RECURSE PROJECT_BUILD_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_library(blt-gp ${PROJECT_BUILD_FILES})

target_compile_options(blt-gp PRIVATE -Wall -Wextra -Werror -Wpedantic -Wno-comment)
target_link_options(blt-gp PRIVATE -Wall -Wextra -Werror -Wpedantic -Wno-comment)

target_link_libraries(blt-gp PRIVATE BLT)

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

        target_link_libraries(${name}-example PRIVATE BLT blt-gp)

        target_compile_options(${name}-example PRIVATE -Wall -Wextra -Wpedantic -Wno-comment)
        target_link_options(${name}-example PRIVATE -Wall -Wextra -Wpedantic -Wno-comment)

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

        add_test(NAME ${name}-test COMMAND ${name}-example)

        project(blt-gp)
    endmacro()

    blt_add_example(blt-gp1 examples/main.cpp)
    blt_add_example(blt-gp2 examples/gp_test_2.cpp)

endif ()