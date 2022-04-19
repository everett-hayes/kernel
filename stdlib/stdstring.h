#pragma once

#include "stddef.h"
#include "stdint.h"
#include "stdbool.h"

int strcmp(const char* str1, const char* str2);
char* strtok_r(char* s, char* delims, char** save_ptr);