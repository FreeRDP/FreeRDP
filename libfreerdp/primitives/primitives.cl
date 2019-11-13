/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Optimized operations using openCL
 * vi:ts=4 sw=4
 *
 * Copyright 2019 David Fort <contact@hardening-consulting.com>
 * Copyright 2019 Rangee Gmbh
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License. You may obtain
 * a copy of the License at http://www.apache.org/licenses/LICENSE-2.0.
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#define STRINGIFY(x) #x

STRINGIFY(
uchar clamp_uc(int v, short l, short h)
{
    if (v > h)
        v = h;
    if (v < l)
        v = l;
    return (uchar)v;
}

__kernel void yuv420_to_argb_1b(
	__global const uchar *bufY, int strideY,
	__global const uchar *bufU, int strideU,
	__global const uchar *bufV, int strideV,
	__global uchar *dest, int strideDest)
{
	unsigned int x = get_global_id(0);
	unsigned int y = get_global_id(1);

	short Y = bufY[y * strideY + x];
	short Udim = bufU[(y / 2) * strideU + (x / 2)] - 128;
	short Vdim = bufV[(y / 2) * strideV + (x / 2)] - 128;

	__global uchar *destPtr = dest + (strideDest * y) + (x * 4);

	/**
	 * | R |   ( | 256     0    403 | |    Y    | )
	 * | G | = ( | 256   -48   -120 | | U - 128 | ) >> 8
	 * | B |   ( | 256   475      0 | | V - 128 | )
	 */
	int y256 = 256 * Y;
	destPtr[0] = 0xff; /* A */
	destPtr[1] = clamp_uc((y256 + (403 * Vdim)) >> 8, 0, 255); /* R */
	destPtr[2] = clamp_uc((y256 - (48 * Udim) - (120 * Vdim)) >> 8 , 0, 255); /* G */
	destPtr[3] = clamp_uc((y256 + (475 * Udim)) >> 8, 0, 255); /* B */
}

__kernel void yuv420_to_bgra_1b(
	__global const uchar *bufY, int strideY,
	__global const uchar *bufU, int strideU,
	__global const uchar *bufV, int strideV,
	__global uchar *dest, int strideDest)
{
	unsigned int x = get_global_id(0);
	unsigned int y = get_global_id(1);

	short Y = bufY[y * strideY + x];
	short U = bufU[(y / 2) * strideU + (x / 2)] - 128;
	short V = bufV[(y / 2) * strideV + (x / 2)] - 128;

	__global uchar *destPtr = dest + (strideDest * y) + (x * 4);

	/**
	 * | R |   ( | 256     0    403 | |    Y    | )
	 * | G | = ( | 256   -48   -120 | | U - 128 | ) >> 8
	 * | B |   ( | 256   475      0 | | V - 128 | )
	 */
	int y256 = 256 * Y;
	destPtr[0] = clamp_uc((y256 + (475 * U)) >> 8, 0, 255); /* B */
	destPtr[1] = clamp_uc((y256 - ( 48 * U) - (120 * V)) >> 8 , 0, 255);	/* G */
	destPtr[2] = clamp_uc((y256 + (403 * V)) >> 8, 0, 255); 	/* R */
	destPtr[3] = 0xff; /* A */
}

__kernel void yuv444_to_bgra_1b(
	__global const uchar *bufY, int strideY,
	__global const uchar *bufU, int strideU,
	__global const uchar *bufV, int strideV,
	__global uchar *dest, int strideDest)
{
	unsigned int x = get_global_id(0);
	unsigned int y = get_global_id(1);

	short Y = bufY[y * strideY + x];
	short U = bufU[y * strideU + x] - 128;
	short V = bufV[y * strideV + x] - 128;

	__global uchar *destPtr = dest + (strideDest * y) + (x * 4);

	/**
	 * | R |   ( | 256     0    403 | |    Y    | )
	 * | G | = ( | 256   -48   -120 | | U - 128 | ) >> 8
	 * | B |   ( | 256   475      0 | | V - 128 | )
	 */
	int y256 = 256 * Y;
	destPtr[0] = clamp_uc((y256 + (475 * U)) >> 8, 0, 255); /* B */
	destPtr[1] = clamp_uc((y256 - ( 48 * U) - (120 * V)) >> 8 , 0, 255);	/* G */
	destPtr[2] = clamp_uc((y256 + (403 * V)) >> 8, 0, 255); 	/* R */
	destPtr[3] = 0xff; /* A */
}

__kernel void yuv444_to_argb_1b(
	__global const uchar *bufY, int strideY,
	__global const uchar *bufU, int strideU,
	__global const uchar *bufV, int strideV,
	__global uchar *dest, int strideDest)
{
	unsigned int x = get_global_id(0);
	unsigned int y = get_global_id(1);

	short Y = bufY[y * strideY + x];
	short U = bufU[y * strideU + x] - 128;
	short V = bufV[y * strideV + x] - 128;

	__global uchar *destPtr = dest + (strideDest * y) + (x * 4);

	/**
	 * | R |   ( | 256     0    403 | |    Y    | )
	 * | G | = ( | 256   -48   -120 | | U - 128 | ) >> 8
	 * | B |   ( | 256   475      0 | | V - 128 | )
	 */
	int y256 = 256 * Y;
	destPtr[3] = clamp_uc((y256 + (475 * U)) >> 8, 0, 255); /* B */
	destPtr[2] = clamp_uc((y256 - ( 48 * U) - (120 * V)) >> 8 , 0, 255);	/* G */
	destPtr[1] = clamp_uc((y256 + (403 * V)) >> 8, 0, 255); 	/* R */
	destPtr[0] = 0xff; /* A */
}
)
