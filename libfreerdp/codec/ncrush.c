/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * NCrush (RDP6) Bulk Data Compression
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/bitstream.h>

#include <freerdp/codec/ncrush.h>

const BYTE NCrushMaskTable[39] =
{
	0x11, /* 0 */
	0x9E, /* 1 */
	0xA1, /* 2 */
	0x00, /* 3 */
	0x00, /* 4 */
	0x01, /* 5 */
	0x00, /* 6 */
	0x03, /* 7 */
	0x00, /* 8 */
	0x07, /* 9 */
	0x00, /* 10 */
	0x0F, /* 11 */
	0x00, /* 12 */
	0x1F, /* 13 */
	0x00, /* 14 */
	0x3F, /* 15 */
	0x00, /* 16 */
	0x7F, /* 17 */
	0x00, /* 18 */
	0xFF, /* 19 */
	0x00, /* 20 */
	0xFF, /* 21 */
	0x01, /* 22 */
	0xFF, /* 23 */
	0x03, /* 24 */
	0xFF, /* 25 */
	0x07, /* 26 */
	0xFF, /* 27 */
	0x0F, /* 28 */
	0xFF, /* 29 */
	0x1F, /* 30 */
	0xFF, /* 31 */
	0x3F, /* 32 */
	0xFF, /* 33 */
	0x7F, /* 34 */
	0xFF, /* 35 */
	0xFF, /* 36 */
	0x00, /* 37 */
	0x00 /* 38 */
};

