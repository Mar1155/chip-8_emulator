#include <curses.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*
unsigned char V[16] --> 16 * 8-bit registers
    * they are referred as Vx. x --> hex decimal digit (0 --> f)
unsigned char I --> 16 bit register
    * store memory addresses (total 4096 bytes, so only rightmost 12 bits are used [0x000 --> 0xFFF]) 3 bytes = 12 bits
unsigned char VF; --> 8 bit register
    * used only by some instructions, no programs use it

unsigned char delay --> 8-bit register,
unsigned char sound --> 8-bit register
   *  when nonzero, they are automatically decremented at 60hz rate
   * they are timers
char PC --> 16-bit register
    * store the currently executing address
unsigned char SP --> 8-bit register
    * stack pointer, it points to the topmost level of the stack

char stack[16]; --> 16 * 16-bit fields
    * it used to store the subroutine return address
    * so chip-8 allows for up 16 levels of nested subsroutines (16 addresses in the stack)

char memory[4096] --> 4KB of memory
    * [0x000 --> 0x1ff] reserved to interpreter

MEMORY --> [0x000 --> 0x1FF] (256 bytes) (2048 bits) <-- display's pixels state
Originally chip-8 used a 32x64 pixel monochrome display
the display state/data is stored in [0x000 to 0x1FF] 32 x 64 bytes
    * some programs use sprites to abstract graphics
    * usually a sprite is 5 bytes long, or 8x5 pixel
    11110000 -->    ****
    10010000        *  *
    10010000        *  *
    10010000        *  *
    11110000        ****
    this is a memory dump example to represent zero digit as a 8x5 pixel sprite
*/
struct chip_8 {
    uint8_t V[16];
    uint8_t VF;
    uint16_t I;
    uint8_t delay;
    uint8_t sound;
    uint16_t PC;
    uint16_t stack[16];
    uint8_t SP;
    uint8_t memory[4096];
    uint8_t key[16];
};

static int chip8_map_key(int ch); /* prototipo static per evitare implicit non-static */
static void input_init(void);
static void input_shutdown(void);
static void input_poll_all(struct chip_8 *arch, int *quit);

static void draw_display(struct chip_8 *arch);
static void clear_display(struct chip_8 *arch);
static void load_fontset(struct chip_8 *arch);
static void init_arch(struct chip_8 *arch);
static int read_program(struct chip_8 *arch, char *program_path);
static unsigned short fetch_instr(struct chip_8 *arch);

typedef struct {
    int quit;        // flag per uscire
    int key_pressed; // -1 se nessun tasto, altrimenti codice
} InputState;

static void input_init() {
    initscr();            // init ncurses
    cbreak();             // input per carattere (no line buffering)
    noecho();             // non fare echo dei tasti
    keypad(stdscr, TRUE); // abilita tasti speciali (frecce, F-keys)
    timeout(0);           // getch() non bloccante (0 = polling)
    // facoltativo: curs_set(0); // nascondi cursore
}

static void input_shutdown() {
    // curs_set(1); // ripristina cursore se era nascosto
    endwin(); // ripristina terminale
}

void input_poll_all(struct chip_8 *arch, int *quit) {
    // reset stato tasti
    memset(arch->key, 0, sizeof(arch->key));

    int ch;
    while ((ch = getch()) != ERR) {
        if (ch == 27) { // ESC
            *quit = 1;
        } else if (ch == KEY_BACKSPACE) {
            *quit = 1;
        }
        // mapping CHIP‑8
        int k = chip8_map_key(ch);
        if (k >= 0) {
            arch->key[k] = 1;
        }
    }
}

// Mappatura CHIP-8 (16 tasti): layout comune:
// CHIP-8:  1 2 3 C
//          4 5 6 D
//          7 8 9 E
//          A 0 B F
// PC:      1 2 3 4
//          Q W E R
//          A S D F
//          Z X C V
// Restituisce indice 0..15 o -1 se non mappato
static int chip8_map_key(int ch) {
    switch (ch) {
    case '1':
        return 0x1;
    case '2':
        return 0x2;
    case '3':
        return 0x3;
    case '4':
        return 0xC;
    case 'q':
    case 'Q':
        return 0x4;
    case 'w':
    case 'W':
        return 0x5;
    case 'e':
    case 'E':
        return 0x6;
    case 'r':
    case 'R':
        return 0xD;
    case 'a':
    case 'A':
        return 0x7;
    case 's':
    case 'S':
        return 0x8;
    case 'd':
    case 'D':
        return 0x9;
    case 'f':
    case 'F':
        return 0xE;
    case 'z':
    case 'Z':
        return 0xA;
    case 'x':
    case 'X':
        return 0x0;
    case 'c':
    case 'C':
        return 0xB;
    case 'v':
    case 'V':
        return 0xF;
    default:
        return -1;
    }
}

