#include <conio.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define C8_STACK_LEN       24
#define C8_FRAMEBUFFER_LEN 8 * 32

struct c8_machine {
    bool running;

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
            switch (N) {
                case 0x0:
                    machine->registers.V[X] = machine->registers.V[Y];

                    break;
                case 0x1:
                    machine->registers.V[X] |= machine->registers.V[Y];

                    break;
                case 0x2:
                    machine->registers.V[X] &= machine->registers.V[Y];

                    break;
                case 0x3:
                    machine->registers.V[X] ^= machine->registers.V[Y];

                    break;
                case 0x4: {
                    uint8_t VX = machine->registers.V[X];
                    machine->registers.V[X] += machine->registers.V[Y];

                    machine->registers.V[0xF] = machine->registers.V[X] <= VX;
                } break;
                case 0x5: {
                    uint8_t VX = machine->registers.V[X];
                    machine->registers.V[X] -= machine->registers.V[Y];

                    machine->registers.V[0xF] = machine->registers.V[X] <= VX;
                } break;
                case 0x6:
                    machine->registers.V[0xF] = machine->registers.V[X] & 0x1;
                    machine->registers.V[X]   = machine->registers.V[X] >> 1;

                    break;
                case 0x7: {
                    uint8_t VX = machine->registers.V[X];
                    uint8_t VY = machine->registers.V[Y];

                    machine->registers.V[X] = VY - VX;

                    machine->registers.V[0xF] = VX <= VY;

                } break;
                case 0xE:
                    machine->registers.V[0xF] = (machine->registers.V[X] >> 7) & 0x1;
                    machine->registers.V[X]   = machine->registers.V[X] << 1;

                    break;
                default:
                    break;
            }

            break;
        case 0x9:
            if (machine->registers.V[X] != machine->registers.V[Y]) {
                machine->registers.PC += 2;
            }

            break;
        case 0xA:
            machine->registers.I = NNN;

            break;
        case 0xB:
            machine->registers.PC = NNN + machine->registers.V[0];

            break;
        case 0xC:
            machine->registers.V[X] = rand() & NN;

            break;
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
            if (NN == 0x9E) {
                if (machine->keypad == machine->registers.V[X]) {
                    machine->registers.PC += 2;
                }
            }

            if (NN == 0xA1) {
                if (machine->keypad != machine->registers.V[X]) {
                    machine->registers.PC += 2;
                }
            }

            break;
        case 0xF:
            switch (NN) {
                case 0x07:
                    machine->registers.V[X] = machine->timers.DT;

                    break;

                case 0x15:
                    machine->timers.DT = machine->registers.V[X];

                    break;

                case 0x18:
                    machine->timers.ST = machine->registers.V[X];

                    break;

                case 0x1E:
                    machine->registers.I += machine->registers.V[X];
                    machine->registers.V[0xF] = machine->registers.I > 0x0FFF;

                    break;

                case 0x0A:
                    if (machine->keypad == 0x69) {
                        machine->registers.PC -= 2;
                    }

                    else {
                        machine->registers.V[X] = machine->keypad;
                    }

                    break;

                case 0x29:
                    machine->registers.I = 0x0050 + (machine->registers.V[X] & 0xF);

                    break;

                case 0x33:
                    machine->memory[machine->registers.I]     = machine->registers.V[X] / 100;
                    machine->memory[machine->registers.I + 1] = (machine->registers.V[X] / 10) % 10;
                    machine->memory[machine->registers.I + 2] = machine->registers.V[X] % 10;

                    break;

                case 0x55:
                    for (int i = machine->registers.V[X]; i >= 0; i--) {
                        machine->memory[machine->registers.I + i] = machine->registers.V[i];
                    }

                    break;

                case 0x65:
                    for (int i = machine->registers.V[X]; i >= 0; i--) {
                        machine->registers.V[i] = machine->memory[machine->registers.I + i];
                    }

                    break;

                default:
                    // Handle any other values of NN if necessary
                    break;
            }

            break;
        default:
            break;
    }
}

