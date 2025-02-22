/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * helper utility, convert image to RAW RGBA32 header ready to be included
 *
 * Copyright 2025 Armin Novak <armin.novak@thincast.com>
 * Copyright 2025 Thincast Technologies GmbH
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
#include <winpr/image.h>
#include <stdio.h>

static void usage(const char* name)
{
	(void)printf("%s <source file> <destination file>\n\n", name);
	(void)printf("\tRead any image format supported by WinPR::wImage\n");
	(void)printf("\tand convert it to raw BGRA data.\n\n");
	(void)printf("\toutput format is a C header with an array ready to be included\n");
}

static int dump_data_hex(FILE* fp, const uint8_t* data, size_t size)
{
	if (size > 0)
	{
		const int rc = fprintf(fp, "0x%02" PRIx8, data[0]);
		if (rc != 4)
			return -1;
	}

	for (size_t x = 1; x < size; x++)
	{
		if (x % 16 == 0)
		{
			const int rc = fprintf(fp, ",\n");
			if (rc != 2)
				return -2;
		}
		else
		{
			const int rc = fprintf(fp, ",");
			if (rc != 1)
				return -2;
		}

		const int rc = fprintf(fp, "0x%02" PRIx8, data[x]);
		if (rc != 4)
			return -2;
	}
	return 0;
}

static int dump_data(const wImage* img, const char* file)
{
	FILE* fp = fopen(file, "w");
	if (!fp)
	{
		(void)fprintf(stderr, "Failed to open file '%s'\n", file);
		return -1;
	}

	int rc = -2;
	int count = fprintf(fp, "#pragma once\n");
	if (count < 0)
		goto fail;
	count = fprintf(fp, "\n");
	if (count < 0)
		goto fail;
	count = fprintf(fp, "#include <stdint.h>\n");
	if (count < 0)
		goto fail;
	count = fprintf(fp, "\n");
	if (count < 0)
		goto fail;
	count = fprintf(fp, "static const uint8_t img_data[] ={\n");
	if (count < 0)
		goto fail;
	count = dump_data_hex(fp, img->data, 1ULL * img->height * img->scanline);
	if (count < 0)
		goto fail;
	count = fprintf(fp, "};\n");
	if (count < 0)
		goto fail;
	rc = 0;
fail:
	fclose(fp);
	return rc;
}

int main(int argc, char* argv[])
{
	if (argc != 3)
	{
		usage(argv[0]);
		return -1;
	}

	int rc = -1;
	const char* src = argv[1];
	const char* dst = argv[2];
	wImage* img = winpr_image_new();
	if (!img)
		goto fail;

	const int res = winpr_image_read(img, src);
	if (res <= 0)
	{
		(void)fprintf(stderr, "Failed to read image file '%s'\n", src);
		goto fail;
	}

	rc = dump_data(img, dst);

	if (rc >= 0)
		(void)printf("Converted '%s' to header '%s'\n", src, dst);
fail:
	if (rc != 0)
		usage(argv[0]);
	winpr_image_free(img, TRUE);
	return rc;
}
