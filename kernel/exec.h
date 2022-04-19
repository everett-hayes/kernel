#pragma once

#include "mem.h"
#include "usermode_entry.h"
#include "stivale2.h"
#include "string.h"
#include "elf.h"
#include "kprint.h"
#include "gdt.h"

#include "stddef.h"
#include "stdint.h"

void exec_setup();
uint64_t locate_module(char* module_name);
void exec(uintptr_t elf_address);