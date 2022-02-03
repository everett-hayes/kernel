#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

#include "stivale2.h"
#include "util.h"

#define DECIMAL 10
#define HEXADECIMAL 16

// Reserve space for the stack
static uint8_t stack[8192];

// Request a terminal from the bootloader
static struct stivale2_header_tag_terminal terminal_hdr_tag = {
	.tag = {
    .identifier = STIVALE2_HEADER_TAG_TERMINAL_ID,
    .next = 0
  },
  .flags = 0
};

// Declare the header for the bootloader
__attribute__((section(".stivale2hdr"), used))
static struct stivale2_header stivale_hdr = {
  // Use ELF file's default entry point
  .entry_point = 0,

  // Use stack (starting at the top)
  .stack = (uintptr_t)stack + sizeof(stack),

  // Bit 1: request pointers in the higher half
  // Bit 2: enable protected memory ranges (specified in PHDR)
  // Bit 3: virtual kernel mappings (no constraints on physical memory)
  // Bit 4: required
  .flags = 0x1E,
  
  // First tag struct
  .tags = (uintptr_t)&terminal_hdr_tag
};

// Find a tag with a given ID
void* find_tag(struct stivale2_struct* hdr, uint64_t id) {
  // Start at the first tag
	struct stivale2_tag* current = (struct stivale2_tag*)hdr->tags;

  // Loop as long as there are more tags to examine
	while (current != NULL) {
    // Does the current tag match?
		if (current->identifier == id) {
			return current;
		}

    // Move to the next tag
		current = (struct stivale2_tag*)current->next;
	}

  // No matching tag found
	return NULL;
}

typedef void (*term_write_t)(const char*, size_t);
term_write_t term_write = NULL;

void term_setup(struct stivale2_struct* hdr) {
  // Look for a terminal tag
  struct stivale2_struct_tag_terminal* tag = find_tag(hdr, STIVALE2_STRUCT_TAG_TERMINAL_ID);

  // Make sure we find a terminal tag
  if (tag == NULL) halt();

  // Save the term_write function pointer
	term_write = (term_write_t)tag->term_write;
}

// Return the number of digits of a number
int digit_len(uint64_t num, int base) {
  int i = 0;

  if (num == 0) {
    return 1;
  }

  while (num != 0) {
    i++;
    num /= base;
  }

  return i;
}

// Return the length of a string
int strlen(const char* str) {
  int i = 0;

  while (str[i] != '\0') {
    i++;
  }

  return i;
}

// Reverse a string in-place
void reverse(char* str, int len) {
  int low = 0;
  int high = len - 1;

  while (low < high) {
    char temp = str[low];
    str[low++] = str[high];
    str[high--] = temp;
  }
}

// Print a single character to the terminal
void kprint_c(char c) {
  term_write(&c, 1);
}

// Print a string to the terminal
void kprint_s(const char* str) {
  term_write(str, strlen(str));
}

void kprint_num(uint64_t value, int base) {

  // digit length + 1 for null
  char arr[digit_len(value, base) + 1];
  int i = 0;

  if (value == 0) {
    arr[i++] = '0';
  }

  while (value != 0)  {
    int digit = value % base;
    char ch = (digit > 9) ? digit - 10 + 'a' : digit + '0';
    arr[i++] = ch; 
    value /= base;
  }

  reverse(arr, i);
  arr[i] = '\0';
  kprint_s(arr);
}

// Print an unsigned 64-bit integer value to the terminal in decimal notation
void kprint_d(uint64_t value) {
  kprint_num(value, DECIMAL);
}
// Print an unsigned 64-bit integer value to the terminal in lowercase hexadecimal notation (no leading zeros or “0x” please!)
void kprint_x(uint64_t value) {
  kprint_num(value, HEXADECIMAL);
}

// Print the value of a pointer to the terminal in lowercase hexadecimal with the prefix “0x”
void kprint_p(void* ptr) { 
  kprint_s("0x");
  kprint_x((uint64_t) ptr);
}

void kprint_f(const char* format, ...) {
  const char* pos = format;
  va_list ap;

  va_start(ap, format);

  while (*pos != '\0') {

    if (*pos == '%') {
      pos++;

      switch (*pos) {
        case 'c':
          kprint_c(va_arg(ap, int));
          break;
        case 's':
          kprint_s(va_arg(ap, const char*));
          break;
        case 'd':
          kprint_d(va_arg(ap, uint64_t));
          break;
        case 'x':
          kprint_x(va_arg(ap, uint64_t));
          break;
        case 'p':
          kprint_p(va_arg(ap, void*));
          break;
        case '%':
          kprint_c('%');
          break;
        default:
          kprint_c('?');
      }
    } else {
      kprint_c(*pos);
    }

    pos++;
  }

  va_end(ap);
}

void findMemory(struct stivale2_struct* hdr) {


  //virtual memory
  struct stivale2_struct_tag_hhdm* virtual_tag = find_tag(hdr, STIVALE2_STRUCT_TAG_HHDM_ID);
  uint64_t virtualMemStart = virtual_tag->addr;

  // physical memory
  struct stivale2_struct_tag_memmap* physical_tag = find_tag(hdr, STIVALE2_STRUCT_TAG_MEMMAP_ID);

  kprint_f("Usable Memory:\n");
  for (int i = 0; i < physical_tag->entries; i++) {
    // if the memory is usable
    if (physical_tag->memmap[i].type == 1) {
      uint64_t start = physical_tag->memmap[i].base;
      uint64_t end = physical_tag->memmap[i].base + physical_tag->memmap[i].length;
      uint64_t virtualStart = start + virtualMemStart;
      uint64_t virtualEnd = end + virtualMemStart;
      kprint_f("\t0x%x-0x%x mapped at 0x%x-0x%x\n", start, end, virtualStart, virtualEnd);
    }
  }
}

void _start(struct stivale2_struct* hdr) {
  // We've booted! Let's start processing tags passed to use from the bootloader
  term_setup(hdr);

  findMemory(hdr);
  kprint_s("\n");
  // Test print functions
  // kprint_c('f');
  // kprint_s("Hello World");
  kprint_f("Hello world it is %s. I am %d years old", "Everett", 12);
  // int test = 10;
  // kprint_p(&test);

	// We're done, just hang...
	halt();
}