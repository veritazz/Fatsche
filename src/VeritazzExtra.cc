#include "VeritazzExtra.h"

VeritazzExtra::VeritazzExtra(const uint8_t *xlate)
{
	this->xlate = xlate;
}

void VeritazzExtra::drawBitmap(int16_t x, int16_t y, const uint8_t *img, uint8_t w, uint8_t h,
                               uint8_t color)
{
	// no need to dar at all of we're offscreen
	if (x + w < 0 || x > WIDTH - 1 || y + h < 0 || y > HEIGHT - 1)
		return;

	int yOffset = abs(y) % 8;
	int sRow = y / 8;
	int sCol = 0;
	int eCol = w - (x + w > WIDTH ? (x + w) % WIDTH : 0);
	if (y < 0) {
		sRow--;
		yOffset = 8 - yOffset;
	}
	if (x < 0)
		sCol = abs(x);

	int rows = h / 8;
	if (h % 8 != 0) rows++;
	int a = 0;
	do {
		int bRow = sRow + a;
		if (bRow > (HEIGHT / 8) - 1) break;
		if (bRow >= 0) {
			for (int iCol = sCol; iCol < eCol; iCol++) {
				if (iCol + x > (WIDTH - 1)) break;
				if      (color == WHITE) this->sBuffer[ (bRow * WIDTH) + x + iCol ] |= pgm_read_byte(img + (a * w) + iCol) << yOffset;
				else if (color == BLACK) this->sBuffer[ (bRow * WIDTH) + x + iCol ] &= ~(pgm_read_byte(img + (a * w) + iCol) << yOffset);
				else                     this->sBuffer[ (bRow * WIDTH) + x + iCol ] ^= pgm_read_byte(img + (a * w) + iCol) << yOffset;
			}
		}

		if (yOffset && bRow < (HEIGHT / 8) - 1 && bRow > -2) {
			for (int iCol = sCol; iCol < eCol; iCol++) {
				if (iCol + x > (WIDTH - 1)) break;
				if      (color == WHITE) this->sBuffer[ ((bRow + 1)*WIDTH) + x + iCol ] |= pgm_read_byte(img + (a * w) + iCol) >> (8 - yOffset);
				else if (color == BLACK) this->sBuffer[ ((bRow + 1)*WIDTH) + x + iCol ] &= ~(pgm_read_byte(img + (a * w) + iCol) >> (8 - yOffset));
				else                     this->sBuffer[ ((bRow + 1)*WIDTH) + x + iCol ] ^= pgm_read_byte(img + (a * w) + iCol) >> (8 - yOffset);
			}
		}
	} while (++a < rows);
}

void VeritazzExtra::drawImage(int16_t x, int16_t y, const uint8_t *img,
                              const uint8_t *mask, uint8_t color)
{
	uint16_t ioffset;
	uint8_t w, h, iunpack;

	w = img_width(img);
	h = img_height(img);
	ioffset = img_offset(img, 0);
	iunpack = !!(ioffset & 0x8000);
	ioffset &= 0x7fff;

	if (mask) {
		uint8_t munpack;
		uint16_t moffset;
		moffset = img_offset(mask, 0);
		munpack = !!(moffset & 0x8000);
		moffset &= 0x7fff;
		if (munpack)
			drawPackedImage(x, y, mask + moffset, w, h, BLACK);
		else
			drawBitmap(x, y, mask + moffset, w, h, BLACK);
	}

	if (iunpack)
		drawPackedImage(x, y, img + ioffset, w, h, WHITE);
	else
		drawBitmap(x, y, img + ioffset, w, h, WHITE);
}

uint8_t VeritazzExtra::nextData(uint16_t o)
{
	uint8_t data;
	static const uint8_t shifts[] = {4, 0};

	data = pgm_read_byte(&packed[(nibble + o) / 2]);
	return (data >> shifts[(nibble + o) & 0x1]) & 0xf;
}

void VeritazzExtra::advanceNibbles(uint16_t nibbles)
{
	nibble += nibbles;
}

void VeritazzExtra::nextToken(void)
{
	last_token = nextData(0);
	switch (last_token) {
	case 0xf:
		/* 8 bit raw data follows */
		value = (nextData(1) << 4) | nextData(2);
		advance = 3;
		count = 1;
		break;
	case 0xe:
		/* 8 bit repeat count follows */
		/* 8 bit raw data follows */
		count = (nextData(1) << 4) | nextData(2);
		value = (nextData(3) << 4) | nextData(4);
		advance = 5;
		break;
	case 0xd:
		/* 8 bit repeat count follows */
		/* 4 bit keyed data follows */
		count = (nextData(1) << 4) | nextData(2);
		value = nextData(3);
		advance = 4;
		break;
	case 0xc:
		/* 8 bit repeat count follows */
		/* repeat * 8 bit raw data follows */
		count = (nextData(1) << 4) | nextData(2);
		value = (nextData(3) << 4) | nextData(4);
		advance = count * 2 + 3;
		break;
	default:
		advance = 1;
		count = 1;
		break;
	}
}

