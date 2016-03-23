#ifndef _HOST_TEST_H
#define _HOST_TEST_H

#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define WIDTH		128
#define HEIGHT		64

#define PROGMEM
#define pgm_read_byte_near(a)		*(a)
#define pgm_read_byte(a)		*(a)

#ifdef __cplusplus
extern "C"
{
#endif

static inline unsigned long millis(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ((ts.tv_sec * 1000) + (ts.tv_nsec / 1000000));
}

extern uint8_t get_inputs(void);
extern void blit_image(int16_t x,
		       int16_t y,
		       const uint8_t *img,
		       const uint8_t *mask,
		       uint8_t flags);
extern void blit_image_frame(int16_t x,
			     int16_t y,
			     const uint8_t *img,
			     const uint8_t *mask,
			     uint8_t nr,
			     uint8_t flags);

extern void draw_hline(uint8_t x, uint8_t y, uint8_t w);
extern void draw_vline(uint8_t x, uint8_t y, uint8_t h);
extern void draw_filled_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h);
extern void draw_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h);
extern void clear_screen(void);
#ifdef __cplusplus
}
#endif

#endif
