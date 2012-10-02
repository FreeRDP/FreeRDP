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

/**
 * Write a foreground/background image to a destination buffer.
 */
static uint8* WRITEFGBGIMAGE(uint8* pbDest, uint32 rowDelta,
	uint8 bitmask, PIXEL fgPel, uint32 cBits)
{
	PIXEL xorPixel;

	DESTREADPIXEL(xorPixel, pbDest - rowDelta);
	if (bitmask & g_MaskBit0)
	{
		DESTWRITEPIXEL(pbDest, xorPixel ^ fgPel);
	}
	else
	{
		DESTWRITEPIXEL(pbDest, xorPixel);
	}
	DESTNEXTPIXEL(pbDest);
	cBits = cBits - 1;
	if (cBits > 0)
	{
		DESTREADPIXEL(xorPixel, pbDest - rowDelta);
		if (bitmask & g_MaskBit1)
		{
			DESTWRITEPIXEL(pbDest, xorPixel ^ fgPel);
		}
		else
		{
			DESTWRITEPIXEL(pbDest, xorPixel);
		}
		DESTNEXTPIXEL(pbDest);
		cBits = cBits - 1;
		if (cBits > 0)
		{
			DESTREADPIXEL(xorPixel, pbDest - rowDelta);
			if (bitmask & g_MaskBit2)
			{
				DESTWRITEPIXEL(pbDest, xorPixel ^ fgPel);
			}
			else
			{
				DESTWRITEPIXEL(pbDest, xorPixel);
			}
			DESTNEXTPIXEL(pbDest);
			cBits = cBits - 1;
			if (cBits > 0)
			{
				DESTREADPIXEL(xorPixel, pbDest - rowDelta);
				if (bitmask & g_MaskBit3)
				{
					DESTWRITEPIXEL(pbDest, xorPixel ^ fgPel);
				}
				else
				{
					DESTWRITEPIXEL(pbDest, xorPixel);
				}
				DESTNEXTPIXEL(pbDest);
				cBits = cBits - 1;
				if (cBits > 0)
				{
					DESTREADPIXEL(xorPixel, pbDest - rowDelta);
					if (bitmask & g_MaskBit4)
					{
						DESTWRITEPIXEL(pbDest, xorPixel ^ fgPel);
					}
					else
					{
						DESTWRITEPIXEL(pbDest, xorPixel);
					}
					DESTNEXTPIXEL(pbDest);
					cBits = cBits - 1;
					if (cBits > 0)
					{
						DESTREADPIXEL(xorPixel, pbDest - rowDelta);
						if (bitmask & g_MaskBit5)
						{
							DESTWRITEPIXEL(pbDest, xorPixel ^ fgPel);
						}
						else
						{
							DESTWRITEPIXEL(pbDest, xorPixel);
						}
						DESTNEXTPIXEL(pbDest);
						cBits = cBits - 1;
						if (cBits > 0)
						{
							DESTREADPIXEL(xorPixel, pbDest - rowDelta);
							if (bitmask & g_MaskBit6)
							{
								DESTWRITEPIXEL(pbDest, xorPixel ^ fgPel);
							}
							else
							{
								DESTWRITEPIXEL(pbDest, xorPixel);
							}
							DESTNEXTPIXEL(pbDest);
							cBits = cBits - 1;
							if (cBits > 0)
							{
								DESTREADPIXEL(xorPixel, pbDest - rowDelta);
								if (bitmask & g_MaskBit7)
								{
									DESTWRITEPIXEL(pbDest, xorPixel ^ fgPel);
								}
								else
								{
									DESTWRITEPIXEL(pbDest, xorPixel);
								}
								DESTNEXTPIXEL(pbDest);
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
static uint8* WRITEFIRSTLINEFGBGIMAGE(uint8* pbDest, uint8 bitmask,
	PIXEL fgPel, uint32 cBits)
{
	if (bitmask & g_MaskBit0)
	{
		DESTWRITEPIXEL(pbDest, fgPel);
	}
	else
	{
		DESTWRITEPIXEL(pbDest, BLACK_PIXEL);
	}
	DESTNEXTPIXEL(pbDest);
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
		DESTNEXTPIXEL(pbDest);
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
			DESTNEXTPIXEL(pbDest);
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
				DESTNEXTPIXEL(pbDest);
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
					DESTNEXTPIXEL(pbDest);
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
						DESTNEXTPIXEL(pbDest);
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
							DESTNEXTPIXEL(pbDest);
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
								DESTNEXTPIXEL(pbDest);
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
void RLEDECOMPRESS(uint8* pbSrcBuffer, uint32 cbSrcBuffer, uint8* pbDestBuffer,
	uint32 rowDelta, uint32 width, uint32 height)
{
	uint8* pbSrc = pbSrcBuffer;
	uint8* pbEnd = pbSrcBuffer + cbSrcBuffer;
	uint8* pbDest = pbDestBuffer;

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

	while (pbSrc < pbEnd)
	{
		/* Watch out for the end of the first scanline. */
		if (fFirstLine)
		{
			if ((uint32)(pbDest - pbDestBuffer) >= rowDelta)
			{
				fFirstLine = false;
				fInsertFgPel = false;
			}
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
					DESTNEXTPIXEL(pbDest);
					runLength = runLength - 1;
				}
				while (runLength >= UNROLL_COUNT)
				{
					UNROLL(
						DESTWRITEPIXEL(pbDest, BLACK_PIXEL);
						DESTNEXTPIXEL(pbDest); );
					runLength = runLength - UNROLL_COUNT;
				}
				while (runLength > 0)
				{
					DESTWRITEPIXEL(pbDest, BLACK_PIXEL);
					DESTNEXTPIXEL(pbDest);
					runLength = runLength - 1;
				}
			}
			else
			{
				if (fInsertFgPel)
				{
					DESTREADPIXEL(temp, pbDest - rowDelta);
					DESTWRITEPIXEL(pbDest, temp ^ fgPel);
					DESTNEXTPIXEL(pbDest);
					runLength = runLength - 1;
				}
				while (runLength >= UNROLL_COUNT)
				{
					UNROLL(
						DESTREADPIXEL(temp, pbDest - rowDelta);
						DESTWRITEPIXEL(pbDest, temp);
						DESTNEXTPIXEL(pbDest); );
					runLength = runLength - UNROLL_COUNT;
				}
				while (runLength > 0)
				{
					DESTREADPIXEL(temp, pbDest - rowDelta);
					DESTWRITEPIXEL(pbDest, temp);
					DESTNEXTPIXEL(pbDest);
					runLength = runLength - 1;
				}
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
					while (runLength >= UNROLL_COUNT)
					{
						UNROLL(
							DESTWRITEPIXEL(pbDest, fgPel);
							DESTNEXTPIXEL(pbDest); );
						runLength = runLength - UNROLL_COUNT;
					}
					while (runLength > 0)
					{
						DESTWRITEPIXEL(pbDest, fgPel);
						DESTNEXTPIXEL(pbDest);
						runLength = runLength - 1;
					}
				}
				else
				{
					while (runLength >= UNROLL_COUNT)
					{
						UNROLL(
							DESTREADPIXEL(temp, pbDest - rowDelta);
							DESTWRITEPIXEL(pbDest, temp ^ fgPel);
							DESTNEXTPIXEL(pbDest); );
						runLength = runLength - UNROLL_COUNT;
					}
					while (runLength > 0)
					{
						DESTREADPIXEL(temp, pbDest - rowDelta);
						DESTWRITEPIXEL(pbDest, temp ^ fgPel);
						DESTNEXTPIXEL(pbDest);
						runLength = runLength - 1;
					}
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
				while (runLength >= UNROLL_COUNT)
				{
					UNROLL(
						DESTWRITEPIXEL(pbDest, pixelA);
						DESTNEXTPIXEL(pbDest);
						DESTWRITEPIXEL(pbDest, pixelB);
						DESTNEXTPIXEL(pbDest); );
					runLength = runLength - UNROLL_COUNT;
				}
				while (runLength > 0)
				{
					DESTWRITEPIXEL(pbDest, pixelA);
					DESTNEXTPIXEL(pbDest);
					DESTWRITEPIXEL(pbDest, pixelB);
					DESTNEXTPIXEL(pbDest);
					runLength = runLength - 1;
				}
				break;

			/* Handle Color Run Orders. */
			case REGULAR_COLOR_RUN:
			case MEGA_MEGA_COLOR_RUN:
				runLength = ExtractRunLength(code, pbSrc, &advance);
				pbSrc = pbSrc + advance;
				SRCREADPIXEL(pixelA, pbSrc);
				SRCNEXTPIXEL(pbSrc);
				while (runLength >= UNROLL_COUNT)
				{
					UNROLL(
						DESTWRITEPIXEL(pbDest, pixelA);
						DESTNEXTPIXEL(pbDest); );
					runLength = runLength - UNROLL_COUNT;
				}
				while (runLength > 0)
				{
					DESTWRITEPIXEL(pbDest, pixelA);
					DESTNEXTPIXEL(pbDest);
					runLength = runLength - 1;
				}
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
						pbDest = WRITEFIRSTLINEFGBGIMAGE(pbDest, bitmask, fgPel, 8);
						runLength = runLength - 8;
					}
				}
				else
				{
					while (runLength > 8)
					{
						bitmask = *pbSrc;
						pbSrc = pbSrc + 1;
						pbDest = WRITEFGBGIMAGE(pbDest, rowDelta, bitmask, fgPel, 8);
						runLength = runLength - 8;
					}
				}
				if (runLength > 0)
				{
					bitmask = *pbSrc;
					pbSrc = pbSrc + 1;
					if (fFirstLine)
					{
						pbDest = WRITEFIRSTLINEFGBGIMAGE(pbDest, bitmask, fgPel, runLength);
					}
					else
					{
						pbDest = WRITEFGBGIMAGE(pbDest, rowDelta, bitmask, fgPel, runLength);
					}
				}
				break;

			/* Handle Color Image Orders. */
			case REGULAR_COLOR_IMAGE:
			case MEGA_MEGA_COLOR_IMAGE:
				runLength = ExtractRunLength(code, pbSrc, &advance);
				pbSrc = pbSrc + advance;
				while (runLength >= UNROLL_COUNT)
				{
					UNROLL(
						SRCREADPIXEL(temp, pbSrc);
						SRCNEXTPIXEL(pbSrc);
						DESTWRITEPIXEL(pbDest, temp);
						DESTNEXTPIXEL(pbDest); );
					runLength = runLength - UNROLL_COUNT;
				}
				while (runLength > 0)
				{
					SRCREADPIXEL(temp, pbSrc);
					SRCNEXTPIXEL(pbSrc);
					DESTWRITEPIXEL(pbDest, temp);
					DESTNEXTPIXEL(pbDest);
					runLength = runLength - 1;
				}
				break;

			/* Handle Special Order 1. */
			case SPECIAL_FGBG_1:
				pbSrc = pbSrc + 1;
				if (fFirstLine)
				{
					pbDest = WRITEFIRSTLINEFGBGIMAGE(pbDest, g_MaskSpecialFgBg1, fgPel, 8);
				}
				else
				{
					pbDest = WRITEFGBGIMAGE(pbDest, rowDelta, g_MaskSpecialFgBg1, fgPel, 8);
				}
				break;

			/* Handle Special Order 2. */
			case SPECIAL_FGBG_2:
				pbSrc = pbSrc + 1;
				if (fFirstLine)
				{
					pbDest = WRITEFIRSTLINEFGBGIMAGE(pbDest, g_MaskSpecialFgBg2, fgPel, 8);
				}
				else
				{
					pbDest = WRITEFGBGIMAGE(pbDest, rowDelta, g_MaskSpecialFgBg2, fgPel, 8);
				}
				break;

				/* Handle White Order. */
			case SPECIAL_WHITE:
				pbSrc = pbSrc + 1;
				DESTWRITEPIXEL(pbDest, WHITE_PIXEL);
				DESTNEXTPIXEL(pbDest);
				break;

			/* Handle Black Order. */
			case SPECIAL_BLACK:
				pbSrc = pbSrc + 1;
				DESTWRITEPIXEL(pbDest, BLACK_PIXEL);
				DESTNEXTPIXEL(pbDest);
				break;
		}
	}
}
