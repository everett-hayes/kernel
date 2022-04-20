#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "stdio.h"
#include "stdmem.h"
#include "stdexec.h"
#include "stdstring.h"

void runShell();
void parseLine(char* cmd);

void _start() {
  runShell();
}

void runShell() {
  char* line = malloc(sizeof(char) * 256);
  int pos = 0;

  while (true) {
    printf("$ ");

    char ch;
    read(0, &ch, 1);

    while (ch != '\n' && ch != '\0') {
      line[pos++] = ch; 
      printf("%c", ch);
      read(0, &ch, 1);
    }

    line[pos] = '\0';

    parseLine(line);

    free(line);
    line = malloc(sizeof(char) * 256);
    pos = 0;
  }
}

void parseLine(char* cmd) {

  int MAX_ARGS = 3;

  char* args [MAX_ARGS + 1];

  int i = 0;
  
  char* saveptr;
  char* ptr = strtok_r(cmd, " \n\0", &saveptr);

  // split line into args 
  while (ptr != NULL) {
    args[i++] = ptr;
    ptr = strtok_r(NULL, " \n\0", &saveptr);
  }

  if (strcmp(args[0], "exec") == 0) {
    printf("\n");
    exec(args[1]);
  } else {
    printf("\nunrecognized command: %s\n", args[0]);
  }
}




