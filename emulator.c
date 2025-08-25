#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
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
    uint16_t I;
    uint8_t delay;
    uint8_t sound;
    uint16_t PC;
    uint16_t stack[16];
    uint8_t SP;
    uint8_t memory[4096];
};

/*
Read the memory section which contains the display state and draw it in terminal with ANSI codes
*/
void draw_display(struct chip_8 *arch) {
    // printf("\033[?25l"); // hide cursor
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
    // printf("\033[?25h"); // show cursor
    fflush(stdout);
}

/*
Set the memory section relative to display state to 0x00
*/
void clear_display(struct chip_8 *arch) {
    for (int i = 0; i < 256; i++) {
        arch->memory[i] = 0x00; // the first 256 bytes of memory --> display state setted to 0
    }
}

void init_arch(struct chip_8 *arch) {
    arch->I = 0x00;
    arch->delay = 0x00;
    arch->sound = 0x00;
    arch->SP = 0;
    arch->PC = 0x200;
    for (int i = 0; i < 4096; i++) {
        arch->memory[i] = 0x00;
    }
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

    fread(&arch->memory[512], 1, filelen, fileptr); // Read in the entire file
    fclose(fileptr);
    return filelen;
}

unsigned short get_PC_inst(struct chip_8 *arch) {
    // high | low (1 bytes each) of 16 bit instructions
    uint8_t high = arch->memory[arch->PC];
    uint8_t low = arch->memory[arch->PC + 1];
    uint16_t inst = (high << 8) | low;
    return inst;
}

int main(int argv, char **args) {
    printf("\033[?25h");
    if (argv != 2) {
        printf("USAGE: emulator <program/to/execute/path>");
        return 1;
    }

    struct chip_8 arch;
    init_arch(&arch);

    char *program_path = args[1];
    long filelen = read_program(&arch, program_path);

    uint16_t inst = get_PC_inst(&arch);
    printf("%04X", inst);

    int running = 1;

    while (running) {
        switch (inst & 0xF000) {
            case 0x0000:
                switch (inst) {
                    case 0x00E0:
                        printf("run 00E0\n");
                        clear_display(&arch);
                        break;
                    case 0x0F02:
                        printf("run 0F02 ignored\n");
                        break;
                    default:
                        printf("inst %04X not implemented\n", inst);
                        running = 0;
                        break;
                }
                break;

            // altri casi
        }
        // draw_display(&arch);
        arch.PC += 2;
        inst = get_PC_inst(&arch);
    }

    return 0;
}
