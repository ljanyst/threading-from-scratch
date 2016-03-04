//------------------------------------------------------------------------------
// Copyright (c) 2016 by Lukasz Janyst <lukasz@jany.st>
//------------------------------------------------------------------------------
// This file is part of thread-bites.
//
// thread-bites is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// thread-bites is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with thread-bites.  If not, see <http://www.gnu.org/licenses/>.
//------------------------------------------------------------------------------

#include "tb.h"
#include "tb-private.h"

#include <linux/time.h>
#include <asm-generic/param.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>

//------------------------------------------------------------------------------
// Print unsigned int to a string
//------------------------------------------------------------------------------
static void printNum(uint64_t num, int base)
{
  if(base <= 0 || base > 16)
    return;
  if(num == 0) {
    tbwrite(1, "0", 1);
    return;
  }
  uint64_t n = num;
  char str[32]; str[31] = 0;
  char *cursor = str+30;
  char digits[] = "0123456789abcdef";
  while(n && cursor != str) {
    int rem = n % base;
    *cursor = digits[rem];
    n /= base;
    --cursor;
  }
  ++cursor;
  tbwrite(1, cursor, 31-(cursor-str));
}

//------------------------------------------------------------------------------
// Print signed int to a string
//------------------------------------------------------------------------------
static void printNumS(int64_t num)
{
  if(num == 0) {
    tbwrite(1, "0", 1);
    return;
  }
  uint64_t n = num;
  char str[32]; str[31] = 0;
  char *cursor = str+30;
  char digits[] = "0123456789";
  while(n && cursor != str) {
    int rem = n % 10;
    *cursor = digits[rem];
    n /= 10;
    --cursor;
  }
  ++cursor;
  tbwrite(1, cursor, 31-(cursor-str));
}

//------------------------------------------------------------------------------
// Print something to stdout
//------------------------------------------------------------------------------
static int print_lock;
void tbprint(const char *format, ...)
{
  tb_futex_lock(&print_lock);
  va_list ap;
  int length = 0;
  int sz     = 0;
  int base   = 0;
  int sgn    = 0;
  const char *cursor = format;
  const char *start  = format;

  va_start(ap, format);
  while(*cursor) {
    if(*cursor == '%') {
      tbwrite(1, start, length);
      ++cursor;
      if(*cursor == 0)
        break;

      if(*cursor == 's') {
        const char *str = va_arg(ap, const char*);
        tbwrite(1, str, strlen(str));
      }

      else {
        while(*cursor == 'l') {
          ++sz;
          ++cursor;
        }
        if(sz > 2) sz = 2;

        if(*cursor == 'x')
          base = 16;
        else if(*cursor == 'u')
          base = 10;
        else if(*cursor == 'o')
          base = 8;
        else if(*cursor == 'd')
          sgn  = 1;

        if(!sgn) {
          uint64_t num;
          if(sz == 0) num = va_arg(ap, unsigned);
          else if(sz == 1) num = va_arg(ap, unsigned long);
          else num = va_arg(ap, unsigned long long);
          printNum(num, base);
        }
        else {
          int64_t num;
          if(sz == 0) num = va_arg(ap, int);
          else if(sz == 1) num = va_arg(ap, long);
          else num = va_arg(ap, long long);
          printNumS(num);
        }
        sz = 0; base = 0; sgn = 0;
      }
      ++cursor;
      start = cursor;
      length = 0;
      continue;
    }
    ++length;
    ++cursor;
  }
  if(length)
    tbwrite(1, start, length);
  va_end (ap);
  tb_futex_unlock(&print_lock);
}

//------------------------------------------------------------------------------
// Write
//------------------------------------------------------------------------------
int tbwrite(int fd, const char *buffer, unsigned long len)
{
  return SYSCALL3(__NR_write, fd, buffer, len);
}

//------------------------------------------------------------------------------
// Sleep
//------------------------------------------------------------------------------
void tbsleep(int secs)
{
  struct timespec ts, rem;
  ts.tv_sec = secs; ts.tv_nsec = 0;
  rem.tv_sec = 0; rem.tv_nsec = 0;
  while(SYSCALL2(__NR_nanosleep, &ts, &rem) == -EINTR)
  {
    ts.tv_sec = rem.tv_sec;
    ts.tv_nsec = rem.tv_nsec;
  }
}

