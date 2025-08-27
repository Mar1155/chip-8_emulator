#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

int main(int argc, char *argv[]) {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_Log("SDL_Init error: %s\n", SDL_GetError());
    return 1;
  }
  SDL_Window *window = SDL_CreateWindow("Finestra SDL3", 800, 600, 0);
  if (!window) {
    SDL_Log("SDL_CreateWindow error: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }
  bool done = false;
  while (!done) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT)
        done = true;
    }
    SDL_Delay(16); // Per non bruciare CPU, facoltativo
  }
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
