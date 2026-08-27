// ApexKernel/Drivers/Input/Keyboard/KeyBuffer.c
// ApexOS -- Keyboard ring buffer

#include "Drivers/Input/Keyboard/KeyBuffer.h"
#include "Drivers/Input/Keyboard/KeyboardInternal.h"

// Ring buffer. uint8_t (not char) so extended key codes (KEY_ARROW_*,
// >= 0x80) can share the buffer with normal ASCII (0-127) without
// ambiguity.
static volatile uint8_t kb_buffer[KEYBOARD_BUFFER_SIZE];
static volatile uint32_t kb_head = 0;
static volatile uint32_t kb_tail = 0;

void kb_buffer_push(uint8_t c)
{
    uint32_t next = (kb_head + 1) % KEYBOARD_BUFFER_SIZE;
    if (next != kb_tail)
    {
        kb_buffer[kb_head] = c;
        kb_head = next;
    }
}

uint8_t kb_buffer_pop(void)
{
    if (kb_head == kb_tail)
        return 0;
    uint8_t c = kb_buffer[kb_tail];
    kb_tail = (kb_tail + 1) % KEYBOARD_BUFFER_SIZE;
    return c;
}

void kb_buffer_reset(void)
{
    kb_head = 0;
    kb_tail = 0;
}

bool keyboard_has_key(void)
{
    return kb_head != kb_tail;
}

uint8_t keyboard_getchar(void)
{
    while (kb_head == kb_tail)
        __asm__ volatile("pause");
    return kb_buffer_pop();
}
