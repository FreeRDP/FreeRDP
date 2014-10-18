/**
 * FreeRDP: A Remote Desktop Protocol Client
 * RLE Compressed Bitmap Stream
 *
 * Copyright 2011 Jay Sorg <jay.sorg@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* do not compile the file directly */

#undef PIXEL_ALIGN
#if BYTESPERPIXEL==2
# define PIXEL_ALIGN uint16
#else
# define PIXEL_ALIGN uint8
#endif

#undef SRCNEXTPIXEL
#define SRCNEXTPIXEL(_src) _src += BYTESPERPIXEL

#undef DESTNEXTPIXEL
#define DESTNEXTPIXEL(_dst, _peol, _row) {_dst += BYTESPERPIXEL; \
	if (_dst >= *(_peol)) {*(_peol) = *(_peol) - _row; _dst = *(_peol) - _row; PREFETCH_WRITE(_dst + PREFETCH_LENGTH); } }


#undef DECREMENTING_LOOP
#define DEREMENTING_LOOP(_value, _dec, _exp) \
	for (; (_value >= 8 * (_dec)); _value -= 8 * (_dec)) { _exp _exp _exp _exp _exp _exp _exp _exp }; \
	switch (_value) { \
	case 7 * (_dec): _exp; \
	case 6 * (_dec): _exp; \
	case 5 * (_dec): _exp; \
	case 4 * (_dec): _exp; \
	case 3 * (_dec): _exp; \
	case 2 * (_dec): _exp; \
	case 1 * (_dec): _exp; \
	}

//	for (; (_value != 0); _value -= _dec) { _exp };

//Line-by-line filling of _count pixels with specified color
//On return:
// _dst adjusted by _count * BYTESPERLPIXEL with respect of vertical flipping
// _count is undefined
// _peol adjusted to next line if necessary
#undef DESTWRITENEXTPIXELS_ANYCOLOR
#define DESTWRITENEXTPIXELS_ANYCOLOR(_count, _pix, _dst, _peol, _row) { _count*= BYTESPERPIXEL; \
		for (;;) { \
			uint32 til_eol = *(_peol) - _dst; \
			if ( _count < til_eol ) { \
				DEREMENTING_LOOP(_count, BYTESPERPIXEL, \
					DESTWRITEPIXEL(_dst, _pix); _dst += BYTESPERPIXEL; ); \
				break; \
			} else if (_count) { \
				*(_peol) = *(_peol) - _row;  \
				PREFETCH_WRITE(*(_peol) - _row); \
				_count-= til_eol; \
				DEREMENTING_LOOP(til_eol, BYTESPERPIXEL, \
					DESTWRITEPIXEL(_dst, _pix); _dst += BYTESPERPIXEL; ); \
				_dst = *(_peol) - _row; \
			} else break; \
		} }



//same as DESTWRITENEXTPIXELS_ANYCOLOR but only for black or white color that allows using of memset that is much faster
#undef DESTWRITENEXTPIXELSBLACKORWHITE
#define DESTWRITENEXTPIXELSBLACKORWHITE(_count, _bw, _dst, _peol, _row) { _count*= BYTESPERPIXEL; \
		for (;;) { \
			const uint32 til_eol = *(_peol) - _dst; \
			if ( _count < til_eol ) { \
				memset((PIXEL_ALIGN*)_dst, _bw, _count); \
				_dst+= _count; \
				break; \
			} else if (_count) { \
				*(_peol) = *(_peol) - _row; \
				PREFETCH_WRITE(*(_peol) - _row); \
				_count -= til_eol; \
				memset((PIXEL_ALIGN*)_dst, _bw, til_eol); \
				_dst = *(_peol) - _row; \
			} else break; \
		} }


