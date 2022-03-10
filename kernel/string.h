#pragma once

#include <stddef.h>

size_t strlen(const char* str);
char* strcpy(char* dest, const char* src);
int strcmp(const char* str1, const char* str2);
char* strtok(char* str, const char* delim);
char* strpbrk(const char* str1, const char* str2);