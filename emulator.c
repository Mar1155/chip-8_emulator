#include <stdio.h>
#include <stdlib.h>
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
*/

/*
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
    unsigned char V[16];
    char I;
    unsigned char VF;
    unsigned char delay;
    unsigned char sound;
    char PC;
    char stack[16];
    uint8_t SP;
    char memory[4096];
};

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
    for (int i = 0; i < 4096; i++) {
        arch->memory[i] = 0x00;
    }
}

/*
    read the file and return the file length
*/
int read_program(struct chip_8 *arch, char *program_path) {
    FILE *fileptr;
    long filelen;

    fileptr = fopen(program_path, "rb"); // Open the file in binary mode
    fseek(fileptr, 0, SEEK_END);         // Jump to the end of the file
    filelen = ftell(fileptr);            // Get the current byte offset in the file
    rewind(fileptr);                     // Jump back to the beginning of the file

    fread(arch->memory[512], 1, filelen, fileptr); // Read in the entire file
    fclose(fileptr);
    return filelen;
}

int main(int argv, char **args) {
    printf("\033[?25h");
    if (argv != 2) {
        printf("USAGE: emulator <program/to/execute/path>");
        return 1;
    }

    struct chip_8 arch;
    char *program_path = args[1];
     
    long filelen = read_program(&arch, program_path);

    init_arch(&arch);

    while (1) {
        char instruction = arch.memory[512 + arch.PC + 1] + arch.memory[512 + arch.PC];
        printf("%c", instruction);
        return 0;

        draw_display(&arch);
    }

    return 0;
}
