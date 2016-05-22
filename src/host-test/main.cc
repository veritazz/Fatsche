#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <ncurses.h>
#include "VeritazzExtra.h"

#define __flag_none                  (0)
#define __flag_color_invert          (1 << 0)
#define __flag_h_mirror              (1 << 1)
#define __flag_v_mirror              (1 << 2)
#define __flag_black                 (1 << 3)
#define __flag_white                 (1 << 4)

static char fb[WIDTH * HEIGHT];
static uint8_t *sBuffer;

static void
convert_sbuffer(void)
{
	int i, o, bit, x, y;

	for (i = 0; i < (WIDTH * HEIGHT); i++) {
		o = i / 8;
		bit = i % 8;
		x = o % WIDTH;
		y = (i / (WIDTH * 8)) * 8 + (i % 8);
		fb[y * WIDTH + x] = sBuffer[o] & (0x1 << bit)?'+': ' ';
	}
}


void
update_screen(void)
{
	int x, y;

	convert_sbuffer();
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

extern void setup(void);
extern void loop(void);
extern VeritazzExtra arduboy;

int main(int argc, char *argv[])
{
	int ret;
	int c;
	int key = 0;

	sBuffer = arduboy.getBuffer();

	initscr();
	clear();
	nodelay(stdscr, TRUE);
	keypad(stdscr, TRUE);

	memset(fb, ' ', sizeof(fb));
	update_screen();
	setup();
	do {
		loop();
	} while (!done);
	clrtoeol();
	endwin();
	return 0;
}
