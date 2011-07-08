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

/*
   RLE Compressed Bitmap Stream (RLE_BITMAP_STREAM)
   http://msdn.microsoft.com/en-us/library/cc240895%28v=prot.10%29.aspx
   pseudo-code
   http://msdn.microsoft.com/en-us/library/dd240593%28v=prot.10%29.aspx
*/

#include <freerdp/utils/stream.h>
#include <freerdp/utils/memory.h>

#define REGULAR_BG_RUN 0x0
#define MEGA_MEGA_BG_RUN 0xF0
#define REGULAR_FG_RUN 0x1
#define MEGA_MEGA_FG_RUN 0xF1
#define LITE_SET_FG_FG_RUN 0xC
#define MEGA_MEGA_SET_FG_RUN 0xF6
#define LITE_DITHERED_RUN 0xE
#define MEGA_MEGA_DITHERED_RUN 0xF8
#define REGULAR_COLOR_RUN 0x3
#define MEGA_MEGA_COLOR_RUN 0xF3
#define REGULAR_FGBG_IMAGE 0x2
#define MEGA_MEGA_FGBG_IMAGE 0xF2
#define LITE_SET_FG_FGBG_IMAGE 0xD
#define MEGA_MEGA_SET_FGBG_IMAGE 0xF7
#define REGULAR_COLOR_IMAGE 0x4
#define MEGA_MEGA_COLOR_IMAGE 0xF4
#define SPECIAL_FGBG_1 0xF9
#define SPECIAL_FGBG_2 0xFA
#define SPECIAL_WHITE 0xFD
#define SPECIAL_BLACK 0xFE

/*
   Bitmasks
*/
static uint8 g_MaskBit0 = 0x01; /* Least significant bit */
static uint8 g_MaskBit1 = 0x02;
static uint8 g_MaskBit2 = 0x04;
static uint8 g_MaskBit3 = 0x08;
static uint8 g_MaskBit4 = 0x10;
static uint8 g_MaskBit5 = 0x20;
static uint8 g_MaskBit6 = 0x40;
static uint8 g_MaskBit7 = 0x80; /* Most significant bit */

static uint8 g_MaskRegularRunLength = 0x1F;
static uint8 g_MaskLiteRunLength = 0x0F;

static uint8 g_MaskSpecialFgBg1 = 0x03;
static uint8 g_MaskSpecialFgBg2 = 0x05;

typedef uint32 PIXEL;

/*
   Returns the color depth (in bits per pixel) that was selected
   for the RDP connection.
*/
static uint32
GetColorDepth(void)
{
	return 8;
}

/*
   Returns the color depth (in bytes per pixel) that was selected
   for the RDP connection.
*/
static uint32
GetColorDepthInBytes(void)
{
	/* bytes per pixel, see above */
	return 1;
}

/*
   PIXEL is a dynamic type that is sized based on the current color
   depth being used for the RDP connection.

   if (GetColorDepth() == 8) then PIXEL is an 8-bit unsigned integer
   if (GetColorDepth() == 15) then PIXEL is a 16-bit unsigned integer
   if (GetColorDepth() == 16) then PIXEL is a 16-bit unsigned integer
   if (GetColorDepth() == 24) then PIXEL is a 24-bit unsigned integer
*/

/*
   Writes a pixel to the specified buffer.
*/
static void
WritePixel(uint8 * pbBuffer, PIXEL pixel)
{
	pbBuffer[0] = pixel;
}

/*
   Reads a pixel from the specified buffer.
*/
static PIXEL
ReadPixel(uint8 * pbBuffer)
{
	return pbBuffer[0];
}

/*
   Returns the size of a pixel in bytes.
*/
static uint32
GetPixelSize(void)
{
	uint32 colorDepth = GetColorDepth();

	if (colorDepth == 8)
	{
		return 1;
	}
	else if (colorDepth == 15 || colorDepth == 16)
	{
		return 2;
	}
	else if (colorDepth == 24)
	{
		return 3;
	}
	return 1;
}

/*
   Returns a pointer to the next pixel in the specified buffer.
*/
static uint8 *
NextPixel(uint8 * pbBuffer)
{
	return pbBuffer + GetPixelSize();
}

