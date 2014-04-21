/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * MPPC Bulk Data Compression
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
#include <winpr/stream.h>
#include <winpr/bitstream.h>

#include <freerdp/codec/mppc.h>

#define MPPC_MATCH_INDEX(_sym1, _sym2, _sym3) \
	((((MPPC_MATCH_TABLE[_sym3] << 16) + (MPPC_MATCH_TABLE[_sym2] << 8) + MPPC_MATCH_TABLE[_sym1]) & 0x07FFF000) >> 12)

const UINT32 MPPC_MATCH_TABLE[256] =
{
	0x00000000, 0x009CCF93, 0x01399F26, 0x01D66EB9, 0x02733E4C, 0x03100DDF, 0x03ACDD72, 0x0449AD05,
	0x04E67C98, 0x05834C2B, 0x06201BBE, 0x06BCEB51, 0x0759BAE4, 0x07F68A77, 0x08935A0A, 0x0930299D,
	0x09CCF930, 0x0A69C8C3, 0x0B069856, 0x0BA367E9, 0x0C40377C, 0x0CDD070F, 0x0D79D6A2, 0x0E16A635,
	0x0EB375C8, 0x0F50455B, 0x0FED14EE, 0x1089E481, 0x1126B414, 0x11C383A7, 0x1260533A, 0x12FD22CD,
	0x1399F260, 0x1436C1F3, 0x14D39186, 0x15706119, 0x160D30AC, 0x16AA003F, 0x1746CFD2, 0x17E39F65,
	0x18806EF8, 0x191D3E8B, 0x19BA0E1E, 0x1A56DDB1, 0x1AF3AD44, 0x1B907CD7, 0x1C2D4C6A, 0x1CCA1BFD,
	0x1D66EB90, 0x1E03BB23, 0x1EA08AB6, 0x1F3D5A49, 0x1FDA29DC, 0x2076F96F, 0x2113C902, 0x21B09895,
	0x224D6828, 0x22EA37BB, 0x2387074E, 0x2423D6E1, 0x24C0A674, 0x255D7607, 0x25FA459A, 0x2697152D,
	0x2733E4C0, 0x27D0B453, 0x286D83E6, 0x290A5379, 0x29A7230C, 0x2A43F29F, 0x2AE0C232, 0x2B7D91C5,
	0x2C1A6158, 0x2CB730EB, 0x2D54007E, 0x2DF0D011, 0x2E8D9FA4, 0x2F2A6F37, 0x2FC73ECA, 0x30640E5D,
	0x3100DDF0, 0x319DAD83, 0x323A7D16, 0x32D74CA9, 0x33741C3C, 0x3410EBCF, 0x34ADBB62, 0x354A8AF5,
	0x35E75A88, 0x36842A1B, 0x3720F9AE, 0x37BDC941, 0x385A98D4, 0x38F76867, 0x399437FA, 0x3A31078D,
	0x3ACDD720, 0x3B6AA6B3, 0x3C077646, 0x3CA445D9, 0x3D41156C, 0x3DDDE4FF, 0x3E7AB492, 0x3F178425,
	0x3FB453B8, 0x4051234B, 0x40EDF2DE, 0x418AC271, 0x42279204, 0x42C46197, 0x4361312A, 0x43FE00BD,
	0x449AD050, 0x45379FE3, 0x45D46F76, 0x46713F09, 0x470E0E9C, 0x47AADE2F, 0x4847ADC2, 0x48E47D55,
	0x49814CE8, 0x4A1E1C7B, 0x4ABAEC0E, 0x4B57BBA1, 0x4BF48B34, 0x4C915AC7, 0x4D2E2A5A, 0x4DCAF9ED,
	0x4E67C980, 0x4F049913, 0x4FA168A6, 0x503E3839, 0x50DB07CC, 0x5177D75F, 0x5214A6F2, 0x52B17685,
	0x534E4618, 0x53EB15AB, 0x5487E53E, 0x5524B4D1, 0x55C18464, 0x565E53F7, 0x56FB238A, 0x5797F31D,
	0x5834C2B0, 0x58D19243, 0x596E61D6, 0x5A0B3169, 0x5AA800FC, 0x5B44D08F, 0x5BE1A022, 0x5C7E6FB5,
	0x5D1B3F48, 0x5DB80EDB, 0x5E54DE6E, 0x5EF1AE01, 0x5F8E7D94, 0x602B4D27, 0x60C81CBA, 0x6164EC4D,
	0x6201BBE0, 0x629E8B73, 0x633B5B06, 0x63D82A99, 0x6474FA2C, 0x6511C9BF, 0x65AE9952, 0x664B68E5,
	0x66E83878, 0x6785080B, 0x6821D79E, 0x68BEA731, 0x695B76C4, 0x69F84657, 0x6A9515EA, 0x6B31E57D,
	0x6BCEB510, 0x6C6B84A3, 0x6D085436, 0x6DA523C9, 0x6E41F35C, 0x6EDEC2EF, 0x6F7B9282, 0x70186215,
	0x70B531A8, 0x7152013B, 0x71EED0CE, 0x728BA061, 0x73286FF4, 0x73C53F87, 0x74620F1A, 0x74FEDEAD,
	0x759BAE40, 0x76387DD3, 0x76D54D66, 0x77721CF9, 0x780EEC8C, 0x78ABBC1F, 0x79488BB2, 0x79E55B45,
	0x7A822AD8, 0x7B1EFA6B, 0x7BBBC9FE, 0x7C589991, 0x7CF56924, 0x7D9238B7, 0x7E2F084A, 0x7ECBD7DD,
	0x7F68A770, 0x80057703, 0x80A24696, 0x813F1629, 0x81DBE5BC, 0x8278B54F, 0x831584E2, 0x83B25475,
	0x844F2408, 0x84EBF39B, 0x8588C32E, 0x862592C1, 0x86C26254, 0x875F31E7, 0x87FC017A, 0x8898D10D,
	0x8935A0A0, 0x89D27033, 0x8A6F3FC6, 0x8B0C0F59, 0x8BA8DEEC, 0x8C45AE7F, 0x8CE27E12, 0x8D7F4DA5,
	0x8E1C1D38, 0x8EB8ECCB, 0x8F55BC5E, 0x8FF28BF1, 0x908F5B84, 0x912C2B17, 0x91C8FAAA, 0x9265CA3D,
	0x930299D0, 0x939F6963, 0x943C38F6, 0x94D90889, 0x9575D81C, 0x9612A7AF, 0x96AF7742, 0x974C46D5,
	0x97E91668, 0x9885E5FB, 0x9922B58E, 0x99BF8521, 0x9A5C54B4, 0x9AF92447, 0x9B95F3DA, 0x9C32C36D
};

