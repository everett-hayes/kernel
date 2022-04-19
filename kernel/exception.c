#include <stdint.h>
#include <stddef.h>

#include "util.h"
#include "exception.h"
#include "kprint.h"
#include "mem.h"

// Make an IDT
idt_entry_t idt[256];

// handles all interrupts that don't have an error code
__attribute__((interrupt))
void interrupt_handler(interrupt_context_t* ctx) {
  kprint_f("generic interrupt handler\n");
  halt();
}

// handles all interrupts with an error code
__attribute__((interrupt))
void interrupt_handler_ec(interrupt_context_t* ctx, uint64_t ec) {

  switch (ec) {
    case 0:
      kprint_f("Division Error (ec=%d)\n", ec);
      break;
    case 1:
      kprint_f("Debug Interrupt (ec=%d)\n", ec);
      break;  
    case 2:
      kprint_f("NMI Interrupt (ec=%d)\n", ec);
      break;
    case 3:
      kprint_f("Breakpoint Interrupt (ec=%d)\n", ec);
      break;
    case 4:
      kprint_f("Overflow Interrupt (ec=%d)\n", ec);
      break;
    case 5:
      kprint_f("Bound Range Exceeded Interrupt (ec=%d)\n", ec);
      break;
    case 6:
      kprint_f("Invalid Opcode Interrupt (ec=%d)\n", ec);
      break;
    case 7:
      kprint_f("Device Not Available (No Math Coprocessor) Interrupt (ec=%d)\n", ec);
      break;
    case 8:
      kprint_f("Double Fault Interrupt (ec=%d)\n", ec);
      break;
    case 9:
      kprint_f("CoProcessor Segment Overrun Interrupt (ec=%d)\n", ec);
      break;
    case 10:
      kprint_f("Invalid TSS Interrupt (ec=%d)\n", ec);
      break;
    case 11:
      kprint_f("Segment Not Present Interrupt (ec=%d)\n", ec);
      break;
    case 12:
      kprint_f("Stack Segment Fault Interrupt (ec=%d)\n", ec);
      break;
    case 13:
      kprint_f("General Protection Interrupt (ec=%d)\n", ec);
      break;
    case 14:
      kprint_f("Page Fault Interrupt (ec=%d)\n", ec);
      break;
    case 16:
      kprint_f("Floating-Point Error Interrupt (ec=%d)\n", ec);
      break;
    case 17:
      kprint_f("Alignment Check Interrupt (ec=%d)\n", ec);
      break;
    case 18:
      kprint_f("Machine Check Interrupt (ec=%d)\n", ec);
      break;
    case 19:
      kprint_f("SIMD Floating-Point Exception Interrupt (ec=%d)\n", ec);
      break;
    case 20:
      kprint_f("Virtualization Exception Interrupt (ec=%d)\n", ec);
      break;
    case 21:
      kprint_f("Control Protection Exception Interrupt (ec=%d)\n", ec);
      break;
    default:
      kprint_f("Unknown Interrupt (ec=%d)\n", ec);
      break;
  }
  
  halt();
}

void idt_set_handler(uint8_t index, void* fn, uint8_t type) {

  idt[index].type = type;
  idt[index].ist = 0;
  idt[index].present = 1;
  idt[index].dpl = 3; // 0 IDT_CODE_SELECTOR
  idt[index].selector = KERNEL_CODE_SELECTOR; // IDT_CODE_SELECTOR

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