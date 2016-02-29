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
  tb-tls.c
  tb-mutexes.c
  tb-cancel.c
  tb-clone.S
  tb-signal-trampoline.S)

macro(add_test name)
  add_executable(${name} ${name}.c)
  target_link_libraries(${name} tb)
endmacro()

foreach(i RANGE 8)
  if(i LESS 10)
    add_test(test-0${i})
  else()
    add_test(test-${i})
  endif()
endforeach()
