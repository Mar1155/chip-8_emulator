#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// CHIP-8 Display dimensions
#define DISPLAY_WIDTH 64
#define DISPLAY_HEIGHT 32
#define WINDOW_WIDTH (DISPLAY_WIDTH * 12)
#define WINDOW_HEIGHT (DISPLAY_HEIGHT * 12)

// CHIP-8 structure
struct chip_8 {
    uint8_t V[16];
    uint8_t VF;
    uint16_t I;
    uint8_t delay;
    uint8_t sound;
    uint16_t PC;
    uint16_t stack[16];
    uint8_t SP;
    uint8_t memory[4096 - (DISPLAY_WIDTH * DISPLAY_HEIGHT / 8)]; // reserve last part of memory for framebuffer
    uint8_t display[DISPLAY_WIDTH * DISPLAY_HEIGHT];
    uint8_t key[16];
};

// SDL structures
struct sdl_context {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_AudioStream *audio_stream;
    SDL_AudioDeviceID audio_device;
};

// Function prototypes
static void init_arch(struct chip_8 *arch);
static void load_fontset(struct chip_8 *arch);
static int read_program(struct chip_8 *arch, char *program_path);
static uint16_t fetch_instr(struct chip_8 *arch);
static void execute_instruction(struct chip_8 *arch, uint16_t inst);
static void update_timers(struct chip_8 *arch);
static void handle_input(struct chip_8 *arch, SDL_Event *event);
static void render_display(struct chip_8 *arch, struct sdl_context *sdl);
static int init_sdl(struct sdl_context *sdl);
static void cleanup_sdl(struct sdl_context *sdl);
static void audio_callback(void *userdata, SDL_AudioStream *stream,
                           int additional_amount, int total_amount);

// CHIP-8 fontset
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

// Key mapping from SDL scancodes to CHIP-8 keys
SDL_Scancode key_map[16] = {
    SDL_SCANCODE_X, // 0
    SDL_SCANCODE_1, // 1
    SDL_SCANCODE_2, // 2
    SDL_SCANCODE_3, // 3
    SDL_SCANCODE_Q, // 4
    SDL_SCANCODE_W, // 5
    SDL_SCANCODE_E, // 6
    SDL_SCANCODE_A, // 7
    SDL_SCANCODE_S, // 8
    SDL_SCANCODE_D, // 9
    SDL_SCANCODE_Z, // A
    SDL_SCANCODE_C, // B
    SDL_SCANCODE_4, // C
    SDL_SCANCODE_R, // D
    SDL_SCANCODE_F, // E
    SDL_SCANCODE_V  // F
};

