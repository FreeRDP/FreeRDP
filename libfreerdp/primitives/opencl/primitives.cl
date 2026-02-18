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

typedef struct
{
	unsigned x;
	unsigned y;
	uchar r;
	uchar g;
	uchar b;
} cl_color_t;

uchar clamp_uc(int v, short l, short h)
{
	if (v > h)
		v = h;
	if (v < l)
		v = l;
	return (uchar)v;
}

short avgUV(__global const uchar* buf, unsigned stride, unsigned x, unsigned y)
{
	const short U00 = buf[y * stride + x];
	if ((x != 0) || (y != 0))
		return U00;
	const unsigned width = get_global_size(0);
	if (x + 1 >= width)
		return U00;
	const unsigned height = get_global_size(1);
	if (y + 1 >= height)
		return U00;

	const short U10 = buf[(y + 1) * stride + x];
	const short U01 = buf[y * stride + x + 1];
	const short U11 = buf[(y + 1) * stride + x + 1];

	const short avg = U00 * 4 - U01 - U10 - U11;
	const short avgU = clamp_uc(avg, 0, 255);
	const short diff = abs(U00 - avgU);
	if (diff < 30)
		return U00;
	return avgU;
}

cl_color_t yuv420_to_rgb(__global const uchar* bufY, unsigned strideY, __global const uchar* bufU,
                         unsigned strideU, __global const uchar* bufV, unsigned strideV)
{
	const unsigned int x = get_global_id(0);
	const unsigned int y = get_global_id(1);

	const short Y = bufY[y * strideY + x];
	const short Udim = bufU[(y / 2) * strideU + (x / 2)] - 128;
	const short Vdim = bufV[(y / 2) * strideV + (x / 2)] - 128;

	/**
	 * | R |   ( | 256     0    403 | |    Y    | )
	 * | G | = ( | 256   -48   -120 | | U - 128 | ) >> 8
	 * | B |   ( | 256   475      0 | | V - 128 | )
	 */
	const int y256 = 256 * Y;
	const int r256 = (y256 + (403 * Vdim));
	const int g256 = (y256 - (48 * Udim) - (120 * Vdim));
	const int b256 = (y256 + (475 * Udim));
	const short r = r256 >> 8;
	const short g = g256 >> 8;
	const short b = b256 >> 8;
	const cl_color_t color = {
		.x = x,
		.y = y,
		.r = clamp_uc(r, 0, 255), /* R */
		.g = clamp_uc(g, 0, 255), /* G */
		.b = clamp_uc(b, 0, 255)  /* B */
	};
	return color;
}

__kernel void yuv420_to_rgba_1b(__global const uchar* bufY, unsigned strideY,
                                __global const uchar* bufU, unsigned strideU,
                                __global const uchar* bufV, unsigned strideV, __global uchar* dest,
                                unsigned strideDest)
{
	cl_color_t color = yuv420_to_rgb(bufY, strideY, bufU, strideU, bufV, strideV);
	__global uchar* destPtr = dest + (strideDest * color.y) + (color.x * 4);
	destPtr[0] = color.r; /* R */
	destPtr[1] = color.g; /* G */
	destPtr[2] = color.b; /* B */
}

__kernel void yuv420_to_argb_1b(__global const uchar* bufY, unsigned strideY,
                                __global const uchar* bufU, unsigned strideU,
                                __global const uchar* bufV, unsigned strideV, __global uchar* dest,
                                unsigned strideDest)
{
	cl_color_t color = yuv420_to_rgb(bufY, strideY, bufU, strideU, bufV, strideV);
	__global uchar* destPtr = dest + (strideDest * color.y) + (color.x * 4);
	destPtr[1] = color.r; /* R */
	destPtr[2] = color.g; /* G */
	destPtr[3] = color.b; /* B */
}

__kernel void yuv420_to_bgra_1b(__global const uchar* bufY, unsigned strideY,
                                __global const uchar* bufU, unsigned strideU,
                                __global const uchar* bufV, unsigned strideV, __global uchar* dest,
                                unsigned strideDest)
{
	cl_color_t color = yuv420_to_rgb(bufY, strideY, bufU, strideU, bufV, strideV);
	__global uchar* destPtr = dest + (strideDest * color.y) + (color.x * 4);
	destPtr[0] = color.b; /* B */
	destPtr[1] = color.g; /* G */
	destPtr[2] = color.r; /* R */
}

