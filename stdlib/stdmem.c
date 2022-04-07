#include "stdmem.h"

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

void* mmap(void *addr, size_t length, int prot, int flags, int fd, int offset) {

    


}


// mmap (this one requires a new system call)

