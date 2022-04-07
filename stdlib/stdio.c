#include "stdio.h"

#define SYS_WRITE 0
#define SYS_READ 1

// read (a wrapper around the syscall invocation
size_t read(int fd, void* buf, size_t count) {
    return syscall(SYS_READ, fd, buf, count);
}

// write (a wrapper around the syscall invocation)
size_t write(int fd, void *buf, size_t count) {
    return syscall(SYS_WRITE, fd, buf, count);
}

size_t strlen(const char* str) {
    size_t count = 0;

    while(*str != '\0') {
        count++;
        str++;
    }

    return count;
}

// Print a string to the terminal
void print_s(const char* str) {

  int len = strlen(str);

  write(1, str, len);
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

void print_num(uint64_t value, int base) {

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
  print_s(arr);
}

void printf(const char* format, ...) {

    // va_arg stuff
    va_list ap;
    va_start(ap, format);

    const char* pos = format;

    // iterate through the format string
    while (*pos != '\0') {

        if (*pos == '%') {

            pos++;

            switch (*pos) {
                case 'c':
                    write(1, pos, 1);
                    break;
                case 's':
                    print_s(va_arg(ap, const char*));
                    break;
                case 'd':
                    print_num(va_arg(ap, uint64_t), 10);
                    break;
                case 'x':
                    print_num(va_arg(ap, uint64_t), 16);
                    break;
                case 'p':
                    write(1, "0x", 2);
                    print_num(va_arg(ap, uint64_t), 16);
                    break;
                case '%':
                    write(1, "%", 1);
                    break;
                default:
                    write(1, "?", 1);
                    break;
            }
        } else {
            write(1, pos, 1);
        }

        pos++;
    }
}

