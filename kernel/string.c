#include <stddef.h>

// get length of null terminated string
size_t strlen(const char* str) {
    size_t count = 0;

    while (*str != '\0') {
        count++;
        str++;
    }

    return count;
}

// copy a null terminated string from src to dest
char* strcpy(char* dest, const char* src) {

    if (dest == NULL) {
        return NULL;
    }

    char* ptr = dest;

    while (*src != '\0') {
        *dest = *src;
        dest++;
        src++;
    }

    *dest = '\0';

    return ptr;
}

// compare str1 and str2, returns 0 on a match
int strcmp(const char* str1, const char* str2) {

    while( *str1) {

        if (*str1 != *str2) {
            break;
        }

        str1++;
        str2++;
    }

    return *(const unsigned char*)str1 - *(const unsigned char*)str2;
}