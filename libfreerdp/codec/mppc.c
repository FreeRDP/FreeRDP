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
#include <winpr/collections.h>

#include <freerdp/codec/mppc_enc.h>
#include <freerdp/codec/mppc_dec.h>

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

int mppc_compress(MPPC_CONTEXT* mppc, BYTE* pSrcData, BYTE* pDstData, int size)
{
	DWORD Flags = 0;
	BYTE* pMatch;
	DWORD MatchIndex;
	UINT32 accumulator;
	BYTE* pEnd;

	BitStream_Attach(mppc->bs, pDstData, size);

	if (((mppc->HistoryOffset + size) < (mppc->HistoryBufferSize - 3)) /* && mppc->HistoryOffset */)
	{
		/* SrcData fits into HistoryBuffer? (YES) */
		CopyMemory(&(mppc->HistoryBuffer[mppc->HistoryOffset]), pSrcData, size);

		mppc->HistoryPtr = 0;
		mppc->pHistoryPtr = &(mppc->HistoryBuffer[mppc->HistoryPtr]);

		mppc->HistoryOffset += size;
		mppc->pHistoryOffset = &(mppc->HistoryBuffer[mppc->HistoryOffset]);
	}
	else
	{
		/* SrcData fits into HistoryBuffer? (NO) */

		mppc->HistoryOffset = 0;
		mppc->pHistoryOffset = &(mppc->HistoryBuffer[mppc->HistoryOffset]);

		Flags |= PACKET_AT_FRONT;
	}

	if (mppc->HistoryPtr < mppc->HistoryOffset)
	{
		/* HistoryPtr < HistoryOffset? (YES) */

		/*
		 * Search for a match of at least 3 bytes from the start of the history buffer
		 * to (HistoryPtr - 1) for the data that immediately follows HistoryPtr
		 */

		mppc->HistoryPtr = 0;
		mppc->pHistoryPtr = &(mppc->HistoryBuffer[mppc->HistoryPtr]);

		pEnd = &(mppc->HistoryBuffer[size - 1]);

		while (mppc->pHistoryPtr < (pEnd - 2))
		{
			MatchIndex = MPPC_MATCH_INDEX(mppc->pHistoryPtr[0], mppc->pHistoryPtr[1], mppc->pHistoryPtr[2]);

			pMatch = &(mppc->HistoryBuffer[mppc->MatchBuffer[MatchIndex]]);

			mppc->MatchBuffer[MatchIndex] = (UINT16) mppc->HistoryPtr;

			if ((mppc->pHistoryPtr[0] != pMatch[0]) || (mppc->pHistoryPtr[1] != pMatch[1]) ||
					(mppc->pHistoryPtr[2] != pMatch[2]) || (mppc->pHistoryPtr == pMatch))
			{
				accumulator = *(mppc->pHistoryPtr);

				printf("%c", accumulator);

				if (accumulator < 0x80)
				{
					/* 8 bits of literal are encoded as-is */
					BitStream_Write_Bits(mppc->bs, accumulator, 8);
				}
				else
				{
					/* bits 10 followed by lower 7 bits of literal */
					accumulator = 0x100 | (accumulator & 0x7F);
					BitStream_Write_Bits(mppc->bs, accumulator, 9);
				}

				mppc->HistoryPtr++;
				mppc->pHistoryPtr = &(mppc->HistoryBuffer[mppc->HistoryPtr]);
			}
			else
			{
				DWORD CopyOffset;
				DWORD LengthOfMatch = 1;

				CopyOffset = (DWORD) (mppc->pHistoryPtr - pMatch);

				while ((mppc->pHistoryPtr[LengthOfMatch] == pMatch[LengthOfMatch]) &&
						((mppc->HistoryPtr + LengthOfMatch) < (size - 1)))
				{
					MatchIndex = MPPC_MATCH_INDEX(mppc->pHistoryPtr[LengthOfMatch],
						mppc->pHistoryPtr[LengthOfMatch + 1], mppc->pHistoryPtr[LengthOfMatch + 2]);

					mppc->MatchBuffer[MatchIndex] = (UINT16) (mppc->HistoryPtr + LengthOfMatch);

					LengthOfMatch++;
				}

				printf("<%d,%d>", (int) CopyOffset, (int) LengthOfMatch);

				mppc->HistoryPtr += LengthOfMatch;
				mppc->pHistoryPtr = &(mppc->HistoryBuffer[mppc->HistoryPtr]);
			}
		}

		/* Encode trailing symbols as literals */

		while (mppc->pHistoryPtr <= pEnd)
		{
			MatchIndex = MPPC_MATCH_INDEX(mppc->pHistoryPtr[0], mppc->pHistoryPtr[1], mppc->pHistoryPtr[2]);

			pMatch = &(mppc->HistoryBuffer[mppc->MatchBuffer[MatchIndex]]);

			mppc->MatchBuffer[MatchIndex] = (UINT16) mppc->HistoryPtr;

			accumulator = *(mppc->pHistoryPtr);

			printf("%c", accumulator);

			if (accumulator < 0x80)
			{
				/* 8 bits of literal are encoded as-is */
				BitStream_Write_Bits(mppc->bs, accumulator, 8);
			}
			else
			{
				/* bits 10 followed by lower 7 bits of literal */
				accumulator = 0x100 | (accumulator & 0x7F);
				BitStream_Write_Bits(mppc->bs, accumulator, 9);
			}

			mppc->HistoryPtr++;
			mppc->pHistoryPtr = &(mppc->HistoryBuffer[mppc->HistoryPtr]);
		}
	}
	else
	{
		/* HistoryPtr < HistoryOffset? (NO) */

		Flags |= PACKET_COMPRESSED;
		/* Send OutputBuffer */

		return 0; /* Finish */
	}

	return 0;
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

		ZeroMemory(&(mppc->HistoryBuffer), sizeof(mppc->HistoryBuffer));
		ZeroMemory(&(mppc->MatchBuffer), sizeof(mppc->MatchBuffer));

		mppc->bs = BitStream_New();
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
