#include <stdint.h>
#include <stddef.h>

#include "stivale2.h"
#include "util.h"
#include "kprint.h"
#include "pic.h"
#include "port.h"
#include "exception.h"

// Reserve space for the stack
static uint8_t stack[8192];

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
    0,  /* 79 - End key*/
    0,  /* Down Arrow */
    0,  /* Page Down */
    0,  /* Insert Key */
    0,  /* Delete Key */
    0,   0,   0,
    0,  /* F11 Key */
    0,  /* F12 Key */
    0,  /* All other keys are undefined */
};

// Request a terminal from the bootloader
static struct stivale2_header_tag_terminal terminal_hdr_tag = {
	.tag = {
    .identifier = STIVALE2_HEADER_TAG_TERMINAL_ID,
    .next = 0
  },
  .flags = 0
};

// Declare the header for the bootloader
__attribute__((section(".stivale2hdr"), used))
static struct stivale2_header stivale_hdr = {
  // Use ELF file's default entry point
  .entry_point = 0,

  // Use stack (starting at the top)
  .stack = (uintptr_t)stack + sizeof(stack),

  // Bit 1: request pointers in the higher half
  // Bit 2: enable protected memory ranges (specified in PHDR)
  // Bit 3: virtual kernel mappings (no constraints on physical memory)
  // Bit 4: required
  .flags = 0x1E,
  
  // First tag struct
  .tags = (uintptr_t)&terminal_hdr_tag
};

// Find a tag with a given ID
void* find_tag(struct stivale2_struct* hdr, uint64_t id) {
  // Start at the first tag
	struct stivale2_tag* current = (struct stivale2_tag*)hdr->tags;

  // Loop as long as there are more tags to examine
	while (current != NULL) {
    // Does the current tag match?
		if (current->identifier == id) {
			return current;
		}

    // Move to the next tag
		current = (struct stivale2_tag*)current->next;
	}

  // No matching tag found
	return NULL;
}

void term_setup(struct stivale2_struct* hdr) {
  // Look for a terminal tag
  struct stivale2_struct_tag_terminal* tag = find_tag(hdr, STIVALE2_STRUCT_TAG_TERMINAL_ID);

  // Make sure we find a terminal tag
  if (tag == NULL) halt();

  // Save the term_write function pointer
  set_term_write((term_write_t)tag->term_write);
}

void find_memory(struct stivale2_struct* hdr) {

  struct stivale2_struct_tag_memmap* physical_tag = find_tag(hdr, STIVALE2_STRUCT_TAG_MEMMAP_ID);
  struct stivale2_struct_tag_hhdm* virtual_tag = find_tag(hdr, STIVALE2_STRUCT_TAG_HHDM_ID);
  uint64_t hhdm_start = virtual_tag->addr;
  //kprint_f("HHDM start: %x\n", hhdm_start);

  kprint_f("Usable Memory:\n");

  for (int i = 0; i < physical_tag->entries; i++) {
    struct stivale2_mmap_entry entry = physical_tag->memmap[i];
    // Check whether memory is usable
    if (entry.type == 1) {
      uint64_t physical_start = entry.base;
      uint64_t physical_end = entry.base + entry.length;
      uint64_t virtual_start = physical_start + hhdm_start;
      uint64_t virtual_end = physical_end + hhdm_start;
      kprint_f("\t0x%x-0x%x mapped at 0x%x-0x%x\n", physical_start, physical_end, virtual_start, virtual_end);
    }
  }
}


#define circ_buffer_len 10
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
  return -1;
  // TODO: this
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

  if (isNumeric(val) || isAlpha(val)) {
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

  while (buffer_count == 0) {}

  int key = read();
  char ch = kbd_US[key];

  if ((leftShiftIsPressed || rightShiftIsPressed) && isAlpha(key)) {
    ch -= 32;
  }

  return ch;
}


void pic_setup() {
  pic_init();
  idt_set_handler(IRQ1_INTERRUPT, keypress_handler, IDT_TYPE_INTERRUPT);
  pic_unmask_irq(1);
}

typedef struct pt_entry {
  uint8_t present : 1;
  uint8_t writable : 1;
  uint8_t kernel : 1;
  uint16_t unused0 : 9;
  uint64_t address : 51;
  uint8_t no_execute : 1;
} __attribute__((packed)) pt_entry;

void translate(uintptr_t page_table, void* address) {

  uint64_t address_int = (uint64_t) address;

  // break the virtual address into pieces
  uint16_t offset = address_int & 0x0000000000000FFF;
  address_int >>= 12;

  uint16_t page_index_1 = address_int & 0x00000000000001FF;
  address_int >>= 9;

  uint16_t page_index_2 = address_int & 0x00000000000001FF;
  address_int >>= 9;

  uint16_t page_index_3 = address_int & 0x00000000000001FF;
  address_int >>= 9;

  uint16_t page_index_4 = address_int & 0x00000000000001FF;
  address_int >>= 9;

  pt_entry* level_four_entry = (pt_entry*) (page_table + (page_index_4 * 64));
  kprint_f("%x\n", level_four_entry);

  kprint_f("level 4 entry address: %p\n", level_four_entry->address);
  kprint_f("level 4 present: %d\n", level_four_entry->present);
  kprint_f("level 4 writable: %d\n", level_four_entry->writable);
  kprint_f("level 4 kernel: %d\n", level_four_entry->kernel);

  // pt_entry* level_three_entry = (pt_entry*) level_four_entry->address[level_three_index];
}

uintptr_t read_cr3() {
  uintptr_t value;
  __asm__("mov %%cr3, %0" : "=r" (value));
  return value;
}

void _start(struct stivale2_struct* hdr) {
  // We've booted! Let's start processing tags passed to use from the bootloader
  term_setup(hdr);
  idt_setup();
  pic_setup();

  // find_memory(hdr);

  // masks the bottom 12 bits
  // this is the start of the level 4 table
  uintptr_t level_4_base = read_cr3();
  level_4_base &= 0xFFFFFFFFFFFFF000;

  kprint_f("%p\n", level_4_base);

  translate(level_4_base, _start);

  // while (1) {
  //   kprint_f("%c", kgetc()); 
  // }

	halt();
}