//#define DEBUG_MPPC	1

int mppc_decompress(MPPC_CONTEXT* mppc, BYTE* pSrcData, UINT32 SrcSize, BYTE** ppDstData, UINT32* pDstSize, UINT32 flags)
{
	BYTE Literal;
	BYTE* SrcPtr;
	UINT32 CopyOffset;
	UINT32 LengthOfMatch;
	UINT32 accumulator;
	BYTE* HistoryPtr;
	UINT32 HistoryOffset;
	BYTE* HistoryBuffer;
	BYTE* HistoryBufferEnd;
	UINT32 HistoryBufferSize;
	UINT32 CompressionLevel;
	wBitStream* bs = mppc->bs;

	HistoryBuffer = mppc->HistoryBuffer;
	HistoryBufferSize = mppc->HistoryBufferSize;
	HistoryBufferEnd = &HistoryBuffer[HistoryBufferSize - 1];
	CompressionLevel = mppc->CompressionLevel;

	BitStream_Attach(bs, pSrcData, SrcSize);
	BitStream_Fetch(bs);

	if (flags & PACKET_AT_FRONT)
	{
		mppc->HistoryOffset = 0;
		mppc->HistoryPtr = HistoryBuffer;
	}

	if (flags & PACKET_FLUSHED)
	{
		mppc->HistoryOffset = 0;
		mppc->HistoryPtr = HistoryBuffer;
		ZeroMemory(HistoryBuffer, mppc->HistoryBufferSize);
	}

	HistoryPtr = mppc->HistoryPtr;
	HistoryOffset = mppc->HistoryOffset;

	if (!(flags & PACKET_COMPRESSED))
	{
		*pDstSize = SrcSize;
		*ppDstData = pSrcData;
		return 1;
	}

	while ((bs->length - bs->position) >= 8)
	{
		accumulator = bs->accumulator;

		/**
		 * Literal Encoding
		 */

		if ((accumulator & 0x80000000) == 0x00000000)
		{
			/**
			 * Literal, less than 0x80
			 * bit 0 followed by the lower 7 bits of the literal
			 */

			Literal = ((accumulator & 0x7F000000) >> 24);

			*(HistoryPtr) = Literal;
			HistoryPtr++;

			BitStream_Shift(bs, 8);

			continue;
		}
		else if ((accumulator & 0xC0000000) == 0x80000000)
		{
			/**
			 * Literal, greater than 0x7F
			 * bits 10 followed by the lower 7 bits of the literal
			 */

			Literal = ((accumulator & 0x3F800000) >> 23) + 0x80;

			*(HistoryPtr) = Literal;
			HistoryPtr++;

			BitStream_Shift(bs, 9);

			continue;
		}

		/**
		 * CopyOffset Encoding
		 */

		CopyOffset = 0;

		if (CompressionLevel) /* RDP5 */
		{
			if ((accumulator & 0xF8000000) == 0xF8000000)
			{
				/**
				 * CopyOffset, range [0, 63]
				 * bits 11111 + lower 6 bits of CopyOffset
				 */

				CopyOffset = ((accumulator >> 21) & 0x3F);
				BitStream_Shift(bs, 11);
			}
			else if ((accumulator & 0xF8000000) == 0xF0000000)
			{
				/**
				 * CopyOffset, range [64, 319]
				 * bits 11110 + lower 8 bits of (CopyOffset - 64)
				 */

				CopyOffset = ((accumulator >> 19) & 0xFF) + 64;
				BitStream_Shift(bs, 13);
			}
			else if ((accumulator & 0xF0000000) == 0xE0000000)
			{
				/**
				 * CopyOffset, range [320, 2367]
				 * bits 1110 + lower 11 bits of (CopyOffset - 320)
				 */

				CopyOffset = ((accumulator >> 17) & 0x7FF) + 320;
				BitStream_Shift(bs, 15);
			}
			else if ((accumulator & 0xE0000000) == 0xC0000000)
			{
				/**
				 * CopyOffset, range [2368, ]
				 * bits 110 + lower 16 bits of (CopyOffset - 2368)
				 */

				CopyOffset = ((accumulator >> 13) & 0xFFFF) + 2368;
				BitStream_Shift(bs, 19);
			}
			else
			{
				/* Invalid CopyOffset Encoding */
				return -1;
			}
		}
		else /* RDP4 */
		{
			if ((accumulator & 0xF0000000) == 0xF0000000)
			{
				/**
				 * CopyOffset, range [0, 63]
				 * bits 1111 + lower 6 bits of CopyOffset
				 */

				CopyOffset = ((accumulator >> 22) & 0x3F);
				BitStream_Shift(bs, 10);
			}
			else if ((accumulator & 0xF0000000) == 0xE0000000)
			{
				/**
				 * CopyOffset, range [64, 319]
				 * bits 1110 + lower 8 bits of (CopyOffset - 64)
				 */

				CopyOffset = ((accumulator >> 20) & 0xFF) + 64;
				BitStream_Shift(bs, 12);
			}
			else if ((accumulator & 0xE0000000) == 0xC0000000)
			{
				/**
				 * CopyOffset, range [320, 8191]
				 * bits 110 + lower 13 bits of (CopyOffset - 320)
				 */

				CopyOffset = ((accumulator >> 16) & 0x1FFF) + 320;
				BitStream_Shift(bs, 16);
			}
			else
			{
				/* Invalid CopyOffset Encoding */
				return -1;
			}
		}

		/**
		 * LengthOfMatch Encoding
		 */

		LengthOfMatch = 0;
		accumulator = bs->accumulator;

		if ((accumulator & 0x80000000) == 0x00000000)
		{
			/**
			 * LengthOfMatch [3]
			 * bit 0 + 0 lower bits of LengthOfMatch
			 */

			LengthOfMatch = 3;
			BitStream_Shift(bs, 1);
		}
		else if ((accumulator & 0xC0000000) == 0x80000000)
		{
			/**
			 * LengthOfMatch [4, 7]
			 * bits 10 + 2 lower bits of LengthOfMatch
			 */

			LengthOfMatch = ((accumulator >> 28) & 0x0003) + 0x0004;
			BitStream_Shift(bs, 4);
		}
		else if ((accumulator & 0xE0000000) == 0xC0000000)
		{
			/**
			 * LengthOfMatch [8, 15]
			 * bits 110 + 3 lower bits of LengthOfMatch
			 */

			LengthOfMatch = ((accumulator >> 26) & 0x0007) + 0x0008;
			BitStream_Shift(bs, 6);
		}
		else if ((accumulator & 0xF0000000) == 0xE0000000)
		{
			/**
			 * LengthOfMatch [16, 31]
			 * bits 1110 + 4 lower bits of LengthOfMatch
			 */

			LengthOfMatch = ((accumulator >> 24) & 0x000F) + 0x0010;
			BitStream_Shift(bs, 8);
		}
		else if ((accumulator & 0xF8000000) == 0xF0000000)
		{
			/**
			 * LengthOfMatch [32, 63]
			 * bits 11110 + 5 lower bits of LengthOfMatch
			 */

			LengthOfMatch = ((accumulator >> 22) & 0x001F) + 0x0020;
			BitStream_Shift(bs, 10);
		}
		else if ((accumulator & 0xFC000000) == 0xF8000000)
		{
			/**
			 * LengthOfMatch [64, 127]
			 * bits 111110 + 6 lower bits of LengthOfMatch
			 */

			LengthOfMatch = ((accumulator >> 20) & 0x003F) + 0x0040;
			BitStream_Shift(bs, 12);
		}
		else if ((accumulator & 0xFE000000) == 0xFC000000)
		{
			/**
			 * LengthOfMatch [128, 255]
			 * bits 1111110 + 7 lower bits of LengthOfMatch
			 */

			LengthOfMatch = ((accumulator >> 18) & 0x007F) + 0x0080;
			BitStream_Shift(bs, 14);
		}
		else if ((accumulator & 0xFF000000) == 0xFE000000)
		{
			/**
			 * LengthOfMatch [256, 511]
			 * bits 11111110 + 8 lower bits of LengthOfMatch
			 */

			LengthOfMatch = ((accumulator >> 16) & 0x00FF) + 0x0100;
			BitStream_Shift(bs, 16);
		}
		else if ((accumulator & 0xFF800000) == 0xFF000000)
		{
			/**
			 * LengthOfMatch [512, 1023]
			 * bits 111111110 + 9 lower bits of LengthOfMatch
			 */

			LengthOfMatch = ((accumulator >> 14) & 0x01FF) + 0x0200;
			BitStream_Shift(bs, 18);
		}
		else if ((accumulator & 0xFFC00000) == 0xFF800000)
		{
			/**
			 * LengthOfMatch [1024, 2047]
			 * bits 1111111110 + 10 lower bits of LengthOfMatch
			 */

			LengthOfMatch = ((accumulator >> 12) & 0x03FF) + 0x0400;
			BitStream_Shift(bs, 20);
		}
		else if ((accumulator & 0xFFE00000) == 0xFFC00000)
		{
			/**
			 * LengthOfMatch [2048, 4095]
			 * bits 11111111110 + 11 lower bits of LengthOfMatch
			 */

			LengthOfMatch = ((accumulator >> 10) & 0x07FF) + 0x0800;
			BitStream_Shift(bs, 22);
		}
		else if ((accumulator & 0xFFF00000) == 0xFFE00000)
		{
			/**
			 * LengthOfMatch [4096, 8191]
			 * bits 111111111110 + 12 lower bits of LengthOfMatch
			 */

			LengthOfMatch = ((accumulator >> 8) & 0x0FFF) + 0x1000;
			BitStream_Shift(bs, 24);
		}
		else if (((accumulator & 0xFFF80000) == 0xFFF00000) && CompressionLevel) /* RDP5 */
		{
			/**
			 * LengthOfMatch [8192, 16383]
			 * bits 1111111111110 + 13 lower bits of LengthOfMatch
			 */

			LengthOfMatch = ((accumulator >> 6) & 0x1FFF) + 0x2000;
			BitStream_Shift(bs, 26);
		}
		else if (((accumulator & 0xFFFC0000) == 0xFFF80000) && CompressionLevel) /* RDP5 */
		{
			/**
			 * LengthOfMatch [16384, 32767]
			 * bits 11111111111110 + 14 lower bits of LengthOfMatch
			 */

			LengthOfMatch = ((accumulator >> 4) & 0x3FFF) + 0x4000;
			BitStream_Shift(bs, 28);
		}
		else if (((accumulator & 0xFFFE0000) == 0xFFFC0000) && CompressionLevel) /* RDP5 */
		{
			/**
			 * LengthOfMatch [32768, 65535]
			 * bits 111111111111110 + 15 lower bits of LengthOfMatch
			 */

			LengthOfMatch = ((accumulator >> 2) & 0x7FFF) + 0x8000;
			BitStream_Shift(bs, 30);
		}
		else
		{
			/* Invalid LengthOfMatch Encoding */
			return -1;
		}

#ifdef DEBUG_MPPC
		printf("<%d,%d>\n", (int) CopyOffset, (int) LengthOfMatch);
#endif

		SrcPtr = HistoryPtr - CopyOffset;

		if (SrcPtr >= HistoryBuffer)
		{
			while (LengthOfMatch > 0)
			{
				*(HistoryPtr) = *SrcPtr;

				HistoryPtr++;
				SrcPtr++;

				LengthOfMatch--;
			}
		}
		else
		{
			SrcPtr = HistoryBufferEnd - (CopyOffset - (HistoryPtr - HistoryBuffer));
			SrcPtr++;

			while (LengthOfMatch && (SrcPtr <= HistoryBufferEnd))
			{
				*HistoryPtr++ = *SrcPtr++;
				LengthOfMatch--;
			}

			SrcPtr = HistoryBuffer;

			while (LengthOfMatch > 0)
			{
				*HistoryPtr++ = *SrcPtr++;
				LengthOfMatch--;
			}
		}
	}

	*pDstSize = (UINT32) (HistoryPtr - mppc->HistoryPtr);
	*ppDstData = mppc->HistoryPtr;
	mppc->HistoryPtr = HistoryPtr;

	return 1;
}

