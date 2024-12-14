#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define C8_STACK_LEN       24
#define C8_FRAMEBUFFER_LEN 8 * 32

struct c8_machine {
    struct {
        uint8_t V[16];

        uint16_t I;
        uint16_t PC;
    } registers;

    uint8_t memory[4096];

    uint16_t stack[C8_STACK_LEN];
    int      stack_top;

    struct {
        uint8_t DT;
        uint8_t ST;
    } timers;

    uint8_t framebuffer[C8_FRAMEBUFFER_LEN];  // 1 row = 8 bytes
    bool    dxyn_called;

    uint8_t keypad;
};

typedef struct c8_machine *c8_machine_t;

void c8_cycle(c8_machine_t machine) {
    // Fetch
    uint16_t instruction_hi = machine->memory[machine->registers.PC];
    uint16_t instruction_lo = machine->memory[machine->registers.PC + 1];

    uint16_t instruction = instruction_hi << 8 | instruction_lo;

    machine->registers.PC += 2;

    // Decode
    uint8_t  T   = instruction_hi >> 4 & 0xF;
    uint8_t  X   = instruction_hi & 0xF;
    uint8_t  Y   = instruction_lo >> 4 & 0xF;
    uint8_t  N   = instruction_lo & 0xF;
    uint8_t  NN  = instruction_lo;
    uint16_t NNN = X << 8 | NN;

    // Execute
    switch (T) {
        case 0x0:
            if (instruction == 0x00E0) {
                memset(machine->framebuffer, 0, C8_FRAMEBUFFER_LEN);
            }

            if (instruction == 0x00EE) {
                machine->registers.PC = machine->stack[machine->stack_top--];
            }

            break;
        case 0x1:
            machine->registers.PC = NNN;

            break;
        case 0x2:
            machine->stack[++machine->stack_top] = machine->registers.PC;
            machine->registers.PC                = NNN;

            break;
        case 0x3:
            if (machine->registers.V[X] == NN) {
                machine->registers.PC += 2;
            }

            break;
        case 0x4:
            if (machine->registers.V[X] != NN) {
                machine->registers.PC += 2;
            }

            break;
        case 0x5:
            if (machine->registers.V[X] == machine->registers.V[Y]) {
                machine->registers.PC += 2;
            }

            break;
        case 0x6:
            machine->registers.V[X] = NN;

            break;
        case 0x7:
            machine->registers.V[X] += NN;

            break;
        case 0x8:
        case 0x9:
            if (machine->registers.V[X] != machine->registers.V[Y]) {
                machine->registers.PC += 2;
            }

            break;
        case 0xA:
            machine->registers.I = NNN;

            break;
        case 0xB:
        case 0xC:
        case 0xD: {
            machine->dxyn_called = true;
            uint8_t pixels[16];  // Max N = 0xF

            uint8_t *addr = &machine->memory[machine->registers.I];  // Starting address

            // Read N bytes from memory starting from I
            memcpy(pixels, addr, N * sizeof(uint8_t));

            // Starting coordinates
            uint8_t pixel_x = machine->registers.V[X];
            uint8_t pixel_y = machine->registers.V[Y];

            // Offset from the framebuffer byte containing the pixel
            uint8_t pixel_offset = pixel_x % 8;

            uint8_t is_collision = 0;

            for (int i = 0; i < N; i++) {
                uint8_t pixel = pixels[i];

                uint8_t value_low  = (pixel >> pixel_offset) & 0xFF;
                uint8_t value_high = (pixel << (8 - pixel_offset)) & 0xFF;

                int index_low  = (pixel_y % 32) * 8 + (pixel_x % 64) / 8;
                int index_high = (pixel_y % 32) * 8 + ((pixel_x + 8) % 64) / 8;

                is_collision |= machine->framebuffer[index_low] & value_low;
                is_collision |= machine->framebuffer[index_high] & value_high;

                machine->framebuffer[index_low] ^= value_low;
                machine->framebuffer[index_high] ^= value_high;

                pixel_y++;
            }

        } break;
        case 0xE:
        case 0xF:
        default:
            break;
    }
}

void c8_init(c8_machine_t machine) {
    machine->registers.PC = 0x0200;

    const uint8_t ibm_logo_rom[] = {
        0x00, 0xE0, 0xA2, 0x2A, 0x60, 0x0C, 0x61, 0x08, 0xD0, 0x1F, 0x70, 0x09,
        0xA2, 0x39, 0xD0, 0x1F, 0xA2, 0x48, 0x70, 0x08, 0xD0, 0x1F, 0x70, 0x04,
        0xA2, 0x57, 0xD0, 0x1F, 0x70, 0x08, 0xA2, 0x66, 0xD0, 0x1F, 0x70, 0x08,
        0xA2, 0x75, 0xD0, 0x1F, 0x12, 0x28, 0xFF, 0x00, 0xFF, 0x00, 0x3C, 0x00,
        0x3C, 0x00, 0x3C, 0x00, 0x3C, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0xFF,
        0x00, 0x38, 0x00, 0x3F, 0x00, 0x3F, 0x00, 0x38, 0x00, 0xFF, 0x00, 0xFF,
        0x80, 0x00, 0xE0, 0x00, 0xE0, 0x00, 0x80, 0x00, 0x80, 0x00, 0xE0, 0x00,
        0xE0, 0x00, 0x80, 0xF8, 0x00, 0xFC, 0x00, 0x3E, 0x00, 0x3F, 0x00, 0x3B,
        0x00, 0x39, 0x00, 0xF8, 0x00, 0xF8, 0x03, 0x00, 0x07, 0x00, 0x0F, 0x00,
        0xBF, 0x00, 0xFB, 0x00, 0xF3, 0x00, 0xE3, 0x00, 0x43, 0xE0, 0x00, 0xE0,
        0x00, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0xE0, 0x00, 0xE0};

    memcpy(&machine->memory[0x0200], ibm_logo_rom, sizeof(ibm_logo_rom));

    machine->stack_top = -1;
}

void c8_handle_input(c8_machine_t machine) {}
void c8_update_timers(c8_machine_t machine) {}

void c8_draw(c8_machine_t machine) {
    printf("\033[H");  // Move to (0, 0)

    for (int i = 0; i < 32; i++) {
        for (int j = 0; j < 8; j++) {
            uint8_t framebuffer_byte = machine->framebuffer[i * 8 + j];

            for (int k = 7; k >= 0; k--) {
                if ((framebuffer_byte >> k) & 1) {
                    printf("\u2588\u2588");
                } else {
                    printf("  ");
                }
            }
        }

        printf("\n");
    }
}

int main(void) {
    printf("\033[=13h");

    c8_machine_t machine = calloc(1, sizeof(struct c8_machine));

    c8_init(machine);

    while (1) {
        c8_handle_input(machine);
        c8_update_timers(machine);
        c8_cycle(machine);

        if (machine->dxyn_called) {
            c8_draw(machine);
            machine->dxyn_called = false;
        }
    }

    return 0;
}