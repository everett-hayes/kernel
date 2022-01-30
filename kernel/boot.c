#include <stdint.h>
#include <stddef.h>

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

// Print an unsigned 64-bit integer value to the terminal in decimal notation (no leading zeros please!)
void kprint_d(uint64_t value) {

  char arr[digit_len(value, DECIMAL)];
  int i = 0;

  while (value != 0) {
    int digit = value % DECIMAL;
    char ch = digit + '0'; 
    arr[i++] = ch; 
    value /= DECIMAL;
  }

  reverse(arr, i);
  arr[i] = '\0';
  kprint_s(arr);
}

// Print an unsigned 64-bit integer value to the terminal in lowercase hexadecimal notation (no leading zeros or “0x” please!)
void kprint_x(uint64_t value) {
  char arr[digit_len(value, HEXADECIMAL)];
  int i = 0;

  while (value != 0) {
    int digit = value % HEXADECIMAL;
    char ch;

    if(digit > 9) {
      ch = digit - 10 + 'a';
    } else {
      ch = digit + '0';
    }
    arr[i++] = ch; 
    value /= HEXADECIMAL;
  }

  reverse(arr, i);
  arr[i] = '\0';
  kprint_s(arr);

}

// Print the value of a pointer to the terminal in lowercase hexadecimal with the prefix “0x”
void kprint_p(void* ptr) { 
  uint64_t ptr_int = (uint64_t) ptr;
  
  char arr[digit_len(ptr_int, HEXADECIMAL) + 2];
  int i = 0;

  while (ptr_int != 0) {
    int digit = ptr_int % HEXADECIMAL;
    char ch;

    if(digit > 9) {
      ch = digit - 10 + 'a';
    } else {
      ch = digit + '0';
    }
    arr[i++] = ch; 
    ptr_int /= HEXADECIMAL;
  }

  // hardcode the 0x prefix
  arr[i] = 'x';
  arr[i+1] = '0';

  reverse(arr, i+2);
  arr[i+2] = '\0';
  kprint_s(arr);
}


void _start(struct stivale2_struct* hdr) {
  // We've booted! Let's start processing tags passed to use from the bootloader
  term_setup(hdr);

  // Print a greeting
  term_write("Hello Kernel!\n", 14);

  // Test print functions
  kprint_c('f');
  kprint_s("Hello World");
  kprint_x(1234567);
  int test = 10;
  kprint_p(&test);

	// We're done, just hang...
	halt();
}