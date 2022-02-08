#include <stdint.h>
#include <stddef.h>

#include "util.h"
#include "exception.h"
#include "kprint.h"

__attribute__((interrupt))
void interrupt_handler(interrupt_context_t* ctx) {
  kprint_f("example interrupt handler\n");
  halt();
}

__attribute__((interrupt))
void interrupt_handler_ec(interrupt_context_t* ctx, uint64_t ec) {
  kprint_f("example interrupt handler (ec=%d)\n", ec);
  halt();
}

void idt_set_handler(uint8_t index, void* fn, uint8_t type) {

  idt[index].type = type;
  idt[index].ist = 0;
  idt[index].present = 1;
  idt[index].dpl = 0;
  idt[index].selector = IDT_CODE_SELECTOR;

  // fn has 8 bytes -> 64 bits
  // offset_0 gets the first 16 bits
  idt[index].offset_0 = (uint64_t) fn & 0xFFFF;

  // offset_1 gets the second 16 bits
  idt[index].offset_1 = ((uint64_t) fn & 0xFFFF0000) >> 16;

  // offset_2 gets the last 32 bits
  idt[index].offset_2 = ((uint64_t) fn & 0xFFFFFFFF00000000) >> 32;
}

// This struct is used to load an IDT once we've set it up
typedef struct idt_record {
  uint16_t size;
  void* base;
} __attribute__((packed)) idt_record_t;

   
// The  memset() function fills the first n bytes of the memory area pointed to by s with the constant byte c.
void* memset(void* ptr, int c, size_t n) {
  unsigned char* curr = ptr;
  int i = 0;

  while (i < n) {
    *curr = (unsigned char) c;
    curr++;
    i++;
  }

  return ptr;
}

/**
 * Initialize an interrupt descriptor table, set handlers for standard exceptions, and install
 * the IDT.
 */
void idt_setup() {

  // Step 1: Zero out the IDT, probably using memset (which you'll have to implement)
  memset(idt, 0, 256 * sizeof(idt_entry_t));
  
  // Step 2: Use idt_set_handler() to set handlers for the standard exceptions (1--21)
  idt_set_handler(1, interrupt_handler, IDT_TYPE_TRAP);
  idt_set_handler(2, interrupt_handler, IDT_TYPE_INTERRUPT);
  idt_set_handler(3, interrupt_handler, IDT_TYPE_TRAP);
  idt_set_handler(4, interrupt_handler, IDT_TYPE_TRAP);
  idt_set_handler(5, interrupt_handler, IDT_TYPE_TRAP);
  idt_set_handler(6, interrupt_handler, IDT_TYPE_TRAP);
  idt_set_handler(7, interrupt_handler, IDT_TYPE_TRAP);
  idt_set_handler(8, interrupt_handler_ec, IDT_TYPE_TRAP);
  idt_set_handler(9, interrupt_handler, IDT_TYPE_TRAP);
  idt_set_handler(10, interrupt_handler_ec, IDT_TYPE_TRAP);
  idt_set_handler(11, interrupt_handler_ec, IDT_TYPE_TRAP);
  idt_set_handler(12, interrupt_handler_ec, IDT_TYPE_TRAP);
  idt_set_handler(13, interrupt_handler_ec, IDT_TYPE_TRAP);
  idt_set_handler(14, interrupt_handler_ec, IDT_TYPE_TRAP);
  idt_set_handler(15, interrupt_handler, IDT_TYPE_TRAP);
  idt_set_handler(16, interrupt_handler, IDT_TYPE_TRAP);
  idt_set_handler(17, interrupt_handler_ec, IDT_TYPE_TRAP);
  idt_set_handler(18, interrupt_handler, IDT_TYPE_TRAP);
  idt_set_handler(19, interrupt_handler, IDT_TYPE_TRAP);
  idt_set_handler(20, interrupt_handler, IDT_TYPE_TRAP);
  idt_set_handler(21, interrupt_handler_ec, IDT_TYPE_TRAP);

  // Step 3: Install the IDT
  idt_record_t record = {
    .size = sizeof(idt),
    .base = idt
  };

  __asm__("lidt %0" :: "m"(record));
}