/*
   Reads the supplied order header and extracts the compression
   order code ID.
*/
static uint32
ExtractCodeId(uint8 bOrderHdr)
{
	int code;

	switch (bOrderHdr)
	{
		case MEGA_MEGA_BG_RUN:
		case MEGA_MEGA_FG_RUN:
		case MEGA_MEGA_SET_FG_RUN:
		case MEGA_MEGA_DITHERED_RUN:
		case MEGA_MEGA_COLOR_RUN:
		case MEGA_MEGA_FGBG_IMAGE:
		case MEGA_MEGA_SET_FGBG_IMAGE:
		case MEGA_MEGA_COLOR_IMAGE:
		case SPECIAL_FGBG_1:
		case SPECIAL_FGBG_2:
		case SPECIAL_WHITE:
		case SPECIAL_BLACK:
			return bOrderHdr;
	}
	code = bOrderHdr >> 5;
	switch (code)
	{
		case REGULAR_BG_RUN:
		case REGULAR_FG_RUN:
		case REGULAR_COLOR_RUN:
		case REGULAR_FGBG_IMAGE:
		case REGULAR_COLOR_IMAGE:
			return code;
	}
	return bOrderHdr >> 4;
}

/*
   Returns TRUE if the supplied code identifier is for a regular-form
   standard compression order. For example IsRegularCode(0x01) returns
   TRUE as 0x01 is the code ID for a Regular Foreground Run Order.
*/
static boolean
IsRegularCode(uint32 codeId)
{
	switch (codeId)
	{
		case REGULAR_BG_RUN:
		case REGULAR_FG_RUN:
		case REGULAR_COLOR_RUN:
		case REGULAR_FGBG_IMAGE:
		case REGULAR_COLOR_IMAGE:
			return True;
	}
	return False;
}

/*
   Returns TRUE if the supplied code identifier is for a lite-form
   standard compression order. For example IsLiteCode(0x0E) returns
   TRUE as 0x0E is the code ID for a Lite Dithered Run Order.
*/
static boolean
IsLiteCode(uint32 codeId)
{
	switch (codeId)
	{
		case LITE_SET_FG_FG_RUN:
		case LITE_DITHERED_RUN:
		case LITE_SET_FG_FGBG_IMAGE:
			return True;
	}
	return False;
}

/*
   Returns TRUE if the supplied code identifier is for a MEGA_MEGA
   type extended compression order. For example IsMegaMegaCode(0xF0)
   returns TRUE as 0xF0 is the code ID for a MEGA_MEGA Background
   Run Order.
*/
static boolean
IsMegaMegaCode(uint32 codeId)
{
	switch (codeId)
	{
		case MEGA_MEGA_BG_RUN:
		case MEGA_MEGA_FG_RUN:
		case MEGA_MEGA_SET_FG_RUN:
		case MEGA_MEGA_DITHERED_RUN:
		case MEGA_MEGA_COLOR_RUN:
		case MEGA_MEGA_FGBG_IMAGE:
		case MEGA_MEGA_SET_FGBG_IMAGE:
		case MEGA_MEGA_COLOR_IMAGE:
			return True;
	}
	return False;
}

/*
   Returns a black pixel.
*/
static PIXEL
GetColorBlack(void)
{
	uint32 colorDepth = GetColorDepth();

	if (colorDepth == 8)
	{
		return (PIXEL) 0x00;
	}
	else if (colorDepth == 15)
	{
		return (PIXEL) 0x0000;
	}
	else if (colorDepth == 16)
	{
		return (PIXEL) 0x0000;
	}
	else if (colorDepth == 24)
	{
		return (PIXEL) 0x000000;
	}
	return 0;
}

/*
   Returns a white pixel.
*/
static PIXEL
GetColorWhite(void)
{
	uint32 colorDepth = GetColorDepth();

	if (colorDepth == 8)
	{
		/*
		   Palette entry #255 holds black.
		*/
		return (PIXEL) 0xFF;
	}
	else if (colorDepth == 15)
	{
		/*
		   5 bits per RGB component:
		   0111 1111 1111 1111 (binary)
		*/
		return (PIXEL) 0x7FFF;
	}
	else if (colorDepth == 16)
	{
		/*
		   5 bits for red, 6 bits for green, 5 bits for green:
		   1111 1111 1111 1111 (binary)
		*/
		return (PIXEL) 0xFFFF;
	}
	else if (colorDepth == 24)
	{
		/*
		   8 bits per RGB component:
		   1111 1111 1111 1111 1111 1111 (binary)
		*/
		return (PIXEL) 0xFFFFFF;
	}
}