int mppc_compress(MPPC_CONTEXT* mppc, BYTE* pSrcData, UINT32 SrcSize, BYTE** ppDstData, UINT32* pDstSize, UINT32* pFlags)
{
	BYTE* pSrcPtr;
	BYTE* pSrcEnd;
	BYTE* pDstEnd;
	BYTE* MatchPtr;
	UINT32 DstSize;
	BYTE* pDstData;
	UINT32 MatchIndex;
	UINT32 accumulator;
	BOOL PacketFlushed;
	BOOL PacketAtFront;
	DWORD CopyOffset;
	DWORD LengthOfMatch;
	BYTE* HistoryBuffer;
	BYTE* HistoryPtr;
	UINT32 HistoryOffset;
	UINT32 HistoryBufferSize;
	BYTE Sym1, Sym2, Sym3;
	UINT32 CompressionLevel;
	wBitStream* bs = mppc->bs;

	HistoryBuffer = mppc->HistoryBuffer;
	HistoryBufferSize = mppc->HistoryBufferSize;
	CompressionLevel = mppc->CompressionLevel;

	HistoryPtr = mppc->HistoryPtr;
	HistoryOffset = mppc->HistoryOffset;

	*pFlags = 0;
	PacketFlushed = FALSE;

	if (((HistoryOffset + SrcSize) < (HistoryBufferSize - 3)) && HistoryOffset)
	{
		PacketAtFront = FALSE;
	}
	else
	{
		if (HistoryOffset == (HistoryBufferSize + 1))
			PacketFlushed = TRUE;

		HistoryOffset = 0;
		PacketAtFront = TRUE;
	}

	HistoryPtr = &(HistoryBuffer[HistoryOffset]);

	pDstData = *ppDstData;

	if (!pDstData)
		return -1;

	DstSize = *pDstSize;

	if (DstSize < SrcSize)
		return -1;

	DstSize = SrcSize;

	BitStream_Attach(bs, pDstData, DstSize);

	pSrcPtr = pSrcData;
	pSrcEnd = &(pSrcData[SrcSize - 1]);
	pDstEnd = &(pDstData[DstSize - 1]);

	while (pSrcPtr < (pSrcEnd - 2))
	{
		Sym1 = pSrcPtr[0];
		Sym2 = pSrcPtr[1];
		Sym3 = pSrcPtr[2];

		*HistoryPtr++ = *pSrcPtr++;

		MatchIndex = MPPC_MATCH_INDEX(Sym1, Sym2, Sym3);
		MatchPtr = &(HistoryBuffer[mppc->MatchBuffer[MatchIndex]]);

		if (MatchPtr != (HistoryPtr - 1))
			mppc->MatchBuffer[MatchIndex] = (UINT16) (HistoryPtr - HistoryBuffer);

		if (mppc->HistoryPtr < HistoryPtr)
			mppc->HistoryPtr = HistoryPtr;

		if ((Sym1 != *(MatchPtr - 1)) || (Sym2 != MatchPtr[0]) || (Sym3 != MatchPtr[1]) ||
				(&MatchPtr[1] > mppc->HistoryPtr) || (MatchPtr == HistoryBuffer) ||
				(MatchPtr == (HistoryPtr - 1)) || (MatchPtr == HistoryPtr))
		{
			if (((bs->position / 8) + 2) > (DstSize - 1))
			{
				ZeroMemory(HistoryBuffer, HistoryBufferSize);
				ZeroMemory(mppc->MatchBuffer, sizeof(mppc->MatchBuffer));
				mppc->HistoryOffset = HistoryBufferSize + 1;
				*pFlags |= PACKET_FLUSHED;
				*pFlags |= CompressionLevel;
				*ppDstData = pSrcData;
				*pDstSize = SrcSize;
				return 1;
			}

			accumulator = Sym1;

#ifdef DEBUG_MPPC
			printf("%c", accumulator);
#endif

			if (accumulator < 0x80)
			{
				/* 8 bits of literal are encoded as-is */
				BitStream_Write_Bits(bs, accumulator, 8);
			}
			else
			{
				/* bits 10 followed by lower 7 bits of literal */
				accumulator = 0x100 | (accumulator & 0x7F);
				BitStream_Write_Bits(bs, accumulator, 9);
			}
		}
		else
		{
			CopyOffset = (HistoryBufferSize - 1) & (HistoryPtr - MatchPtr);

			*HistoryPtr++ = Sym2;
			*HistoryPtr++ = Sym3;
			pSrcPtr += 2;

			LengthOfMatch = 3;
			MatchPtr += 2;

			while ((*pSrcPtr == *MatchPtr) && (pSrcPtr < pSrcEnd) && (MatchPtr <= mppc->HistoryPtr))
			{
				MatchPtr++;
				*HistoryPtr++ = *pSrcPtr++;
				LengthOfMatch++;
			}

#ifdef DEBUG_MPPC
			printf("<%d,%d>", (int) CopyOffset, (int) LengthOfMatch);
#endif

			/* Encode CopyOffset */

			if (((bs->position / 8) + 7) > (DstSize - 1))
			{
				ZeroMemory(HistoryBuffer, HistoryBufferSize);
				ZeroMemory(mppc->MatchBuffer, sizeof(mppc->MatchBuffer));
				mppc->HistoryOffset = HistoryBufferSize + 1;
				*pFlags |= PACKET_FLUSHED;
				*pFlags |= CompressionLevel;
				*ppDstData = pSrcData;
				*pDstSize = SrcSize;
				return 1;
			}

			if (CompressionLevel) /* RDP5 */
			{
				if (CopyOffset < 64)
				{
					/* bits 11111 + lower 6 bits of CopyOffset */
					accumulator = 0x07C0 | (CopyOffset & 0x003F);
					BitStream_Write_Bits(bs, accumulator, 11);
				}
				else if ((CopyOffset >= 64) && (CopyOffset < 320))
				{
					/* bits 11110 + lower 8 bits of (CopyOffset - 64) */
					accumulator = 0x1E00 | ((CopyOffset - 64) & 0x00FF);
					BitStream_Write_Bits(bs, accumulator, 13);
				}
				else if ((CopyOffset >= 320) && (CopyOffset < 2368))
				{
					/* bits 1110 + lower 11 bits of (CopyOffset - 320) */
					accumulator = 0x7000 | ((CopyOffset - 320) & 0x07FF);
					BitStream_Write_Bits(bs, accumulator, 15);
				}
				else
				{
					/* bits 110 + lower 16 bits of (CopyOffset - 2368) */
					accumulator = 0x060000 | ((CopyOffset - 2368) & 0xFFFF);
					BitStream_Write_Bits(bs, accumulator, 19);
				}
			}
			else /* RDP4 */
			{
				if (CopyOffset < 64)
				{
					/* bits 1111 + lower 6 bits of CopyOffset */
					accumulator = 0x03C0 | (CopyOffset & 0x003F);
					BitStream_Write_Bits(bs, accumulator, 10);
				}
				else if ((CopyOffset >= 64) && (CopyOffset < 320))
				{
					/* bits 1110 + lower 8 bits of (CopyOffset - 64) */
					accumulator = 0x0E00 | ((CopyOffset - 64) & 0x00FF);
					BitStream_Write_Bits(bs, accumulator, 12);
				}
				else if ((CopyOffset >= 320) && (CopyOffset < 8192))
				{
					/* bits 110 + lower 13 bits of (CopyOffset - 320) */
					accumulator = 0xC000 | ((CopyOffset - 320) & 0x1FFF);
					BitStream_Write_Bits(bs, accumulator, 16);
				}
			}

			/* Encode LengthOfMatch */

			if (LengthOfMatch == 3)
			{
				/* 0 + 0 lower bits of LengthOfMatch */
				BitStream_Write_Bits(bs, 0, 1);
			}
			else if ((LengthOfMatch >= 4) && (LengthOfMatch < 8))
			{
				/* 10 + 2 lower bits of LengthOfMatch */
				accumulator = 0x0008 | (LengthOfMatch & 0x0003);
				BitStream_Write_Bits(bs, accumulator, 4);
			}
			else if ((LengthOfMatch >= 8) && (LengthOfMatch < 16))
			{
				/* 110 + 3 lower bits of LengthOfMatch */
				accumulator = 0x0030 | (LengthOfMatch & 0x0007);
				BitStream_Write_Bits(bs, accumulator, 6);
			}
			else if ((LengthOfMatch >= 16) && (LengthOfMatch < 32))
			{
				/* 1110 + 4 lower bits of LengthOfMatch */
				accumulator = 0x00E0 | (LengthOfMatch & 0x000F);
				BitStream_Write_Bits(bs, accumulator, 8);
			}
			else if ((LengthOfMatch >= 32) && (LengthOfMatch < 64))
			{
				/* 11110 + 5 lower bits of LengthOfMatch */
				accumulator = 0x03C0 | (LengthOfMatch & 0x001F);
				BitStream_Write_Bits(bs, accumulator, 10);
			}
			else if ((LengthOfMatch >= 64) && (LengthOfMatch < 128))
			{
				/* 111110 + 6 lower bits of LengthOfMatch */
				accumulator = 0x0F80 | (LengthOfMatch & 0x003F);
				BitStream_Write_Bits(bs, accumulator, 12);
			}
			else if ((LengthOfMatch >= 128) && (LengthOfMatch < 256))
			{
				/* 1111110 + 7 lower bits of LengthOfMatch */
				accumulator = 0x3F00 | (LengthOfMatch & 0x007F);
				BitStream_Write_Bits(bs, accumulator, 14);
			}
			else if ((LengthOfMatch >= 256) && (LengthOfMatch < 512))
			{
				/* 11111110 + 8 lower bits of LengthOfMatch */
				accumulator = 0xFE00 | (LengthOfMatch & 0x00FF);
				BitStream_Write_Bits(bs, accumulator, 16);
			}
			else if ((LengthOfMatch >= 512) && (LengthOfMatch < 1024))
			{
				/* 111111110 + 9 lower bits of LengthOfMatch */
				accumulator = 0x3FC00 | (LengthOfMatch & 0x01FF);
				BitStream_Write_Bits(bs, accumulator, 18);
			}
			else if ((LengthOfMatch >= 1024) && (LengthOfMatch < 2048))
			{
				/* 1111111110 + 10 lower bits of LengthOfMatch */
				accumulator = 0xFF800 | (LengthOfMatch & 0x03FF);
				BitStream_Write_Bits(bs, accumulator, 20);
			}
			else if ((LengthOfMatch >= 2048) && (LengthOfMatch < 4096))
			{
				/* 11111111110 + 11 lower bits of LengthOfMatch */
				accumulator = 0x3FF000 | (LengthOfMatch & 0x07FF);
				BitStream_Write_Bits(bs, accumulator, 22);
			}
			else if ((LengthOfMatch >= 4096) && (LengthOfMatch < 8192))
			{
				/* 111111111110 + 12 lower bits of LengthOfMatch */
				accumulator = 0xFFE000 | (LengthOfMatch & 0x0FFF);
				BitStream_Write_Bits(bs, accumulator, 24);
			}
			else if (((LengthOfMatch >= 8192) && (LengthOfMatch < 16384)) && CompressionLevel) /* RDP5 */
			{
				/* 1111111111110 + 13 lower bits of LengthOfMatch */
				accumulator = 0x3FFC000 | (LengthOfMatch & 0x1FFF);
				BitStream_Write_Bits(bs, accumulator, 26);
			}
			else if (((LengthOfMatch >= 16384) && (LengthOfMatch < 32768)) && CompressionLevel) /* RDP5 */
			{
				/* 11111111111110 + 14 lower bits of LengthOfMatch */
				accumulator = 0xFFF8000 | (LengthOfMatch & 0x3FFF);
				BitStream_Write_Bits(bs, accumulator, 28);
			}
			else if (((LengthOfMatch >= 32768) && (LengthOfMatch < 65536)) && CompressionLevel) /* RDP5 */
			{
				/* 111111111111110 + 15 lower bits of LengthOfMatch */
				accumulator = 0x3FFF0000 | (LengthOfMatch & 0x7FFF);
				BitStream_Write_Bits(bs, accumulator, 30);
			}
		}
	}

	/* Encode trailing symbols as literals */

	while (pSrcPtr <= pSrcEnd)
	{
		if (((bs->position / 8) + 2) > (DstSize - 1))
		{
			ZeroMemory(HistoryBuffer, HistoryBufferSize);
			ZeroMemory(mppc->MatchBuffer, sizeof(mppc->MatchBuffer));
			mppc->HistoryOffset = HistoryBufferSize + 1;
			*pFlags |= PACKET_FLUSHED;
			*pFlags |= CompressionLevel;
			*ppDstData = pSrcData;
			*pDstSize = SrcSize;
			return 1;
		}

		accumulator = *pSrcPtr;

#ifdef DEBUG_MPPC
		printf("%c", accumulator);
#endif

		if (accumulator < 0x80)
		{
			/* 8 bits of literal are encoded as-is */
			BitStream_Write_Bits(bs, accumulator, 8);
		}
		else
		{
			/* bits 10 followed by lower 7 bits of literal */
			accumulator = 0x100 | (accumulator & 0x7F);
			BitStream_Write_Bits(bs, accumulator, 9);
		}

		*HistoryPtr++ = *pSrcPtr++;
	}

	BitStream_Flush(bs);

	*pFlags |= PACKET_COMPRESSED;
	*pFlags |= CompressionLevel;

	if (PacketAtFront)
		*pFlags |= PACKET_AT_FRONT;

	if (PacketFlushed)
		*pFlags |= PACKET_FLUSHED;

	*pDstSize = ((bs->position + 7) / 8);

	mppc->HistoryPtr = HistoryPtr;
	mppc->HistoryOffset = HistoryPtr - HistoryBuffer;

#ifdef DEBUG_MPPC
	printf("\n");
#endif

	return 1;
}