const BYTE HuffLengthLEC[294] =
{
	6, /* 0 */
	6, /* 1 */
	6, /* 2 */
	7, /* 3 */
	7, /* 4 */
	7, /* 5 */
	7, /* 6 */
	7, /* 7 */
	7, /* 8 */
	7, /* 9 */
	7, /* 10 */
	8, /* 11 */
	8, /* 12 */
	8, /* 13 */
	8, /* 14 */
	8, /* 15 */
	8, /* 16 */
	8, /* 17 */
	9, /* 18 */
	8, /* 19 */
	9, /* 20 */
	9, /* 21 */
	9, /* 22 */
	9, /* 23 */
	8, /* 24 */
	8, /* 25 */
	9, /* 26 */
	9, /* 27 */
	9, /* 28 */
	9, /* 29 */
	9, /* 30 */
	9, /* 31 */
	8, /* 32 */
	9, /* 33 */
	9, /* 34 */
	10, /* 35 */
	9, /* 36 */
	9, /* 37 */
	9, /* 38 */
	9, /* 39 */
	9, /* 40 */
	9, /* 41 */
	9, /* 42 */
	10, /* 43 */
	9, /* 44 */
	10, /* 45 */
	10, /* 46 */
	10, /* 47 */
	9, /* 48 */
	9, /* 49 */
	10, /* 50 */
	9, /* 51 */
	10, /* 52 */
	9, /* 53 */
	10, /* 54 */
	9, /* 55 */
	9, /* 56 */
	9, /* 57 */
	10, /* 58 */
	10, /* 59 */
	9, /* 60 */
	10, /* 61 */
	9, /* 62 */
	9, /* 63 */
	8, /* 64 */
	9, /* 65 */
	9, /* 66 */
	9, /* 67 */
	9, /* 68 */
	10, /* 69 */
	10, /* 70 */
	10, /* 71 */
	9, /* 72 */
	9, /* 73 */
	10, /* 74 */
	10, /* 75 */
	10, /* 76 */
	10, /* 77 */
	10, /* 78 */
	10, /* 79 */
	9, /* 80 */
	9, /* 81 */
	10, /* 82 */
	10, /* 83 */
	10, /* 84 */
	10, /* 85 */
	10, /* 86 */
	10, /* 87 */
	10, /* 88 */
	9, /* 89 */
	10, /* 90 */
	10, /* 91 */
	10, /* 92 */
	10, /* 93 */
	10, /* 94 */
	10, /* 95 */
	8, /* 96 */
	10, /* 97 */
	10, /* 98 */
	10, /* 99 */
	10, /* 100 */
	10, /* 101 */
	10, /* 102 */
	10, /* 103 */
	10, /* 104 */
	10, /* 105 */
	10, /* 106 */
	10, /* 107 */
	10, /* 108 */
	10, /* 109 */
	10, /* 110 */
	10, /* 111 */
	9, /* 112 */
	10, /* 113 */
	10, /* 114 */
	10, /* 115 */
	10, /* 116 */
	10, /* 117 */
	10, /* 118 */
	10, /* 119 */
	9, /* 120 */
	10, /* 121 */
	10, /* 122 */
	10, /* 123 */
	10, /* 124 */
	10, /* 125 */
	10, /* 126 */
	9, /* 127 */
	7, /* 128 */
	9, /* 129 */
	9, /* 130 */
	10, /* 131 */
	9, /* 132 */
	10, /* 133 */
	10, /* 134 */
	10, /* 135 */
	9, /* 136 */
	10, /* 137 */
	10, /* 138 */
	10, /* 139 */
	10, /* 140 */
	10, /* 141 */
	10, /* 142 */
	10, /* 143 */
	9, /* 144 */
	10, /* 145 */
	10, /* 146 */
	10, /* 147 */
	10, /* 148 */
	10, /* 149 */
	10, /* 150 */
	10, /* 151 */
	10, /* 152 */
	10, /* 153 */
	10, /* 154 */
	10, /* 155 */
	10, /* 156 */
	10, /* 157 */
	10, /* 158 */
	10, /* 159 */
	10, /* 160 */
	10, /* 161 */
	10, /* 162 */
	10, /* 163 */
	10, /* 164 */
	10, /* 165 */
	10, /* 166 */
	10, /* 167 */
	10, /* 168 */
	10, /* 169 */
	10, /* 170 */
	13, /* 171 */
	10, /* 172 */
	10, /* 173 */
	10, /* 174 */
	10, /* 175 */
	10, /* 176 */
	10, /* 177 */
	11, /* 178 */
	10, /* 179 */
	10, /* 180 */
	10, /* 181 */
	10, /* 182 */
	10, /* 183 */
	10, /* 184 */
	10, /* 185 */
	10, /* 186 */
	10, /* 187 */
	10, /* 188 */
	10, /* 189 */
	10, /* 190 */
	10, /* 191 */
	9, /* 192 */
	10, /* 193 */
	10, /* 194 */
	10, /* 195 */
	10, /* 196 */
	10, /* 197 */
	9, /* 198 */
	10, /* 199 */
	10, /* 200 */
	10, /* 201 */
	10, /* 202 */
	10, /* 203 */
	9, /* 204 */
	10, /* 205 */
	10, /* 206 */
	10, /* 207 */
	9, /* 208 */
	10, /* 209 */
	10, /* 210 */
	10, /* 211 */
	10, /* 212 */
	10, /* 213 */
	10, /* 214 */
	10, /* 215 */
	10, /* 216 */
	10, /* 217 */
	10, /* 218 */
	10, /* 219 */
	10, /* 220 */
	10, /* 221 */
	10, /* 222 */
	10, /* 223 */
	9, /* 224 */
	10, /* 225 */
	10, /* 226 */
	10, /* 227 */
	10, /* 228 */
	10, /* 229 */
	10, /* 230 */
	10, /* 231 */
	10, /* 232 */
	10, /* 233 */
	10, /* 234 */
	10, /* 235 */
	10, /* 236 */
	10, /* 237 */
	9, /* 238 */
	10, /* 239 */
	8, /* 240 */
	9, /* 241 */
	9, /* 242 */
	10, /* 243 */
	9, /* 244 */
	10, /* 245 */
	10, /* 246 */
	10, /* 247 */
	9, /* 248 */
	10, /* 249 */
	10, /* 250 */
	10, /* 251 */
	9, /* 252 */
	9, /* 253 */
	8, /* 254 */
	7, /* 255 */
	13, /* 256 */
	13, /* 257 */
	7, /* 258 */
	7, /* 259 */
	10, /* 260 */
	7, /* 261 */
	7, /* 262 */
	6, /* 263 */
	6, /* 264 */
	6, /* 265 */
	6, /* 266 */
	5, /* 267 */
	6, /* 268 */
	6, /* 269 */
	6, /* 270 */
	5, /* 271 */
	6, /* 272 */
	5, /* 273 */
	6, /* 274 */
	6, /* 275 */
	6, /* 276 */
	6, /* 277 */
	6, /* 278 */
	6, /* 279 */
	6, /* 280 */
	6, /* 281 */
	6, /* 282 */
	6, /* 283 */
	6, /* 284 */
	6, /* 285 */
	6, /* 286 */
	6, /* 287 */
	8, /* 288 */
	5, /* 289 */
	6, /* 290 */
	7, /* 291 */
	7, /* 292 */
	13 /* 293 */
};