__kernel void yuv420_to_abgr_1b(__global const uchar* bufY, unsigned strideY,
                                __global const uchar* bufU, unsigned strideU,
                                __global const uchar* bufV, unsigned strideV, __global uchar* dest,
                                unsigned strideDest)
{
	cl_color_t color = yuv420_to_rgb(bufY, strideY, bufU, strideU, bufV, strideV);
	__global uchar* destPtr = dest + (strideDest * color.y) + (color.x * 4);
	destPtr[1] = color.b; /* B */
	destPtr[2] = color.g; /* G */
	destPtr[3] = color.r; /* R */
}

cl_color_t yuv444_to_rgb(__global const uchar* bufY, unsigned strideY, __global const uchar* bufU,
                         unsigned strideU, __global const uchar* bufV, unsigned strideV)
{
	const unsigned int x = get_global_id(0);
	const unsigned int y = get_global_id(1);

	const short Y = bufY[y * strideY + x];
	const short U = avgUV(bufU, strideU, x, y);
	const short V = avgUV(bufV, strideV, x, y);
	const short D = U - 128;
	const short E = V - 128;

	/**
	 * | R |   ( | 256     0    403 | |    Y    | )
	 * | G | = ( | 256   -48   -120 | | U - 128 | ) >> 8
	 * | B |   ( | 256   475      0 | | V - 128 | )
	 */
	const int y256 = 256 * Y;
	const int r256 = (y256 + (403 * E));
	const int g256 = (y256 - (48 * D) - (120 * E));
	const int b256 = (y256 + (475 * D));
	const short r = r256 >> 8;
	const short g = g256 >> 8;
	const short b = b256 >> 8;
	const cl_color_t color = {
		.x = x,
		.y = y,
		.r = clamp_uc(r, 0, 255), /* R */
		.g = clamp_uc(g, 0, 255), /* G */
		.b = clamp_uc(b, 0, 255)  /* B */
	};
	return color;
}

__kernel void yuv444_to_abgr_1b(__global const uchar* bufY, unsigned strideY,
                                __global const uchar* bufU, unsigned strideU,
                                __global const uchar* bufV, unsigned strideV, __global uchar* dest,
                                unsigned strideDest)
{
	cl_color_t color = yuv444_to_rgb(bufY, strideY, bufU, strideU, bufV, strideV);

	__global uchar* destPtr = dest + (strideDest * color.y) + (color.x * 4);
	destPtr[1] = color.b; /* B */
	destPtr[2] = color.g; /* G */
	destPtr[3] = color.r; /* R */
}

__kernel void yuv444_to_rgba_1b(__global const uchar* bufY, unsigned strideY,
                                __global const uchar* bufU, unsigned strideU,
                                __global const uchar* bufV, unsigned strideV, __global uchar* dest,
                                unsigned strideDest)
{
	cl_color_t color = yuv444_to_rgb(bufY, strideY, bufU, strideU, bufV, strideV);

	__global uchar* destPtr = dest + (strideDest * color.y) + (color.x * 4);
	destPtr[0] = color.r; /* R */
	destPtr[1] = color.g; /* G */
	destPtr[2] = color.b; /* B */
}

__kernel void yuv444_to_bgra_1b(__global const uchar* bufY, unsigned strideY,
                                __global const uchar* bufU, unsigned strideU,
                                __global const uchar* bufV, unsigned strideV, __global uchar* dest,
                                unsigned strideDest)
{
	cl_color_t color = yuv444_to_rgb(bufY, strideY, bufU, strideU, bufV, strideV);

	__global uchar* destPtr = dest + (strideDest * color.y) + (color.x * 4);
	destPtr[0] = color.b; /* B */
	destPtr[1] = color.g; /* G */
	destPtr[2] = color.r; /* R */
}

__kernel void yuv444_to_argb_1b(__global const uchar* bufY, unsigned strideY,
                                __global const uchar* bufU, unsigned strideU,
                                __global const uchar* bufV, unsigned strideV, __global uchar* dest,
                                unsigned strideDest)
{
	cl_color_t color = yuv444_to_rgb(bufY, strideY, bufU, strideU, bufV, strideV);

	__global uchar* destPtr = dest + (strideDest * color.y) + (color.x * 4);
	destPtr[1] = color.r; /* R */
	destPtr[2] = color.g; /* G */
	destPtr[3] = color.b; /* B */
}
