# cmake_minimum_required(VERSION <specify CMake version here>)
cmake_minimum_required(VERSION 3.16)
project(verify_positions)

#Release Debug
set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -Wall")
add_compile_options(-std=c++17 -Wall -O2)

SET(ROOT_DIR "/home/work/sys/lib")
macro(SUBDIRLIST result curdir)
    file(GLOB children RELATIVE ${curdir} ${curdir}/*)
    set(dirlist "")
    foreach(child ${children})
        if(IS_DIRECTORY ${curdir}/${child})
            LIST(APPEND dirlist ${child})
        endif()
    endforeach()
    set(${result} ${dirlist})
endmacro()

SUBDIRLIST(all_sub_index ${ROOT_DIR})
foreach(child ${all_sub_index})
    list(APPEND all_include ${ROOT_DIR}/${child}/include)
    list(APPEND all_lib ${ROOT_DIR}/${child}/lib)
endforeach()

include_directories(${all_include})
link_directories(${all_lib})

aux_source_directory (./src SRC)

SET(BROKER "verify_positions")

add_executable(${BROKER} ${SRC})
target_link_libraries(${BROKER}
        gtest gtest_main feeder coral swordfish x stdc++fs yaml-cpp  clickhouse-cpp-lib-static boost_date_time boost_filesystem boost_regex boost_system  boost_chrono boost_log boost_program_options boost_thread boost_iostreams z protobuf protobuf-lite sodium zmq ssl crypto iconv pthread dl)

