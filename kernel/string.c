#include <stddef.h>

size_t strlen(const char* str) {
    size_t count = 0;

    while (*str != '\0') {
        count++;
        str++;
    }

    return count;
}

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