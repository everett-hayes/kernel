#pragma once

#include "stddef.h"
#include <stdarg.h>
#include "stdint.h"

size_t read(int fd, void* buf, size_t count);
size_t write(int fd, void *buf, size_t count);
void printf(const char* format, ...);
uint64_t exit();