#undef DESTWRITENEXTPIXELS
#define DESTWRITENEXTPIXELS(_count, _pix, _dst, _peol, _row) switch (_pix) { \
			case BLACK_PIXEL: \
				DESTWRITENEXTPIXELSBLACKORWHITE(runLength, 0, _dst, _peol, _row); \
				break; \
			case WHITE_PIXEL: \
				DESTWRITENEXTPIXELSBLACKORWHITE(runLength, 0xff, _dst, _peol, _row); \
				break; \
			default: \
				DESTWRITENEXTPIXELS_ANYCOLOR(_count, _pix, _dst, _peol, _row); \
		}


//Line-by-line filling of _count pixel pairs with specified color
//On return:
// _dst adjusted by 2 * _count * BYTESPERLPIXEL with respect of vertical flipping
// _count is undefined
// _peol adjusted to next line if necessary
#undef DESTDOUBLEWRITENEXTPIXELS
#define DESTDOUBLEWRITENEXTPIXELS(_count, _pixA, _pixB, _dst, _peol, _row) { _count*= 2 * BYTESPERPIXEL; \
		for (;;) { \
			uint32 til_eol = *(_peol) - _dst; \
			if ( _count < til_eol ) { \
				DEREMENTING_LOOP(_count, 2 * BYTESPERPIXEL, \
					DESTWRITEPIXEL(_dst, _pixA); _dst+= BYTESPERPIXEL; \
					DESTWRITEPIXEL(_dst, _pixB); _dst+= BYTESPERPIXEL; ); \
				break; \
			} else if (_count) { \
				*(_peol) = *(_peol) - _row;  \
				PREFETCH_WRITE(*(_peol) - _row); \
				if (til_eol % (2 * BYTESPERPIXEL) ) { \
					_count -= til_eol + BYTESPERPIXEL; \
					til_eol -= BYTESPERPIXEL; \
					DEREMENTING_LOOP(til_eol, 2 * BYTESPERPIXEL, \
						DESTWRITEPIXEL(_dst, _pixA); _dst+= BYTESPERPIXEL; \
						DESTWRITEPIXEL(_dst, _pixB); _dst+= BYTESPERPIXEL; ); \
					DESTWRITEPIXEL(_dst, _pixA); \
					_dst = *(_peol) - _row; \
					DESTWRITEPIXEL(_dst, _pixB); \
					if (_row==BYTESPERPIXEL) { \
						*(_peol) = *(_peol) - _row; \
						_dst = *(_peol) - _row; \
					} else { _dst+= BYTESPERPIXEL; } \
				} else {\
					_count-= til_eol; \
					DEREMENTING_LOOP(til_eol, 2 * BYTESPERPIXEL, \
						DESTWRITEPIXEL(_dst, _pixA); _dst+= BYTESPERPIXEL; \
						DESTWRITEPIXEL(_dst, _pixB); _dst+= BYTESPERPIXEL; ); \
					_dst = *(_peol) - _row; \
				} \
			} else break; \
		} }



//Line-by-line copy of _count pixels from _src to _dst
//On return:
// _dst adjusted by _count * BYTESPERLPIXEL with respect of vertical flipping
// _src incremented by _count * BYTESPERLPIXEL
// _count is undefined
// _peol adjusted to next line if necessary
#undef SRCREADNEXTDESTWRITENEXTPIXELS
#define SRCREADNEXTDESTWRITENEXTPIXELS(_count, _src, _dst, _peol, _row) { _count*= BYTESPERPIXEL; \
		for (;;) { \
			const uint32 til_eol = *(_peol) - _dst; \
			if ( _count < til_eol ) { \
				memcpy((PIXEL_ALIGN*)_dst, (const PIXEL_ALIGN*)_src, _count); \
				_dst+= _count; _src+= _count; \
				break; \
			} else if (_count) { \
				*(_peol) = *(_peol) - _row; \
				PREFETCH_READ(_src + til_eol); \
				PREFETCH_WRITE(*(_peol) - _row); \
				_count -= til_eol; \
				memcpy((PIXEL_ALIGN*)_dst, (const PIXEL_ALIGN*)_src, til_eol); \
				_src += til_eol; \
				_dst = *(_peol) - _row; \
			} else break; \
		} }


