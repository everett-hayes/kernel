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

#define SYS_WRITE 0
#define SYS_READ 1
#define SYS_MMAP 2

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

// void term_setup(struct stivale2_struct* hdr) {
//   // Look for a terminal tag
//   struct stivale2_struct_tag_terminal* tag = find_tag(hdr, STIVALE2_STRUCT_TAG_TERMINAL_ID);

//   // Make sure we find a terminal tag
//   if (tag == NULL) halt();

//   // Save the term_write function pointer
//   set_term_write((term_write_t)tag->term_write);
// }

void pic_setup() {
  pic_init();
  idt_set_handler(IRQ1_INTERRUPT, keypress_handler, IDT_TYPE_INTERRUPT);
  pic_unmask_irq(1);
}

uint64_t read_cr0() {
  uintptr_t value;
  __asm__("mov %%cr0, %0" : "=r" (value));
  return value;
}

void write_cr0(uint64_t value) {
  __asm__("mov %0, %%cr0" : : "r" (value));
}

size_t syscall_read(int fd, void* buf, size_t count) {

  if (fd != 0) {
    return -1;
  }

  char* char_buf = (char*) buf; 

  for (int i = 0; i < count; i++) {
    char val = kgetc(); // need to check if this is a backspace

    // MAKE SURE WE DON"T BACKSPACE TOO MUCH !!!!! TODO
    if (val == '\b') {
      *char_buf = '\0';
      char_buf -= 1; // move address backward 1 byte
      i -= 2; // don't count the backspace or the eaten char
    } else {
      *char_buf = val;
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

uint64_t malloc_pointer = 0;

uint64_t syscall_memmap(uintptr_t address, bool user, bool writable, bool executable, size_t length) {

  // if the bottom pointer is not yet created: we need to make it
  if (malloc_pointer == 0) {
    malloc_pointer = get_hhdm_base() + 0x1000;
  }

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

  uint64_t temper;

  switch (num) {
    case 0:
      syscall_write(arg0, arg1, arg2);
      break;
    case 1:
      syscall_read(arg0, arg1, arg2);
      break;
    case 2:
      return syscall_memmap(arg0, arg1, arg2, arg3, arg4);
      break;
    default:
      kprint_f("you've called a syscall that doesn't exist!!\n");
  }
  return num;
}

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
  idt_set_handler(0x80, syscall_entry, IDT_TYPE_TRAP);

  exec(init_start);

	halt();
}