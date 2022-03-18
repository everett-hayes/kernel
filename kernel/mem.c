#include "mem.h"

#include "stivale2.h"

#include <stdint.h>
#include <stdbool.h>

#define PAGE_SIZE 0x1000

// a page table entry
typedef struct pt_entry {
  uint8_t present : 1;
  uint8_t writable : 1;
  uint8_t user : 1;
  uint16_t unused0 : 9;
  uint64_t address : 51;
  uint8_t no_execute : 1;
} __attribute__((packed)) pt_entry;

// Define the structure of a node within the freelist
typedef struct free_list_node {
  struct free_list_node* next;
} free_list_node_t;

free_list_node_t* free_list_head = NULL;

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

uint64_t hhdm_base;

uintptr_t read_cr3() {
  uintptr_t value;
  __asm__("mov %%cr3, %0" : "=r" (value));
  return value;
}

uintptr_t get_top_table() {
  return read_cr3() & 0xFFFFFFFFFFFFF000;
}

void initialize_physical_area(uint64_t start, uint64_t end) {

  uint64_t curr = start;

  while (curr < end) {
    
    if (free_list_head == NULL) {
      free_list_node_t* node = (free_list_node_t*) (curr + hhdm_base);
      free_list_head = node;
    } else {
      free_list_node_t* node = (free_list_node_t*) (curr + hhdm_base);
      node->next = free_list_head;
      free_list_head = node;
    }

    curr += PAGE_SIZE;
  }
}

void initialize_memory(struct stivale2_struct_tag_memmap* physical_tag, struct stivale2_struct_tag_hhdm* virtual_tag) {

  hhdm_base = virtual_tag->addr;

  for (int i = 0; i < physical_tag->entries; i++) {

    struct stivale2_mmap_entry entry = physical_tag->memmap[i];

    if (entry.type == 1) {
      uint64_t physical_start = entry.base;
      uint64_t physical_end = entry.base + entry.length;
      initialize_physical_area(physical_start, physical_end);
    }
  }
}

/**
 * Allocate a page of physical memory.
 * \returns the physical address of the allocated physical memory or 0 on error.
 */
uintptr_t pmem_alloc() {
  if (free_list_head == NULL) {
    return 0;
  }

  uintptr_t val = (uintptr_t) free_list_head;
  free_list_head = free_list_head->next;

  return val - hhdm_base;
}

/**
 * Free a page of physical memory.
 * \param p is the physical address of the page to free, which must be page-aligned.
 */
void pmem_free(uintptr_t p) {

  if (p == NULL) {
    return;
  }

  if (free_list_head == NULL) {
    free_list_node_t* node = (free_list_node_t*) (p + hhdm_base);
    free_list_head = node;
  } else {
    free_list_node_t* node = (free_list_node_t*) (p + hhdm_base);
    node->next = free_list_head;
    free_list_head = node;
  }
}

// a temp getter function TODO make it so this can be a private variable
uint64_t get_hhdm_base() {
  return hhdm_base;
}

uintptr_t translate_virtual_to_physcial(void* address) {

  // Mask the bottom 12 bits
  uintptr_t root = get_top_table();

  pt_entry* table = (pt_entry*) (root + get_hhdm_base());

  // Break the virtual address into pieces
  uint64_t address_int = (uint64_t) address;

  uint64_t offset = address_int & 0xFFF;
  uint16_t indices[] = {
    (address_int >> 12) & 0x1FF,
    (address_int >> 21) & 0x1FF,
    (address_int >> 30) & 0x1FF,
    (address_int >> 39) & 0x1FF
  };

  bool isFound = true;

  for (int i = 3; i >= 1; i--) {

    // get the current entry
    pt_entry* curr_entry = (pt_entry*) (table + indices[i]);

    if (!curr_entry->present) {
      isFound = false;
      break;
    }

    // advance pointer to next level of table
    table = (pt_entry*) (curr_entry->address << 12);
  }

  return (isFound) ? (uintptr_t) table + indices[0] + offset : 0;
}
