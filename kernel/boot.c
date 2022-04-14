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
#include "elf.h"
#include "string.h"
#include "mem.h"
#include "gdt.h"
#include "usermode_entry.h"
#include "syscallC.h"

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

// void exec_syscall(struct stivale2_struct* hdr, char* module_name) {

//   uint64_t module_address = locate_module(hdr, module_name);

//   if (module_address == -1) return;

//   exec(module_address);
// }

uint64_t locate_module(struct stivale2_struct* hdr, char* module_name) {
  struct stivale2_struct_tag_modules* tag = find_tag(hdr, STIVALE2_STRUCT_TAG_MODULES_ID);

  for (int i = 0; i < tag->module_count; i++) {
    struct stivale2_module* temp = &(tag->modules[i]);

    if (strcmp(temp->string, module_name) == 0) {
      return temp->begin;
    }
  }

  return -1;
}

void exec(uintptr_t elf_address) {

  elf64_hdr_t* header = (elf64_hdr_t*) elf_address;

  elf64_prg_hdr_t* prg_header = (elf64_prg_hdr_t*) (elf_address + header->e_phoff);

  int ph_size = header->e_phentsize;
  int ph_num = header->e_phnum;

  uintptr_t root = get_top_table();
  uintptr_t temp = prg_header;

  for (int i = 0; i < ph_num; i++) {
    elf64_prg_hdr_t* prg_header_curr = (elf64_prg_hdr_t*) (temp);

    if (prg_header_curr->p_type == 1 && prg_header_curr->p_memsz > 0) {
      kprint_f("trying to map at %p\n", prg_header_curr->p_vaddr);
      bool res = vm_map(root, prg_header_curr->p_vaddr, 1, 1, 1);

      if (!res) {
        return;
      }

      int try = 4;
      int temp = prg_header_curr->p_flags;
      bool readable = false;
      bool writable = false;
      bool executable = false;

      for (int i = 0; i < 3; i++) {

        if (temp - try >= 0) {

          if (try == 4) {
            readable = true;
          } else if (try == 2) {
            writable = true;
          } else {  
            executable = true;
          }

          temp -= try;
        }        

        try >>= 1;
      }


      // copy contents of elf file to virtual address
      // ask charlie about this line!
      memcpy(prg_header_curr->p_vaddr, elf_address + prg_header_curr->p_offset, prg_header_curr->p_memsz);
      vm_protect(root, prg_header_curr->p_vaddr, true, writable, executable);
    }

    // advance pointer by size of program header
    temp += ph_size;
  }

  // now switch to usermode!!!
  // Pick an arbitrary location and size for the user-mode stack
  uintptr_t user_stack = 0x70000000000;
  size_t user_stack_size = 8 * 0x1000;

  // Map the user-mode-stack
  for (uintptr_t p = user_stack; p < user_stack + user_stack_size; p += 0x1000) {
    // Map a page that is user-accessible, writable, but not executable
    vm_map(get_top_table(), p, true, true, false);
  }

  kprint_f("about to jump!\n");

  // And now jump to the entry point
  usermode_entry(USER_DATA_SELECTOR | 0x3,          // User data selector with priv=3
                user_stack + user_stack_size - 8,   // Stack starts at the high address minus 8 bytes
                USER_CODE_SELECTOR | 0x3,           // User code selector with priv=3
                header->e_entry);        

  // typedef void (*elf_exec_t)();
  // elf_exec_t current_exe = (elf_exec_t) (header->e_entry);
  // current_exe();
}


void _start(struct stivale2_struct* hdr) {

  idt_setup();
  initialize_memory(find_tag(hdr, STIVALE2_STRUCT_TAG_MEMMAP_ID), find_tag(hdr, STIVALE2_STRUCT_TAG_HHDM_ID));
  term_init();
  unmap_lower_half();
  pic_setup();
  gdt_setup();

  // kprint_f("Hello\n");

  // kprint_f("the physical address of start: %p\n", translate_virtual_to_physcial(_start));

  uint64_t init_start = locate_module(hdr, "init");

  kprint_f("init_start: %x\n", init_start);
  // enables syscalls?
  setup_syscall();

  exec(init_start);

	halt();
}