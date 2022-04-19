#include "exec.h"

struct stivale2_struct_tag_modules* tag = NULL;

void exec_setup(struct stivale2_struct_tag_modules* param_tag) {
    tag = param_tag;
}

uint64_t locate_module(char* module_name) {

  for (int i = 0; i < tag->module_count; i++) {
    struct stivale2_module* temp = &(tag->modules[i]);

    if (strcmp(temp->string, module_name) == 0) {
      return temp->begin;
    }
  }

  return -1;
}

void exec(uintptr_t elf_address) {

  unmap_lower_half();

  elf64_hdr_t* header = (elf64_hdr_t*) elf_address;

  elf64_prg_hdr_t* prg_header = (elf64_prg_hdr_t*) (elf_address + header->e_phoff);

  int ph_size = header->e_phentsize;
  int ph_num = header->e_phnum;

  uintptr_t root = get_top_table();
  uintptr_t temp = prg_header;

  for (int i = 0; i < ph_num; i++) {
    elf64_prg_hdr_t* prg_header_curr = (elf64_prg_hdr_t*) (temp);

    if (prg_header_curr->p_type == 1 && prg_header_curr->p_memsz > 0) {
      // kprint_f("trying to map at %p\n", prg_header_curr->p_vaddr);
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
}