/*
   Extract the run length of a Regular-Form Foreground/Background
   Image Order.
*/
static uint32
ExtractRunLengthRegularFgBg(uint8 * pbOrderHdr, uint32 * advance)
{
	uint32 runLength;

	runLength = (*pbOrderHdr) & g_MaskRegularRunLength;
	if (runLength == 0)
	{
		runLength = (*(pbOrderHdr + 1)) + 1;
		*advance += 1;
	}
	else
	{
		runLength = runLength * 8;
	}
	return runLength;
}

/*
   Extract the run length of a Lite-Form Foreground/Background
   Image Order.
*/
static uint32
ExtractRunLengthLiteFgBg(uint8 * pbOrderHdr, uint32 * advance)
{
	uint32 runLength;

	runLength = (*pbOrderHdr) & g_MaskLiteRunLength;
	if (runLength == 0)
	{
		runLength = (*(pbOrderHdr + 1)) + 1;
		*advance += 1;
	}
	else
	{
		runLength = runLength * 8;
	}
	return runLength;
}

/*
   Extract the run length of a regular-form compression order.
*/
static uint32
ExtractRunLengthRegular(uint8 * pbOrderHdr, uint32 * advance)
{
	uint32 runLength;

	runLength = (*pbOrderHdr) & g_MaskRegularRunLength;
	if (runLength == 0)
	{
		/* An extended (MEGA) run. */
		runLength = (*(pbOrderHdr + 1)) + 32;
		*advance += 1;
	}
	return runLength;
}

/*
   Extract the run length of a lite-form compression order.
*/
static uint32
ExtractRunLengthLite(uint8 * pbOrderHdr, uint32 * advance)
{
	uint32 runLength;

	runLength = (*pbOrderHdr) & g_MaskLiteRunLength;
	if (runLength == 0)
	{
		/* An extended (MEGA) run. */
		runLength = (*(pbOrderHdr + 1)) + 16;
		*advance += 1;
	}
	return runLength;
}

/*
   Extract the run length of a MEGA_MEGA-type compression order.
*/
static uint32
ExtractRunLengthMegaMega(uint8 * pbOrderHdr, uint32 * advance)
{
	uint32 runLength;

	pbOrderHdr = pbOrderHdr + 1;
	runLength = ((uint16) pbOrderHdr[0]) | ((uint16) (pbOrderHdr[1] << 8));
	*advance += 2;
	return runLength;
}

/*
   Extract the run length of a compression order.
*/
static uint32
ExtractRunLength(uint32 code, uint8 * pbOrderHdr, uint32 * advance)
{
	uint32 runLength;

	*advance = 1;
	if (code == REGULAR_FGBG_IMAGE)
	{
		runLength = ExtractRunLengthRegularFgBg(pbOrderHdr, advance);
	}
	else if (code == LITE_SET_FG_FGBG_IMAGE)
	{
		runLength = ExtractRunLengthLiteFgBg(pbOrderHdr, advance);
	}
	else if (IsRegularCode(code))
	{
		runLength = ExtractRunLengthRegular(pbOrderHdr, advance);
	}
	else if (IsLiteCode(code))
	{
		runLength = ExtractRunLengthLite(pbOrderHdr, advance);
	}
	else if (IsMegaMegaCode(code))
	{
		runLength = ExtractRunLengthMegaMega(pbOrderHdr, advance);
	}
	else
	{
		runLength = 0;
	}
	return runLength;
}

