#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

#include <array>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

int SCALE = 2;

#define MODE_MAPS 0
#define MODE_TILES 1

// #define MODE MODE_TILES
#define MODE MODE_MAPS

int w = 512;
int h = 512;

#define NATIVE_TILE_SIZE 8
#define PIXELS_PER_ROW 8
#define RENDERED_TILE_SIZE NATIVE_TILE_SIZE * SCALE
#define PIXELS_PER_TILE 64
#define BITS_PER_PIXEL 2
#define BYTES_PER_TILE PIXELS_PER_ROW * BITS_PER_PIXEL
#define TILES_PER_ROW_IN_MAP 32

#if MODE == MODE_MAPS
	#define MAX_TILES TILES_PER_ROW_IN_MAP * TILES_PER_ROW_IN_MAP
#else
	#define MAX_TILES 384
#endif

std::vector<SDL_Texture *> textures(MAX_TILES);
std::vector<std::vector<uint32_t>> pixels_list;

SDL_Window *window;
SDL_Renderer *renderer;

uint8_t get_colour(int bit, uint8_t *row)
{
	uint8_t bit_n_values[2];

	bit_n_values[0] = !!(row[0] & (0x80 >> bit));
	bit_n_values[1] = !!(row[1] & (0x80 >> bit));

	uint8_t combined = 0;
	combined |= bit_n_values[1] << 1;
	combined |= bit_n_values[0];

	return combined;
}

void get_tile_from_data(uint8_t *vram, int tile_id)
{
	const int MAX_TILES_W = w / RENDERED_TILE_SIZE;
	int tile_x = tile_id % MAX_TILES_W;
	int tile_y = tile_id / MAX_TILES_W;

	int px_index = 0;
	for (int y = 0; y < NATIVE_TILE_SIZE; y++)
	{
		for (int x = 0; x < NATIVE_TILE_SIZE; x++)
		{
			int tile_byte = (tile_id * BYTES_PER_TILE) + ((y % NATIVE_TILE_SIZE) * 2);

			uint8_t row_data[2];
			row_data[0] = vram[tile_byte];
			row_data[1] = vram[tile_byte + 1];

			uint8_t colour = get_colour(x, row_data);

			uint32_t px_colour = 0;

			switch (colour)
			{
				case 0:
					px_colour = 0xAAAAAA;
					break;
				case 1:
					px_colour = 0x808080;
					break;
				case 2:
					px_colour = 0xC0C0C0;
					break;
				case 3:
					px_colour = 0x000000;
					break;
				default:
					break;
			}

			pixels_list[tile_id][px_index++] = px_colour;
		}
	}

	SDL_UpdateTexture(textures[tile_id], NULL, pixels_list[tile_id].data(), 8 * sizeof(uint32_t));
}

void get_tile_from_map(uint8_t *vram, int i, int tile_x, int tile_y, uint16_t maps_start, uint16_t data_start)
{
	const int MAX_TILES_W = w / RENDERED_TILE_SIZE;

	int tile_index_in_map = (tile_y * TILES_PER_ROW_IN_MAP) + tile_x;
	int tile_id = vram[maps_start + tile_index_in_map];
	int tile_data_addr = tile_id * BYTES_PER_TILE;

	if (tile_id != 32) {
		// printf("x %d  y %d  t %d  h %02X  a 0x%04X\n", tile_x, tile_y, tile_id, tile_id, 0x8000 + maps_start + tile_index_in_map);
	}

	int px_index = 0;

	for (int y = 0; y < NATIVE_TILE_SIZE; y++)
	{
		for (int x = 0; x < NATIVE_TILE_SIZE; x++)
		{
			int row_in_tile = y % NATIVE_TILE_SIZE;
			int row_byte_offset = row_in_tile * 2;
			int tile_byte_addr = data_start + tile_data_addr + row_byte_offset;

			uint8_t row_data[2];
			row_data[0] = vram[tile_byte_addr];
			row_data[1] = vram[tile_byte_addr + 1];

			uint8_t colour = get_colour(x, row_data);

			uint32_t px_colour = 0;

			switch (colour)
			{
				case 0:
					px_colour = 0xAAAAAA;
					break;
				case 1:
					px_colour = 0x808080;
					break;
				case 2:
					px_colour = 0xC0C0C0;
					break;
				case 3:
					px_colour = 0x000000;
					break;
				default:
					break;
			}

			pixels_list[i][px_index++] = px_colour;
		}
	}

	SDL_UpdateTexture(textures[i], NULL, pixels_list[i].data(), 8 * sizeof(uint32_t));
}

