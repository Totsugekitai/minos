#pragma once
#include <stdint.h>

struct InterruptFrame {
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};

extern void general_protection(void);
extern void page_fault(void);
extern void timer_interrupt(void);
extern void keyboard_interrupt(void);
extern void com1_interrupt(void);
extern void syscall_handler(void);
void gp_handler_deluxe(struct InterruptFrame *frame, uint64_t error_code);
void my_timer_handler(void);
void timer_handler(struct InterruptFrame *frame);
void keyboard_handler(struct InterruptFrame *frame);
void keyboard_handler_dash(void);
void com1_handler(struct InterruptFrame *frame);
void com1_handler_dash(void);
void mouse_handler(struct InterruptFrame *frame);
void empty_handler(struct InterruptFrame *frame);
