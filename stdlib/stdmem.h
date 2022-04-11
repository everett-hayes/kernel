#pragma once

#include "stddef.h"
#include "stdint.h"

uint64_t syscall(uint64_t num, ...);
void* memset(void* ptr, int c, size_t n);
void* memcpy(void* dest, const void* src, size_t size);
void* malloc(size_t sz);