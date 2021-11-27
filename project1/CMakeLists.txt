cmake_minimum_required(VERSION 2.8)
project(563s21_p1_cpp)

set(CMAKE_CXX_STANDARD 11)

set(
        sim_pipe_src sim_pipe.cc
)
set(
        sim_pipe_hdr sim_pipe.h
)
set(
        sim_pipe_fp_src sim_pipe_fp.cc
)
set(
        sim_pipe_fp_hdr sim_pipe_fp.h
)

add_library(
        sim_pipe STATIC
        ${sim_pipe_src}
        ${sim_pipe_hdr}
)
target_include_directories(sim_pipe PUBLIC .)

add_library(
        sim_pipe_fp STATIC
        ${sim_pipe_fp_src}
        ${sim_pipe_fp_hdr}
)
target_include_directories(sim_pipe_fp PUBLIC .)

add_subdirectory(testcases)

