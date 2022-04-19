#include "stdstring.h"

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
