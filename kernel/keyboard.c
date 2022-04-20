#include <stddef.h>

#include "keyboard.h"
#include "port.h"
#include "pic.h"
#include "kprint.h"


#define circ_buffer_len 10

// citation: https://gist.github.com/davazp/d2fde634503b2a5bc664
char kbd_US [128] =
{
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',   
  '\t', /* <-- Tab */
  'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',     
    0, /* <-- control key */
  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',  0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',   0,
  '*',
    0,  /* Alt */
  ' ',  /* Space bar */
    0,  /* Caps lock */
    0,  /* 59 - F1 key ... > */
    0,   0,   0,   0,   0,   0,   0,   0,
    0,  /* < ... F10 */
    0,  /* 69 - Num lock*/
    0,  /* Scroll Lock */
    0,  /* Home key */
    0,  /* Up Arrow */
    0,  /* Page Up */
  '-',
    0,  /* Left Arrow */
    0,
    0,  /* Right Arrow */
    '+',
    0,
    0,
    0,  /* Page Down */
    0,  /* Insert Key */
    0,  /* Delete Key */
    0,   0,   0,
    0,  /* F11 Key */
    0,  /* F12 Key */
    0,  /* All other keys are undefined */
};


int circ_buffer[circ_buffer_len];     // N elements circular buffer
int end = 0;    // write index
int start = 0;  // read index
volatile size_t buffer_count = 0; // current capacity
int leftShiftIsPressed = 0;
int rightShiftIsPressed = 0;

void write(int item) {
  circ_buffer[end++] = item;
  buffer_count++;
  end %= circ_buffer_len;
}

int read() {
  int item = circ_buffer[start++];
  start %= circ_buffer_len;
  buffer_count--;
  return item;
}

int isNumeric(int key) {
  return (key >= 2 && key <= 11);
}

int isAlpha(int key) {
  return (key >= 16 && key <= 25) || (key >= 30 && key <= 38) || (key >= 44 && key <= 50);
}

int isSpecial(int key) {
  return (key == 57 || key == 14 || key == 28 || key == 39);
}

__attribute__((interrupt))
void keypress_handler(interrupt_context_t* ctx) {

  uint8_t val = inb(0x60);

  if (val == 0x2A) {
    leftShiftIsPressed = 1;
  } else if (val == 0xAA) {
    leftShiftIsPressed = 0;
  }

  if (val == 0x36) {
    rightShiftIsPressed = 1;
  } else if (val == 0xB6) {
    rightShiftIsPressed = 0;
  }

  if (isNumeric(val) || isAlpha(val) || isSpecial(val)) {
    write(val);
  }

  outb(PIC1_COMMAND, PIC_EOI);
}

/**
 * Read one character from the keyboard buffer. If the keyboard buffer is empty this function will
 * block until a key is pressed.
 *
 * \returns the next character input from the keyboard
 */
char kgetc() {

  // spin until there is something to read
  while (buffer_count == 0) {}

  int key = read();
  char ch = kbd_US[key];

  // check whether to upper case the letter
  if ((leftShiftIsPressed || rightShiftIsPressed) && isAlpha(key)) {
    ch -= 32;
  }

  return ch;
}