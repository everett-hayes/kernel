#include "stdexec.h"

#define SYS_EXEC 3

int exec(char* module_name) {
    return (int) syscall(SYS_EXEC, module_name);
}