//Line-by-line copy of _count pixels from _dst + _row to _dst
//On return:
// _dst adjusted by _count * BYTESPERLPIXEL with respect of vertical flipping
// _count is undefined
// _peol adjusted to next line if necessary
#undef DESTWRITENEXTPIXELSFROMPREVROW
#define DESTWRITENEXTPIXELSFROMPREVROW(_count, _dst, _peol, _row) { _count*= BYTESPERPIXEL; \
		for (;;) { \
			const uint32 til_eol = *(_peol) - _dst; \
			if ( _count < til_eol ) { \
				memcpy((PIXEL_ALIGN*)_dst, (const PIXEL_ALIGN*)(_dst + _row), _count); \
				_dst+= _count; \
				break; \
			} else if (_count) { \
				*(_peol) = *(_peol) - _row; \
				PREFETCH_WRITE(*(_peol) - _row); \
				_count -= til_eol; \
				memcpy((PIXEL_ALIGN*)_dst, (const PIXEL_ALIGN*)(_dst + _row), til_eol); \
				_dst = *(_peol) - _row; \
			} else break; \
		} }



//Line-by-line copy of _count pixels from _dst + _row to _dst with xorig by __pel 
//On return:
// _dst adjusted by _count * BYTESPERLPIXEL with respect of vertical flipping
// _count is undefined
// _peol adjusted to next line if necessary
#define XOREDDESTWRITENEXTPIXELSFROMPREVROW(_count, _pel, _dst, _peol, _row) { _count*= BYTESPERPIXEL; \
		for (;;) { \
			uint32 til_eol = *(_peol) - _dst; \
			if ( _count < til_eol ) { \
				DEREMENTING_LOOP(_count, BYTESPERPIXEL, \
						DESTREADPIXEL(temp, _dst + _row); \
						DESTWRITEPIXEL(_dst, temp ^ _pel); \
						_dst+= BYTESPERPIXEL; ); \
				break; \
			} else if (_count) { \
				*(_peol) = *(_peol) - _row; \
				PREFETCH_WRITE(*(_peol) - _row); \
				_count-= til_eol; \
				DEREMENTING_LOOP(til_eol, BYTESPERPIXEL, \
						DESTREADPIXEL(temp, _dst + _row); \
						DESTWRITEPIXEL(_dst, temp ^ _pel); \
						_dst+= BYTESPERPIXEL; ); \
				_dst = *(_peol) - _row; \
			} else break; \
		} }




/**
 * Write a foreground/background image to a destination buffer.
 */
