/**
 * FreeRDP: A Remote Desktop Protocol client.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freerdp/utils/memory.h>
#include "rfx_bitstream.h"

#include "rfx_rlgr.h"

/* Constants used within the RLGR1/RLGR3 algorithm */
#define KPMAX	(80)  /* max value for kp or krp */
#define LSGR	(3)   /* shift count to convert kp to k */
#define UP_GR	(4)   /* increase in kp after a zero run in RL mode */
#define DN_GR	(6)   /* decrease in kp after a nonzero symbol in RL mode */
#define UQ_GR	(3)   /* increase in kp after nonzero symbol in GR mode */
#define DQ_GR	(3)   /* decrease in kp after zero symbol in GR mode */

/* Gets (returns) the next nBits from the bitstream */
#define GetBits(nBits, r) rfx_bitstream_get_bits(bs, nBits, r)

/* From current output pointer, write "value", check and update buffer_size */
#define WriteValue(value) \
{ \
	if (buffer_size > 0) \
		*dst++ = (value); \
	buffer_size--; \
}

/* From current output pointer, write next nZeroes terms with value 0, check and update buffer_size */
#define WriteZeroes(nZeroes) \
{ \
	int nZeroesWritten = (nZeroes); \
	if (nZeroesWritten > buffer_size) \
		nZeroesWritten = buffer_size; \
	if (nZeroesWritten > 0) \
	{ \
		memset(dst, 0, nZeroesWritten * sizeof(sint16)); \
		dst += nZeroesWritten; \
	} \
	buffer_size -= (nZeroes); \
}

/* Returns the least number of bits required to represent a given value */
#define GetMinBits(_val, _nbits) \
{ \
	uint32 _v = _val; \
	_nbits = 0; \
	while (_v) \
	{ \
		_v >>= 1; \
		_nbits++; \
	} \
}

/* Converts from (2 * magnitude - sign) to integer */
#define GetIntFrom2MagSign(twoMs) (((twoMs) & 1) ? -1 * (sint16)(((twoMs) + 1) >> 1) : (sint16)((twoMs) >> 1))

/*
 * Update the passed parameter and clamp it to the range [0, KPMAX]
 * Return the value of parameter right-shifted by LSGR
 */
#define UpdateParam(_param, _deltaP, _k) \
{ \
	_param += _deltaP; \
	if (_param > KPMAX) \
		_param = KPMAX; \
	if (_param < 0) \
		_param = 0; \
	_k = (_param >> LSGR); \
}

/* Outputs the Golomb/Rice encoding of a non-negative integer */
#define GetGRCode(krp, kr, vk, _mag) \
	vk = 0; \
	_mag = 0; \
	/* chew up/count leading 1s and escape 0 */ \
	do { \
		GetBits(1, r); \
		if (r == 1) \
			vk++; \
		else \
			break; \
	} while (1); \
	/* get next *kr bits, and combine with leading 1s */ \
	GetBits(*kr, _mag); \
	_mag |= (vk << *kr); \
	/* adjust krp and kr based on vk */ \
	if (!vk) { \
		UpdateParam(*krp, -2, *kr); \
	} \
	else if (vk != 1) { \
		UpdateParam(*krp, vk, *kr); /* at 1, no change! */ \
	}