uint8_t chip8_fontset[80] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

void load_fontset(struct chip_8 *arch) {
    for (int i = 0; i < 80; i++)
        arch->memory[i] = chip8_fontset[i];
}

/*
Read the memory section which contains the display state and draw it in terminal with ANSI codes
*/
void draw_display(struct chip_8 *arch) {
    printf("\033[?25l"); // hide cursor
    printf("\033[H\033[2J");
    fflush(stdout);
    for (int y = 0; y < 32; y++) {     // row
        for (int x = 0; x < 64; x++) { // columns
            int pixel_index = y * 64 + x;
            int byte_pos = pixel_index / 8;
            int bit_pos = 7 - (pixel_index % 8);
            if ((arch->memory[byte_pos] >> bit_pos) & 1) {
                printf("\033[%d;%dH█", y + 1, x + 1);
            } else {
                printf("\033[%d;%dH ", y + 1, x + 1);
            }
        }
    }
    printf("\033[?25h"); // show cursor
    fflush(stdout);
}

/*
Set the memory section relative to display state to 0x00
*/
void clear_display(struct chip_8 *arch) {
    for (int i = 0; i < 256; i++) {
        arch->memory[i] = 0; // the first 256 bytes of memory --> display state setted to 0
    }
}

void init_arch(struct chip_8 *arch) {
    arch->I = 0;
    arch->delay = 0;
    arch->sound = 0;
    arch->SP = 0;
    arch->PC = 0x200;
    memset(arch->V, 0, sizeof(arch->V));
    memset(arch->stack, 0, sizeof(arch->stack));
    memset(arch->memory, 0, sizeof(arch->memory));
    memset(arch->key, 0, sizeof(arch->key));
    load_fontset(arch);
}

/*
Read the file and return the file length
*/
int read_program(struct chip_8 *arch, char *program_path) {
    FILE *fileptr;
    long filelen;

    fileptr = fopen(program_path, "rb"); // Open the file in binary mode
    fseek(fileptr, 0, SEEK_END);         // Jump to the end of the file
    filelen = ftell(fileptr);            // Get the current byte offset in the file
    rewind(fileptr);                     // Jump back to the beginning of the file

    fread(&arch->memory[0x200], 1, filelen, fileptr); // Read in the entire file
    fclose(fileptr);
    return filelen;
}

unsigned short fetch_instr(struct chip_8 *arch) {
    // high | low (1 bytes each) of 16 bit instructions
    return (arch->memory[arch->PC] << 8) | arch->memory[arch->PC + 1];
}