static uint8* WRITEFGBGIMAGE(uint8* pbDest, uint8** ppbDestEndOfLine, uint32 rowDelta,
	uint8 bitmask, PIXEL fgPel, uint8 cBits)
{
	PIXEL xorPixel;
	DESTREADPIXEL(xorPixel, pbDest + rowDelta);
	if (bitmask & g_MaskBit0)
	{
		DESTWRITEPIXEL(pbDest, xorPixel ^ fgPel);
	}
	else
	{
		DESTWRITEPIXEL(pbDest, xorPixel);
	}
	DESTNEXTPIXEL(pbDest, ppbDestEndOfLine, rowDelta);
	cBits = cBits - 1;
	if (cBits > 0)
	{
		DESTREADPIXEL(xorPixel, pbDest + rowDelta);
		if (bitmask & g_MaskBit1)
		{
			DESTWRITEPIXEL(pbDest, xorPixel ^ fgPel);
		}
		else
		{
			DESTWRITEPIXEL(pbDest, xorPixel);
		}
		DESTNEXTPIXEL(pbDest, ppbDestEndOfLine, rowDelta);
		cBits = cBits - 1;
		if (cBits > 0)
		{
			DESTREADPIXEL(xorPixel, pbDest + rowDelta);
			if (bitmask & g_MaskBit2)
			{
				DESTWRITEPIXEL(pbDest, xorPixel ^ fgPel);
			}
			else
			{
				DESTWRITEPIXEL(pbDest, xorPixel);
			}
			DESTNEXTPIXEL(pbDest, ppbDestEndOfLine, rowDelta);
			cBits = cBits - 1;
			if (cBits > 0)
			{
				DESTREADPIXEL(xorPixel, pbDest + rowDelta);
				if (bitmask & g_MaskBit3)
				{
					DESTWRITEPIXEL(pbDest, xorPixel ^ fgPel);
				}
				else
				{
					DESTWRITEPIXEL(pbDest, xorPixel);
				}
				DESTNEXTPIXEL(pbDest, ppbDestEndOfLine, rowDelta);
				cBits = cBits - 1;
				if (cBits > 0)
				{
					DESTREADPIXEL(xorPixel, pbDest + rowDelta);
					if (bitmask & g_MaskBit4)
					{
						DESTWRITEPIXEL(pbDest, xorPixel ^ fgPel);
					}
					else
					{
						DESTWRITEPIXEL(pbDest, xorPixel);
					}
					DESTNEXTPIXEL(pbDest, ppbDestEndOfLine, rowDelta);
					cBits = cBits - 1;
					if (cBits > 0)
					{
						DESTREADPIXEL(xorPixel, pbDest + rowDelta);
						if (bitmask & g_MaskBit5)
						{
							DESTWRITEPIXEL(pbDest, xorPixel ^ fgPel);
						}
						else
						{
							DESTWRITEPIXEL(pbDest, xorPixel);
						}
						DESTNEXTPIXEL(pbDest, ppbDestEndOfLine, rowDelta);
						cBits = cBits - 1;
						if (cBits > 0)
						{
							DESTREADPIXEL(xorPixel, pbDest + rowDelta);
							if (bitmask & g_MaskBit6)
							{
								DESTWRITEPIXEL(pbDest, xorPixel ^ fgPel);
							}
							else
							{
								DESTWRITEPIXEL(pbDest, xorPixel);
							}
							DESTNEXTPIXEL(pbDest, ppbDestEndOfLine, rowDelta);
							cBits = cBits - 1;
							if (cBits > 0)
							{
								DESTREADPIXEL(xorPixel, pbDest + rowDelta);
								if (bitmask & g_MaskBit7)
								{
									DESTWRITEPIXEL(pbDest, xorPixel ^ fgPel);
								}
								else
								{
									DESTWRITEPIXEL(pbDest, xorPixel);
								}
								DESTNEXTPIXEL(pbDest, ppbDestEndOfLine, rowDelta);
							}
						}
					}
				}
			}
		}
	}
	return pbDest;
}

/**
 * Write a foreground/background image to a destination buffer
 * for the first line of compressed data.
 */
