/**
 * WinPR: Windows Portable Runtime
 * Image Utils
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

#include <winpr/image.h>

int winpr_bitmap_write(char* filename, BYTE* data, int width, int height, int bpp)
{
	FILE* fp;
	WINPR_BITMAP_MAGIC magic;
	WINPR_BITMAP_CORE_HEADER header;
	WINPR_BITMAP_INFO_HEADER info_header;

	fp = fopen(filename, "w+b");

	if (!fp)
	{
		fprintf(stderr, "failed to open file %s\n", filename);
		return -1;
	}

	magic.magic[0] = 'B';
	magic.magic[1] = 'M';

	header.creator1 = 0;
	header.creator2 = 0;

	header.bmp_offset = sizeof(WINPR_BITMAP_MAGIC) +
			sizeof(WINPR_BITMAP_CORE_HEADER) +
			sizeof(WINPR_BITMAP_INFO_HEADER);

	info_header.bmp_bytesz = width * height * (bpp / 8);

	header.filesz = header.bmp_offset + info_header.bmp_bytesz;

	info_header.width = width;
	info_header.height = (-1) * height;
	info_header.nplanes = 1;
	info_header.bitspp = bpp;
	info_header.compress_type = 0;
	info_header.hres = width;
	info_header.vres = height;
	info_header.ncolors = 0;
	info_header.nimpcolors = 0;
	info_header.header_sz = sizeof(WINPR_BITMAP_INFO_HEADER);

	fwrite((void*) &magic, sizeof(WINPR_BITMAP_MAGIC), 1, fp);
	fwrite((void*) &header, sizeof(WINPR_BITMAP_CORE_HEADER), 1, fp);
	fwrite((void*) &info_header, sizeof(WINPR_BITMAP_INFO_HEADER), 1, fp);
	fwrite((void*) data, info_header.bmp_bytesz, 1, fp);

	fclose(fp);

	return 1;
}
