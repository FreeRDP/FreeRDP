/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RemoteFX Codec Library - RLGR
 *
 * Copyright 2011 Vic Lee
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

/**
 * This implementation of RLGR refers to
 * [MS-RDPRFX] 3.1.8.1.7.3 RLGR1/RLGR3 Pseudocode
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/sysinfo.h>
#include <winpr/bitstream.h>
#include <winpr/intrin.h>

#include "rfx_bitstream.h"

#include "rfx_rlgr.h"

/* Constants used in RLGR1/RLGR3 algorithm */
#define KPMAX (80) /* max value for kp or krp */
#define LSGR (3)   /* shift count to convert kp to k */
#define UP_GR (4)  /* increase in kp after a zero run in RL mode */
#define DN_GR (6)  /* decrease in kp after a nonzero symbol in RL mode */
#define UQ_GR (3)  /* increase in kp after nonzero symbol in GR mode */
#define DQ_GR (3)  /* decrease in kp after zero symbol in GR mode */

/* Returns the least number of bits required to represent a given value */
#define GetMinBits(_val, _nbits) \
	{                            \
		UINT32 _v = _val;        \
		_nbits = 0;              \
		while (_v)               \
		{                        \
			_v >>= 1;            \
			_nbits++;            \
		}                        \
	}

/*
 * Update the passed parameter and clamp it to the range [0, KPMAX]
 * Return the value of parameter right-shifted by LSGR
 */
#define UpdateParam(_param, _deltaP, _k) \
	{                                    \
		_param += _deltaP;               \
		if (_param > KPMAX)              \
			_param = KPMAX;              \
		if (_param < 0)                  \
			_param = 0;                  \
		_k = (_param >> LSGR);           \
	}

static BOOL g_LZCNT = FALSE;

static INIT_ONCE rfx_rlgr_init_once = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK rfx_rlgr_init(PINIT_ONCE once, PVOID param, PVOID* context)
{
	g_LZCNT = IsProcessorFeaturePresentEx(PF_EX_LZCNT);
	return TRUE;
}

static INLINE UINT32 lzcnt_s(UINT32 x)
{
	if (!x)
		return 32;

	if (!g_LZCNT)
	{
		UINT32 y;
		int n = 32;
		y = x >> 16;
		if (y != 0)
		{
			n = n - 16;
			x = y;
		}
		y = x >> 8;
		if (y != 0)
		{
			n = n - 8;
			x = y;
		}
		y = x >> 4;
		if (y != 0)
		{
			n = n - 4;
			x = y;
		}
		y = x >> 2;
		if (y != 0)
		{
			n = n - 2;
			x = y;
		}
		y = x >> 1;
		if (y != 0)
			return n - 2;
		return n - x;
	}

	return __lzcnt(x);
}