/*
   Write a foreground/background image to a destination buffer.
*/
static uint8 *
WriteFgBgImage(uint8 * pbDest, uint32 rowDelta, uint8 bitmask,
	PIXEL fgPel, uint32 cBits)
{
	PIXEL xorPixel;

	xorPixel = ReadPixel(pbDest - rowDelta);
	if (bitmask & g_MaskBit0)
	{
		WritePixel(pbDest, xorPixel ^ fgPel);
	}
	else
	{
		WritePixel(pbDest, xorPixel);
	}
	pbDest = NextPixel(pbDest);
	cBits = cBits - 1;

	if (cBits > 0)
	{
		xorPixel = ReadPixel(pbDest - rowDelta);
		if (bitmask & g_MaskBit1)
		{
			WritePixel(pbDest, xorPixel ^ fgPel);
		}
		else
		{
			WritePixel(pbDest, xorPixel);
		}
		pbDest = NextPixel(pbDest);
		cBits = cBits - 1;

		if (cBits > 0)
		{
			xorPixel = ReadPixel(pbDest - rowDelta);
			if (bitmask & g_MaskBit2)
			{
				WritePixel(pbDest, xorPixel ^ fgPel);
			}
			else
			{
				WritePixel(pbDest, xorPixel);
			}
			pbDest = NextPixel(pbDest);
			cBits = cBits - 1;

			if (cBits > 0)
			{
				xorPixel = ReadPixel(pbDest - rowDelta);
				if (bitmask & g_MaskBit3)
				{
					WritePixel(pbDest, xorPixel ^ fgPel);
				}
				else
				{
					WritePixel(pbDest, xorPixel);
				}
				pbDest = NextPixel(pbDest);
				cBits = cBits - 1;

				if (cBits > 0)
				{
					xorPixel = ReadPixel(pbDest - rowDelta);
					if (bitmask & g_MaskBit4)
					{
						WritePixel(pbDest, xorPixel ^ fgPel);
					}
					else
					{
						WritePixel(pbDest, xorPixel);
					}
					pbDest = NextPixel(pbDest);
					cBits = cBits - 1;

					if (cBits > 0)
					{
						xorPixel = ReadPixel(pbDest - rowDelta);
						if (bitmask & g_MaskBit5)
						{
							WritePixel(pbDest, xorPixel ^ fgPel);
						}
						else
						{
							WritePixel(pbDest, xorPixel);
						}
						pbDest = NextPixel(pbDest);
						cBits = cBits - 1;

						if (cBits > 0)
						{
							xorPixel = ReadPixel(pbDest - rowDelta);
							if (bitmask & g_MaskBit6)
							{
								WritePixel(pbDest, xorPixel ^ fgPel);
							}
							else
							{
								WritePixel(pbDest, xorPixel);
							}
							pbDest = NextPixel(pbDest);
							cBits = cBits - 1;

							if (cBits > 0)
							{
								xorPixel = ReadPixel(pbDest - rowDelta);
								if (bitmask & g_MaskBit7)
								{
									WritePixel(pbDest, xorPixel ^ fgPel);
								}
								else
								{
									WritePixel(pbDest, xorPixel);
								}
								pbDest = NextPixel(pbDest);
							}
						}
					}
				}
			}
		}
	}
	return pbDest;
}

/*
   Write a foreground/background image to a destination buffer
   for the first line of compressed data.
*/
static uint8 *
WriteFirstLineFgBgImage(uint8 * pbDest, uint8 bitmask,
	PIXEL fgPel, uint32 cBits)
{
	if (bitmask & g_MaskBit0)
	{
		WritePixel(pbDest, fgPel);
	}
	else
	{
		WritePixel(pbDest, GetColorBlack());
	}
	pbDest = NextPixel(pbDest);
	cBits = cBits - 1;

	if (cBits > 0)
	{
		if (bitmask & g_MaskBit1)
		{
			WritePixel(pbDest, fgPel);
		}
		else
		{
			WritePixel(pbDest, GetColorBlack());
		}
		pbDest = NextPixel(pbDest);
		cBits = cBits - 1;

		if (cBits > 0)
		{
			if (bitmask & g_MaskBit2)
			{
				WritePixel(pbDest, fgPel);
			}
			else
			{
				WritePixel(pbDest, GetColorBlack());
			}
			pbDest = NextPixel(pbDest);
			cBits = cBits - 1;

			if (cBits > 0)
			{
				if (bitmask & g_MaskBit3)
				{
					WritePixel(pbDest, fgPel);
				}
				else
				{
					WritePixel(pbDest, GetColorBlack());
				}
				pbDest = NextPixel(pbDest);
				cBits = cBits - 1;

				if (cBits > 0)
				{
					if (bitmask & g_MaskBit4)
					{
						WritePixel(pbDest, fgPel);
					}
					else
					{
						WritePixel(pbDest, GetColorBlack());
					}
					pbDest = NextPixel(pbDest);
					cBits = cBits - 1;

					if (cBits > 0)
					{
						if (bitmask & g_MaskBit5)
						{
							WritePixel(pbDest, fgPel);
						}
						else
						{
							WritePixel(pbDest, GetColorBlack());
						}
						pbDest = NextPixel(pbDest);
						cBits = cBits - 1;

						if (cBits > 0)
						{
							if (bitmask & g_MaskBit6)
							{
								WritePixel(pbDest, fgPel);
							}
							else
							{
								WritePixel(pbDest, GetColorBlack());
							}
							pbDest = NextPixel(pbDest);
							cBits = cBits - 1;

							if (cBits > 0)
							{
								if (bitmask & g_MaskBit7)
								{
									WritePixel(pbDest, fgPel);
								}
								else
								{
									WritePixel(pbDest, GetColorBlack());
								}
								pbDest = NextPixel(pbDest);
							}
						}
					}
				}
			}
		}
	}
	return pbDest;
}

