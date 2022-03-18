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

#define SYS_WRITE 0
#define SYS_READ 1

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

  pt_entry* table = (pt_entry*) (root + get_hhdm_base());

  uint64_t offset = address & 0xFFF;
  uint16_t indices[] = {
    (address >> 12) & 0x1FF,
    (address >> 21) & 0x1FF,
    (address >> 30) & 0x1FF,
    (address >> 39) & 0x1FF // The fourth (highest) table index
  };

  pt_entry* curr_entry = NULL;

  // Traverse down the virtual address to level 1
  for (int i = 3; i >= 1; i--) {
    uint16_t index = indices[i];
    curr_entry = (pt_entry*) (table + index);
    // kprint_f("Level %d (index %d of %p)\n", i + 1, indices[i], table);

    if (curr_entry->present) {
      // Change curr_entry and continue
      table = (pt_entry*) (curr_entry->address << 12);
    } else {
      // Make a pt_entry on the current level, set to present
      curr_entry->present = 1;
      curr_entry->user = 1;
      curr_entry->writable = 1;
      curr_entry->no_execute = 0;

      // Make a page table on the below level and initialize it to all not presents
      uintptr_t newly_created_table = pmem_alloc();
      // kprint_f("newly_created_table is %p\n", newly_created_table);

      // We have no more physical memory left! we must fail the mapping
      if (newly_created_table == 0) {
        return false;
      }

      // Set the table to all 0s
      memset(newly_created_table + get_hhdm_base(), 0, 4096);
      // kprint_f("newly_created_table is %p\n", newly_created_table);

      // Make our current pt_entry point to this newly created table
      curr_entry->address = newly_created_table >> 12;
      table = (curr_entry->address << 12) + get_hhdm_base();
    }
  }

  pt_entry* dest = (table + indices[0]);
  // kprint_f("the curr_entry->address is %p\n", (curr_entry->address << 12));
  // kprint_f("Level %d (index %d of %p)\n", 1, indices[0], dest);
  
  dest->address = pmem_alloc() >> 12;
  dest->present = 1;
  dest->user = user;
  dest->writable = writable;
  dest->no_execute = !executable;

 return true;
}

/**
 * Unmap a page from a virtual address space
 * \param root The physical address of the top-level page table structure
 * \param address The virtual address to unmap from the address space
 * \returns true if successful, or false if anything goes wrong
 */
bool vm_unmap(uintptr_t root, uintptr_t address) {

  pt_entry* table = (pt_entry*) (root + get_hhdm_base());

  uint64_t offset = address & 0xFFF;
  uint16_t indices[] = {
    (address >> 12) & 0x1FF,
    (address >> 21) & 0x1FF,
    (address >> 30) & 0x1FF,
    (address >> 39) & 0x1FF
  };

  pt_entry* curr_entry = NULL;

  for (int i = 3; i >= 1; i--) {
    uint16_t index = indices[i];
    curr_entry = (pt_entry*) (table + index);

    kprint_f("Level %d (index %d of %p)\n", i + 1, indices[i], table);

    if (curr_entry->present) {
      table = (pt_entry*) (curr_entry->address << 12);
    } else {
      return false;
    }
  }

  if (curr_entry->present) {
    uintptr_t virtual_to_free = curr_entry->address << 12;
    uintptr_t physical_to_free = virtual_to_free - get_hhdm_base();
    pmem_free(physical_to_free);
    return true;
  } 
  return false;
}

/**
 * Change the protections for a page in a virtual address space
 * \param root The physical address of the top-level page table structure
 * \param address The virtual address to update
 * \param user Should the page be user-accessible or kernel only?
 * \param writable Should the page be writable?
 * \param executable Should the page be executable?
 * \returns true if successful, or false if anything goes wrong (e.g. page is not mapped)
 */
bool vm_protect(uintptr_t root, uintptr_t address, bool user, bool writable, bool executable) {
  pt_entry* table = (pt_entry*) (root + get_hhdm_base());

  uint64_t offset = address & 0xFFF;
  uint16_t indices[] = {
    (address >> 12) & 0x1FF,
    (address >> 21) & 0x1FF,
    (address >> 30) & 0x1FF,
    (address >> 39) & 0x1FF
  };

  pt_entry* curr_entry = NULL;

  for (int i = 3; i >= 1; i--) {
    uint16_t index = indices[i];
    curr_entry = (pt_entry*) (table + index);

    kprint_f("Level %d (index %d of %p)\n", i + 1, indices[i], table);

    if (curr_entry->present) {
      table = (pt_entry*) (curr_entry->address << 12);
    } else {
      return false;
    }
  }

  if (curr_entry->present) {
    curr_entry->user = user;
    curr_entry->writable = writable;
    curr_entry->no_execute = !executable;
    return true;
  } 

  return false;
}

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

// TODO
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


// No more arguments than 6!
uint64_t syscall(uint64_t num, ...);
void syscall_entry();

int64_t syscall_handler(uint64_t num, uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
  kprint_f("Hello world from the syscall!\n");

  switch (num) {
    case 0:
      syscall_write(arg0, arg1, arg2);
      break;
    case 1:
      syscall_read(arg0, arg1, arg2);
      break;
    default:
      kprint_f("you've done something very wrong\n");
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

  typedef void (*elf_exec_t)();
  elf_exec_t current_exe = (elf_exec_t) (header->e_entry);
  current_exe();

  kprint_f("elf mapped in memory!!!\n");
}

void _start(struct stivale2_struct* hdr) {
  // We've booted! Let's start processing tags passed to use from the bootloader
  term_setup(hdr);
  idt_setup();
  pic_setup();
  initialize_memory(find_tag(hdr, STIVALE2_STRUCT_TAG_MEMMAP_ID), find_tag(hdr, STIVALE2_STRUCT_TAG_HHDM_ID));

  kprint_f("%p\n", translate_virtual_to_physcial(_start));

  // uint64_t init_start = locate_module(hdr, "init");

  // kprint_f("init_start: %x\n", init_start);
  // idt_set_handler(0x80, syscall_entry, IDT_TYPE_TRAP);

  // exec(init_start);

	halt();
}