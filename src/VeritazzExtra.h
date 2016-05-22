#ifndef VeritazzExtra_h
#define VeritazzExtra_h

#include "Arduboy.h"

#ifdef HOST_TEST
extern uint8_t get_inputs(void);
#endif

#define __flag_none                  (0)
#define __flag_color_invert          (1 << 0)
#define __flag_h_mirror              (1 << 1)
#define __flag_v_mirror              (1 << 2)
#define __flag_black                 (1 << 3)
#define __flag_white                 (1 << 4)

#define img_width(i)                 pgm_read_byte((i) + 0)
#define img_height(i)                pgm_read_byte((i) + 1)
#define img_offset(i, o)             pgm_read_word((i) + 2 + ((o) * 2))

class VeritazzExtra: public Arduboy
{
public:
	VeritazzExtra(const uint8_t *xlate);

	void drawBitmap(int16_t x, int16_t y, const uint8_t *img, uint8_t w, uint8_t h,
			uint8_t color);
	void drawImage(int16_t x, int16_t y, const uint8_t *bitmap,
			const uint8_t *mask, uint8_t color);

	void drawPackedImage(int16_t x, int16_t y, const uint8_t *img, uint8_t w, uint8_t h,
				uint8_t color);

	void drawImageFrame(int16_t x, int16_t y, const uint8_t *img,
				   const uint8_t *mask, uint8_t nr,
				   uint8_t color);

private:
	uint8_t nextData(uint16_t o);
	void advanceNibbles(uint16_t nibbles);
	void nextToken(void);
	void setStartByte(const uint8_t *data, uint16_t offset);
	void unpackBytes(uint8_t *buf, uint16_t len);
	uint16_t nibble;
	uint8_t last_token;
	const uint8_t *packed;
	const uint8_t *xlate;
	uint8_t count;
	uint8_t value;
	uint16_t advance; /* how many nibbles to advance for next token */
};

#endif