/*
   Decompress an RLE compressed bitmap.
*/
void RleDecompress(uint8 * pbSrcBuffer, uint32 cbSrcBuffer, uint8 * pbDestBuffer, uint32 rowDelta)
{
	uint8 * pbSrc = pbSrcBuffer;
	uint8 * pbEnd = pbSrcBuffer + cbSrcBuffer;
	uint8 * pbDest = pbDestBuffer;

	PIXEL fgPel = GetColorWhite();
	boolean fInsertFgPel = False;
	boolean fFirstLine = True;

	uint8 bitmask;
	PIXEL pixelA, pixelB;

	uint32 runLength;
	uint32 code;

	uint32 advance;

	while (pbSrc < pbEnd)
	{
		/* Watch out for the end of the first scanline. */
		if (fFirstLine)
		{
			if (pbDest - pbDestBuffer >= rowDelta)
			{
				fFirstLine = False;
				fInsertFgPel = False;
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
					WritePixel(pbDest, fgPel);
					pbDest = NextPixel(pbDest);
					runLength = runLength - 1;
				}
				while (runLength > 0)
				{
					WritePixel(pbDest, GetColorBlack());
					pbDest = NextPixel(pbDest);
					runLength = runLength - 1;
				}
			}
			else
			{
				if (fInsertFgPel)
				{
					WritePixel(pbDest, ReadPixel(pbDest - rowDelta) ^ fgPel);
					pbDest = NextPixel(pbDest);
					runLength = runLength - 1;
				}

				while (runLength > 0)
				{
					WritePixel(pbDest, ReadPixel(pbDest - rowDelta));
					pbDest = NextPixel(pbDest);
					runLength = runLength - 1;
				}
			}

			/*
			   A follow-on background run order will need a
			   foreground pel inserted.
			*/
			fInsertFgPel = True;
			continue;
		}

		/*
		   For any of the other run-types a follow-on background run
		   order does not need a foreground pel inserted.
		*/
		fInsertFgPel = False;

		/* Handle Foreground Run Orders. */
		if (code == REGULAR_FG_RUN || code == MEGA_MEGA_FG_RUN ||
			code == LITE_SET_FG_FG_RUN || code == MEGA_MEGA_SET_FG_RUN)
		{
			runLength = ExtractRunLength(code, pbSrc, &advance);
			pbSrc = pbSrc + advance;

			if (code == LITE_SET_FG_FG_RUN || code == MEGA_MEGA_SET_FG_RUN)
			{
				fgPel = ReadPixel(pbSrc);
				pbSrc = NextPixel(pbSrc);
			}

			while (runLength > 0)
			{
				if (fFirstLine)
				{
					WritePixel(pbDest, fgPel);
					pbDest = NextPixel(pbDest);
				}
				else
				{
					WritePixel(pbDest, ReadPixel(pbDest - rowDelta) ^ fgPel);
					pbDest = NextPixel(pbDest);
				}

				runLength = runLength - 1;
			}

			continue;
		}

		/* Handle Dithered Run Orders. */
		if (code == LITE_DITHERED_RUN || code == MEGA_MEGA_DITHERED_RUN)
		{
			runLength = ExtractRunLength(code, pbSrc, &advance);
			pbSrc = pbSrc + advance;

			pixelA = ReadPixel(pbSrc);
			pbSrc = NextPixel(pbSrc);
			pixelB = ReadPixel(pbSrc);
			pbSrc = NextPixel(pbSrc);

			while (runLength > 0)
			{
				WritePixel(pbDest, pixelA);
				pbDest = NextPixel(pbDest);
				WritePixel(pbDest, pixelB);
				pbDest = NextPixel(pbDest);

				runLength = runLength - 1;
			}

			continue;
		}

		/* Handle Color Run Orders. */
		if (code == REGULAR_COLOR_RUN || code == MEGA_MEGA_COLOR_RUN)
		{
			runLength = ExtractRunLength(code, pbSrc, &advance);
			pbSrc = pbSrc + advance;

			pixelA = ReadPixel(pbSrc);
			pbSrc = NextPixel(pbSrc);

			while (runLength > 0)
			{
				WritePixel(pbDest, pixelA);
				pbDest = NextPixel(pbDest);

				runLength = runLength - 1;
			}

			continue;
		}

		/* Handle Foreground/Background Image Orders. */
		if (code == REGULAR_FGBG_IMAGE || code == MEGA_MEGA_FGBG_IMAGE ||
			code == LITE_SET_FG_FGBG_IMAGE || code == MEGA_MEGA_SET_FGBG_IMAGE)
		{
			runLength = ExtractRunLength(code, pbSrc, &advance);
			pbSrc = pbSrc + advance;

			if (code == LITE_SET_FG_FGBG_IMAGE || code == MEGA_MEGA_SET_FGBG_IMAGE)
			{
				fgPel = ReadPixel(pbSrc);
				pbSrc = NextPixel(pbSrc);
			}

			while (runLength > 8)
			{
				bitmask = *pbSrc;
				pbSrc = pbSrc + 1;

				if (fFirstLine)
				{
					pbDest = WriteFirstLineFgBgImage(pbDest, bitmask, fgPel, 8 );
				}
				else
				{
					pbDest = WriteFgBgImage(pbDest, rowDelta, bitmask, fgPel, 8 );
				}

				runLength = runLength - 8;
			}

			if (runLength > 0)
			{
				bitmask = *pbSrc;
				pbSrc = pbSrc + 1;

				if (fFirstLine)
				{
					pbDest = WriteFirstLineFgBgImage(pbDest, bitmask, fgPel, runLength);
				}
				else
				{
					pbDest = WriteFgBgImage(pbDest, rowDelta, bitmask, fgPel, runLength);
				}
			}
			continue;
		}

		/* Handle Color Image Orders. */
		if (code == REGULAR_COLOR_IMAGE || code == MEGA_MEGA_COLOR_IMAGE)
		{
			runLength = ExtractRunLength(code, pbSrc, &advance);
			pbSrc = pbSrc + advance;
			while (runLength > 0)
			{
				WritePixel(pbDest, ReadPixel(pbSrc));
				pbDest = NextPixel(pbDest);
				pbSrc = NextPixel(pbSrc);
				runLength = runLength - 1;
			}
			continue;
		}

		/* Handle Special Order 1. */
		if (code == SPECIAL_FGBG_1)
		{
			pbSrc = pbSrc + 1;
			if (fFirstLine)
			{
				pbDest = WriteFirstLineFgBgImage(pbDest, g_MaskSpecialFgBg1, fgPel, 8);
			}
			else
			{
				pbDest = WriteFgBgImage(pbDest, rowDelta, g_MaskSpecialFgBg1, fgPel, 8);
			}
			continue;
		}

		/* Handle Special Order 2. */
		if (code == SPECIAL_FGBG_2)
		{
			pbSrc = pbSrc + 1;
			if (fFirstLine)
			{
				pbDest = WriteFirstLineFgBgImage(pbDest, g_MaskSpecialFgBg2, fgPel, 8);
			}
			else
			{
				pbDest = WriteFgBgImage(pbDest, rowDelta, g_MaskSpecialFgBg2, fgPel, 8);
			}
			continue;
		}

		/* Handle White Order. */
		if (code == SPECIAL_WHITE)
		{
			pbSrc = pbSrc + 1;
			WritePixel(pbDest, GetColorWhite());
			pbDest = NextPixel(pbDest);
			continue;
		}

		/* Handle Black Order. */
		if (code == SPECIAL_BLACK)
		{
			pbSrc = pbSrc + 1;
			WritePixel(pbDest, GetColorBlack());
			pbDest = NextPixel(pbDest);
			continue;
		}
	}
}
