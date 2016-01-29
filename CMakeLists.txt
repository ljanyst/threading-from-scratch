cmake_minimum_required(VERSION 2.4)
project (thread-bites)

enable_language(ASM)
include_directories(".")

set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -g")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")

add_library(
	tb SHARED
  tb-utils.c
  tb-threads.c
  tb-clone.S)

add_executable(test-00 test-00.c)
target_link_libraries(test-00 tb)

add_executable(test-01 test-01.c)
target_link_libraries(test-01 tb)