int rfx_rlgr_decode(RLGR_MODE mode, const uint8* data, int data_size, sint16* buffer, int buffer_size)
{
	int k;
	int kp;
	int kr;
	int krp;
	uint16 r;
	sint16* dst;
	RFX_BITSTREAM* bs;

	int vk;
	uint16 mag16;

	bs = xnew(RFX_BITSTREAM);
	rfx_bitstream_attach(bs, data, data_size);
	dst = buffer;

	/* initialize the parameters */
	k = 1;
	kp = k << LSGR;
	kr = 1;
	krp = kr << LSGR;

	while (!rfx_bitstream_eos(bs) && buffer_size > 0)
	{
		int run;
		if (k)
		{
			int mag;
			uint32 sign;

			/* RL MODE */
			while (!rfx_bitstream_eos(bs))
			{
				GetBits(1, r);
				if (r)
					break;
				/* we have an RL escape "0", which translates to a run (1<<k) of zeros */
				WriteZeroes(1 << k);
				UpdateParam(kp, UP_GR, k); /* raise k and kp up because of zero run */
			}

			/* next k bits will contain remaining run or zeros */
			GetBits(k, run);
			WriteZeroes(run);

			/* get nonzero value, starting with sign bit and then GRCode for magnitude -1 */
			GetBits(1, sign);

			/* magnitude - 1 was coded (because it was nonzero) */
			GetGRCode(&krp, &kr, vk, mag16)
			mag = (int) (mag16 + 1);

			WriteValue(sign ? -mag : mag);
			UpdateParam(kp, -DN_GR, k); /* lower k and kp because of nonzero term */
		}
		else
		{
			uint32 mag;
			uint32 nIdx;
			uint32 val1;
			uint32 val2;

			/* GR (GOLOMB-RICE) MODE */
			GetGRCode(&krp, &kr, vk, mag16) /* values coded are 2 * magnitude - sign */
			mag = (uint32) mag16;

			if (mode == RLGR1)
			{
				if (!mag)
				{
					WriteValue(0);
					UpdateParam(kp, UQ_GR, k); /* raise k and kp due to zero */
				}
				else
				{
					WriteValue(GetIntFrom2MagSign(mag));
					UpdateParam(kp, -DQ_GR, k); /* lower k and kp due to nonzero */
				}
			}
			else /* mode == RLGR3 */
			{
				/*
				 * In GR mode FOR RLGR3, we have encoded the
				 * sum of two (2 * mag - sign) values
				 */

				/* maximum possible bits for first term */
				GetMinBits(mag, nIdx);

				/* decode val1 is first term's (2 * mag - sign) value */
				GetBits(nIdx, val1);

				/* val2 is second term's (2 * mag - sign) value */
				val2 = mag - val1;

				if (val1 && val2)
				{
					/* raise k and kp if both terms nonzero */
					UpdateParam(kp, -2 * DQ_GR, k);
				}
				else if (!val1 && !val2)
				{
					/* lower k and kp if both terms zero */
					UpdateParam(kp, 2 * UQ_GR, k);
				}

				WriteValue(GetIntFrom2MagSign(val1));
				WriteValue(GetIntFrom2MagSign(val2));
			}
		}
	}

	xfree(bs);

	return (dst - buffer);
}

/* Returns the next coefficient (a signed int) to encode, from the input stream */
#define GetNextInput(_n) \
{ \
	if (data_size > 0) \
	{ \
		_n = *data++; \
		data_size--; \
	} \
	else \
	{ \
		_n = 0; \
	} \
}

/* Emit bitPattern to the output bitstream */
#define OutputBits(numBits, bitPattern) rfx_bitstream_put_bits(bs, bitPattern, numBits)

/* Emit a bit (0 or 1), count number of times, to the output bitstream */
#define OutputBit(count, bit) \
{	\
	uint16 _b = (bit ? 0xFFFF : 0); \
	int _c = (count); \
	for (; _c > 0; _c -= 16) \
		rfx_bitstream_put_bits(bs, _b, (_c > 16 ? 16 : _c)); \
}

/* Converts the input value to (2 * abs(input) - sign(input)), where sign(input) = (input < 0 ? 1 : 0) and returns it */
#define Get2MagSign(input) ((input) >= 0 ? 2 * (input) : -2 * (input) - 1)

/* Outputs the Golomb/Rice encoding of a non-negative integer */
#define CodeGR(krp, val) rfx_rlgr_code_gr(bs, krp, val)

static void rfx_rlgr_code_gr(RFX_BITSTREAM* bs, int* krp, uint32 val)
{
	int kr = *krp >> LSGR;

	/* unary part of GR code */

	uint32 vk = (val) >> kr;
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

int rfx_rlgr_encode(RLGR_MODE mode, const sint16* data, int data_size, uint8* buffer, int buffer_size)
{
	int k;
	int kp;
	int krp;
	RFX_BITSTREAM* bs;
	int processed_size;

	bs = xnew(RFX_BITSTREAM);
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
			sign = (input < 0 ? 1 : 0);  /* sign of input coefficient */

			OutputBit(1, sign); /* output the sign bit */
			CodeGR(&krp, mag ? mag - 1 : 0); /* output GR code for (mag - 1) */

			UpdateParam(kp, -DN_GR, k);
		}
		else
		{
			/* GOLOMB-RICE MODE */

			if (mode == RLGR1)
			{
				uint32 twoMs;

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
				uint32 twoMs1;
				uint32 twoMs2;
				uint32 sum2Ms;
				uint32 nIdx;

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

	processed_size = rfx_bitstream_get_processed_bytes(bs);
	xfree(bs);

	return processed_size;
}
