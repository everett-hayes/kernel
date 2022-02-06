#include <stdint.h>
#include <stddef.h>

#include "stivale2.h"
#include "util.h"
#include "kprint.h"

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
    .next = (uintptr_t)&unmap_null_hdr_tag
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

// Every interrupt handler must specify a code selector. We'll use entry 5 (5*8=0x28), which
// is where our bootloader set up a usable code selector for 64-bit mode.
#define IDT_CODE_SELECTOR 0x28

// IDT entry types
#define IDT_TYPE_INTERRUPT 0xE
#define IDT_TYPE_TRAP 0xF

// A struct the matches the layout of an IDT entry
typedef struct idt_entry {
  uint16_t offset_0;
  uint16_t selector;
  uint8_t ist : 3;
  uint8_t _unused_0 : 5;
  uint8_t type : 4;
  uint8_t _unused_1 : 1;
  uint8_t dpl : 2;
  uint8_t present : 1;
  uint16_t offset_1;
  uint32_t offset_2;
  uint32_t _unused_2;
} __attribute__((packed)) idt_entry_t;

// Make an IDT
idt_entry_t idt[256];

typedef struct interrupt_context {
  uintptr_t ip;
  uint64_t cs;
  uint64_t flags;
  uintptr_t sp;
  uint64_t ss;
} __attribute__((packed)) interrupt_context_t;

__attribute__((interrupt))
void example_handler(interrupt_context_t* ctx) {
  kprint_f("example interrupt handler\n");
  halt();
}

__attribute__((interrupt))
void example_handler_ec(interrupt_context_t* ctx, uint64_t ec) {
  kprint_f("example interrupt handler (ec=%d)\n", ec);
  halt();
}

/**
 * Set an interrupt handler for the given interrupt number.
 *
 * \param index The interrupt number to handle
 * \param fn    A pointer to the interrupt handler function
 * \param type  The type of interrupt handler being installed.
 *              Pass IDT_TYPE_INTERRUPT or IDT_TYPE_TRAP from above.
 */
void idt_set_handler(uint8_t index, void* fn, uint8_t type) {

  idt[index].type = type;
  idt[index].ist = 0;
  idt[index].present = 1;
  idt[index].dpl = 0;
  idt[index].selector = IDT_CODE_SELECTOR;

  // fn has 8 bytes -> 64 bits
  // offset_0 gets the first 16 bits
  idt[index].offset_0 = (uint64_t) fn & 0xFFFF;

  // offset_1 gets the second 16 bits
  idt[index].offset_1 = (uint64_t) fn & 0xFFFF0000;

  // offset_2 gets the last 32 bits
  idt[index].offset_2 = (uint64_t) fn & 0xFFFFFFFF00000000;
}

// This struct is used to load an IDT once we've set it up
typedef struct idt_record {
  uint16_t size;
  void* base;
} __attribute__((packed)) idt_record_t;

   
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

/**
 * Initialize an interrupt descriptor table, set handlers for standard exceptions, and install
 * the IDT.
 */
void idt_setup() {

  // Step 1: Zero out the IDT, probably using memset (which you'll have to implement)
  memset(idt, 0, 256 * sizeof(idt_entry_t));
  
  // Step 2: Use idt_set_handler() to set handlers for the standard exceptions (1--21)
  idt_set_handler(1, &example_handler, IDT_TYPE_TRAP);
  idt_set_handler(2, &example_handler, IDT_TYPE_INTERRUPT);
  idt_set_handler(3, &example_handler, IDT_TYPE_TRAP);
  idt_set_handler(4, &example_handler, IDT_TYPE_TRAP);
  idt_set_handler(5, &example_handler, IDT_TYPE_TRAP);
  idt_set_handler(6, &example_handler, IDT_TYPE_TRAP);
  idt_set_handler(7, &example_handler, IDT_TYPE_TRAP);
  idt_set_handler(8, &example_handler_ec, IDT_TYPE_TRAP);
  idt_set_handler(9, &example_handler, IDT_TYPE_TRAP);
  idt_set_handler(10, &example_handler_ec, IDT_TYPE_TRAP);
  idt_set_handler(11, &example_handler_ec, IDT_TYPE_TRAP);
  idt_set_handler(12, &example_handler_ec, IDT_TYPE_TRAP);
  idt_set_handler(13, &example_handler_ec, IDT_TYPE_TRAP);
  idt_set_handler(14, &example_handler_ec, IDT_TYPE_TRAP);
  idt_set_handler(15, &example_handler, IDT_TYPE_TRAP);
  idt_set_handler(16, &example_handler, IDT_TYPE_TRAP);
  idt_set_handler(17, &example_handler_ec, IDT_TYPE_TRAP);
  idt_set_handler(18, &example_handler, IDT_TYPE_TRAP);
  idt_set_handler(19, &example_handler, IDT_TYPE_TRAP);
  idt_set_handler(20, &example_handler, IDT_TYPE_TRAP);
  idt_set_handler(21, &example_handler_ec, IDT_TYPE_TRAP);

  // Step 3: Install the IDT
  idt_record_t record = {
    .size = sizeof(idt),
    .base = idt
  };

  __asm__("lidt %0" :: "m"(record));
}

void _start(struct stivale2_struct* hdr) {
  // We've booted! Let's start processing tags passed to use from the bootloader
  term_setup(hdr);

  kprint_f("%p\n", (uint64_t) example_handler);
  kprint_f("%p\n", (uint64_t) example_handler & 0x000000000000FFFF);
  kprint_f("%p\n", (uint64_t) example_handler & 0x00000000FFFF0000);
  kprint_f("%p\n", (uint64_t) example_handler & 0xFFFFFFFF00000000);

	halt();
}