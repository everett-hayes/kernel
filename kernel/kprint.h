#pragma once

// it's basically printf
void kprint_f(const char* format, ...);

// the type of a term write
typedef void (*term_write_t)(const char*, size_t);

// set the terminal writing functino
void set_term_write(term_write_t fn);