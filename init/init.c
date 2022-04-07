#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "stdio.h"

void _start() {

  write(1, "Hello\n", 6);

  printf("we dem bois\n");

  printf("i am %d years old and my name is %s\n", 15, "johnny");

  // Loop forever
  for(;;){}
}
