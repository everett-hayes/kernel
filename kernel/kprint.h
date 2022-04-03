#pragma once

#include "string.h"
#include "term.h"

#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>

// it's basically printf
void kprint_f(const char* format, ...);