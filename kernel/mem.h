#pragma once

#include "stivale2.h"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

void* memset(void* ptr, int c, size_t n);
void* memcpy(void* dest, const void* src, size_t size);
uint64_t get_hhdm_base();
uintptr_t get_top_table();
void initialize_memory(struct stivale2_struct_tag_memmap* physical_tag, struct stivale2_struct_tag_hhdm* virtual_tag);
uintptr_t pmem_alloc();
void pmem_free(uintptr_t p);
uintptr_t ptov(void* address);
uintptr_t translate_virtual_to_physcial(void* address);
bool vm_map(uintptr_t root, uintptr_t address, bool user, bool writable, bool executable);
bool vm_unmap(uintptr_t root, uintptr_t address);
bool vm_protect(uintptr_t root, uintptr_t address, bool user, bool writable, bool executable);
void unmap_lower_half();