static uint8* WRITEFIRSTLINEFGBGIMAGE(uint8* pbDest, uint8** ppbDestEndOfLine, uint32 rowDelta,
	uint8 bitmask, PIXEL fgPel, uint8 cBits)
{
	if (!bitmask)
	{
		DESTWRITENEXTPIXELSBLACKORWHITE(cBits, 0x00, pbDest, ppbDestEndOfLine, rowDelta);
		return pbDest;
	}

	if (bitmask & g_MaskBit0)
	{
		DESTWRITEPIXEL(pbDest, fgPel);
	}
	else
	{
		DESTWRITEPIXEL(pbDest, BLACK_PIXEL);
	}
	DESTNEXTPIXEL(pbDest, ppbDestEndOfLine, rowDelta);
	cBits = cBits - 1;
	if (cBits > 0)
	{
		if (bitmask & g_MaskBit1)
		{
			DESTWRITEPIXEL(pbDest, fgPel);
		}
		else
		{
			DESTWRITEPIXEL(pbDest, BLACK_PIXEL);
		}
		DESTNEXTPIXEL(pbDest, ppbDestEndOfLine, rowDelta);
		cBits = cBits - 1;
		if (cBits > 0)
		{
			if (bitmask & g_MaskBit2)
			{
				DESTWRITEPIXEL(pbDest, fgPel);
			}
			else
			{
				DESTWRITEPIXEL(pbDest, BLACK_PIXEL);
			}
			DESTNEXTPIXEL(pbDest, ppbDestEndOfLine, rowDelta);
			cBits = cBits - 1;
			if (cBits > 0)
			{
				if (bitmask & g_MaskBit3)
				{
					DESTWRITEPIXEL(pbDest, fgPel);
				}
				else
				{
					DESTWRITEPIXEL(pbDest, BLACK_PIXEL);
				}
				DESTNEXTPIXEL(pbDest, ppbDestEndOfLine, rowDelta);
				cBits = cBits - 1;
				if (cBits > 0)
				{
					if (bitmask & g_MaskBit4)
					{
						DESTWRITEPIXEL(pbDest, fgPel);
					}
					else
					{
						DESTWRITEPIXEL(pbDest, BLACK_PIXEL);
					}
					DESTNEXTPIXEL(pbDest, ppbDestEndOfLine, rowDelta);
					cBits = cBits - 1;
					if (cBits > 0)
					{
						if (bitmask & g_MaskBit5)
						{
							DESTWRITEPIXEL(pbDest, fgPel);
						}
						else
						{
							DESTWRITEPIXEL(pbDest, BLACK_PIXEL);
						}
						DESTNEXTPIXEL(pbDest, ppbDestEndOfLine, rowDelta);
						cBits = cBits - 1;
						if (cBits > 0)
						{
							if (bitmask & g_MaskBit6)
							{
								DESTWRITEPIXEL(pbDest, fgPel);
							}
							else
							{
								DESTWRITEPIXEL(pbDest, BLACK_PIXEL);
							}
							DESTNEXTPIXEL(pbDest, ppbDestEndOfLine, rowDelta);
							cBits = cBits - 1;
							if (cBits > 0)
							{
								if (bitmask & g_MaskBit7)
								{
									DESTWRITEPIXEL(pbDest, fgPel);
								}
								else
								{
									DESTWRITEPIXEL(pbDest, BLACK_PIXEL);
								}
								DESTNEXTPIXEL(pbDest, ppbDestEndOfLine, rowDelta);
							}
						}
					}
				}
			}
		}
	}
	return pbDest;
}

/**
 * Decompress an RLE compressed bitmap.
 */