int rfx_rlgr_decode(RLGR_MODE mode, const BYTE* pSrcData, UINT32 SrcSize, INT16* pDstData,
                    UINT32 DstSize)
{
	int vk;
	int run;
	int cnt;
	int size;
	int nbits;
	size_t offset;
	INT16 mag;
	UINT32 k;
	INT32 kp;
	UINT32 kr;
	INT32 krp;
	UINT16 code;
	UINT32 sign;
	UINT32 nIdx;
	UINT32 val1;
	UINT32 val2;
	INT16* pOutput;
	wBitStream* bs;
	wBitStream s_bs;

	InitOnceExecuteOnce(&rfx_rlgr_init_once, rfx_rlgr_init, NULL, NULL);

	k = 1;
	kp = k << LSGR;

	kr = 1;
	krp = kr << LSGR;

	if ((mode != RLGR1) && (mode != RLGR3))
		mode = RLGR1;

	if (!pSrcData || !SrcSize)
		return -1;

	if (!pDstData || !DstSize)
		return -1;

	pOutput = pDstData;

	bs = &s_bs;

	BitStream_Attach(bs, pSrcData, SrcSize);
	BitStream_Fetch(bs);

	while ((BitStream_GetRemainingLength(bs) > 0) && ((pOutput - pDstData) < DstSize))
	{
		if (k)
		{
			/* Run-Length (RL) Mode */

			run = 0;

			/* count number of leading 0s */

			cnt = lzcnt_s(bs->accumulator);

			nbits = BitStream_GetRemainingLength(bs);

			if (cnt > nbits)
				cnt = nbits;

			vk = cnt;

			while ((cnt == 32) && (BitStream_GetRemainingLength(bs) > 0))
			{
				BitStream_Shift32(bs);

				cnt = lzcnt_s(bs->accumulator);

				nbits = BitStream_GetRemainingLength(bs);

				if (cnt > nbits)
					cnt = nbits;

				vk += cnt;
			}

			BitStream_Shift(bs, (vk % 32));

			if (BitStream_GetRemainingLength(bs) < 1)
				break;

			BitStream_Shift(bs, 1);

			while (vk--)
			{
				run += (1 << k); /* add (1 << k) to run length */

				/* update k, kp params */

				kp += UP_GR;

				if (kp > KPMAX)
					kp = KPMAX;

				k = kp >> LSGR;
			}

			/* next k bits contain run length remainder */

			if (BitStream_GetRemainingLength(bs) < k)
				break;

			bs->mask = ((1 << k) - 1);
			run += ((bs->accumulator >> (32 - k)) & bs->mask);
			BitStream_Shift(bs, k);

			/* read sign bit */

			if (BitStream_GetRemainingLength(bs) < 1)
				break;

			sign = (bs->accumulator & 0x80000000) ? 1 : 0;
			BitStream_Shift(bs, 1);

			/* count number of leading 1s */

			cnt = lzcnt_s(~(bs->accumulator));

			nbits = BitStream_GetRemainingLength(bs);

			if (cnt > nbits)
				cnt = nbits;

			vk = cnt;

			while ((cnt == 32) && (BitStream_GetRemainingLength(bs) > 0))
			{
				BitStream_Shift32(bs);

				cnt = lzcnt_s(~(bs->accumulator));

				nbits = BitStream_GetRemainingLength(bs);

				if (cnt > nbits)
					cnt = nbits;

				vk += cnt;
			}

			BitStream_Shift(bs, (vk % 32));

			if (BitStream_GetRemainingLength(bs) < 1)
				break;

			BitStream_Shift(bs, 1);

			/* next kr bits contain code remainder */

			if (BitStream_GetRemainingLength(bs) < kr)
				break;

			bs->mask = ((1 << kr) - 1);
			code = (UINT16)((bs->accumulator >> (32 - kr)) & bs->mask);
			BitStream_Shift(bs, kr);

			/* add (vk << kr) to code */

			code |= (vk << kr);

			if (!vk)
			{
				/* update kr, krp params */

				krp -= 2;

				if (krp < 0)
					krp = 0;

				kr = krp >> LSGR;
			}
			else if (vk != 1)
			{
				/* update kr, krp params */

				krp += vk;

				if (krp > KPMAX)
					krp = KPMAX;

				kr = krp >> LSGR;
			}

			/* update k, kp params */

			kp -= DN_GR;

			if (kp < 0)
				kp = 0;

			k = kp >> LSGR;

			/* compute magnitude from code */

			if (sign)
				mag = ((INT16)(code + 1)) * -1;
			else
				mag = (INT16)(code + 1);

			/* write to output stream */

			offset = (pOutput - pDstData);
			size = run;

			if ((offset + size) > DstSize)
				size = DstSize - offset;

			if (size)
			{
				ZeroMemory(pOutput, size * sizeof(INT16));
				pOutput += size;
			}

			if ((pOutput - pDstData) < DstSize)
			{
				*pOutput = mag;
				pOutput++;
			}
		}
		else
		{
			/* Golomb-Rice (GR) Mode */

			/* count number of leading 1s */

			cnt = lzcnt_s(~(bs->accumulator));

			nbits = BitStream_GetRemainingLength(bs);

			if (cnt > nbits)
				cnt = nbits;

			vk = cnt;

			while ((cnt == 32) && (BitStream_GetRemainingLength(bs) > 0))
			{
				BitStream_Shift32(bs);

				cnt = lzcnt_s(~(bs->accumulator));

				nbits = BitStream_GetRemainingLength(bs);

				if (cnt > nbits)
					cnt = nbits;

				vk += cnt;
			}

			BitStream_Shift(bs, (vk % 32));

			if (BitStream_GetRemainingLength(bs) < 1)
				break;

			BitStream_Shift(bs, 1);

			/* next kr bits contain code remainder */

			if (BitStream_GetRemainingLength(bs) < kr)
				break;

			bs->mask = ((1 << kr) - 1);
			code = (UINT16)((bs->accumulator >> (32 - kr)) & bs->mask);
			BitStream_Shift(bs, kr);

			/* add (vk << kr) to code */

			code |= (vk << kr);

			if (!vk)
			{
				/* update kr, krp params */

				krp -= 2;

				if (krp < 0)
					krp = 0;

				kr = krp >> LSGR;
			}
			else if (vk != 1)
			{
				/* update kr, krp params */

				krp += vk;

				if (krp > KPMAX)
					krp = KPMAX;

				kr = krp >> LSGR;
			}

			if (mode == RLGR1) /* RLGR1 */
			{
				if (!code)
				{
					/* update k, kp params */

					kp += UQ_GR;

					if (kp > KPMAX)
						kp = KPMAX;

					k = kp >> LSGR;

					mag = 0;
				}
				else
				{
					/* update k, kp params */

					kp -= DQ_GR;

					if (kp < 0)
						kp = 0;

					k = kp >> LSGR;

					/*
					 * code = 2 * mag - sign
					 * sign + code = 2 * mag
					 */

					if (code & 1)
						mag = ((INT16)((code + 1) >> 1)) * -1;
					else
						mag = (INT16)(code >> 1);
				}

				if ((pOutput - pDstData) < DstSize)
				{
					*pOutput = mag;
					pOutput++;
				}
			}
			else if (mode == RLGR3) /* RLGR3 */
			{
				nIdx = 0;

				if (code)
				{
					mag = (UINT32)code;
					nIdx = 32 - lzcnt_s(mag);
				}

				if (BitStream_GetRemainingLength(bs) < nIdx)
					break;

				bs->mask = ((1 << nIdx) - 1);
				val1 = ((bs->accumulator >> (32 - nIdx)) & bs->mask);
				BitStream_Shift(bs, nIdx);

				val2 = code - val1;

				if (val1 && val2)
				{
					/* update k, kp params */

					kp -= (2 * DQ_GR);

					if (kp < 0)
						kp = 0;

					k = kp >> LSGR;
				}
				else if (!val1 && !val2)
				{
					/* update k, kp params */

					kp += (2 * UQ_GR);

					if (kp > KPMAX)
						kp = KPMAX;

					k = kp >> LSGR;
				}

				if (val1 & 1)
					mag = ((INT16)((val1 + 1) >> 1)) * -1;
				else
					mag = (INT16)(val1 >> 1);

				if ((pOutput - pDstData) < DstSize)
				{
					*pOutput = mag;
					pOutput++;
				}

				if (val2 & 1)
					mag = ((INT16)((val2 + 1) >> 1)) * -1;
				else
					mag = (INT16)(val2 >> 1);

				if ((pOutput - pDstData) < DstSize)
				{
					*pOutput = mag;
					pOutput++;
				}
			}
		}
	}

	offset = (pOutput - pDstData);

	if (offset < DstSize)
	{
		size = DstSize - offset;
		ZeroMemory(pOutput, size * 2);
		pOutput += size;
	}

	offset = (pOutput - pDstData);

	if (offset != DstSize)
		return -1;

	return 1;
}