const BYTE HuffLengthL[32] =
{
	4, /* 0 */
	2, /* 1 */
	3, /* 2 */
	4, /* 3 */
	3, /* 4 */
	4, /* 5 */
	4, /* 6 */
	5, /* 7 */
	4, /* 8 */
	5, /* 9 */
	5, /* 10 */
	6, /* 11 */
	6, /* 12 */
	7, /* 13 */
	7, /* 14 */
	8, /* 15 */
	7, /* 16 */
	8, /* 17 */
	8, /* 18 */
	9, /* 19 */
	9, /* 20 */
	8, /* 21 */
	9, /* 22 */
	9, /* 23 */
	9, /* 24 */
	9, /* 25 */
	9, /* 26 */
	9, /* 27 */
	9, /* 28 */
	9, /* 29 */
	9, /* 30 */
	9 /* 31 */
};

int ncrush_decompress(NCRUSH_CONTEXT* ncrush, BYTE* pSrcData, UINT32 SrcSize, BYTE** ppDstData, UINT32* pDstSize, UINT32 flags)
{
	UINT32 bits;
	UINT32 nbits;
	BYTE* SrcPtr;
	UINT32 CopyOffset;
	UINT32 LengthOfMatch;
	UINT32 accumulator;
	BYTE* HistoryPtr;
	UINT32 HistoryOffset;
	BYTE* HistoryBuffer;
	BYTE* HistoryBufferEnd;
	UINT32 HistoryBufferSize;
	wBitStream* bs = ncrush->bs;

	HistoryBuffer = ncrush->HistoryBuffer;
	HistoryBufferSize = ncrush->HistoryBufferSize;
	HistoryBufferEnd = &HistoryBuffer[HistoryBufferSize - 1];

	BitStream_Attach(bs, pSrcData, SrcSize);
	BitStream_Fetch(bs);

	if (flags & PACKET_AT_FRONT)
	{
		if ((ncrush->HistoryPtr - 32768) <= HistoryBuffer)
			return -1;

		CopyMemory(HistoryBuffer, (ncrush->HistoryPtr - 32768), 32768);
		ncrush->HistoryOffset = 32768;
		ncrush->HistoryPtr = &(HistoryBuffer[ncrush->HistoryOffset]);
	}

	if (flags & PACKET_FLUSHED)
	{
		ncrush->HistoryOffset = 0;
		ncrush->HistoryPtr = HistoryBuffer;
		ZeroMemory(HistoryBuffer, ncrush->HistoryBufferSize);
		ZeroMemory(&(ncrush->OffsetCache), sizeof(ncrush->OffsetCache));
	}

	HistoryPtr = ncrush->HistoryPtr;
	HistoryOffset = ncrush->HistoryOffset;

	if (!(flags & PACKET_COMPRESSED))
	{
		CopyMemory(HistoryPtr, pSrcData, SrcSize);
		HistoryPtr += SrcSize;
		HistoryOffset += SrcSize;
		ncrush->HistoryPtr = HistoryPtr;
		ncrush->HistoryOffset = HistoryOffset;
		*ppDstData = HistoryPtr;
		*pDstSize = SrcSize;
		return 1;
	}

	nbits = 32;
	bits = *((UINT32*) pSrcData);
	SrcPtr = pSrcData + 4;

	accumulator = bits & *((UINT16*) &NCrushMaskTable[29]);

	printf("MaskedBits: 0x%04X Mask: 0x%04X\n", accumulator, *((UINT16*) &NCrushMaskTable[29]));

	return 1;
}

int ncrush_compress(NCRUSH_CONTEXT* ncrush, BYTE* pSrcData, UINT32 SrcSize, BYTE* pDstData, UINT32* pDstSize, UINT32* pFlags)
{
	return 1;
}

NCRUSH_CONTEXT* ncrush_context_new(BOOL Compressor)
{
	NCRUSH_CONTEXT* ncrush;

	ncrush = (NCRUSH_CONTEXT*) calloc(1, sizeof(NCRUSH_CONTEXT));

	if (ncrush)
	{
		ncrush->Compressor = Compressor;

		ncrush->bs = BitStream_New();

		ncrush->HistoryBufferSize = 65536;
		ZeroMemory(&(ncrush->HistoryBuffer), sizeof(ncrush->HistoryBuffer));
		ZeroMemory(&(ncrush->OffsetCache), sizeof(ncrush->OffsetCache));

		ncrush->HistoryOffset = 0;
		ncrush->HistoryPtr = &(ncrush->HistoryBuffer[ncrush->HistoryOffset]);
	}

	return ncrush;
}

void ncrush_context_free(NCRUSH_CONTEXT* ncrush)
{
	if (ncrush)
	{
		BitStream_Free(ncrush->bs);

		free(ncrush);
	}
}
