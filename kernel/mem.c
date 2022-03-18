#include "mem.h"

#include <stdint.h>

// The  memset() function fills the first n bytes of the memory area pointed to by s with the constant byte c.
void* memset(void* ptr, int c, size_t n) {
  unsigned char* curr = ptr;
  int i = 0;

  while (i < n) {
    *curr = (unsigned char) c;
    curr++;
    i++;
  }

  return ptr;
}

void* memcpy(void* dest, const void* src, size_t size) {
    char* target = (char*) dest;
    char* source = (char*) src;
    for(int i = 0; i < size; i++) {
        target[i] = source[i];
    }

    return dest;
}

uintptr_t read_cr3() {
  uintptr_t value;
  __asm__("mov %%cr3, %0" : "=r" (value));
  return value;
}

uintptr_t get_top_table() {
  return read_cr3() & 0xFFFFFFFFFFFFF000;
}