/* Returns the next coefficient (a signed int) to encode, from the input stream */
#define GetNextInput(_n)   \
	{                      \
		if (data_size > 0) \
		{                  \
			_n = *data++;  \
			data_size--;   \
		}                  \
		else               \
		{                  \
			_n = 0;        \
		}                  \
	}

/* Emit bitPattern to the output bitstream */
#define OutputBits(numBits, bitPattern) rfx_bitstream_put_bits(bs, bitPattern, numBits)

/* Emit a bit (0 or 1), count number of times, to the output bitstream */
#define OutputBit(count, bit)                                    \
	{                                                            \
		UINT16 _b = (bit ? 0xFFFF : 0);                          \
		int _c = (count);                                        \
		for (; _c > 0; _c -= 16)                                 \
			rfx_bitstream_put_bits(bs, _b, (_c > 16 ? 16 : _c)); \
	}

/* Converts the input value to (2 * abs(input) - sign(input)), where sign(input) = (input < 0 ? 1 :
 * 0) and returns it */
#define Get2MagSign(input) ((input) >= 0 ? 2 * (input) : -2 * (input)-1)

/* Outputs the Golomb/Rice encoding of a non-negative integer */
#define CodeGR(krp, val) rfx_rlgr_code_gr(bs, krp, val)

