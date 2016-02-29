#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include <ncurses.h>

#define WIDTH		128
#define HEIGHT		64
static char fb[WIDTH * HEIGHT];

static void
update_screen(void)
{
	int x, y;

	printf("\x1b[%d;%df", 0, 0);
	for (y = 0; y < HEIGHT; y++) {
		for (x = 0; x < WIDTH; x++)
			printf("%c", fb[y * WIDTH + x]);
		printf("\n\r");
	}
}

static int done = 0;

uint8_t
get_inputs(void)
{
	int key;
	uint8_t keys = 0;

	do {
		key = getch();
		switch (key) {
		case KEY_UP:
			keys |= 1;
			break;
		case KEY_DOWN:
			keys |= 2;
			break;
		case KEY_LEFT:
			keys |= 4;
			break;
		case KEY_RIGHT:
			keys |= 8;
			break;
		case 'a':
			keys |= 16;
			break;
		case 'b':
			keys |= 32;
			break;
		case 27:
			done = 1;
			break;
		}
	} while (key != ERR);

	return keys;
}

void
blit_image(uint8_t px, uint8_t py, const uint8_t *img, uint8_t flags)
{
	int x, y;
	uint8_t w, h;
	uint16_t i = 2;
	uint16_t bit = 0;
	uint16_t byte = 2;
	uint16_t o;
	const uint8_t *p = img;

	w = img[0];
	h = img[1];

	for (y = py; y < (py + h); y++) {
		for (x = px; x < (px + w); x++) {
			fb[y * WIDTH + x] = (p[(y - py) / 8 * w + (x - px) + 2] & (0x80 >> ((y - py) % 8))? '+': ' ');
		}
	}
}

void
blit_image_frame(uint8_t px, uint8_t py, const uint8_t *img, uint8_t nr, uint8_t flags)
{
	int x, y;
	uint8_t w, h;
	uint16_t i = 2;
	uint16_t bit = 0;
	uint16_t byte = 2;
	const uint8_t *p;

	w = img[0];
	h = img[1];

	p = &img[(w * ((h + 7) / 8)) * nr];

	for (y = py; y < (py + h); y++) {
		for (x = px; x < (px + w); x++) {
			fb[y * WIDTH + x] = (p[(y - py) / 8 * w + (x - px) + 2] & (0x80 >> ((y - py) % 8))? '+': ' ');
		}
	}
}

void
draw_hline(uint8_t x, uint8_t y, uint8_t w)
{
	uint8_t i;

	for (i = x; i < x + w; i++)
		fb[y * WIDTH + i] = '+';
}

void
draw_filled_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h)
{
	while (h--)
		draw_hline(x, y + h, w);
}

extern void setup(void);
extern void loop(void);

int main(int argc, char *argv[])
{
	int c;
	int key = 0;

	initscr();
	clear();
	nodelay(stdscr, TRUE);
	keypad(stdscr, TRUE);

	memset(fb, ' ', sizeof(fb));
	update_screen();
	setup();
	do {
		loop();
		update_screen();
	} while (!done);
	clrtoeol();
	endwin();
	return 0;
}
