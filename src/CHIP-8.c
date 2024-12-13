#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define C8_STACK_LEN 24
#define C8_FRAMEBUFFER_LEN 16 * 32

struct c8_machine {
    struct {
        uint8_t V0;
        uint8_t V1;
        uint8_t V2;
        uint8_t V3;
        uint8_t V4;
        uint8_t V5;
        uint8_t V6;
        uint8_t V7;
        uint8_t V8;
        uint8_t V9;
        uint8_t VA;
        uint8_t VB;
        uint8_t VC;
        uint8_t VD;
        uint8_t VE;
        uint8_t VF;

        uint16_t I;
        uint16_t PC;
    } registers;

    uint8_t  memory[4096];
    uint16_t stack[C8_STACK_LEN];

    struct {
        uint8_t DT;
        uint8_t ST;
    } timers;

    uint8_t framebuffer[C8_FRAMEBUFFER_LEN];  // 1 row = 16 bytes

    uint8_t keypad;
};

typedef struct c8_machine *c8_machine_t;



int main(void) {
    c8_machine_t machine;

    c8_init(machine);

    while(1) {
        c8_handle_input(machine);
        c8_update_timers(machine);
        c8_cycle(machine);
        c8_draw(machine);
    }

    return 0;
}