int main(int argv, char **args) {
    printf("\033[?25h");
    if (argv != 2) {
        printf("USAGE: emulator <program/to/execute/path>");
        return 1;
    }

    input_init();

    struct chip_8 arch;
    init_arch(&arch);

    char *program_path = args[1];
    long filelen = read_program(&arch, program_path);

    int running = 1;
    int n_instr = 0;
    int incr = 1;
    while (running) {
        uint16_t inst = fetch_instr(&arch);
        arch.PC += 2;

        printf("%d:  %04X --> ", n_instr, inst);
        // decode / execute
        if (inst == 0x00E0) {
            clear_display(&arch);
            printf("CLS\n");
        } else if (inst == 0x00EE) {
            arch.PC = arch.stack[--arch.SP];
            printf("RET\n");
        } else if ((inst & 0xF000) == 0x0000) { // ignored by modern interpreters
            // uint16_t nnn = (inst >> 4) & 0xFFF;
            // arch.PC = nnn;
            printf("%04X ignored\n", inst);
        } else if ((inst & 0xF000) == 0x1000) {
            uint16_t nnn = inst & 0xFFF;
            arch.PC = nnn;
            printf("JP %04X\n", nnn);
        } else if ((inst & 0xF000) == 0x2000) {
            uint16_t nnn = inst & 0xFFF;
            arch.stack[arch.SP] = arch.PC;
            arch.SP++;
            arch.PC = nnn;
            printf("CALL %03X\n", nnn);
        } else if ((inst & 0xF000) == 0x3000) {
            uint8_t x = (inst >> 8) & 0xF; // register index
            uint8_t kk = inst & 0xFF;      // 8 bit lowest bits of the instruction
            if (arch.V[x] == kk) {
                arch.PC += 2;
            }
            printf("SE V%X, V%X\n", x, kk);
        } else if ((inst & 0xF000) == 0x4000) {
            uint8_t x = (inst >> 8) & 0xF; // register index
            uint8_t kk = inst & 0xFF;      // 8 bit lowest bits of the instruction
            if (arch.V[x] != kk) {
                arch.PC += 2;
            }
            printf("SNE V%d, %02X\n", x, kk);
        } else if ((inst & 0xF000) == 0x5000) {
            uint8_t x = (inst >> 8) & 0xF; // register index x
            uint8_t y = (inst >> 4) & 0xF; // register index y - FIXED
            if (arch.V[x] == arch.V[y]) {
                arch.PC += 2;
            }
            printf("SE V%X, V%X\n", x, y);
        } else if ((inst & 0xF000) == 0x6000) {
            uint8_t x = (inst >> 8) & 0xF; // register index
            uint8_t kk = inst & 0xFF;      // 8 bit lowest bits of the instruction
            printf("LD V%d, %02X\n", x, kk);
            arch.V[x] = kk;
        } else if ((inst & 0xF000) == 0x7000) {
            uint8_t x = (inst >> 8) & 0xF; // register index
            uint8_t kk = inst & 0xFF;
            arch.V[x] += kk;
            printf("ADD V%X, %X\n", x, kk);
        } else if ((inst & 0xF000) == 0x8000) {
            uint8_t x = (inst >> 8) & 0xF; // register index x
            uint8_t y = (inst >> 4) & 0xF; // register index y - FIXED
            uint8_t n = inst & 0xF;        // last nibble - FIXED
            if (n == 0) {
                arch.V[x] = arch.V[y];
                printf("LD V%X, V%X\n", x, y);
            } else if (n == 1) {
                arch.V[x] |= arch.V[y];
                printf("OR V%X, V%X\n", x, y);
            } else if (n == 2) {
                arch.V[x] &= arch.V[y];
                printf("AND V%X, V%X\n", x, y);
            } else if (n == 3) {
                arch.V[x] ^= arch.V[y];
                printf("XOR V%X, V%X\n", x, y);
            } else if (n == 4) {
                uint16_t sum = arch.V[x] + arch.V[y];
                arch.VF = (sum > 255) ? 1 : 0;
                arch.V[x] = sum & 0xFF;
                printf("ADD V%X, V%X\n", x, y);
            } else if (n == 5) {
                arch.VF = (arch.V[x] > arch.V[y]) ? 1 : 0;
                arch.V[x] -= arch.V[y];
                printf("SUB V%X, V%X\n", x, y);
            } else if (n == 6) {
                arch.VF = arch.V[x] & 1;
                arch.V[x] >>= 1;
                printf("SHR V%X\n", x);
            } else if (n == 7) {
                arch.VF = (arch.V[y] > arch.V[x]) ? 1 : 0;
                arch.V[x] = arch.V[y] - arch.V[x];
                printf("SUBN V%X, V%X\n", x, y);
            } else if (n == 0xE) {
                arch.VF = (arch.V[x] & 0x80) ? 1 : 0;
                arch.V[x] <<= 1;
                printf("SHL V%X\n", x);
            } else {
                printf("8%X%X%X Not implemented\n", x, y, n);
                return 1;
            }
        } else if ((inst & 0xF000) == 0x9000) {
            uint8_t x = (inst >> 8) & 0xF; // register index x
            uint8_t y = (inst >> 4) & 0xF; // register index y - FIXED
            if (arch.V[x] != arch.V[y]) {
                arch.PC += 2;
            }
            printf("SNE V%X, V%X\n", x, y);
        } else if ((inst & 0xF000) == 0xA000) {
            uint16_t nnn = inst & 0xFFF;
            arch.I = nnn;
            printf("LD I, %04X\n", nnn);
        } else if ((inst & 0xF000) == 0xB000) {
            uint16_t nnn = inst & 0xFFF;
            arch.PC = nnn + arch.V[0];
            printf("JP V0, %04X\n", nnn);
        } else if ((inst & 0xF000) == 0xD000) {
            uint8_t x = (inst >> 8) & 0xF;
            uint8_t y = (inst >> 4) & 0xF;
            uint8_t n = inst & 0xF;
            uint8_t x_coord = arch.V[x] % 64;
            uint8_t y_coord = arch.V[y] % 32;
            arch.VF = 0;

            for (uint8_t row = 0; row < n; row++) {
                uint8_t sprite_byte = arch.memory[arch.I + row];                    // read byte memory
                for (uint8_t col = 0; col < 8; col++) {                             // iterate over the 8 bits's byte
                    uint8_t sprite_pixel = (sprite_byte >> (7 - col)) & 1;          // read col_th sprite bit
                    uint16_t screen_index = (y_coord + row) * 64 + (x_coord + col); // get screen coordinates
                    uint8_t *screen_pixel = &arch.memory[screen_index / 8];         // prev screen byte
                    uint8_t bit_pos = 7 - (screen_index % 8);                       // prev screen bit position

                    // XOR del pixel
                    uint8_t prev = (*screen_pixel >> bit_pos) & 1; // prev screen pixel
                    *screen_pixel ^= sprite_pixel << bit_pos;      // xor with new pixel, to check collisions

                    if (prev && sprite_pixel) {
                        arch.VF = 1; // set collision flag to one
                    }
                }
            }
            printf("DRW V%X, V%X, %X\n", x, y, n);
        } else if ((inst & 0xF000) == 0xE000) {
            uint8_t x = (inst >> 8) & 0xF;
            uint8_t kk = inst & 0xFF;

            if (kk == 0x9E) {
                uint8_t key_id = arch.V[x] & 0xF;
                if (arch.key[key_id] != 0) {
                    arch.PC += 2;
                }
                printf("SKP V%X (key %X)\n", x, key_id);
            } else if (kk == 0xA1) {
                uint8_t key_id = arch.V[x] & 0xF;
                if (arch.key[key_id] == 0) {
                    arch.PC += 2;
                }
                printf("SKNP V%X (key %X)\n", x, key_id);
            } else {
                printf("E%02X Not implemented\n", kk);
                return 1;
            }
        } else if ((inst & 0xF000) == 0xF000) {
            uint8_t x = (inst >> 8) & 0xF; // register index
            uint8_t kk = inst & 0xFF;
            if (kk == 0x65) {
                for (int i = 0; i <= x; i++) {
                    arch.V[i] = arch.memory[arch.I + i];
                }
                printf("LD V%d, [I]\n", x);
            } else if (kk == 0x55) {
                for (int i = 0; i < x; i++) {
                    arch.memory[arch.I + i] = arch.V[i];
                }
                printf("LD [I], V%d\n", x);
            } else if (kk == 0x33) {
                uint8_t x = (inst >> 8) & 0xF;
                uint16_t n = arch.V[x];
                uint8_t h = (int)n / 100;
                uint8_t t = ((int)n / 10) % 10;
                uint8_t u = n % 10;
                arch.memory[arch.I] = h;
                arch.memory[arch.I + 1] = t;
                arch.memory[arch.I + 2] = u;
                printf("LD B, V%X\n", x);
            } else if (kk == 0x1E) {
                uint8_t x = (inst >> 8) & 0xF;
                arch.I += arch.V[x];
                printf("ADD, I, V%X\n", x);
            } else if (kk == 0x15) {
                uint8_t x = (inst >> 8) & 0xF;
                arch.delay = arch.V[x];
                printf("LD DT, V%X\n", x);
            } else if (kk == 0x07) {
                uint8_t x = (inst >> 8) & 0xF;
                arch.V[x] = arch.delay;
                printf("LD V%X, DT\n", x);
            } else if (kk == 0x18) {
                uint8_t x = (inst >> 8) & 0xF;
                arch.sound = arch.V[x];
                printf("LD ST, V%X\n", x);
            } else if (kk == 0x29) {
                // FX29 - Set I = location of sprite for digit VX
                uint8_t x = (inst >> 8) & 0xF;
                arch.I = arch.V[x] * 5; // ogni carattere font è 5 bytes
                printf("LD F, V%X\n", x);
            } else if (kk == 0x0A) {
                // FX0A - Wait for key press, store in VX
                uint8_t x = (inst >> 8) & 0xF;
                int key_pressed = -1;

                // Cerca se qualche tasto è premuto
                for (int i = 0; i < 16; i++) {
                    if (arch.key[i]) {
                        key_pressed = i;
                        break;
                    }
                }

                if (key_pressed >= 0) {
                    arch.V[x] = key_pressed;
                    printf("LD V%X, K (got key %X)\n", x, key_pressed);
                } else {
                    // Nessun tasto premuto, ripeti questa istruzione
                    arch.PC -= 2;
                    printf("LD V%X, K (waiting...)\n", x);
                }
            } else if (kk == 0x9E) {
                uint8_t x = (inst >> 8) & 0xF;
                if (arch.key[arch.V[x]] != 0) {
                    arch.PC += 2;
                }
            }
            // not implemented branch
            else {
                printf("%04X Not implemented", inst);
                return 1;
            }
        }
        // not implemented branch
        else {
            printf("%04X Not implemented", inst);
            return 1;
        }
        fflush(stdout);

        draw_display(&arch);

        int quit = 0;
        input_poll_all(&arch, &quit);
        if (quit)
            running = 0;

        // timers
        if (arch.delay > 0)
            arch.delay--;
        if (arch.sound > 0) {
            arch.sound--;
            printf("\a");
        }

        struct timespec ts = {.tv_sec = 0, .tv_nsec = 16666666};
        nanosleep(&ts, NULL);
    }

    input_shutdown();
    return 0;
}
