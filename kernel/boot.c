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
#include "string.h"
#include "mem.h"
#include "gdt.h"
#include "usermode_entry.h"
#include "syscallC.h"
#include "exec.h"

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

void pic_setup() {
  pic_init();
  idt_set_handler(IRQ1_INTERRUPT, keypress_handler, IDT_TYPE_INTERRUPT);
  pic_unmask_irq(1);
}

void _start(struct stivale2_struct* hdr) {

  // setup various parts of the kernel
  idt_setup();
  initialize_memory(find_tag(hdr, STIVALE2_STRUCT_TAG_MEMMAP_ID), find_tag(hdr, STIVALE2_STRUCT_TAG_HHDM_ID));
  term_init();
  unmap_lower_half();
  pic_setup();
  gdt_setup();
  syscall_setup();
  exec_setup(find_tag(hdr, STIVALE2_STRUCT_TAG_MODULES_ID));

  // start the shell up for the user
  uint64_t shell_start = locate_module("shell");
  exec(shell_start);

	halt();
}