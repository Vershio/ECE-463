cmake_minimum_required(VERSION 2.8)
project(563s21_p2_cpp)

set(CMAKE_CXX_STANDARD 11)

set(
        sim_ooo_src sim_ooo.cc
)
set(
        sim_ooo_hdr sim_ooo.h
)

add_library(
        sim_ooo STATIC
        ${sim_ooo_src}
        ${sim_ooo_hdr}
)
target_include_directories(sim_ooo PUBLIC .)

add_subdirectory(testcases)
