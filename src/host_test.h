#ifndef _HOST_TEST_H
#define _HOST_TEST_H

#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#define PROGMEM
#define pgm_read_byte_near(a)		*(a)

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
extern void blit_image(uint8_t x,
		       uint8_t y,
		       const uint8_t *img,
		       uint8_t flags);
extern void blit_image_frame(uint8_t x,
			     uint8_t y,
			     const uint8_t *img,
			     uint8_t nr,
			     uint8_t flags);

#ifdef __cplusplus
}
#endif

#endif
