Thread Bites
============

Thread Bites is a simple and, admittedly, a pretty useless threading library.
Its raison d'être is to be as simple as possible in order to illustrate how
one can implement a threading library that somewhat resembles pthreads and plays
well with x86_64 Linux. Thread Bites does not, and likely will not, support the
compiler generated thread-local storage, `__thread int number;`, and other
linker trickery, such as
`__attribute__ ((section ("__libc_thread_freeres_fn")))`. This fact, in
conjunction with glibc expecting a bunch of globals to be in TLS, makes calling
anything non-trivial from glibc a rather risky business.

The goal of this project is to, over time, implement all/most of pthreads
functionality.

See http://jany.st/post/2016-01-30-thread-bites-1.html for more details.