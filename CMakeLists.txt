cmake_minimum_required(VERSION 3.15)
project(s_task_test C ASM)

set(CMAKE_C_STANDARD 11)

aux_source_directory(. DIR_SRCS)

add_subdirectory(s_task)

add_executable(s_task_test ${DIR_SRCS})

target_link_libraries(s_task_test s_task_lib)