#include "kprint.h"

#define DECIMAL 10
#define HEXADECIMAL 16

term_write_t term_write = NULL;

void set_term_write(term_write_t fn) {
    term_write = fn;
}

// Return the number of digits of a value with respect to a base
int digit_len(uint64_t num, int base) {
  int i = 0;

  if (num == 0) {
    return 1;
  }

  while (num != 0) {
    i++;
    num /= base;
  }

  return i;
}

// Reverse a string in-place
void reverse(char* str, int len) {
  int low = 0;
  int high = len - 1;

  while (low < high) {
    char temp = str[low];
    str[low++] = str[high];
    str[high--] = temp;
  }
}

// Print a single character to the terminal
void kprint_c(char c) {
  term_putchar(c);
}

// Print a string to the terminal
void kprint_s(const char* str) {

  int len = strlen(str);

  for (int i = 0; i < len; i++) {
    term_putchar(str[i]);
  }
}

void kprint_num(uint64_t value, int base) {

  // + 1 for the null terminator
  char arr[digit_len(value, base) + 1];
  int i = 0;

  if (value == 0) {
    arr[i++] = '0';
  }

  while (value != 0)  {
    int digit = value % base;
    char ch = (digit > 9) ? digit - 10 + 'a' : digit + '0';
    arr[i++] = ch; 
    value /= base;
  }

  reverse(arr, i);
  arr[i] = '\0';
  kprint_s(arr);
}

// Print an unsigned 64-bit integer value to the terminal in decimal notation
void kprint_d(uint64_t value) {
  kprint_num(value, DECIMAL);
}
// Print an unsigned 64-bit integer value to the terminal in lowercase hexadecimal notation (no leading zeros or “0x” please!)
void kprint_x(uint64_t value) {
  kprint_num(value, HEXADECIMAL);
}

// Print the value of a pointer to the terminal in lowercase hexadecimal with the prefix “0x”
void kprint_p(void* ptr) { 
  kprint_s("0x");
  kprint_x((uint64_t) ptr);
}

void kprint_f(const char* format, ...) {
  const char* pos = format;
  va_list ap;

  va_start(ap, format);

  while (*pos != '\0') {

    if (*pos == '%') {
      pos++;

      switch (*pos) {
        case 'c':
          kprint_c(va_arg(ap, int));
          break;
        case 's':
          kprint_s(va_arg(ap, const char*));
          break;
        case 'd':
          kprint_d(va_arg(ap, uint64_t));
          break;
        case 'x':
          kprint_x(va_arg(ap, uint64_t));
          break;
        case 'p':
          kprint_p(va_arg(ap, void*));
          break;
        case '%':
          kprint_c('%');
          break;
        default:
          kprint_c('?');
      }
    } else {
      kprint_c(*pos);
    }

    pos++;
  }

  va_end(ap);
}