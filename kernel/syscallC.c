#include "syscallC.h"

#define SYS_WRITE 0
#define SYS_READ 1
#define SYS_MMAP 2
#define SYS_EXEC 3

size_t syscall_read(int fd, void* buf, size_t count) {

  if (fd != 0) {
    return -1;
  }

  char* char_buf = (char*) buf; 

  for (int i = 0; i < count; i++) {
    char val = kgetc(); // need to check if this is a backspace
    kprint_f("the val inside the syscall was %c\n", val);

    // MAKE SURE WE DON"T BACKSPACE TOO MUCH !!!!! TODO
    if (val == '\b') {
      *char_buf = '\0';
      char_buf -= 1; // move address backward 1 byte
      i -= 2; // don't count the backspace or the eaten char
    } else {
      kprint_f("inside the non backspace one!\n");
      *char_buf = val;
      kprint_f("*char_buf now looks like %c\n", *char_buf);
      char_buf += 1; // move address forward 1 byte
    } 
  }

  return count;
}

size_t syscall_write(int fd, void *buf, size_t count) {

  if (fd != 1 && fd != 2) {
    return -1;
  }

  char* char_buf = (char*) buf; 

  // Pop off character from the buffer and print it 
  for (int i = 0; i < count; i++) {
    char val = *char_buf;
    kprint_f("%c", val);
    char_buf += 1; // Move address 1 byte backwards
  }

  return count;
}

// starting malloc pointer
uint64_t malloc_pointer = 0x80000000000;;

uint64_t syscall_memmap(uintptr_t address, bool user, bool writable, bool executable, size_t length) {

  bool res = vm_map(get_top_table(), malloc_pointer, user, writable, executable);

  if (res) {
    uint64_t allocated_address = malloc_pointer;
    // bump malloc_pointer
    malloc_pointer += 0x1000 * (length / 4096);
    return allocated_address;
  } 

  return 0x0;
}

// No more arguments than 6!
uint64_t syscall(uint64_t num, ...);
void syscall_entry();

uint64_t syscall_handler(uint64_t num, uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {

  switch (num) {
    case 0:
      return syscall_write(arg0, arg1, arg2);
    case 1:
      return syscall_read(arg0, arg1, arg2);
    case 2:
      return syscall_memmap(arg0, arg1, arg2, arg3, arg4);
    case 3:
      break;
    default:
      kprint_f("you've called a syscall that doesn't exist!!\n");
  }

  return num;
}

void setup_syscall() {
    idt_set_handler(0x80, syscall_entry, IDT_TYPE_TRAP);
}