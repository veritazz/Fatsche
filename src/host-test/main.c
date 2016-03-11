#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include <ncurses.h>

#define __flag_none                  (0)
#define __flag_color_invert          (1 << 0)
#define __flag_h_mirror              (1 << 1)
#define __flag_v_mirror              (1 << 2)
#define __flag_black                 (1 << 3)
#define __flag_white                 (1 << 4)

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
blit_image(int16_t px, int16_t py, const uint8_t *img, const uint8_t *mask,
	   uint8_t flags)
{
	int16_t x, y;
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
blit_image_frame(int16_t px, int16_t py, const uint8_t *img,
		 const uint8_t *mask, uint8_t nr, uint8_t flags)
{
	int16_t x, y;
	uint8_t w, h, ws, hs;
	uint16_t i = 2;
	uint16_t bit = 0;
	uint16_t byte = 2;
	const uint8_t *p;
	uint8_t value;

	w = img[0];
	h = img[1];

	ws = WIDTH - px;
	hs = HEIGHT - py;
	if (ws > w)
		ws = w;
	if (hs > h)
		hs = h;

	p = &img[(w * ((h + 7) / 8)) * nr];

	for (y = py; y < (py + hs); y++) {
		for (x = px; x < (px + ws); x++) {
			value = p[(y - py) / 8 * w + (x - px) + 2] & (0x80 >> ((y - py) % 8));
			if (value && (flags & __flag_white))
				fb[y * WIDTH + x] = '+';
			else if (value && (flags & __flag_black))
				fb[y * WIDTH + x] = ' ';
			else
				fb[y * WIDTH + x] = value ? '+': ' ';
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
draw_vline(uint8_t x, uint8_t y, uint8_t h)
{
	uint8_t i;

	for (i = y; i < y + h; i++)
		fb[i * WIDTH + x] = '+';
}

void
draw_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h)
{
	draw_hline(x, y, w);
	draw_hline(x, y + h - 1, w);
	draw_vline(x, y, h);
	draw_vline(x + w - 1, y, h);
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
