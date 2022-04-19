#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "stdio.h"
#include "stdmem.h"
#include "stdexec.h"

void runShell();
void parseLine(char* cmd);
char* strtok_r(char* s, char* delims, char** save_ptr);
int strcmp(const char* str1, const char* str2);

void _start() {

  runShell();

  // Loop forever
  for(;;){}
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
    printf("\nI am about to try exec from the shell!! on %s\n", args[1]);
    int result = exec(args[1]);
    printf("Finished exec with result %d\n", result);
  } else {
    printf("\nI am not exec :(\n");
  }
}

int strcmp(const char* str1, const char* str2) {
    while(*str1) {
        if (*str1 != *str2) {
            break;
        }

        str1++;
        str2++;
    }

    return *(const unsigned char*)str1 - *(const unsigned char*)str2;
}

// The strspn() function spans the initial part of the null-terminated string s as long as the
//      characters from s occur in the null-terminated string charset.  In other words, it computes
//      the string array index of the first character of s which is not in charset, else the index of
//      the first null character.
size_t strspn(char* s, char* delims) {

  size_t i;
  size_t j;

  for (i = 0; s[i] != '\0'; i++) {

    bool notDelim = true;
    for (j = 0; delims[j] != '\0'; j++) {
      if (s[i] == delims[j]) {
        notDelim = false;
        break;
      }
    }

    if (notDelim) {
      return i;
    }
  }

  return i;
}

// The strcspn() function spans the initial part of the null-terminated string s as long as the
//      characters from s do not occur in the null-terminated string charset (it spans the complement
//      of charset).  In other words, it computes the string array index of the first character of s
//      which is also in charset, else the index of the first null character.
size_t strcspn(char* s, char* delims) { 

  size_t i;
  size_t j;

  for (i = 0; s[i] != '\0'; i++) {
    for (j = 0; delims[j] != '\0'; j++) {
      if (s[i] == delims[j]) {
        return i;
      }
    }
  }

  return i;
}


/*
    return a pointer to the beginning of each subsequent
    token in the string, after replacing the token itself with a NUL character.  When no more
    tokens remain, a null pointer is returned.

    s is the input string
    delims are the characters to be delimed by (null terminated)
*/
char* strtok_r(char* s, char* delims, char** save_ptr) {

  char* end;

  if (s == NULL) {
    s = *save_ptr;
  }

  if (*s == '\0') {
    *save_ptr = s;
    return NULL;
  }
  
  /* Scan leading delimiters.  */
  s += strspn(s, delims);
  if (*s == '\0') {
    *save_ptr = s;
    return NULL;
  }

  /* Find the end of the token.  */
  end = s + strcspn(s, delims);
  if (*end == '\0') {
    *save_ptr = end;
    return s;
  }
  /* Terminate the token and make *SAVE_PTR point past it.  */
  *end = '\0';
  *save_ptr = end + 1;
  return s;
}




