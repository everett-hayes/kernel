#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "stdio.h"

void _start() {

  write(1, "Hello\n", 6);

  printf("we dem bois\n");

  printf("i am %d years old and my name is %s\n", 15, "johnny");

  typedef struct node {
    int num;
  } node_t;

  int temper = sizeof(node_t);
  printf("the size of node_t is %d\n", temper);

  // uintptr_t address = (uintptr_t) malloc(sizeof(node_t));

  printf("%p\n", (uint64_t) malloc(sizeof(node_t)));

  // node_t* temp = (node_t*) malloc(sizeof(node_t));

  // printf("%p\n", temp);

  // temp->num = 4;

  // printf("num: %d\n", temp->num);

  // Loop forever
  for(;;){}
}
