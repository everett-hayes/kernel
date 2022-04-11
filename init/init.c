#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "stdio.h"
#include "stdmem.h"

void _start() {

  write(1, "Hello\n", 6);

  printf("we dem bois\n");

  printf("i am %d years old and my name is %s\n", 15, "johnny");

  typedef struct node {
    int num;
  } node_t;

  node_t* pointer = (node_t*) malloc(sizeof(node_t));

  pointer->num = 4;

  printf("%d\n", pointer->num);

  // Loop forever
  for(;;){}
}