void VeritazzExtra::setStartByte(const uint8_t *data, uint16_t offset)
{
	uint16_t o = 0;

	nibble = 0;
	packed = data;

	for (;;) {
		nextToken();
		if (o + count > offset)
			break;
		o += count;
		advanceNibbles(advance);
	}
	if (o + count > offset) {
		uint8_t diff = o + count - offset;
		switch (last_token) {
		case 0xe:
			/* 8 bit repeat count follows */
			/* 8 bit raw data follows */
			count = diff;
			break;
		case 0xd:
			/* 8 bit repeat count follows */
			/* 4 bit keyed data follows */
			count = diff;
			break;
		case 0xc:
			/* 8 bit repeat count follows */
			/* repeat * 8 bit raw data follows */
			advance = (count - diff) * 2;// + 3;
			count = diff;
			advanceNibbles(advance);
			value = (nextData(3) << 4) | nextData(4);
			advance = 0;
			break;
		}
	}
}

void VeritazzExtra::unpackBytes(uint8_t *buf, uint16_t len)
{
	uint16_t i = 0;

	for (;;) {
		switch (last_token) {
		case 0xf:
			/* 8 bit raw data follows */
			buf[i] = value;
			break;
		case 0xe:
			/* 8 bit repeat count follows */
			/* 8 bit raw data follows */
			buf[i] = value;
			break;
		case 0xd:
			/* 8 bit repeat count follows */
			/* 4 bit keyed data follows */
			buf[i] = pgm_read_byte(&xlate[value]);
			break;
		case 0xc:
			/* 8 bit repeat count follows */
			/* repeat * 8 bit raw data follows */
			buf[i] = value;
			advanceNibbles(2);
			value = (nextData(3) << 4) | nextData(4);
			advance = 3;
			break;
		default:
			buf[i] = pgm_read_byte(&xlate[last_token]);
			break;
		}
		count--;
		i++;
		if (count == 0) {
			advanceNibbles(advance);
			nextToken();
		}
		if (i == len)
			break;
	}
}

void VeritazzExtra::drawPackedImage(int16_t x, int16_t y, const uint8_t *img,
                                    uint8_t w, uint8_t h, uint8_t color)
{
	// no need to dar at all of we're offscreen
	if (x + w < 0 || x > WIDTH - 1 || y + h < 0 || y > HEIGHT - 1)
		return;

	int yOffset = abs(y) % 8;
	int sRow = y / 8;
	int sCol = 0;
	int eCol = w - (x + w > WIDTH ? (x + w) % WIDTH : 0);
	if (y < 0) {
		sRow--;
		yOffset = 8 - yOffset;
	}
	if (x < 0)
		sCol = abs(x);
	uint8_t buf[w];

	setStartByte(img, 0);

	int rows = h / 8;
	if (h % 8 != 0) rows++;
	for (int a = 0; a < rows; a++) {
		int bRow = sRow + a;
		unpackBytes(buf, w);
		if (bRow > (HEIGHT / 8) - 1) break;
		if (bRow >= 0) {
			for (int iCol = sCol; iCol < eCol; iCol++) {
				if (iCol + x > (WIDTH - 1)) break;
				if      (color == WHITE) this->sBuffer[ (bRow * WIDTH) + x + iCol ] |= buf[iCol] << yOffset;
				else if (color == BLACK) this->sBuffer[ (bRow * WIDTH) + x + iCol ] &= ~(buf[iCol] << yOffset);
				else                     this->sBuffer[ (bRow * WIDTH) + x + iCol ] ^= buf[iCol] << yOffset;
			}
		}

		if (yOffset && bRow < (HEIGHT / 8) - 1 && bRow > -2) {
			for (int iCol = sCol; iCol < eCol; iCol++) {
				if (iCol + x > (WIDTH - 1)) break;
				if      (color == WHITE) this->sBuffer[ ((bRow + 1)*WIDTH) + x + iCol ] |= buf[iCol] >> (8 - yOffset);
				else if (color == BLACK) this->sBuffer[ ((bRow + 1)*WIDTH) + x + iCol ] &= ~(buf[iCol] >> (8 - yOffset));
				else                     this->sBuffer[ ((bRow + 1)*WIDTH) + x + iCol ] ^= buf[iCol] >> (8 - yOffset);
			}
		}
	}
}

void VeritazzExtra::drawImageFrame(int16_t x, int16_t y, const uint8_t *img,
                                   const uint8_t *mask, uint8_t nr,
                                   uint8_t color)
{
	uint16_t ioffset;
	uint8_t w, h, iunpack;

	w = img_width(img);
	h = img_height(img);
	ioffset = img_offset(img, nr);
	iunpack = !!(ioffset & 0x8000);
	ioffset &= 0x7fff;

	if (mask) {
		uint16_t moffset;
		uint8_t munpack;
		moffset = img_offset(mask, nr);
		munpack = !!(moffset & 0x8000);
		moffset &= 0x7fff;
		if (munpack)
			drawPackedImage(x, y, mask + moffset, w, h, BLACK);
		else
			drawBitmap(x, y, mask + moffset, w, h, BLACK);
	}

	if (iunpack)
		drawPackedImage(x, y, img + ioffset, w, h, WHITE);
	else
		drawBitmap(x, y, img + ioffset, w, h, WHITE);
}