//------------------------------------------------------------------------------
// Time
//------------------------------------------------------------------------------
uint64_t tbtime()
{
  return SYSCALL1(__NR_time, 0);
}

//------------------------------------------------------------------------------
// Random
//------------------------------------------------------------------------------
uint32_t tbrandom(uint32_t *seed)
{
  *seed = 1103515245 * (*seed) + 12345;
  return *seed;
}

//------------------------------------------------------------------------------
// Mmap
//------------------------------------------------------------------------------
void *tbmmap(void *addr, unsigned long length, int prot, int flags, int fd,
  unsigned long offset)
{
  return (void *)SYSCALL6(__NR_mmap, addr, length, prot, flags, fd, offset);
}

//------------------------------------------------------------------------------
// Munmap
//------------------------------------------------------------------------------
int tbmunmap(void *addr, unsigned long length)
{
  return SYSCALL2(__NR_munmap, addr, length);
}

//------------------------------------------------------------------------------
// Brk
//------------------------------------------------------------------------------
void *tbbrk(void *addr)
{
  return (void *)SYSCALL1(__NR_brk, addr);
}

//------------------------------------------------------------------------------
// Malloc helper structs
//------------------------------------------------------------------------------
typedef struct memchunk
{
  struct memchunk *next;
  uint64_t         size;
} memchunk_t;

static memchunk_t  head;
static void       *heap_limit;
#define MEMCHUNK_USED 0x4000000000000000

//------------------------------------------------------------------------------
// Malloc
//------------------------------------------------------------------------------
static int memory_lock;
void *malloc(size_t size)
{
  tb_futex_lock(&memory_lock);

  //----------------------------------------------------------------------------
  // Allocating anything less than 16 bytes is kind of pointless, the
  // book-keeping overhead is too big. We will also align to 8 bytes.
  //----------------------------------------------------------------------------
  size_t alloc_size = (((size-1)>>3)<<3)+8;
  if(alloc_size < 16)
    alloc_size = 16;

  //----------------------------------------------------------------------------
  // Try to find a suitable chunk that is unused
  //----------------------------------------------------------------------------
  memchunk_t *cursor = &head;
  memchunk_t *chunk  = 0;
  while(cursor->next) {
    chunk = cursor->next;
    if(!(chunk->size & MEMCHUNK_USED) && chunk->size >= alloc_size)
      break;
    cursor = cursor->next;
  }

  //----------------------------------------------------------------------------
  // No chunk found, ask Linux for more memory
  //----------------------------------------------------------------------------
  if (!cursor->next) {
    //--------------------------------------------------------------------------
    // We have been called for the first time and don't know the heap limit yet.
    // On Linux, the brk syscall will return the previous heap limit on error.
    // We try to set the heap limit at 0, which is obviously wrong, so that we
    // could figure out what the current heap limit is.
    //--------------------------------------------------------------------------
    if(!heap_limit)
      heap_limit = tbbrk(0);

    //--------------------------------------------------------------------------
    // We will allocate at least one page at a time
    //--------------------------------------------------------------------------
    size_t chunk_size = (size+sizeof(memchunk_t)-1)/EXEC_PAGESIZE;
    chunk_size *= EXEC_PAGESIZE;
    chunk_size += EXEC_PAGESIZE;

    void     *new_heap_limit = tbbrk((char*)heap_limit + chunk_size);
    uint64_t  new_chunk_size = (char *)new_heap_limit - (char *)heap_limit;

    if(heap_limit == new_heap_limit)
    {
      tb_futex_unlock(&memory_lock);
      return 0;
    }

    cursor->next = heap_limit;
    chunk        = cursor->next;
    chunk->size  = new_chunk_size-sizeof(memchunk_t);
    chunk->next  = 0;
    heap_limit   = new_heap_limit;
  }

  //----------------------------------------------------------------------------
  // Split the chunk if it's big enough to contain one more header and at least
  // 16 more bytes
  //----------------------------------------------------------------------------
  if(chunk->size > alloc_size + sizeof(memchunk_t) + 16)
  {
    memchunk_t *new_chunk = (memchunk_t *)((char *)chunk+sizeof(memchunk_t)+alloc_size);
    new_chunk->size = chunk->size-alloc_size-sizeof(memchunk_t);
    new_chunk->next = chunk->next;
    chunk->next = new_chunk;
    chunk->size = alloc_size;
  }

  //----------------------------------------------------------------------------
  // Mark the chunk as used and return the memory
  //----------------------------------------------------------------------------
  chunk->size |= MEMCHUNK_USED;
  tb_futex_unlock(&memory_lock);
  return (char*)chunk+sizeof(memchunk_t);
}

