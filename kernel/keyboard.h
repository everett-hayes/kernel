#pragma once

#include "exception.h"

// available for the setup in boot
void keypress_handler(interrupt_context_t* ctx);

/**
 * Read one character from the keyboard buffer. If the keyboard buffer is empty this function will
 * block until a key is pressed.
 *
 * \returns the next character input from the keyboard
 */
char kgetc();