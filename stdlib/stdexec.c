#include "stdexec.h"

#define SYS_EXEC 3

uint64_t syscall(uint64_t num, ...);

int exec(char* module_name) {
    return (int) syscall(SYS_EXEC, module_name);
}