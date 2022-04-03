#include "mem.h"

#define PAGE_SIZE 0x1000

// a page table entry
typedef struct pt_entry {
  bool present : 1;
  bool writable : 1;
  bool user : 1;
  bool write_through : 1;
  bool cache_disable : 1;
  bool accessed : 1;
  bool dirty : 1;
  bool page_size : 1;
  uint8_t _unused0 : 4;
  uintptr_t address : 40;
  uint16_t _unused1 : 11;
  bool no_execute : 1;
} __attribute__((packed)) pt_entry_t;

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

uintptr_t translate_physical_to_virtual(void* address) {
  uint64_t address_int = (uint64_t) address;
  return address_int + hhdm_base;
}

uintptr_t translate_virtual_to_physcial(void* address) {

  uintptr_t root = get_top_table();

  pt_entry_t* table = (pt_entry_t*) (root + hhdm_base);

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
    pt_entry_t* curr_entry = (pt_entry_t*) (table + indices[i]);

    if (!curr_entry->present) {
      isFound = false;
      break;
    }

    // advance pointer to next level of table
    table = (pt_entry_t*) (curr_entry->address << 12);
  }

  return (isFound) ? (uintptr_t) table + indices[0] + offset : 0;
}

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

  pt_entry_t* table = (pt_entry_t*) (root + hhdm_base);

  uint64_t offset = address & 0xFFF;
  uint16_t indices[] = {
    (address >> 12) & 0x1FF,
    (address >> 21) & 0x1FF,
    (address >> 30) & 0x1FF,
    (address >> 39) & 0x1FF // The fourth (highest) table index
  };

  pt_entry_t* curr_entry = NULL;

  // Traverse down the virtual address to level 1
  for (int i = 3; i >= 1; i--) {

    curr_entry = (pt_entry_t*) (table + indices[i]);

    if (curr_entry->present) {
      table = (pt_entry_t*) (curr_entry->address << 12);
    } else {
      // Make a pt_entry_t on the current level, set to present
      curr_entry->present = 1;
      curr_entry->user = 1;
      curr_entry->writable = 1;
      curr_entry->no_execute = 0;

      // Make a page table on the below level and initialize it to all not presents
      uintptr_t newly_created_table = pmem_alloc();

      // We have no more physical memory left! we must fail the mapping
      if (newly_created_table == 0) {
        return false;
      }

      // Set the table to all 0s
      memset(newly_created_table + hhdm_base, 0, 4096);

      // Make our current pt_entry_t point to this newly created table
      curr_entry->address = newly_created_table >> 12;
      table = (curr_entry->address << 12) + hhdm_base;
    }
  }

  pt_entry_t* dest = (table + indices[0]);
  
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

  pt_entry_t* table = (pt_entry_t*) (root + hhdm_base);

  uint64_t offset = address & 0xFFF;
  uint16_t indices[] = {
    (address >> 12) & 0x1FF,
    (address >> 21) & 0x1FF,
    (address >> 30) & 0x1FF,
    (address >> 39) & 0x1FF
  };

  pt_entry_t* curr_entry = NULL;

  for (int i = 3; i >= 1; i--) {

    curr_entry = (pt_entry_t*) (table + indices[i]);

    if (curr_entry->present) {
      table = (pt_entry_t*) (curr_entry->address << 12);
    } else {
      return false;
    }
  }

  pt_entry_t* bottom_entry = (pt_entry_t*) (table + indices[0]);

  if (bottom_entry->present) {
    uintptr_t virtual_to_free = table + indices[0];
    uintptr_t physical_to_free = virtual_to_free - hhdm_base;
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

  pt_entry_t* table = (pt_entry_t*) (root + hhdm_base);

  uint64_t offset = address & 0xFFF;
  uint16_t indices[] = {
    (address >> 12) & 0x1FF,
    (address >> 21) & 0x1FF,
    (address >> 30) & 0x1FF,
    (address >> 39) & 0x1FF
  };

  pt_entry_t* curr_entry = NULL;

  for (int i = 3; i >= 1; i--) {

    curr_entry = (pt_entry_t*) (table + indices[i]);

    if (curr_entry->present) {
      table = (pt_entry_t*) (curr_entry->address << 12);
    } else {
      return false;
    }
  }

  pt_entry_t* bottom_entry = (pt_entry_t*) (table + indices[0]);

  if (bottom_entry->present) {
    bottom_entry->user = user;
    bottom_entry->writable = writable;
    bottom_entry->no_execute = !executable;
    return true;
  }

  return false;
}