void c8_init(c8_machine_t machine, const char *path_to_rom) {
    machine->running      = true;
    machine->registers.PC = 0x0200;
    machine->stack_top    = -1;

    const uint8_t font_data[] = {
        0xF0, 0x90, 0x90, 0x90, 0xF0,  // 0
        0x20, 0x60, 0x20, 0x20, 0x70,  // 1
        0xF0, 0x10, 0xF0, 0x80, 0xF0,  // 2
        0xF0, 0x10, 0xF0, 0x10, 0xF0,  // 3
        0x90, 0x90, 0xF0, 0x10, 0x10,  // 4
        0xF0, 0x80, 0xF0, 0x10, 0xF0,  // 5
        0xF0, 0x80, 0xF0, 0x90, 0xF0,  // 6
        0xF0, 0x10, 0x20, 0x40, 0x40,  // 7
        0xF0, 0x90, 0xF0, 0x90, 0xF0,  // 8
        0xF0, 0x90, 0xF0, 0x10, 0xF0,  // 9
        0xF0, 0x90, 0xF0, 0x90, 0x90,  // A
        0xE0, 0x90, 0xE0, 0x90, 0xE0,  // B
        0xF0, 0x80, 0x80, 0x80, 0xF0,  // C
        0xE0, 0x90, 0x90, 0x90, 0xE0,  // D
        0xF0, 0x80, 0xF0, 0x80, 0xF0,  // E
        0xF0, 0x80, 0xF0, 0x80, 0x80,  // F
    };

    memcpy(&machine->memory[0x0050], font_data, sizeof(font_data));

    FILE *f = fopen(path_to_rom, "rb");

    if (!f) {
        printf("Could not read ROM at %s.\n", path_to_rom);
        return;
    }

    uint8_t rom_buffer[4096];

    size_t rom_size = fread(rom_buffer, sizeof(uint8_t), 4096, f);

    memcpy(&machine->memory[0x0200], rom_buffer, rom_size);
}

void c8_handle_input(c8_machine_t machine) {
    if (!_kbhit()) {
        machine->keypad = 0x69;
        return;
    }

    char c = _getch();

    switch (c) {
        case '1':
            machine->keypad = 0x1;
            break;
        case '2':
            machine->keypad = 0x2;
            break;
        case '3':
            machine->keypad = 0x3;
            break;
        case '4':
            machine->keypad = 0xC;
            break;
        case 'Q':
        case 'q':
            machine->keypad = 0x4;
            break;
        case 'W':
        case 'w':
            machine->keypad = 0x5;
            break;
        case 'E':
        case 'e':
            machine->keypad = 0x6;
            break;
        case 'R':
        case 'r':
            machine->keypad = 0xD;
            break;
        case 'A':
        case 'a':
            machine->keypad = 0x7;
            break;
        case 'S':
        case 's':
            machine->keypad = 0x8;
            break;
        case 'D':
        case 'd':
            machine->keypad = 0x9;
            break;
        case 'F':
        case 'f':
            machine->keypad = 0xE;
            break;
        case 'Z':
        case 'z':
            machine->keypad = 0xA;
            break;
        case 'X':
        case 'x':
            machine->keypad = 0x0;
            break;
        case 'C':
        case 'c':
            machine->keypad = 0xB;
            break;
        case 'V':
        case 'v':
            machine->keypad = 0xF;
            break;
        case 27:
            machine->running = false;
            break;
        default:
            machine->keypad = 0x69;
            break;  // No key pressed or invalid key
    }
}

void c8_update_timers(c8_machine_t machine, unsigned int dt_milliseconds) {
    static unsigned int t = 0;

    t += dt_milliseconds;

    if (t < 16) return;

    machine->timers.DT -= machine->timers.DT == 0 ? 0 : t / 16;
    machine->timers.ST -= machine->timers.ST == 0 ? 0 : t / 16;

    t = 0;
}

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

int main(int argc, const char *argv[]) {
    if (argc != 2) {
        printf("Usage: CHIP-8 <path_to_rom>");
        return 1;
    }

    printf("\033[=13h");

    c8_machine_t machine = calloc(1, sizeof(struct c8_machine));

    c8_init(machine);

    while (machine->running) {
        unsigned int time_start = timeGetTime();

        c8_handle_input(machine);
        c8_cycle(machine);

        if (machine->dxyn_called) {
            c8_draw(machine);
            machine->dxyn_called = false;
        }

        unsigned int time_end = timeGetTime();

        c8_update_timers(machine, time_end - time_start);
    }

    return 0;
}