void get_maps(uint8_t *vram)
{
	int bg_map, tile_data;
	uint16_t maps_start, tile_data_start;

	bg_map = 0;
	tile_data = 1;

	maps_start = bg_map ? 0x1C00 : 0x1800;
	tile_data_start = tile_data ? 0x0 : 0x1000;

	int i = 0;
	for (int y = 0; y < TILES_PER_ROW_IN_MAP; y++)
	{
		for (int x = 0; x < TILES_PER_ROW_IN_MAP; x++)
		{
			get_tile_from_map(vram, i++, x, y, maps_start, tile_data_start);
		}
	}
}

void get_tiles(uint8_t *vram)
{
	for (int i = 0; i < MAX_TILES; i++)
	{
		get_tile_from_data(vram, i);
	}
}

void set_pixels(uint8_t *vram)
{
#if MODE == MODE_MAPS
	get_maps(vram);
#else
	get_tiles(vram);
#endif
}

int main(int argc, char **argv)
{
	if (argc == 1)
	{
		puts("Usage: ./viewer [path-to-vram.bin]");
		return 1;
	}

	std::cout << std::hex << std::setfill('0');

	uint8_t *vram = (uint8_t *)malloc(0x2000);
	FILE *bin = fopen(argv[1], "rb");
	if (!bin)
	{
		puts("File not found.");
		free(vram);
		exit(1);
	}
	fread(vram, 0x2000, 1, bin);
	fclose(bin);

	SDL_Init(SDL_INIT_VIDEO);
	window = SDL_CreateWindow("DMG VRAM Viewer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h, 0);
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE);

	pixels_list.resize(MAX_TILES);
	for (int i = 0; i < MAX_TILES; i++)
	{
		pixels_list[i].resize(PIXELS_PER_TILE);
		textures[i] = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STATIC, NATIVE_TILE_SIZE, NATIVE_TILE_SIZE);
		pixels_list[i][0] = { 0xFF00FA };
		SDL_UpdateTexture(textures[i], NULL, pixels_list[i].data(), NATIVE_TILE_SIZE * sizeof(uint32_t));
	}

	set_pixels(vram);
	free(vram);

	bool run = true;
	SDL_Event e;
	while (run)
	{
		if (SDL_PollEvent(&e))
		{
			switch (e.type)
			{
				case SDL_QUIT:
					run = false;
					break;
				case SDL_KEYDOWN:
					if (e.key.keysym.sym == SDLK_q)
					{
						run = false;
					}
					break;
				default:
					break;
			}
		}

		int x = -RENDERED_TILE_SIZE;
		int y = 0;
		SDL_RenderClear(renderer);
		for (int i = 0; i < MAX_TILES; i++)
		{
			if (x == (w - RENDERED_TILE_SIZE)) {
				y += RENDERED_TILE_SIZE;
				x = 0;
			}
			else {
				x += RENDERED_TILE_SIZE;
			}

			SDL_Rect dest = { x, y, RENDERED_TILE_SIZE, RENDERED_TILE_SIZE };

			SDL_RenderCopy(renderer, textures[i], NULL, &dest);
		}
		SDL_RenderPresent(renderer);
		usleep(1667);
	}

	for (int i = 0; i < MAX_TILES; i++)
	{
		SDL_DestroyTexture(textures[i]);
	}

	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();

}
