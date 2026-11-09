cmake_minimum_required(VERSION 3.16)
project(yargps3_core CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(YARG_SANITIZE "Build host tests with address/undefined sanitizers" ON)

add_library(yarg_core STATIC
    src/midi.cpp
    src/tempo.cpp
    src/guitar_chart.cpp
    src/song_ini.cpp
    src/smf_writer.cpp
    src/selftest.cpp)
target_include_directories(yarg_core PUBLIC include)
target_compile_options(yarg_core PRIVATE -Wall -Wextra -Werror)

if(YARG_SANITIZE AND NOT MSVC)
    target_compile_options(yarg_core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(yarg_core PUBLIC -fsanitize=address,undefined)
endif()

add_executable(yarg_core_tests tests/test_main.cpp)
target_link_libraries(yarg_core_tests PRIVATE yarg_core)

add_executable(chartdump tools/chartdump.cpp)
target_link_libraries(chartdump PRIVATE yarg_core)

add_executable(gen_corpus tools/gen_corpus.cpp)
target_link_libraries(gen_corpus PRIVATE yarg_core)

enable_testing()
add_test(NAME core_tests COMMAND yarg_core_tests)