void load_fontset(struct chip_8 *arch) {
    for (int i = 0; i < 80; i++) {
        arch->memory[i] = chip8_fontset[i];
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
    memset(arch->display, 0, sizeof(arch->display));
    load_fontset(arch);
}

int read_program(struct chip_8 *arch, char *program_path) {
    FILE *fileptr = fopen(program_path, "rb");
    if (!fileptr) {
        printf("Error: Could not open file %s\n", program_path);
        return -1;
    }

    fseek(fileptr, 0, SEEK_END);
    long filelen = ftell(fileptr);
    rewind(fileptr);

    if (filelen > (4096 - 0x200)) {
        printf("Error: Program too large\n");
        fclose(fileptr);
        return -1;
    }

    fread(&arch->memory[0x200], 1, filelen, fileptr);
    fclose(fileptr);
    return filelen;
}

uint16_t fetch_instr(struct chip_8 *arch) {
    return (arch->memory[arch->PC] << 8) | arch->memory[arch->PC + 1];
}

void execute_instruction(struct chip_8 *arch, uint16_t inst) {
    uint8_t x, y, n, kk;
    uint16_t nnn;

    switch (inst & 0xF000) {
    case 0x0000:
        if (inst == 0x00E0) {
            // Clear display
            printf("%s\n", "CLS");
            memset(arch->display, 0, sizeof(arch->display));
        } else if (inst == 0x00EE) {
            // Return from subroutine
            printf("%s\n", "RET");
            arch->PC = arch->stack[--arch->SP];
        }
        break;

    case 0x1000:
        // Jump to address nnn
        nnn = inst & 0xFFF;
        printf("%s %03X\n", "JP", nnn);
        arch->PC = nnn;
        return; // Don't increment PC

    case 0x2000:
        // Call subroutine at nnn
        nnn = inst & 0xFFF;
        printf("%s %03X\n", "CALL", nnn);
        arch->stack[arch->SP++] = arch->PC;
        arch->PC = nnn;
        return; // Don't increment PC

    case 0x3000:
        // Skip next instruction if Vx = kk
        x = (inst >> 8) & 0xF;
        kk = inst & 0xFF;
        printf("%s V%X, %02X\n", "SE", x, kk);
        if (arch->V[x] == kk) {
            arch->PC += 2;
        }
        break;

    case 0x4000:
        // Skip next instruction if Vx != kk
        x = (inst >> 8) & 0xF;
        kk = inst & 0xFF;
        printf("%s V%X, %02X\n", "SNE", x, kk);
        if (arch->V[x] != kk) {
            arch->PC += 2;
        }
        break;

    case 0x5000:
        // Skip next instruction if Vx = Vy
        x = (inst >> 8) & 0xF;
        y = (inst >> 4) & 0xF;
        printf("%s V%X, V%X\n", "SE", x, y);
        if (arch->V[x] == arch->V[y]) {
            arch->PC += 2;
        }
        break;

    case 0x6000:
        // Set Vx = kk
        x = (inst >> 8) & 0xF;
        kk = inst & 0xFF;
        printf("%s V%X, %02X\n", "LD", x, kk);
        arch->V[x] = kk;
        break;

    case 0x7000:
        // Set Vx = Vx + kk
        x = (inst >> 8) & 0xF;
        kk = inst & 0xFF;
        printf("%s V%X, %02X\n", "ADD", x, kk);
        arch->V[x] += kk;
        break;

    case 0x8000:
        x = (inst >> 8) & 0xF;
        y = (inst >> 4) & 0xF;
        n = inst & 0xF;

        switch (n) {
        case 0x0:
            printf("%s V%X, V%X\n", "LD", x, y);
            arch->V[x] = arch->V[y];
            break;
        case 0x1:
            printf("%s V%X, V%X\n", "OR", x, y);
            arch->V[x] |= arch->V[y];
            break;
        case 0x2:
            printf("%s V%X, V%X\n", "AND", x, y);
            arch->V[x] &= arch->V[y];
            break;
        case 0x3:
            printf("%s V%X, V%X\n", "XOR", x, y);
            arch->V[x] ^= arch->V[y];
            break;
        case 0x4: {
            printf("%s V%X, V%X\n", "ADD", x, y);
            uint16_t sum = arch->V[x] + arch->V[y];
            arch->V[0xF] = (sum > 255) ? 1 : 0;
            arch->V[x] = sum & 0xFF;
            break;
        }
        case 0x5:
            printf("%s V%X, V%X\n", "SUB", x, y);
            arch->V[0xF] = (arch->V[x] > arch->V[y]) ? 1 : 0;
            arch->V[x] -= arch->V[y];
            break;
        case 0x6:
            printf("%s V%X\n", "SHR", x);
            arch->V[0xF] = arch->V[x] & 1;
            arch->V[x] >>= 1;
            break;
        case 0x7:
            printf("%s V%X, V%X\n", "SUBN", x, y);
            arch->V[0xF] = (arch->V[y] > arch->V[x]) ? 1 : 0;
            arch->V[x] = arch->V[y] - arch->V[x];
            break;
        case 0xE:
            printf("%s V%X\n", "SHL", x);
            arch->V[0xF] = (arch->V[x] & 0x80) ? 1 : 0;
            arch->V[x] <<= 1;
            break;
        }
        break;

    case 0x9000:
        // Skip next instruction if Vx != Vy
        x = (inst >> 8) & 0xF;
        y = (inst >> 4) & 0xF;
        printf("%s V%X, V%X\n", "SNE", x, y);
        if (arch->V[x] != arch->V[y]) {
            arch->PC += 2;
        }
        break;

    case 0xA000:
        // Set I = nnn
        nnn = inst & 0xFFF;
        printf("%s I, %03X\n", "LD", nnn);
        arch->I = nnn;
        break;

    case 0xB000:
        // Jump to location nnn + V0
        nnn = inst & 0xFFF;
        printf("%s V0, %03X\n", "JP", nnn);
        arch->PC = nnn + arch->V[0];
        return; // Don't increment PC

    case 0xC000:
        // Set Vx = random byte AND kk
        x = (inst >> 8) & 0xF;
        kk = inst & 0xFF;
        printf("%s V%X, %02X\n", "RND", x, kk);
        arch->V[x] = (rand() % 256) & kk;
        break;

    case 0xD000: {
        // Display n-byte sprite starting at memory location I at (Vx, Vy), set VF =
        // collision
        x = (inst >> 8) & 0xF;
        y = (inst >> 4) & 0xF;
        n = inst & 0xF;
        printf("%s V%X, V%X, %X\n", "DRW", x, y, n);

        uint8_t x_coord = arch->V[x] % DISPLAY_WIDTH;
        uint8_t y_coord = arch->V[y] % DISPLAY_HEIGHT;
        arch->V[0xF] = 0;

        for (int row = 0; row < n; row++) {
            uint8_t sprite_byte = arch->memory[arch->I + row];

            for (int col = 0; col < 8; col++) {
                uint8_t sprite_pixel = (sprite_byte >> (7 - col)) & 1;
                int screen_x = (x_coord + col) % DISPLAY_WIDTH;
                int screen_y = (y_coord + row) % DISPLAY_HEIGHT;
                int screen_index = screen_y * DISPLAY_WIDTH + screen_x;

                if (sprite_pixel) {
                    if (arch->display[screen_index]) {
                        arch->V[0xF] = 1; // Collision detected
                    }
                    arch->display[screen_index] ^= 1;
                }
            }
        }
        break;
    }

    case 0xE000:
        x = (inst >> 8) & 0xF;
        kk = inst & 0xFF;

        if (kk == 0x9E) {
            // Skip next instruction if key with the value of Vx is pressed
            printf("%s V%X\n", "SKP", x);
            if (arch->key[arch->V[x] & 0xF]) {
                arch->PC += 2;
            }
        } else if (kk == 0xA1) {
            // Skip next instruction if key with the value of Vx is not pressed
            printf("%s V%X\n", "SKNP", x);
            if (!arch->key[arch->V[x] & 0xF]) {
                arch->PC += 2;
            }
        }
        break;

    case 0xF000:
        x = (inst >> 8) & 0xF;
        kk = inst & 0xFF;

        switch (kk) {
        case 0x07:
            printf("%s V%X, DT\n", "LD", x);
            arch->V[x] = arch->delay;
            break;
        case 0x0A: {
            printf("%s V%X, K\n", "LD", x);
            // Wait for a key press, store the value of the key in Vx
            int key_pressed = -1;
            for (int i = 0; i < 16; i++) {
                if (arch->key[i]) {
                    key_pressed = i;
                    break;
                }
            }
            if (key_pressed >= 0) {
                arch->V[x] = key_pressed;
            } else {
                // No key pressed, repeat this instruction
                arch->PC -= 2;
            }
            break;
        }
        case 0x15:
            printf("%s DT, V%X\n", "LD", x);
            arch->delay = arch->V[x];
            break;
        case 0x18:
            printf("%s ST, V%X\n", "LD", x);
            arch->sound = arch->V[x];
            break;
        case 0x1E:
            printf("%s I, V%X\n", "ADD", x);
            arch->I += arch->V[x];
            break;
        case 0x29:
            printf("%s F, V%X\n", "LD", x);
            // Set I = location of sprite for digit Vx
            arch->I = (arch->V[x] & 0xF) * 5;
            break;
        case 0x33: {
            printf("%s B, V%X\n", "LD", x);
            // Store BCD representation of Vx in memory locations I, I+1, and I+2
            uint8_t value = arch->V[x];
            arch->memory[arch->I] = value / 100;
            arch->memory[arch->I + 1] = (value / 10) % 10;
            arch->memory[arch->I + 2] = value % 10;
            break;
        }
        case 0x55:
            printf("%s [I], V%X\n", "LD", x);
            // Store registers V0 through Vx in memory starting at location I
            for (int i = 0; i <= x; i++) {
                arch->memory[arch->I + i] = arch->V[i];
            }
            break;
        case 0x65:
            printf("%s V%X, [I]\n", "LD", x);
            // Read registers V0 through Vx from memory starting at location I
            for (int i = 0; i <= x; i++) {
                arch->V[i] = arch->memory[arch->I + i];
            }
            break;
        }
        break;
    }

    arch->PC += 2; // Increment program counter for most instructions
}

void update_timers(struct chip_8 *arch) {
    if (arch->delay > 0) {
        arch->delay--;
    }
    if (arch->sound > 0) {
        arch->sound--;
    }
}

void handle_input(struct chip_8 *arch, SDL_Event *event) {
    const bool *keyboard_state = SDL_GetKeyboardState(NULL);

    // Update key states
    for (int i = 0; i < 16; i++) {
        arch->key[i] = keyboard_state[key_map[i]] ? 1 : 0;
    }
}

void render_display(struct chip_8 *arch, struct sdl_context *sdl) {
    // Clear screen with black
    SDL_SetRenderDrawColor(sdl->renderer, 0, 0, 0, 255);
    SDL_RenderClear(sdl->renderer);

    // Set white color for pixels
    SDL_SetRenderDrawColor(sdl->renderer, 255, 255, 255, 255);

    // Draw each pixel as a rectangle
    SDL_FRect rect;
    rect.w = WINDOW_WIDTH / (float)DISPLAY_WIDTH;
    rect.h = WINDOW_HEIGHT / (float)DISPLAY_HEIGHT;

    for (int y = 0; y < DISPLAY_HEIGHT; y++) {
        for (int x = 0; x < DISPLAY_WIDTH; x++) {
            if (arch->display[y * DISPLAY_WIDTH + x]) {
                rect.x = x * rect.w;
                rect.y = y * rect.h;
                SDL_RenderFillRect(sdl->renderer, &rect);
            }
        }
    }

    SDL_RenderPresent(sdl->renderer);
}

void audio_callback(void *userdata, SDL_AudioStream *stream,
                    int additional_amount, int total_amount) {
    struct chip_8 *arch = (struct chip_8 *)userdata;

    if (arch->sound > 0) {
        // Generate a simple square wave beep
        static float phase = 0.0f;
        const float frequency = 440.0f; // A note
        const float sample_rate = 44100.0f;
        const float amplitude = 0.3f;

        float samples[1024];
        int num_samples = additional_amount / sizeof(float);

        for (int i = 0; i < num_samples; i++) {
            samples[i] = amplitude * (phase < 0.5f ? 1.0f : -1.0f);
            phase += frequency / sample_rate;
            if (phase >= 1.0f)
                phase -= 1.0f;
        }

        SDL_PutAudioStreamData(stream, samples, additional_amount);
    }
}

int init_sdl(struct sdl_context *sdl) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        printf("SDL_Init error: %s\n", SDL_GetError());
        return 0;
    }

    sdl->window = SDL_CreateWindow("CHIP-8 Emulator", WINDOW_WIDTH, WINDOW_HEIGHT,
                                   SDL_WINDOW_RESIZABLE);
    if (!sdl->window) {
        printf("SDL_CreateWindow error: %s\n", SDL_GetError());
        SDL_Quit();
        return 0;
    }

    sdl->renderer = SDL_CreateRenderer(sdl->window, NULL);
    if (!sdl->renderer) {
        printf("SDL_CreateRenderer error: %s\n", SDL_GetError());
        SDL_DestroyWindow(sdl->window);
        SDL_Quit();
        return 0;
    }

    // Setup audio
    SDL_AudioSpec audio_spec = {
        .format = SDL_AUDIO_F32, .channels = 1, .freq = 44100};

    sdl->audio_device =
        SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &audio_spec);
    if (!sdl->audio_device) {
        printf("SDL_OpenAudioDevice error: %s\n", SDL_GetError());
    } else {
        sdl->audio_stream = SDL_CreateAudioStream(&audio_spec, &audio_spec);
        if (sdl->audio_stream) {
            SDL_SetAudioStreamGetCallback(sdl->audio_stream, audio_callback, NULL);
            SDL_BindAudioStream(sdl->audio_device, sdl->audio_stream);
            SDL_ResumeAudioDevice(sdl->audio_device);
        }
    }

    return 1;
}

