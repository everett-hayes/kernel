#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "stivale2.h"
#include "util.h"
#include "kprint.h"
#include "pic.h"
#include "port.h"
#include "exception.h"
#include "keyboard.h"

// Reserve space for the stack
static uint8_t stack[8192];

static struct stivale2_tag unmap_null_hdr_tag = {
  .identifier = STIVALE2_HEADER_TAG_UNMAP_NULL_ID,
  .next = 0
};

// Request a terminal from the bootloader
static struct stivale2_header_tag_terminal terminal_hdr_tag = {
	.tag = {
    .identifier = STIVALE2_HEADER_TAG_TERMINAL_ID,
    .next = (uint64_t) &unmap_null_hdr_tag
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

void term_setup(struct stivale2_struct* hdr) {
  // Look for a terminal tag
  struct stivale2_struct_tag_terminal* tag = find_tag(hdr, STIVALE2_STRUCT_TAG_TERMINAL_ID);

  // Make sure we find a terminal tag
  if (tag == NULL) halt();

  // Save the term_write function pointer
  set_term_write((term_write_t)tag->term_write);
}

intptr_t hhdm_base;

#define PAGE_SIZE 0x1000

typedef struct free_list_node {
  struct free_list_node* next;
} free_list_node_t;

free_list_node_t* free_list_head = NULL;

void initialize_physical_area(uint64_t start, uint64_t end) {

  uint64_t curr = start;

  while (curr < end) {
    
    if (free_list_head == NULL) {
      // make a new node
      // set to head
      // insert at area of physical memory
      free_list_node_t* node = (free_list_node_t*) (curr + hhdm_base);
      free_list_head = node;
    } else {
      // make a new node
      // attach before head
      // insert at area of physical memory
      free_list_node_t* node = (free_list_node_t*) (curr + hhdm_base);
      node->next = free_list_head;
      free_list_head = node;
    }

    curr += PAGE_SIZE;
  }
}

void check_answer() {

  free_list_node_t* curr = free_list_head;
  int i = 0;

  while (curr != NULL) {
    i++;
    curr = curr->next;
  }

  kprint_f("there are %d nodes available in your free list", i);
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

  // round down to multiple of 4096 (0x1000) maybe  

  if (free_list_head == NULL) {
    free_list_node_t* node = (free_list_node_t*) (p + hhdm_base);
    free_list_head = node;
  } else {
    free_list_node_t* node = (free_list_node_t*) (p + hhdm_base);
    node->next = free_list_head;
    free_list_head = node;
  }
}

typedef struct pt_entry {
  uint8_t present : 1;
  uint8_t writable : 1;
  uint8_t user : 1;
  uint16_t unused0 : 9;
  uint64_t address : 51;
  uint8_t no_execute : 1;
} __attribute__((packed)) pt_entry;

/**
 * Map a single page of memory into a virtual address space.
 * \param root The physical address of the top-level page table structure
 * \param address The virtual address to map into the address space, must be page-aligned
 * \param user Should the page be user-accessible?
 * \param writable Should the page be writable?
 * \param executable Should the page be executable?
 * \returns true if the mapping succeeded, or false if there was an error
 */
bool vm_map(uintptr_t root, uintptr_t address, bool user, bool writable, bool executable) {

  bool succeeded = true;
  pt_entry* table = (pt_entry*) (root + hhdm_base);

  uint64_t offset = address & 0xFFF;
  uint16_t indices[] = {
    (address >> 12) & 0x1FF,
    (address >> 21) & 0x1FF,
    (address >> 30) & 0x1FF,
    (address >> 39) & 0x1FF // the fourth (highest) table index
  };

  pt_entry* curr_entry = NULL;

  // traverse down the virtual address to level 1
  for (int i = 3; i >= 1; i--) {
    uint16_t index = indices[i];
    curr_entry = (pt_entry*) (table + index);

    kprint_f("Level %d (index %d of %p)\n", i + 1, indices[i], table);

    if (curr_entry->present) {
      // change curr_entry and continue
      table = (pt_entry*) (curr_entry->address << 12);
    } else {
      // make a pt_entry on the current level, set to present
      curr_entry->present = 1;
      curr_entry->user = 1;
      curr_entry->writable = 1;
      curr_entry->no_execute = 0;
      // make a page table on the below level and initialize it to all not presents
      // use pmem_alloc for this step! we will need 8 pages! Contiguous?
      uintptr_t newly_created_table = pmem_alloc();

      memset(newly_created_table, 0, 4096);

      // make our current pt_entry point to this newly created table
      curr_entry->address = newly_created_table >> 12;
      curr_entry = (curr_entry->address << 12);
    }
  }

  pt_entry* dest = (pt_entry*) ((curr_entry->address << 12) + offset);
  kprint_f("Level 1 (index %d of %p)\n", indices[0], curr_entry);

  dest->address = pmem_alloc() >> 12;
  dest->present = 1;
  dest->user = user;
  dest->writable = writable;
  dest->no_execute = executable;

  // traverse down the virtual address 
    // if I find a pt_entry continue
    // if I don't make a new page table

  // when I reach the bottom, put the head of my free list inside the pt_entry at the page_table head + offset
  // advance the head of the free list

  // set the bits of the bottom level pt_entry

  /*
    Starting at the top-level page table structure (which we called the level-4 page table in class) 
    you will need to traverse down through the levels just as you did in your translate function. 
    However, this time you need to be prepared for missing entries in the page table. 
    When you find a missing entry anywhere except the last-level page table you will need to allocate and fill in a new page table.
  */

 return succeeded;
}

void find_memory(struct stivale2_struct* hdr) {

  struct stivale2_struct_tag_memmap* physical_tag = find_tag(hdr, STIVALE2_STRUCT_TAG_MEMMAP_ID);
  struct stivale2_struct_tag_hhdm* virtual_tag = find_tag(hdr, STIVALE2_STRUCT_TAG_HHDM_ID);
  uint64_t hhdm_start = virtual_tag->addr;
  hhdm_base = hhdm_start;
  kprint_f("HHDM start: %x\n", hhdm_start);

  kprint_f("Usable Memory:\n");

  for (int i = 0; i < physical_tag->entries; i++) {

    struct stivale2_mmap_entry entry = physical_tag->memmap[i];

    // Check whether memory is usable
    if (entry.type == 1) {
      uint64_t physical_start = entry.base;
      uint64_t physical_end = entry.base + entry.length;
      initialize_physical_area(physical_start, physical_end);
      uint64_t virtual_start = physical_start + hhdm_start;
      uint64_t virtual_end = physical_end + hhdm_start;
      kprint_f("\t0x%x-0x%x mapped at 0x%x-0x%x\n", physical_start, physical_end, virtual_start, virtual_end);
    }
  }

  kprint_f("ALL DONE :)\n");
}

void pic_setup() {
  pic_init();
  idt_set_handler(IRQ1_INTERRUPT, keypress_handler, IDT_TYPE_INTERRUPT);
  pic_unmask_irq(1);
}

uintptr_t read_cr3() {
  uintptr_t value;
  __asm__("mov %%cr3, %0" : "=r" (value));
  return value;
}

void translate(void* address) {

  kprint_f("Translating %p\n", address);

  // masks the bottom 12 bits
  // this is the start of the level 4 table
  uintptr_t root = read_cr3() & 0xFFFFFFFFFFFFF000;

  pt_entry* table = (pt_entry*) (root + hhdm_base);

  // break the virtual address into pieces
  uint64_t address_int = (uint64_t) address;

  uint64_t offset = address_int & 0xFFF;
  uint16_t indices[] = {
    (address_int >> 12) & 0x1FF,
    (address_int >> 21) & 0x1FF,
    (address_int >> 30) & 0x1FF,
    (address_int >> 39) & 0x1FF // the fourth (highest) table index
  };

  bool isFound = true;

  for (int i = 3; i >= 0; i--) {
    uint16_t index = indices[i];
    pt_entry* curr_entry = (pt_entry*) (table + index);

    kprint_f("Level %d (index %d of %p)\n", i + 1, indices[i], table);

    if (curr_entry->present) {
      kprint_f("\t%s %s %s\n", 
        (curr_entry->user) ? "user" : "kernel", 
        (curr_entry->writable) ? "writable" : "non-writable",
        (curr_entry->no_execute) ? "non-executable" : "executable");
    } else {
      kprint_f("\tIs not present");
      isFound = false;
      break;
    }

    if (i != 0) {
      table = (pt_entry*) (curr_entry->address << 12);
    }
  }

  if (isFound) {
    uintptr_t result = (uintptr_t) table + offset;
    kprint_f("%p maps to %p\n", address, result);
  }
}

uint64_t read_cr0() {
  uintptr_t value;
  __asm__("mov %%cr0, %0" : "=r" (value));
  return value;
}

void write_cr0(uint64_t value) {
  __asm__("mov %0, %%cr0" : : "r" (value));
}

void _start(struct stivale2_struct* hdr) {
  // We've booted! Let's start processing tags passed to use from the bootloader
  term_setup(hdr);
  idt_setup();
  pic_setup();
  find_memory(hdr);

  // Enable write protection
  uint64_t cr0 = read_cr0();
  cr0 |= 0x10000;
  write_cr0(cr0);  

  uintptr_t root = read_cr3() & 0xFFFFFFFFFFFFF000;
  int* p = (int*)0x50004000;
  bool result = vm_map(root, (uintptr_t)p, false, true, false);
  if (result) {
    *p = 123;
    kprint_f("Stored %d at %p\n", *p, p);
  } else {
    kprint_f("vm_map failed with an error\n");
  }

	halt();
}