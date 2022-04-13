cmake_minimum_required(VERSION 3.23)
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
  tb-sched.c
  tb-rwlock.c
  tb-condvar.c
  tb-clone.S
  tb-signal-trampoline.S)

macro(add_test name)
  add_executable(${name} ${name}.c)
  target_link_libraries(${name} tb)
endmacro()

add_test(test-00-memory-management)
add_test(test-01-spawning-threads)
add_test(test-02-thread-local-storage)
add_test(test-03-mutex)
add_test(test-04-joining)
add_test(test-05-dynamic-init)
add_test(test-06-signals)
add_test(test-07-cancel)
add_test(test-08-cancel-once)
add_test(test-09-scheduling)
add_test(test-10-priority-mutex)
add_test(test-11-rw-lock)
add_test(test-12-condition-variable)