void RLEDECOMPRESS(uint8* pbSrcBuffer, uint32 cbSrcBuffer, uint8* pbDest,
	uint32 rowDelta, uint32 width, uint32 height)
{
	uint8* pbSrc = pbSrcBuffer;
#ifdef PREFETCH_LENGTH
	uint8* pbSrcPrefetch = pbSrcBuffer;
#endif
	uint8* pbEnd = pbSrcBuffer + cbSrcBuffer;
	uint8* pbDestEndOfLine = pbDest + rowDelta * height;
	uint8* pbDestLastLine = pbDestEndOfLine - rowDelta;
	pbDest = pbDestLastLine;

	PIXEL temp;
	PIXEL fgPel = WHITE_PIXEL;
	boolean fInsertFgPel = false;
	boolean fFirstLine = true;
	uint8 bitmask;
	PIXEL pixelA, pixelB;

	uint32 runLength;
	uint32 code;

	uint32 advance;

	RLEEXTRA

	PREFETCH_WRITE(pbDest + PREFETCH_LENGTH);

	while (pbSrc < pbEnd)
	{
#ifdef PREFETCH_LENGTH
		if (pbSrc>=pbSrcPrefetch)
		{
			pbSrcPrefetch = pbSrc + PREFETCH_LENGTH;
			PREFETCH_READ(pbSrcPrefetch);
		}
#endif
		/* Watch out for the end of the first scanline. */
		if (fFirstLine && pbDest < pbDestLastLine)
		{
			fFirstLine = false;
			fInsertFgPel = false;
		}

		/*
		   Extract the compression order code ID from the compression
		   order header.
		*/
		code = ExtractCodeId(*pbSrc);

		/* Handle Background Run Orders. */
		if (code == REGULAR_BG_RUN || code == MEGA_MEGA_BG_RUN)
		{
			runLength = ExtractRunLength(code, pbSrc, &advance);
			pbSrc = pbSrc + advance;
			if (fFirstLine)
			{
				if (fInsertFgPel)
				{
					DESTWRITEPIXEL(pbDest, fgPel);
					DESTNEXTPIXEL(pbDest, &pbDestEndOfLine, rowDelta);
					runLength = runLength - 1;
				}
				DESTWRITENEXTPIXELSBLACKORWHITE(runLength, 0x00, pbDest, &pbDestEndOfLine, rowDelta);
			}
			else
			{
				if (fInsertFgPel)
				{
					DESTREADPIXEL(temp, pbDest + rowDelta);
					DESTWRITEPIXEL(pbDest, temp ^ fgPel);
					DESTNEXTPIXEL(pbDest, &pbDestEndOfLine, rowDelta);
					runLength = runLength - 1;
				}
				DESTWRITENEXTPIXELSFROMPREVROW(runLength, pbDest, &pbDestEndOfLine, rowDelta);
			}
			/* A follow-on background run order will need a foreground pel inserted. */
			fInsertFgPel = true;
			continue;
		}

		/* For any of the other run-types a follow-on background run
			order does not need a foreground pel inserted. */
		fInsertFgPel = false;

		switch (code)
		{
			/* Handle Foreground Run Orders. */
			case REGULAR_FG_RUN:
			case MEGA_MEGA_FG_RUN:
			case LITE_SET_FG_FG_RUN:
			case MEGA_MEGA_SET_FG_RUN:
				runLength = ExtractRunLength(code, pbSrc, &advance);
				pbSrc = pbSrc + advance;
				if (code == LITE_SET_FG_FG_RUN || code == MEGA_MEGA_SET_FG_RUN)
				{
					SRCREADPIXEL(fgPel, pbSrc);
					SRCNEXTPIXEL(pbSrc);
				}
				if (fFirstLine)
				{
					DESTWRITENEXTPIXELS(runLength, fgPel, pbDest, &pbDestEndOfLine, rowDelta);
				}
				else
				{
					XOREDDESTWRITENEXTPIXELSFROMPREVROW(runLength, fgPel, pbDest, &pbDestEndOfLine, rowDelta);
				}
				break;

			/* Handle Dithered Run Orders. */
			case LITE_DITHERED_RUN:
			case MEGA_MEGA_DITHERED_RUN:
				runLength = ExtractRunLength(code, pbSrc, &advance);
				pbSrc = pbSrc + advance;
				SRCREADPIXEL(pixelA, pbSrc);
				SRCNEXTPIXEL(pbSrc);
				SRCREADPIXEL(pixelB, pbSrc);
				SRCNEXTPIXEL(pbSrc);
				DESTDOUBLEWRITENEXTPIXELS(runLength, pixelA, pixelB, pbDest, &pbDestEndOfLine, rowDelta);
				break;

			/* Handle Color Run Orders. */
			case REGULAR_COLOR_RUN:
			case MEGA_MEGA_COLOR_RUN:
				runLength = ExtractRunLength(code, pbSrc, &advance);
				pbSrc = pbSrc + advance;
				SRCREADPIXEL(pixelA, pbSrc);
				SRCNEXTPIXEL(pbSrc);
				DESTWRITENEXTPIXELS(runLength, pixelA, pbDest, &pbDestEndOfLine, rowDelta);
				break;

			/* Handle Foreground/Background Image Orders. */
			case REGULAR_FGBG_IMAGE:
			case MEGA_MEGA_FGBG_IMAGE:
			case LITE_SET_FG_FGBG_IMAGE:
			case MEGA_MEGA_SET_FGBG_IMAGE:
				runLength = ExtractRunLength(code, pbSrc, &advance);
				pbSrc = pbSrc + advance;
				if (code == LITE_SET_FG_FGBG_IMAGE || code == MEGA_MEGA_SET_FGBG_IMAGE)
				{
					SRCREADPIXEL(fgPel, pbSrc);
					SRCNEXTPIXEL(pbSrc);
				}
				if (fFirstLine)
				{
					while (runLength > 8)
					{
						bitmask = *pbSrc;
						pbSrc = pbSrc + 1;
						pbDest = WRITEFIRSTLINEFGBGIMAGE(pbDest, &pbDestEndOfLine, rowDelta, bitmask, fgPel, 8);
						runLength = runLength - 8;
					}
				}
				else
				{
					while (runLength > 8)
					{
						bitmask = *pbSrc;
						pbSrc = pbSrc + 1;
						pbDest = WRITEFGBGIMAGE(pbDest, &pbDestEndOfLine, rowDelta, bitmask, fgPel, 8);
						runLength = runLength - 8;
					}
				}
				if (runLength > 0)
				{
					bitmask = *pbSrc;
					pbSrc = pbSrc + 1;
					if (fFirstLine)
					{
						pbDest = WRITEFIRSTLINEFGBGIMAGE(pbDest, &pbDestEndOfLine, rowDelta, bitmask, fgPel, (uint8)runLength);
					}
					else
					{
						pbDest = WRITEFGBGIMAGE(pbDest, &pbDestEndOfLine, rowDelta, bitmask, fgPel, (uint8)runLength);
					}
				}
				break;

			/* Handle Color Image Orders. */
			case REGULAR_COLOR_IMAGE:
			case MEGA_MEGA_COLOR_IMAGE:
				runLength = ExtractRunLength(code, pbSrc, &advance);
				pbSrc = pbSrc + advance;
				SRCREADNEXTDESTWRITENEXTPIXELS(runLength, pbSrc, pbDest, &pbDestEndOfLine, rowDelta);
				break;

			/* Handle Special Order 1. */
			case SPECIAL_FGBG_1:
				pbSrc = pbSrc + 1;
				if (fFirstLine)
				{
					pbDest = WRITEFIRSTLINEFGBGIMAGE(pbDest, &pbDestEndOfLine, rowDelta, g_MaskSpecialFgBg1, fgPel, 8);
				}
				else
				{
					pbDest = WRITEFGBGIMAGE(pbDest, &pbDestEndOfLine, rowDelta, g_MaskSpecialFgBg1, fgPel, 8);
				}
				break;

			/* Handle Special Order 2. */
			case SPECIAL_FGBG_2:
				pbSrc = pbSrc + 1;
				if (fFirstLine)
				{
					pbDest = WRITEFIRSTLINEFGBGIMAGE(pbDest, &pbDestEndOfLine, rowDelta, g_MaskSpecialFgBg2, fgPel, 8);
				}
				else
				{
					pbDest = WRITEFGBGIMAGE(pbDest, &pbDestEndOfLine, rowDelta, g_MaskSpecialFgBg2, fgPel, 8);
				}
				break;

				/* Handle White Order. */
			case SPECIAL_WHITE:
				pbSrc = pbSrc + 1;
				DESTWRITEPIXEL(pbDest, WHITE_PIXEL);
				DESTNEXTPIXEL(pbDest, &pbDestEndOfLine, rowDelta);
				break;

			/* Handle Black Order. */
			case SPECIAL_BLACK:
				pbSrc = pbSrc + 1;
				DESTWRITEPIXEL(pbDest, BLACK_PIXEL);
				DESTNEXTPIXEL(pbDest, &pbDestEndOfLine, rowDelta);
				break;
		}
	}
}
