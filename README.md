Threading From Scratch
======================

This project is my attempt to to implement *pthreads*-like threading library on
x86_64 Linux for purely educational purposes. Some parts of it are based on
ideas found in *glibc*.

**Your feedback is very welcome. Drop me a line at lukasz@jany.st if you have
any comments.** It would be cool if someone could review the synchronisation
routines. Some cancelation points are missing, but it's deliberate. Things are
not particularly efficient because I favored clarity and conciseness over
performance. For the same reason, much of error checking has been omitted as
well.

#### Utilities ####

 * [syscalls][1] - invoking syscalls from C
 * [utils][2] - malloc, mmap, brk, print, sleep and friends
 * [linked list][3], [ext1][4], [ext2][5] - a simple doubly-linked list
 * [strerror][6] - a completely inefficient implementation of strerror
 * [sigaction][7] - install signal handlers

#### Spawning threads ####

 * [clone][8] - call the clone syscall in assembly
 * [threads][9] - create and run threads
 * [comments][10] - a blog post with some of my comments

#### Thread-local storage ####

 * [self][11] - get a pointer to the thread structure of the current thread
 * [tls][12] - create and destroy keys, set and get thread-specific pointers
 * [comments][13] - a blog post with some of my comments

#### Mutexes ####

 * [mutex][14] - normal, error-check and recursive mutexes
 * [comments][15] - a blog post with some of my comments

#### Joining and initialization ####

 * [descriptor cache][16] - cache and re-use thread descriptors
 * [main thread][17] - set our descriptor for the main thread
 * [join & co][18] - join, exit and detach
 * [equal][19] - pthread_equal
 * [once][20] - once initialization
 * [comments][21] - a blog post with some of my comments

#### Cancelation ####

 * [cancellation][22] - deferred and async cancellation
 * [clean-up][23] - clean-up handlers
 * [once cancellation][24] - restart the initialization of a thread gets canceled
 * [comments][25] - a blog post with some of my comments

#### Scheduling ####

 * [scheduling][26] - set thread scheduler
 * [priority mutexes][27] - PRIO_INHERIT and PRIO_PROTECT mutexes
 * [comments][28] - a blog post with some of my comments

#### RW Locks ####

 * [rwlock][29] - implement a read-write lock
 * [comments][30] - a blog post with some of my comments

#### Condition variables ####

 * [condvar][31] - implement a condition variable
 * [comments][32] - a blog post with some of my comments

#### Conclusion ####

 * [comments][33] - some final notes

[1]:  https://github.com/ljanyst/threading-from-scratch/blob/b1ef686bc0f5bded4e527d5d7d0a912d59b88638/tb.h#L70
[2]:  https://github.com/ljanyst/threading-from-scratch/blob/b1ef686bc0f5bded4e527d5d7d0a912d59b88638/tb-utils.c
[3]:  https://github.com/ljanyst/threading-from-scratch/commit/b65d2a9d9457c5f23b1f4d5991491df009e98ae7
[4]:  https://github.com/ljanyst/threading-from-scratch/commit/d69a51c8a994a6bc3709295040a56b4760bb659b
[5]:  https://github.com/ljanyst/threading-from-scratch/commit/19b4a9e1d9482f5d2d54effa3d9b40f6769a54e4
[6]:  https://github.com/ljanyst/threading-from-scratch/commit/f1d9529cda54150f4d4b7f87a721ffd84a35af02
[7]:  https://github.com/ljanyst/threading-from-scratch/commit/d259b89910907e04d9578228f4f8f49937fe9c1d
[8]:  https://github.com/ljanyst/threading-from-scratch/blob/b1ef686bc0f5bded4e527d5d7d0a912d59b88638/tb-clone.S
[9]:  https://github.com/ljanyst/threading-from-scratch/blob/b1ef686bc0f5bded4e527d5d7d0a912d59b88638/tb-threads.c
[10]: https://jany.st/post/2016-01-30-threading-from-scratch-1-syscalls-memory-and-your-first-therad.html
[11]: https://github.com/ljanyst/threading-from-scratch/commit/eca7aa2443ce0ab36ac1bd0e0874e6a445cb6b67
[12]: https://github.com/ljanyst/threading-from-scratch/commit/7cfbe2668af68f71e20e3308e899be8a2c69c812
[13]: https://jany.st/post/2016-02-03-threading-from-scratch-2-the-pointer-to-self-and-thread-local-storage.html
[14]: https://github.com/ljanyst/threading-from-scratch/commit/9f36c5a462f46177a002e1a2d6b0188b322708b8
[15]: https://jany.st/post/2016-02-15-threading-from-scratch-3-futexes-mutexes-and-memory-sychronization.html
[16]: https://github.com/ljanyst/threading-from-scratch/commit/fc5d869cc04bfbe750f8bb8f0271bde96018bc23
[17]: https://github.com/ljanyst/threading-from-scratch/commit/a35ef4193985be6abb52d3a368aa4bf414bdcc02
[18]: https://github.com/ljanyst/threading-from-scratch/commit/a693f8491499a286417aeafffda904a65c58ef66
[19]: https://github.com/ljanyst/threading-from-scratch/commit/9791b90bad2369b2031befa2fce729234307c964
[20]: https://github.com/ljanyst/threading-from-scratch/commit/8a8c6cce8375e8a44f950f10cbae0445b244fb2d
[21]: https://jany.st/post/2016-02-24-threading-from-scratch-4-joining-threads-and-dynamic-initialization.html
[22]: https://github.com/ljanyst/threading-from-scratch/commit/acf3a7af79ea56cf820ce928240982b95441bac7
[23]: https://github.com/ljanyst/threading-from-scratch/commit/00efcdaa18844c55a73cd393998937417b0b0992
[24]: https://github.com/ljanyst/threading-from-scratch/commit/e18bc979014815b15c42d9b68e803908a0c75fbb
[25]: https://jany.st/post/2016-03-01-threading-from-scratch-5-cancelation.html
[26]: https://github.com/ljanyst/threading-from-scratch/commit/ee80953ad650ae0bf954f008a9a074cb16f22691
[27]: https://github.com/ljanyst/threading-from-scratch/commit/cc90ae476b0588177b05532fc03cd795a5075f08
[28]: https://jany.st/post/2016-03-05-threading-from-scratch-6-scheduling-and-task-priority.html
[29]: https://github.com/ljanyst/threading-from-scratch/commit/e489e263ad64aa3655c87fccbbd2a06f31b2a190
[30]: https://jany.st/post/2016-03-09-threading-from-scratch-7-rw-locks.html
[31]: https://github.com/ljanyst/threading-from-scratch/commit/872ca2219078b8936db96d94ba4a3903418e02f0
[32]: https://jany.st/post/2016-03-18-threading-from-scratch-8-condition-variables.html
[33]: https://jany.st/post/2016-03-19-threading-from-scratch-9-final-thoughts.html
