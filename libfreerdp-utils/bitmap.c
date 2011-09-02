/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Bitmap File Format Utils
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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
#include <string.h>

#include <freerdp/types.h>

#include <freerdp/utils/bitmap.h>

typedef struct
{
	uint8 magic[2];
} bmpfile_magic;

typedef struct
{
	uint32 filesz;
	uint16 creator1;
	uint16 creator2;
	uint32 bmp_offset;
} bmpfile_header;

typedef struct
{
	uint32 header_sz;
	sint32 width;
	sint32 height;
	uint16 nplanes;
	uint16 bitspp;
	uint32 compress_type;
	uint32 bmp_bytesz;
	sint32 hres;
	sint32 vres;
	uint32 ncolors;
	uint32 nimpcolors;
} BITMAPINFOHEADER;

void freerdp_bitmap_write(char* filename, void* data, int width, int height, int bpp)
{
	FILE* fp;
	bmpfile_magic magic;
	bmpfile_header header;
	BITMAPINFOHEADER info_header;

	fp = fopen(filename, "w+b");

	if (fp == NULL)
	{
		printf("failed to open file %s\n", filename);
		return;
	}

	magic.magic[0] = 'B';
	magic.magic[1] = 'M';

	header.creator1 = 0;
	header.creator2 = 0;

	header.bmp_offset =
			sizeof(bmpfile_magic) +
			sizeof(bmpfile_header) +
			sizeof(BITMAPINFOHEADER);

	info_header.bmp_bytesz = width * height * (bpp / 8);

	header.filesz =
		header.bmp_offset +
		info_header.bmp_bytesz;

	info_header.width = width;
	info_header.height = (-1) * height;
	info_header.nplanes = 1;
	info_header.bitspp = bpp;
	info_header.compress_type = 0;
	info_header.hres = width;
	info_header.vres = height;
	info_header.ncolors = 0;
	info_header.nimpcolors = 0;
	info_header.header_sz = sizeof(BITMAPINFOHEADER);

	fwrite((void*) &magic, sizeof(bmpfile_magic), 1, fp);
	fwrite((void*) &header, sizeof(bmpfile_header), 1, fp);
	fwrite((void*) &info_header, sizeof(BITMAPINFOHEADER), 1, fp);
	fwrite((void*) data, info_header.bmp_bytesz, 1, fp);

	fclose(fp);
}

