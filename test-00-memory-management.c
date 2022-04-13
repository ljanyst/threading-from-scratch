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

#include <tb.h>
#include <asm-generic/param.h>

//------------------------------------------------------------------------------
// Prototype for a hidden function
//------------------------------------------------------------------------------
void tb_heap_state(uint64_t *total, uint64_t *allocated);

//------------------------------------------------------------------------------
// Check if the state of the heap memory is consistent
//------------------------------------------------------------------------------
int memory_correct(void **addrs, uint32_t *sizes)
{
  for(int i = 0; i < 256; ++i) {
    unsigned char *c = addrs[i];
    for(int j = 0; j < sizes[i]; ++j, ++c)
      if(*c != i)
        return 0;
  }
  return 1;
}

//------------------------------------------------------------------------------
// Start the show
//------------------------------------------------------------------------------
int main(int argc, char **argv)
{
  uint32_t  seed = tbtime();
  void     *addrs[256];
  uint32_t  sizes[256];

  tbprint("Testing memory allocator. It may take a while...\n");

  //----------------------------------------------------------------------------
  // Allocate all the chunks
  //----------------------------------------------------------------------------
  for(int i = 0; i < 256; ++i) {
    sizes[i] = tbrandom(&seed) % (2*EXEC_PAGESIZE);
    addrs[i] = calloc(sizes[i], 1);
    unsigned char *c = addrs[i];
    for(int j = 0; j < sizes[i]; ++j, *c++ = i);
  }

  if(!memory_correct(addrs, sizes)) {
    tbprint("Memory corruption after initialization\n");
    return 1;
  }

  //----------------------------------------------------------------------------
  // In each iteration realloc 20% of each chunks, mark the memory and verify
  // correctness of the whole thing.
  //----------------------------------------------------------------------------
  for(int i = 0; i < 10000; ++i) {
    for(int k = 0; k < 50; ++k) {
      int ind = tbrandom(&seed) % 256;
      uint32_t new_size = tbrandom(&seed) % (2*EXEC_PAGESIZE);
      addrs[ind] = realloc(addrs[ind], new_size);
      if(new_size > sizes[ind]) {
        unsigned char *c = addrs[ind];
        c += sizes[ind];
        for(int l = sizes[ind]; l < new_size; ++l, *c++ = ind);
      }
      sizes[ind] = new_size;
    }
    if(!memory_correct(addrs, sizes)) {
      tbprint("Memory corruption after iteration: %d\n", i);
      return 1;
    }
  }

  //----------------------------------------------------------------------------
  // Free all the chunks
  //----------------------------------------------------------------------------
  for(int i = 0; i < 256; ++i)
    free(addrs[i]);

  //----------------------------------------------------------------------------
  // Check up the heap
  //----------------------------------------------------------------------------
  uint64_t total, allocated;
  tb_heap_state(&total, &allocated);
  tbprint("Total chunks on the heap: %llu, allocated: %llu\n",
    total, allocated);
  return 0;
};
