#define SDL_MAIN_HANDLED
#ifdef WIN32
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif

#include <stdbool.h>
#include <stdint.h>

#include <array>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using u8 = uint8_t;
using u32 = uint32_t;

#define TEXTURE_WIDTH 256

#ifdef INCLUDE_SIGNED_TILES
#define TEXTURE_HEIGHT 512
#else
#define TEXTURE_HEIGHT 256
#endif

static constexpr int kTextureSize{TEXTURE_WIDTH * TEXTURE_HEIGHT};

void print_usage() { puts("Usage: ./viewer [path-to-vram.bin]"); }

/* Returns false on failure. */
bool process_args(int argc, char **argv) {
  if (argc == 1) {
    print_usage();
    return false;
  }
  return true;
}

u8 *read_file(const char *path) {
  FILE *stream{fopen(path, "rb")};
  u8 *buffer{nullptr};

  if (!stream) {
    fprintf(stderr, "File not found: '%s'\n", path);
    return nullptr;
  }

  buffer = (u8 *)malloc(0x2000);
  if (!buffer) {
    fprintf(stderr, "Failed to allocate memory\n");
    fclose(stream);
    return nullptr;
  }

  auto read_bytes = fread(buffer, 1, 0x2000, stream);
  if (read_bytes != 0x2000) {
    fprintf(stderr, "Failed to read 0x2000 bytes (read %zd)\n", read_bytes);
    fclose(stream);
    free(buffer);
    return nullptr;
  }

  fclose(stream);

  return buffer;
}

bool create_window(SDL_Window *&window, SDL_Renderer *&renderer) {
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    fprintf(stderr, "SDL failed to initialise: %s\n", SDL_GetError());
    return false;
  }

  window = SDL_CreateWindow("DMG VRAM Viewer", SDL_WINDOWPOS_CENTERED,
                            SDL_WINDOWPOS_CENTERED, 512, 512, 0);

  if (!window) {
    fprintf(stderr, "SDL failed to create window: %s\n", SDL_GetError());
    SDL_Quit();
    return false;
  }

  renderer = SDL_CreateRenderer(
      window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE);
  if (!renderer) {
    fprintf(stderr, "SDL failed to create renderer: %s\n", SDL_GetError());
    SDL_DestroyWindow(window);
    SDL_Quit();
    return false;
  }

  return true;
}

std::vector<u32> read_tiles(u8 *vram) {
  u32 palette[4] = {0xFFBECAB1, 0xFF7F4C33, 0xFFA06F86, 0xFF000000};

  std::vector<u32> pixels(kTextureSize);
  std::fill(pixels.begin(), pixels.end(), 0xFFFF00FF);

  for (int map_x = 0; map_x < 32; map_x++) {
    /* Unsigned tile indices. */
    for (int map_y = 0; map_y < 32; map_y++) {
      int map_index_offset = 0x1800 + (map_y * 32 + map_x);
      int tile_offset = vram[map_index_offset] * 16;
      const u8 *tile = &vram[tile_offset];
      for (int y = 0; y < 8; y++) {
        const u8 *row = &tile[y * 2];
        for (int x = 0; x < 8; x++) {
          u32 colour = 0xFF000000;

          u8 hi_bit = !!(row[1] & (0x80 >> x));
          u8 lo_bit = !!(row[0] & (0x80 >> x));

          u8 pal_id = (hi_bit << 1) | lo_bit;

          pixels[map_y * 8 * TEXTURE_WIDTH + map_x * 8 + y * TEXTURE_WIDTH +
                 x] = palette[pal_id];
        }
      }
    }
    /* Signed tile indices. */
#ifdef INCLUDE_SIGNED_TILES
    for (int map_y = 32; map_y < 64; map_y++) {
      int map_index_offset = 0x1C00 + (map_y * 32 + map_x);
      int tile_offset = int8_t(vram[map_index_offset]) * 16;
      const u8 *tile = &vram[0x1000 + tile_offset];
      for (int y = 0; y < 8; y++) {
        const u8 *row = &tile[y * 2];
        for (int x = 0; x < 8; x++) {
          u32 colour = 0xFF000000;

          u8 hi_bit = !!(row[1] & (0x80 >> x));
          u8 lo_bit = !!(row[0] & (0x80 >> x));

          u8 pal_id = (hi_bit << 1) | lo_bit;
          u8 shade = palette[pal_id];

          colour |= (shade << 16) | (shade << 8) | shade;

          pixels[map_y * 8 * TEXTURE_WIDTH + map_x * 8 + y * TEXTURE_WIDTH +
                 x] = colour;
        }
      }
    }
#endif
  }
  return pixels;
}

int main(int argc, char **argv) {
  if (!process_args(argc, argv)) {
    return EXIT_FAILURE;
  }

  u8 *vram = read_file(argv[1]);
  if (!vram) {
    return EXIT_FAILURE;
  }

  SDL_Window *window;
  SDL_Renderer *renderer;
  if (!create_window(window, renderer)) {
    free(vram);
    return EXIT_FAILURE;
  }

  SDL_Texture *texture{SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                         SDL_TEXTUREACCESS_STATIC,
                                         TEXTURE_WIDTH, TEXTURE_HEIGHT)};
  if (!texture) {
    fprintf(stderr, "SDL failed to create texture: %s\n", SDL_GetError());
  }

  std::vector<u32> pixels = read_tiles(vram);
  free(vram);

  SDL_UpdateTexture(texture, NULL, pixels.data(), TEXTURE_WIDTH * 4);

  bool run = true;
  SDL_Event e;
  while (run) {
    if (SDL_PollEvent(&e)) {
      switch (e.type) {
      case SDL_QUIT:
        run = false;
        break;
      case SDL_KEYDOWN:
        if (e.key.keysym.sym == SDLK_q) {
          run = false;
        }
        break;
      default:
        break;
      }
    }

    SDL_RenderCopy(renderer, texture, NULL, NULL);

    SDL_RenderPresent(renderer);
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
  }

  SDL_DestroyTexture(texture);

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
