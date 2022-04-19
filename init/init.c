#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "stdio.h"
#include "stdmem.h"

void _start() {

  printf("I am inside the init program :) which has been called from the shell\n");

  typedef struct node {
    int num;
  } node_t;

  node_t* pointer = (node_t*) malloc(sizeof(node_t));

  printf("sample malloc result %p\n", pointer);

  pointer->num = 4;

  printf("result of writing to the malloc'ed pointer %d\n", pointer->num);

  exit();
}
