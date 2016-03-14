#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <ncurses.h>

#define pgm_read_byte(a)		*(a)
#define min(a, b)			((a) < (b)? (a): (b))
#define max(a, b)			((a) > (b)? (a): (b))
#define _BV(i)				(1 << (i))

#define __flag_none                  (0)
#define __flag_color_invert          (1 << 0)
#define __flag_h_mirror              (1 << 1)
#define __flag_v_mirror              (1 << 2)
#define __flag_black                 (1 << 3)
#define __flag_white                 (1 << 4)

#define WIDTH		128
#define HEIGHT		64
static char fb[WIDTH * HEIGHT];

#define WHITE                        1
#define BLACK                        2
static uint8_t sBuffer[WIDTH*HEIGHT/8];

static int update = 0;

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


static void
update_screen(void)
{
	int x, y;

	if (!update)
		return;

	update = 0;

	memset(fb, ' ', sizeof(fb));

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

void drawBitmap(int16_t x, int16_t y, const uint8_t *bitmap, uint8_t w, uint8_t h, uint8_t color)
{
	uint8_t ws, hs;

  // no need to dar at all of we're offscreen
  if (x+w < 0 || x > WIDTH-1 || y+h < 0 || y > HEIGHT-1)
    return;

  int yOffset = abs(y) % 8;
  int sRow = y / 8;
  if (y < 0) {
    sRow--;
    yOffset = 8 - yOffset;
  }
  int rows = h/8;
  if (h%8!=0) rows++;
  for (int a = 0; a < rows; a++) {
    int bRow = sRow + a;
    if (bRow > (HEIGHT/8)-1) break;
    if (bRow > -2) {
      for (int iCol = 0; iCol<w; iCol++) {
        if (iCol + x > (WIDTH-1)) break;
        if (iCol + x >= 0) {
          if (bRow >= 0) {
            if      (color == WHITE) sBuffer[ (bRow*WIDTH) + x + iCol ] |= pgm_read_byte(bitmap+(a*w)+iCol) << yOffset;
            else if (color == BLACK) sBuffer[ (bRow*WIDTH) + x + iCol ] &= ~(pgm_read_byte(bitmap+(a*w)+iCol) << yOffset);
            else                     sBuffer[ (bRow*WIDTH) + x + iCol ] ^= pgm_read_byte(bitmap+(a*w)+iCol) << yOffset;
          }
          if (yOffset && bRow<(HEIGHT/8)-1 && bRow > -2) {
            if      (color == WHITE) sBuffer[ ((bRow+1)*WIDTH) + x + iCol ] |= pgm_read_byte(bitmap+(a*w)+iCol) >> (8-yOffset);
            else if (color == BLACK) sBuffer[ ((bRow+1)*WIDTH) + x + iCol ] &= ~(pgm_read_byte(bitmap+(a*w)+iCol) >> (8-yOffset));
            else                     sBuffer[ ((bRow+1)*WIDTH) + x + iCol ] ^= pgm_read_byte(bitmap+(a*w)+iCol) >> (8-yOffset);
          }
        }
      }
    }
  }
}




void
blit_image(int16_t px, int16_t py, const uint8_t *img, const uint8_t *mask,
	   uint8_t flags)
{
	uint8_t w, h;
	const uint8_t *p = img + 2;

	w = img[0];
	h = img[1];
	drawBitmap(px, py, p, w, h, WHITE);

	update = 1;
}

void
blit_image_frame(int16_t px, int16_t py, const uint8_t *img,
		 const uint8_t *mask, uint8_t nr, uint8_t flags)
{
	uint8_t w, h;
	const uint8_t *p;

	w = img[0];
	h = img[1];
	p = &img[(w * ((h + 7) / 8)) * nr];
	drawBitmap(px, py, p + 2, w, h, WHITE);
	update = 1;
}

void
draw_hline(uint8_t x, uint8_t y, uint8_t w)
{
  uint8_t color = WHITE;
  // Do bounds/limit checks
  if (y < 0 || y >= HEIGHT) {
    return;
  }

  // make sure we don't try to draw below 0
  if (x < 0) {
    w += x;
    x = 0;
  }

  // make sure we don't go off the edge of the display
  if ((x + w) > WIDTH) {
    w = (WIDTH - x);
  }

  // if our width is now negative, punt
  if (w <= 0) {
    return;
  }

  // buffer pointer plus row offset + x offset
  register uint8_t *pBuf = sBuffer + ((y/8) * WIDTH) + x;

  // pixel mask
  register uint8_t mask = 1 << (y&7);

  switch (color)
  {
    case WHITE:
      while(w--) {
        *pBuf++ |= mask;
      };
      break;

    case BLACK:
      mask = ~mask;
      while(w--) {
        *pBuf++ &= mask;
      };
      break;
  }

}

void drawPixel(int x, int y, uint8_t color)
{
  uint8_t row = (uint8_t)y / 8;
  if (color)
  {
    sBuffer[(row*WIDTH) + (uint8_t)x] |=   _BV((uint8_t)y % 8);
  }
  else
  {
    sBuffer[(row*WIDTH) + (uint8_t)x] &= ~ _BV((uint8_t)y % 8);
  }
}


void
draw_vline(uint8_t x, uint8_t y, uint8_t h)
{
  int color = WHITE;
  int end = y+h;
  for (int a = max(0,y); a < min(end,HEIGHT); a++)
  {
    drawPixel(x,a,color);
  }

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

void
clear_screen(void)
{
	memset(sBuffer, 0, sizeof(sBuffer));
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

	memset(sBuffer, 0, sizeof(sBuffer));
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
