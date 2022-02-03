#include <stdint.h>
#include <stddef.h>

#include "stivale2.h"
#include "util.h"
#include "kprint.h"

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

void term_setup(struct stivale2_struct* hdr) {
  // Look for a terminal tag
  struct stivale2_struct_tag_terminal* tag = find_tag(hdr, STIVALE2_STRUCT_TAG_TERMINAL_ID);

  // Make sure we find a terminal tag
  if (tag == NULL) halt();

  // Save the term_write function pointer
  set_term_write((term_write_t)tag->term_write);
}

void find_memory(struct stivale2_struct* hdr) {

  struct stivale2_struct_tag_memmap* physical_tag = find_tag(hdr, STIVALE2_STRUCT_TAG_MEMMAP_ID);
  struct stivale2_struct_tag_hhdm* virtual_tag = find_tag(hdr, STIVALE2_STRUCT_TAG_HHDM_ID);
  uint64_t hhdm_start = virtual_tag->addr;
  //kprint_f("HHDM start: %x\n", hhdm_start);

  kprint_f("Usable Memory:\n");

  for (int i = 0; i < physical_tag->entries; i++) {
    struct stivale2_mmap_entry entry = physical_tag->memmap[i];
    // Check whether memory is usable
    if (entry.type == 1) {
      uint64_t physical_start = entry.base;
      uint64_t physical_end = entry.base + entry.length;
      uint64_t virtual_start = physical_start + hhdm_start;
      uint64_t virtual_end = physical_end + hhdm_start;
      kprint_f("\t0x%x-0x%x mapped at 0x%x-0x%x\n", physical_start, physical_end, virtual_start, virtual_end);
    }
  }
}

typedef struct_idt_entry {
  uint16_t offset_0;
  uint16_t segment;
  uint8_t ist : 3;
  uint8_t zeros : 5;
  uint8_t type : 4;
  uint8_t zero : 1;
  uint8_t dp1 : 3;
  bool present : 1;
  

} __attribute__((packed))
idt_entry_t;

void _start(struct stivale2_struct* hdr) {
  // We've booted! Let's start processing tags passed to use from the bootloader
  term_setup(hdr);

  find_memory(hdr);
  kprint_f("\n");

	// We're done, just hang...
	halt();
}