void mppc_set_compression_level(MPPC_CONTEXT* mppc, DWORD CompressionLevel)
{
	if (CompressionLevel < 1)
	{
		mppc->CompressionLevel = 0;
		mppc->HistoryBufferSize = 8192;
	}
	else
	{
		mppc->CompressionLevel = 1;
		mppc->HistoryBufferSize = 65536;
	}
}

void mppc_context_reset(MPPC_CONTEXT* mppc)
{
	ZeroMemory(&(mppc->HistoryBuffer), sizeof(mppc->HistoryBuffer));
	ZeroMemory(&(mppc->MatchBuffer), sizeof(mppc->MatchBuffer));

	mppc->HistoryOffset = 0;
	mppc->HistoryPtr = &(mppc->HistoryBuffer[mppc->HistoryOffset]);
}

MPPC_CONTEXT* mppc_context_new(DWORD CompressionLevel, BOOL Compressor)
{
	MPPC_CONTEXT* mppc;

	mppc = calloc(1, sizeof(MPPC_CONTEXT));

	if (mppc)
	{
		mppc->Compressor = Compressor;

		if (CompressionLevel < 1)
		{
			mppc->CompressionLevel = 0;
			mppc->HistoryBufferSize = 8192;
		}
		else
		{
			mppc->CompressionLevel = 1;
			mppc->HistoryBufferSize = 65536;
		}

		mppc->bs = BitStream_New();

		ZeroMemory(&(mppc->HistoryBuffer), sizeof(mppc->HistoryBuffer));
		ZeroMemory(&(mppc->MatchBuffer), sizeof(mppc->MatchBuffer));

		mppc->HistoryOffset = 0;
		mppc->HistoryPtr = &(mppc->HistoryBuffer[mppc->HistoryOffset]);
	}

	return mppc;
}

void mppc_context_free(MPPC_CONTEXT* mppc)
{
	if (mppc)
	{
		BitStream_Free(mppc->bs);

		free(mppc);
	}
}
