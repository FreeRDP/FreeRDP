/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Color Conversion Unit Tests
 *
 * Copyright 2010 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <freerdp/freerdp.h>
#include <freerdp/gdi/gdi.h>
#include <freerdp/codec/color.h>
#include "test_color.h"

int init_color_suite(void)
{
	return 0;
}

int clean_color_suite(void)
{
	return 0;
}

int add_color_suite(void)
{
	add_test_suite(color);

	add_test_function(color_GetRGB32);
	add_test_function(color_GetBGR32);
	add_test_function(color_GetRGB_565);
	add_test_function(color_GetRGB16);
	add_test_function(color_GetBGR_565);
	add_test_function(color_GetBGR16);

	return 0;
}

/* GDI Color Space Conversions: http://msdn.microsoft.com/en-us/library/ff566496(VS.85).aspx */

void test_color_GetRGB32(void)
{
	int r, g, b;
	uint32 rgb32 = 0x00AABBCC;
	GetRGB32(r, g, b, rgb32);

	CU_ASSERT(r == 0xAA);
	CU_ASSERT(g == 0xBB);
	CU_ASSERT(b == 0xCC);
}

void test_color_GetBGR32(void)
{
	int r, g, b;
	uint32 bgr32 = 0x00CCBBAA;
	GetBGR32(r, g, b, bgr32);

	CU_ASSERT(r == 0xAA);
	CU_ASSERT(g == 0xBB);
	CU_ASSERT(b == 0xCC);
}

void test_color_GetRGB_565(void)
{
	/*
		R: 0x15, 10101
	 	G: 0x33, 110011
	 	B: 0x1D, 11101

	 	0xAE7D, 10101110 01111101
	*/
	
	int r, g, b;
	uint16 rgb16 = 0xAE7D;
	GetRGB_565(r, g, b, rgb16);
	
	CU_ASSERT(r == 0x15);
	CU_ASSERT(g == 0x33);
	CU_ASSERT(b == 0x1D);
}

void test_color_GetRGB16(void)
{
	/*
		R: 0x15 -> 0xAD, 10101 -> 10101101
	 	G: 0x33 -> 0xCF, 110011 -> 11001111
	 	B: 0x1D -> 0xEF, 11101 -> 11101101

	 	0xAE7D -> 0xADCFEF
	 	10101110 01111101 -> 10101101 11001111 11101101
	*/
	
	int r, g, b;
	uint16 rgb16 = 0xAE7D;
	GetRGB16(r, g, b, rgb16);
	
	CU_ASSERT(r == 0xAD);
	CU_ASSERT(g == 0xCF);
	CU_ASSERT(b == 0xEF);
}

void test_color_GetBGR_565(void)
{
	/*
	 	B: 0x1D, 11101
	 	G: 0x33, 110011
	 	R: 0x15, 10101

	 	0xEE75, 11101110 01110101
	*/
		
	int r, g, b;
	uint16 bgr16 = 0xEE75;
	GetBGR_565(r, g, b, bgr16);
	
	CU_ASSERT(r == 0x15);
	CU_ASSERT(g == 0x33);
	CU_ASSERT(b == 0x1D);
}

void test_color_GetBGR16(void)
{
	/*
	 	B: 0x1D -> 0xEF, 11101 -> 11101101
	 	G: 0x33 -> 0xCF, 110011 -> 11001111
	 	R: 0x15 -> 0xAD, 10101 -> 10101101

	 	0xEE75 -> 0xADCFEF
	 	11101110 01110101 -> 10101101 11001111 11101101
	*/
		
	int r, g, b;
	uint16 bgr16 = 0xEE75;
	GetBGR16(r, g, b, bgr16);
	
	CU_ASSERT(r == 0xAD);
	CU_ASSERT(g == 0xCF);
	CU_ASSERT(b == 0xEF);
}