static void rfx_rlgr_code_gr(RFX_BITSTREAM* bs, int* krp, UINT32 val)
{
	int kr = *krp >> LSGR;

	/* unary part of GR code */

	UINT32 vk = (val) >> kr;
	OutputBit(vk, 1);
	OutputBit(1, 0);

	/* remainder part of GR code, if needed */
	if (kr)
	{
		OutputBits(kr, val & ((1 << kr) - 1));
	}

	/* update krp, only if it is not equal to 1 */
	if (vk == 0)
	{
		UpdateParam(*krp, -2, kr);
	}
	else if (vk > 1)
	{
		UpdateParam(*krp, vk, kr);
	}
}

int rfx_rlgr_encode(RLGR_MODE mode, const INT16* data, UINT32 data_size, BYTE* buffer,
                    UINT32 buffer_size)
{
	int k;
	int kp;
	int krp;
	RFX_BITSTREAM* bs;
	int processed_size;

	if (!(bs = (RFX_BITSTREAM*)calloc(1, sizeof(RFX_BITSTREAM))))
		return 0;

	rfx_bitstream_attach(bs, buffer, buffer_size);

	/* initialize the parameters */
	k = 1;
	kp = 1 << LSGR;
	krp = 1 << LSGR;

	/* process all the input coefficients */
	while (data_size > 0)
	{
		int input;

		if (k)
		{
			int numZeros;
			int runmax;
			int mag;
			int sign;

			/* RUN-LENGTH MODE */

			/* collect the run of zeros in the input stream */
			numZeros = 0;
			GetNextInput(input);
			while (input == 0 && data_size > 0)
			{
				numZeros++;
				GetNextInput(input);
			}

			// emit output zeros
			runmax = 1 << k;
			while (numZeros >= runmax)
			{
				OutputBit(1, 0); /* output a zero bit */
				numZeros -= runmax;
				UpdateParam(kp, UP_GR, k); /* update kp, k */
				runmax = 1 << k;
			}

			/* output a 1 to terminate runs */
			OutputBit(1, 1);

			/* output the remaining run length using k bits */
			OutputBits(k, numZeros);

			/* note: when we reach here and the last byte being encoded is 0, we still
			   need to output the last two bits, otherwise mstsc will crash */

			/* encode the nonzero value using GR coding */
			mag = (input < 0 ? -input : input); /* absolute value of input coefficient */
			sign = (input < 0 ? 1 : 0);         /* sign of input coefficient */

			OutputBit(1, sign);              /* output the sign bit */
			CodeGR(&krp, mag ? mag - 1 : 0); /* output GR code for (mag - 1) */

			UpdateParam(kp, -DN_GR, k);
		}
		else
		{
			/* GOLOMB-RICE MODE */

			if (mode == RLGR1)
			{
				UINT32 twoMs;

				/* RLGR1 variant */

				/* convert input to (2*magnitude - sign), encode using GR code */
				GetNextInput(input);
				twoMs = Get2MagSign(input);
				CodeGR(&krp, twoMs);

				/* update k, kp */
				/* NOTE: as of Aug 2011, the algorithm is still wrongly documented
				   and the update direction is reversed */
				if (twoMs)
				{
					UpdateParam(kp, -DQ_GR, k);
				}
				else
				{
					UpdateParam(kp, UQ_GR, k);
				}
			}
			else /* mode == RLGR3 */
			{
				UINT32 twoMs1;
				UINT32 twoMs2;
				UINT32 sum2Ms;
				UINT32 nIdx;

				/* RLGR3 variant */

				/* convert the next two input values to (2*magnitude - sign) and */
				/* encode their sum using GR code */

				GetNextInput(input);
				twoMs1 = Get2MagSign(input);
				GetNextInput(input);
				twoMs2 = Get2MagSign(input);
				sum2Ms = twoMs1 + twoMs2;

				CodeGR(&krp, sum2Ms);

				/* encode binary representation of the first input (twoMs1). */
				GetMinBits(sum2Ms, nIdx);
				OutputBits(nIdx, twoMs1);

				/* update k,kp for the two input values */

				if (twoMs1 && twoMs2)
				{
					UpdateParam(kp, -2 * DQ_GR, k);
				}
				else if (!twoMs1 && !twoMs2)
				{
					UpdateParam(kp, 2 * UQ_GR, k);
				}
			}
		}
	}

	rfx_bitstream_flush(bs);
	processed_size = rfx_bitstream_get_processed_bytes(bs);
	free(bs);

	return processed_size;
}