void cleanup_sdl(struct sdl_context *sdl) {
    if (sdl->audio_stream) {
        SDL_DestroyAudioStream(sdl->audio_stream);
    }
    if (sdl->audio_device) {
        SDL_CloseAudioDevice(sdl->audio_device);
    }
    if (sdl->renderer) {
        SDL_DestroyRenderer(sdl->renderer);
    }
    if (sdl->window) {
        SDL_DestroyWindow(sdl->window);
    }
    SDL_Quit();
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <rom_file>\n", argv[0]);
        return 1;
    }

    srand(time(NULL));

    struct chip_8 arch;
    struct sdl_context sdl = {0};

    init_arch(&arch);

    if (read_program(&arch, argv[1]) < 0) {
        return 1;
    }

    if (!init_sdl(&sdl)) {
        return 1;
    }

    if (sdl.audio_stream) {
        SDL_SetAudioStreamGetCallback(sdl.audio_stream, audio_callback, &arch);
    }

    bool running = true;

    // --- IMPOSTAZIONI DI TIMING PER RISOLVERE IL PROBLEMA ---
    const int instructions_per_frame = 10; // Velocità della CPU: 10 istruzioni per frame
    const float target_fps = 60.0f;
    const float frame_time_ms = 1000.0f / target_fps;
    Uint64 last_time = SDL_GetTicks();
    // --- FINE IMPOSTAZIONI ---

    printf("CHIP-8 Emulator started. Controls:\n");
    printf("1234  -->  1234\n");
    printf("QWER  -->  4567\n");
    printf("ASDF  -->  890A\n");
    printf("ZXCV  -->  BCEF\n");
    printf("Press ESC to exit\n");

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            } else if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_ESCAPE) {
                    running = false;
                }
            }
            handle_input(&arch, &event);
        }

        Uint64 current_time = SDL_GetTicks();
        // Esegui il ciclo solo una volta ogni frame (circa 16.6 ms)
        if (current_time - last_time >= frame_time_ms) {
            last_time = current_time;

            // Esegui un numero controllato di istruzioni
            for (int i = 0; i < instructions_per_frame; ++i) {
                uint16_t instruction = fetch_instr(&arch);
                execute_instruction(&arch, instruction);
            }

            // Aggiorna timer e schermo solo una volta per frame
            update_timers(&arch);
            render_display(&arch, &sdl);
        }
    }

    cleanup_sdl(&sdl);
    return 0;
}