//------------------------------------------------------------------------------
// Free
//------------------------------------------------------------------------------
void free(void *ptr)
{
  if(!ptr)
    return;

  tb_futex_lock(&memory_lock);
  memchunk_t *chunk = (memchunk_t *)((char *)ptr-sizeof(memchunk_t));
  chunk->size &= ~MEMCHUNK_USED;
  tb_futex_unlock(&memory_lock);
}

//------------------------------------------------------------------------------
// Calloc
//------------------------------------------------------------------------------
void *calloc(size_t nmemb, size_t size)
{
  size_t alloc_size = nmemb*size;
  void *ptr = malloc(alloc_size);
  char *cptr = ptr;
  if(!ptr)
    return ptr;
  for(int i = 0; i < alloc_size; ++i, *cptr++ = 0);
  return ptr;
}

//------------------------------------------------------------------------------
// Realloc
//------------------------------------------------------------------------------
void *realloc(void *ptr, size_t size)
{
  memchunk_t *chunk = (memchunk_t *)((char *)ptr-sizeof(memchunk_t));
  void       *new_ptr = malloc(size);
  char       *s = ptr;
  char       *d = new_ptr;
  size_t      min = chunk->size > size ? size : chunk->size;

  for(int i = 0; i < min; ++i, *d++ = *s++);
  free(ptr);
  return new_ptr;
}

//------------------------------------------------------------------------------
// Heap state for diagnostics
//------------------------------------------------------------------------------
void tb_heap_state(uint64_t *total, uint64_t *allocated)
{
  memchunk_t *cursor = &head;
  memchunk_t *chunk  = 0;

  *total    = 0;
  *allocated = 0;

  while(cursor->next) {
    chunk = cursor->next;
    if(chunk->size & MEMCHUNK_USED)
      ++(*allocated);
    ++(*total);
    cursor = cursor->next;
  }
}

//------------------------------------------------------------------------------
// Add an element
//------------------------------------------------------------------------------
int list_add_elem(list_t *list, void *element, int front)
{
  list_t *node = malloc(sizeof(list_t));
  if(!node)
    return -ENOMEM;
  node->element = element;
  list_add(list, node, front);
  return 0;
};

//------------------------------------------------------------------------------
// Add a node
//------------------------------------------------------------------------------
void list_add(list_t *list, list_t *node, int front)
{
  list_t *cursor = list;
  if(!front)
    for(; cursor->next; cursor = cursor->next);
  node->next = cursor->next;
  cursor->next = node;
  node->prev = cursor;
  if(node->next)
    node->next->prev = node;
}

//------------------------------------------------------------------------------
// Add a node here
//------------------------------------------------------------------------------
void list_add_here(list_t *list, list_t *node, int (*here)(void*, void*))
{
  list_t *cursor = list;
  for(; cursor->next && !(*here)(node->element, cursor->next->element);
        cursor = cursor->next);
  node->next = cursor->next;
  cursor->next = node;
  node->prev = cursor;
  if(node->next)
    node->next->prev = node;
}

//------------------------------------------------------------------------------
// Remove a node
//------------------------------------------------------------------------------
void list_rm(list_t *node)
{
  node->prev->next = node->next;
  if(node->next)
    node->next->prev = node->prev;
}

//------------------------------------------------------------------------------
// Find an element
//------------------------------------------------------------------------------
list_t *list_find_elem(list_t *list, void *element)
{
  list_t *cursor = list;
  for(; cursor->next && cursor->next->element != element;
        cursor = cursor->next);
  return cursor->next;
}

