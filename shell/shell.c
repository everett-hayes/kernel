#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "stdio.h"
#include "stdmem.h"

void _start() {

  char* line = NULL;
  size_t* line_size = 0;

  // Loop forever
  while (true) {
    char ch;
    read(0, &ch, 1);
    printf("%c", ch);
  }

  // Loop forever
  for(;;){}
}
