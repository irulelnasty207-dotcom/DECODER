cmake_minimum_required(VERSION 3.16)
project(yurlaps_decoder_native LANGUAGES CXX)

option(YURLAPS_WITH_RTLSDR "Build the RTL-SDR hardware backend" OFF)
option(YURLAPS_WITH_ZMQ "Build the ZeroMQ publisher" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(yurlaps_decoder_core
    src/protocol.cpp
    src/opn.cpp
    src/yurlaps.cpp
    src/iq.cpp
    src/passing.cpp
    src/amb.cpp
    src/rtl.cpp
    src/zmq.cpp
)
target_include_directories(yurlaps_decoder_core PUBLIC include)
target_compile_options(yurlaps_decoder_core PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

if(YURLAPS_WITH_RTLSDR)
    find_path(RTLSDR_INCLUDE_DIR rtl-sdr.h REQUIRED)
    find_library(RTLSDR_LIBRARY rtlsdr REQUIRED)
    target_compile_definitions(yurlaps_decoder_core PRIVATE YURLAPS_WITH_RTLSDR)
    target_include_directories(yurlaps_decoder_core PRIVATE ${RTLSDR_INCLUDE_DIR})
    target_link_libraries(yurlaps_decoder_core PRIVATE ${RTLSDR_LIBRARY})
endif()

if(YURLAPS_WITH_ZMQ)
    find_path(ZMQ_INCLUDE_DIR zmq.h REQUIRED)
    find_library(ZMQ_LIBRARY zmq REQUIRED)
    target_compile_definitions(yurlaps_decoder_core PRIVATE YURLAPS_WITH_ZMQ)
    target_include_directories(yurlaps_decoder_core PRIVATE ${ZMQ_INCLUDE_DIR})
    target_link_libraries(yurlaps_decoder_core PRIVATE ${ZMQ_LIBRARY})
endif()

add_executable(YurLapsDecoder src/main.cpp)
target_link_libraries(YurLapsDecoder PRIVATE yurlaps_decoder_core)

enable_testing()
add_executable(protocol_test tests/protocol_test.cpp)
target_link_libraries(protocol_test PRIVATE yurlaps_decoder_core)
add_test(NAME protocol_test COMMAND protocol_test)

add_executable(iq_test tests/iq_test.cpp)
target_link_libraries(iq_test PRIVATE yurlaps_decoder_core)
add_test(NAME iq_test COMMAND iq_test)

add_executable(amb_test tests/amb_test.cpp)
target_link_libraries(amb_test PRIVATE yurlaps_decoder_core)
add_test(NAME amb_test COMMAND amb_test)

add_executable(passing_test tests/passing_test.cpp)
target_link_libraries(passing_test PRIVATE yurlaps_decoder_core)
add_test(NAME passing_test COMMAND passing_test)