//------------------------------------------------------------------------------
// Find an element using comparator fucntion
//------------------------------------------------------------------------------
list_t *list_find_elem_func(list_t *list, void *element,
  int (*func)(void*, void*))
{
  list_t *cursor = list;
  for(; cursor->next && !(*func)(element, cursor->next->element);
        cursor = cursor->next);
  return cursor->next;
}

//------------------------------------------------------------------------------
// Invoke a function for each element
//------------------------------------------------------------------------------
void list_for_each_elem(list_t *list, void (*func)(void *))
{
  for(list_t *cursor = list->next; cursor; cursor = cursor->next)
    func(cursor->element);
}

//------------------------------------------------------------------------------
// Delete all the nodes
//------------------------------------------------------------------------------
void list_clear(list_t *list)
{
  while(list->next) {
    list_t *node = list->next;
    list->next = list->next->next;
    free(node);
  }
}

//------------------------------------------------------------------------------
// Translate errno to a message
//------------------------------------------------------------------------------
struct errinfo
{
  int errno;
  const char *msg;
};

static struct errinfo errors[] = {
  {E2BIG,           "Argument list too long"},
  {EACCES,          "Permission denied"},
  {EADDRINUSE,      "Address already in use"},
  {EADDRNOTAVAIL,   "Address not available"},
  {EAFNOSUPPORT,    "Address family not supported"},
  {EAGAIN,          "Resource temporarily unavailable"},
  {EALREADY,        "Connection already in progress"},
  {EBADE,           "Invalid exchange"},
  {EBADF,           "Bad file descriptor"},
  {EBADFD,          "File descriptor in bad state"},
  {EBADMSG,         "Bad message"},
  {EBADR,           "Invalid request descriptor"},
  {EBADRQC,         "Invalid request code"},
  {EBADSLT,         "Invalid slot"},
  {EBUSY,           "Device or resource busy"},
  {ECANCELED,       "Operation canceled"},
  {ECHILD,          "No child processes"},
  {ECHRNG,          "Channel number out of range"},
  {ECOMM,           "Communication error on send"},
  {ECONNABORTED,    "Connection aborted"},
  {ECONNREFUSED,    "Connection refused"},
  {ECONNRESET,      "Connection reset"},
  {EDEADLK,         "Resource deadlock avoided"},
  {EDEADLOCK,       "Synonym for EDEADLK"},
  {EDESTADDRREQ,    "Destination address required"},
  {EDOM,            "Mathematics argument out of domain of function"},
  {EDQUOT,          "Disk quota exceeded"},
  {EEXIST,          "File exists"},
  {EFAULT,          "Bad address"},
  {EFBIG,           "File too large"},
  {EHOSTDOWN,       "Host is down"},
  {EHOSTUNREACH,    "Host is unreachable"},
  {EIDRM,           "Identifier removed"},
  {EILSEQ,          "Illegal byte sequence"},
  {EINPROGRESS,     "Operation in progress"},
  {EINTR,           "Interrupted function call"},
  {EINVAL,          "Invalid argument"},
  {EIO,             "Input/output error"},
  {EISCONN,         "Socket is connected"},
  {EISDIR,          "Is a directory"},
  {EISNAM,          "Is a named type file"},
  {EKEYEXPIRED,     "Key has expired"},
  {EKEYREJECTED,    "Key was rejected by service"},
  {EKEYREVOKED,     "Key has been revoked"},
  {EL2HLT,          "Level 2 halted"},
  {EL2NSYNC,        "Level 2 not synchronized"},
  {EL3HLT,          "Level 3 halted"},
  {EL3RST,          "Level 3 halted"},
  {ELIBACC,         "Cannot access a needed shared library"},
  {ELIBBAD,         "Accessing a corrupted shared library"},
  {ELIBMAX,         "Attempting to link in too many shared libraries"},
  {ELIBSCN,         "lib section in a.out corrupted"},
  {ELIBEXEC,        "Cannot exec a shared library directly"},
  {ELOOP,           "Too many levels of symbolic links"},
  {EMEDIUMTYPE,     "Wrong medium type"},
  {EMFILE,          "Too many open files"},
  {EMLINK,          "Too many links"},
  {EMSGSIZE,        "Message too long"},
  {EMULTIHOP,       "Multihop attempted"},
  {ENAMETOOLONG,    "Filename too long"},
  {ENETDOWN,        "Network is down"},
  {ENETRESET,       "Connection aborted by network"},
  {ENETUNREACH,     "Network unreachable"},
  {ENFILE,          "Too many open files in system"},
  {ENOBUFS,         "No buffer space available"},
  {ENODATA,         "No message is available on the STREAM head read queue"},
  {ENODEV,          "No such device"},
  {ENOENT,          "No such file or directory"},
  {ENOEXEC,         "Exec format error"},
  {ENOKEY,          "Required key not available"},
  {ENOLCK,          "No locks available"},
  {ENOLINK,         "Link has been severed"},
  {ENOMEDIUM,       "No medium found"},
  {ENOMEM,          "Not enough space"},
  {ENOMSG,          "No message of the desired type"},
  {ENONET,          "Machine is not on the network"},
  {ENOPKG,          "Package not installed"},
  {ENOPROTOOPT,     "Protocol not available"},
  {ENOSPC,          "No space left on device"},
  {ENOSR,           "No STREAM resources"},
  {ENOSTR,          "Not a STREAM"},
  {ENOSYS,          "Function not implemented"},
  {ENOTBLK,         "Block device required"},
  {ENOTCONN,        "The socket is not connected"},
  {ENOTDIR,         "Not a directory"},
  {ENOTEMPTY,       "Directory not empty"},
  {ENOTSOCK,        "Not a socket"},
  {ENOTTY,          "Inappropriate I/O control operation"},
  {ENOTUNIQ,        "Name not unique on network"},
  {ENXIO,           "No such device or address"},
  {EOPNOTSUPP,      "Operation not supported on socket"},
  {EOVERFLOW,       "Value too large to be stored in data type"},
  {EPERM,           "Operation not permitted"},
  {EPFNOSUPPORT,    "Protocol family not supported"},
  {EPIPE,           "Broken pipe"},
  {EPROTO,          "Protocol error"},
  {EPROTONOSUPPORT, "Protocol not supported"},
  {EPROTOTYPE,      "Protocol wrong type for socket"},
  {ERANGE,          "Result too large"},
  {EREMCHG,         "Remote address changed"},
  {EREMOTE,         "Object is remote"},
  {EREMOTEIO,       "Remote I/O error"},
  {ERESTART,        "Interrupted system call should be restarted"},
  {EROFS,           "Read-only filesystem"},
  {ESHUTDOWN,       "Cannot send after transport endpoint shutdown"},
  {ESPIPE,          "Invalid seek"},
  {ESOCKTNOSUPPORT, "Socket type not supported"},
  {ESRCH,           "No such process"},
  {ESTALE,          "Stale file handle"},
  {ESTRPIPE,        "Streams pipe error"},
  {ETIME,           "Timer expired"},
  {ETIMEDOUT,       "Connection timed out"},
  {ETXTBSY,         "Text file busy"},
  {EUCLEAN,         "Structure needs cleaning"},
  {EUNATCH,         "Protocol driver not attached"},
  {EUSERS,          "Too many users"},
  {EWOULDBLOCK,     "Operation would block"},
  {EXDEV,           "Improper link"},
  {EXFULL,          "Exchange full"},
  {0,               "Unknown"}};

const char *tbstrerror(int errno)
{
  int i = 0;
  for(; errors[i].errno && errors[i].errno != errno; ++i);
  return errors[i].msg;
}

//------------------------------------------------------------------------------
// Sigaction
//------------------------------------------------------------------------------
void __restore_rt();
#define SA_RESTORER 0x04000000

int tbsigaction(int signum, struct sigaction *act, struct sigaction *old)
{
  act->sa_flags |= SA_RESTORER;
  act->sa_restorer = __restore_rt;
  return SYSCALL4(__NR_rt_sigaction, signum, act, old, sizeof(sigset_t));
}
