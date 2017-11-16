/**
 * RdTk: Remote Desktop Toolkit
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

#include <errno.h>

#include <winpr/wtypes.h>
#include <winpr/crt.h>
#include <winpr/path.h>
#include <winpr/print.h>

#include "rdtk_engine.h"
#include "rdtk_resources.h"
#include "rdtk_surface.h"

#include "rdtk_font.h"

static int rdtk_font_draw_glyph(rdtkSurface* surface, int nXDst, int nYDst, rdtkFont* font,
                                rdtkGlyph* glyph)
{
	int x, y;
	int nXSrc;
	int nYSrc;
	int nWidth;
	int nHeight;
	int nSrcStep;
	int nDstStep;
	BYTE* pSrcData;
	BYTE* pSrcPixel;
	BYTE* pDstData;
	BYTE* pDstPixel;
	BYTE A, R, G, B;
	nXDst += glyph->offsetX;
	nYDst += glyph->offsetY;
	nXSrc = glyph->rectX;
	nYSrc = glyph->rectY;
	nWidth = glyph->rectWidth;
	nHeight = glyph->rectHeight;
	nSrcStep = font->image->scanline;
	pSrcData = font->image->data;
	pDstData = surface->data;
	nDstStep = surface->scanline;

	for (y = 0; y < nHeight; y++)
	{
		pSrcPixel = &pSrcData[((nYSrc + y) * nSrcStep) + (nXSrc * 4)];
		pDstPixel = &pDstData[((nYDst + y) * nDstStep) + (nXDst * 4)];

		for (x = 0; x < nWidth; x++)
		{
			B = pSrcPixel[0];
			G = pSrcPixel[1];
			R = pSrcPixel[2];
			A = pSrcPixel[3];
			pSrcPixel += 4;

			if (1)
			{
				/* tint black */
				R = 255 - R;
				G = 255 - G;
				B = 255 - B;
			}

			if (A == 255)
			{
				pDstPixel[0] = B;
				pDstPixel[1] = G;
				pDstPixel[2] = R;
			}
			else
			{
				R = (R * A) / 255;
				G = (G * A) / 255;
				B = (B * A) / 255;
				pDstPixel[0] = B + (pDstPixel[0] * (255 - A) + (255 / 2)) / 255;
				pDstPixel[1] = G + (pDstPixel[1] * (255 - A) + (255 / 2)) / 255;
				pDstPixel[2] = R + (pDstPixel[2] * (255 - A) + (255 / 2)) / 255;
			}

			pDstPixel[3] = 0xFF;
			pDstPixel += 4;
		}
	}

	return 1;
}

int rdtk_font_draw_text(rdtkSurface* surface, int nXDst, int nYDst, rdtkFont* font,
                        const char* text)
{
	int index;
	int length;
	rdtkGlyph* glyph;
	font = surface->engine->font;
	length = strlen(text);

	for (index = 0; index < length; index++)
	{
		glyph = &font->glyphs[text[index] - 32];
		rdtk_font_draw_glyph(surface, nXDst, nYDst, font, glyph);
		nXDst += (glyph->width + 1);
	}

	return 1;
}

int rdtk_font_text_draw_size(rdtkFont* font, int* width, int* height, const char* text)
{
	int index;
	int length;
	int glyphIndex;
	rdtkGlyph* glyph;
	*width = 0;
	*height = 0;
	length = strlen(text);

	for (index = 0; index < length; index++)
	{
		glyphIndex = text[index] - 32;

		if (glyphIndex < font->glyphCount)
		{
			glyph = &font->glyphs[glyphIndex];
			*width += (glyph->width + 1);
		}
	}

	*height = font->height + 2;
	return 1;
}

static char* rdtk_font_load_descriptor_file(const char* filename, int* pSize)
{
	BYTE* buffer;
	FILE* fp = NULL;
	size_t readSize;
	size_t fileSize;
	fp = fopen(filename, "r");

	if (!fp)
		return NULL;

	_fseeki64(fp, 0, SEEK_END);
	fileSize = _ftelli64(fp);
	_fseeki64(fp, 0, SEEK_SET);

	if (fileSize < 1)
	{
		fclose(fp);
		return NULL;
	}

	buffer = (BYTE*) malloc(fileSize + 2);

	if (!buffer)
	{
		fclose(fp);
		return NULL;
	}

	readSize = fread(buffer, fileSize, 1, fp);

	if (!readSize)
	{
		if (!ferror(fp))
			readSize = fileSize;
	}

	fclose(fp);

	if (readSize < 1)
	{
		free(buffer);
		return NULL;
	}

	buffer[fileSize] = '\0';
	buffer[fileSize + 1] = '\0';
	*pSize = (int) fileSize;
	return (char*) buffer;
}

static int rdtk_font_convert_descriptor_code_to_utf8(const char* str, BYTE* utf8)
{
	int len = strlen(str);
	*((UINT32*) utf8) = 0;

	if (len < 1)
		return 1;

	if (len == 1)
	{
		if ((str[0] > 31) && (str[0] < 127))
		{
			utf8[0] = str[0];
		}
	}
	else
	{
		if (str[0] == '&')
		{
			const char* acc = &str[1];

			if (strcmp(acc, "quot;") == 0)
				utf8[0] = '"';
			else if (strcmp(acc, "amp;") == 0)
				utf8[0] = '&';
			else if (strcmp(acc, "lt;") == 0)
				utf8[0] = '<';
			else if (strcmp(acc, "gt;") == 0)
				utf8[0] = '>';
		}
	}

	return 1;
}

static int rdtk_font_parse_descriptor_buffer(rdtkFont* font, BYTE* buffer, int size)
{
	char* p;
	char* q;
	char* r;
	char* beg;
	char* end;
	char* tok[4];
	int index;
	int count;
	rdtkGlyph* glyph;
	p = strstr((char*) buffer, "<?xml version=\"1.0\" encoding=\"utf-8\"?>");

	if (!p)
		return -1;

	p += sizeof("<?xml version=\"1.0\" encoding=\"utf-8\"?>") - 1;
	p = strstr(p, "<Font ");

	if (!p)
		return -1;

	p += sizeof("<Font ") - 1;
	/* find closing font tag */
	end = strstr(p, "</Font>");

	if (!end)
		return -1;

	/* parse font size */
	p = strstr(p, "size=\"");

	if (!p)
		return -1;

	p += sizeof("size=\"") - 1;
	q = strchr(p, '"');

	if (!q)
		return -1;

	*q = '\0';
	errno = 0;
	{
		long val = strtol(p, NULL, 0);

		if ((errno != 0) || (val < INT32_MIN) || (val > INT32_MAX))
			return -1;

		font->size = val;
	}
	*q = '"';

	if (font->size <= 0)
		return -1;

	p = q + 1;
	/* parse font family */
	p = strstr(p, "family=\"");

	if (!p)
		return -1;

	p += sizeof("family=\"") - 1;
	q = strchr(p, '"');

	if (!q)
		return -1;

	*q = '\0';
	font->family = _strdup(p);
	*q = '"';

	if (!font->family)
		return -1;

	p = q + 1;
	/* parse font height */
	p = strstr(p, "height=\"");

	if (!p)
		return -1;

	p += sizeof("height=\"") - 1;
	q = strchr(p, '"');

	if (!q)
		return -1;

	*q = '\0';
	errno = 0;
	{
		long val = strtol(p, NULL, 0);

		if ((errno != 0) || (val < INT32_MIN) || (val > INT32_MAX))
			return -1;

		font->height = val;
	}
	*q = '"';

	if (font->height <= 0)
		return -1;

	p = q + 1;
	/* parse font style */
	p = strstr(p, "style=\"");

	if (!p)
		return -1;

	p += sizeof("style=\"") - 1;
	q = strchr(p, '"');

	if (!q)
		return -1;

	*q = '\0';
	font->style = _strdup(p);
	*q = '"';

	if (!font->style)
		return -1;

	p = q + 1;
	//printf("size: %d family: %s height: %d style: %s\n",
	//		font->size, font->family, font->height, font->style);
	beg = p;
	count = 0;

	while (p < end)
	{
		p = strstr(p, "<Char ");

		if (!p)
			return -1;

		p += sizeof("<Char ") - 1;
		r = strstr(p, "/>");

		if (!r)
			return -1;

		*r = '\0';
		p = r + sizeof("/>");
		*r = '/';
		count++;
	}

	font->glyphCount = count;
	font->glyphs = NULL;

	if (count > 0)
		font->glyphs = (rdtkGlyph*) calloc(font->glyphCount, sizeof(rdtkGlyph));

	if (!font->glyphs)
		return -1;

	p = beg;
	index = 0;

	while (p < end)
	{
		p = strstr(p, "<Char ");

		if (!p)
			return -1;

		p += sizeof("<Char ") - 1;
		r = strstr(p, "/>");

		if (!r)
			return -1;

		*r = '\0';
		/* start parsing glyph */
		glyph = &font->glyphs[index];
		/* parse glyph width */
		p = strstr(p, "width=\"");

		if (!p)
			return -1;

		p += sizeof("width=\"") - 1;
		q = strchr(p, '"');

		if (!q)
			return -1;

		*q = '\0';
		errno = 0;
		{
			long val = strtoul(p, NULL, 0);

			if ((errno != 0) || (val < INT32_MIN) || (val > INT32_MAX))
				return -1;

			glyph->width = val;
		}
		*q = '"';

		if (glyph->width < 0)
			return -1;

		p = q + 1;
		/* parse glyph offset x,y */
		p = strstr(p, "offset=\"");

		if (!p)
			return -1;

		p += sizeof("offset=\"") - 1;
		q = strchr(p, '"');

		if (!q)
			return -1;

		*q = '\0';
		tok[0] = p;
		p = strchr(tok[0] + 1, ' ');

		if (!p)
			return -1;

		*p = 0;
		tok[1] = p + 1;
		errno = 0;
		{
			long val = strtol(tok[0], NULL, 0);

			if ((errno != 0) || (val < INT32_MIN) || (val > INT32_MAX))
				return -1;

			glyph->offsetX = val;
		}
		{
			long val = strtol(tok[1], NULL, 0);

			if ((errno != 0) || (val < INT32_MIN) || (val > INT32_MAX))
				return -1;

			glyph->offsetY = val;
		}
		*q = '"';
		p = q + 1;
		/* parse glyph rect x,y,w,h */
		p = strstr(p, "rect=\"");

		if (!p)
			return -1;

		p += sizeof("rect=\"") - 1;
		q = strchr(p, '"');

		if (!q)
			return -1;

		*q = '\0';
		tok[0] = p;
		p = strchr(tok[0] + 1, ' ');

		if (!p)
			return -1;

		*p = 0;
		tok[1] = p + 1;
		p = strchr(tok[1] + 1, ' ');

		if (!p)
			return -1;

		*p = 0;
		tok[2] = p + 1;
		p = strchr(tok[2] + 1, ' ');

		if (!p)
			return -1;

		*p = 0;
		tok[3] = p + 1;
		errno = 0;
		{
			long val = strtol(tok[0], NULL, 0);

			if ((errno != 0) || (val < INT32_MIN) || (val > INT32_MAX))
				return -1;

			glyph->rectX = val;
		}
		{
			long val = strtol(tok[1], NULL, 0);

			if ((errno != 0) || (val < INT32_MIN) || (val > INT32_MAX))
				return -1;

			glyph->rectY = val;
		}
		{
			long val = strtol(tok[2], NULL, 0);

			if ((errno != 0) || (val < INT32_MIN) || (val > INT32_MAX))
				return -1;

			glyph->rectWidth = val;
		}
		{
			long val = strtol(tok[3], NULL, 0);

			if ((errno != 0) || (val < INT32_MIN) || (val > INT32_MAX))
				return -1;

			glyph->rectHeight = val;
		}
		*q = '"';
		p = q + 1;
		/* parse code */
		p = strstr(p, "code=\"");

		if (!p)
			return -1;

		p += sizeof("code=\"") - 1;
		q = strchr(p, '"');

		if (!q)
			return -1;

		*q = '\0';
		rdtk_font_convert_descriptor_code_to_utf8(p, glyph->code);
		*q = '"';
		/* finish parsing glyph */
		p = r + sizeof("/>");
		*r = '/';
		index++;
	}

	return 1;
}

static int rdtk_font_load_descriptor(rdtkFont* font, const char* filename)
{
	int size;
	char* buffer;
	buffer = rdtk_font_load_descriptor_file(filename, &size);

	if (!buffer)
		return -1;

	return rdtk_font_parse_descriptor_buffer(font, (BYTE*) buffer, size);
}
rdtkFont* rdtk_font_new(rdtkEngine* engine, const char* path, const char* file)
{
	int status;
	int length;
	rdtkFont* font = NULL;
	char* fontBaseFile = NULL;
	char* fontImageFile = NULL;
	char* fontDescriptorFile = NULL;
	fontBaseFile = GetCombinedPath(path, file);

	if (!fontBaseFile)
		goto cleanup;

	length = strlen(fontBaseFile);
	fontImageFile = (char*) malloc(length + 8);

	if (!fontImageFile)
		goto cleanup;

	strcpy(fontImageFile, fontBaseFile);
	strcpy(&fontImageFile[length], ".png");
	fontDescriptorFile = (char*) malloc(length + 8);

	if (!fontDescriptorFile)
		goto cleanup;

	strcpy(fontDescriptorFile, fontBaseFile);
	strcpy(&fontDescriptorFile[length], ".xml");
	free(fontBaseFile);

	if (!PathFileExistsA(fontImageFile))
		goto cleanup;

	if (!PathFileExistsA(fontDescriptorFile))
		goto cleanup;

	font = (rdtkFont*) calloc(1, sizeof(rdtkFont));

	if (!font)
		goto cleanup;

	font->engine = engine;
	font->image = winpr_image_new();

	if (!font->image)
		goto cleanup;

	status = winpr_image_read(font->image, fontImageFile);

	if (status < 0)
		goto cleanup;

	status = rdtk_font_load_descriptor(font, fontDescriptorFile);

	if (status < 0)
		goto cleanup;

	free(fontImageFile);
	free(fontDescriptorFile);
	return font;
cleanup:
	free(fontImageFile);
	free(fontDescriptorFile);

	if (font)
	{
		if (font->image)
			winpr_image_free(font->image, TRUE);

		free(font);
	}

	return NULL;
}
static rdtkFont* rdtk_embedded_font_new(rdtkEngine* engine, BYTE* imageData, int imageSize,
                                        BYTE* descriptorData, int descriptorSize)
{
	int size;
	int status;
	BYTE* buffer;
	rdtkFont* font;
	font = (rdtkFont*) calloc(1, sizeof(rdtkFont));

	if (!font)
		return NULL;

	font->engine = engine;
	font->image = winpr_image_new();

	if (!font->image)
	{
		free(font);
		return NULL;
	}

	status = winpr_image_read_buffer(font->image, imageData, imageSize);

	if (status < 0)
	{
		winpr_image_free(font->image, TRUE);
		free(font);
		return NULL;
	}

	size = descriptorSize;
	buffer = (BYTE*) malloc(size);

	if (!buffer)
	{
		winpr_image_free(font->image, TRUE);
		free(font);
		return NULL;
	}

	CopyMemory(buffer, descriptorData, size);
	status = rdtk_font_parse_descriptor_buffer(font, buffer, size);
	free(buffer);

	if (status < 0)
	{
		winpr_image_free(font->image, TRUE);
		free(font);
		return NULL;
	}

	return font;
}
void rdtk_font_free(rdtkFont* font)
{
	if (font)
	{
		free(font->family);
		free(font->style);
		winpr_image_free(font->image, TRUE);
		free(font->glyphs);
		free(font);
	}
}
int rdtk_font_engine_init(rdtkEngine* engine)
{
	if (!engine->font)
	{
		int imageSize;
		int descriptorSize;
		BYTE* imageData = NULL;
		BYTE* descriptorData = NULL;
		imageSize = rdtk_get_embedded_resource_file("source_serif_pro_regular_12.png", &imageData);
		descriptorSize = rdtk_get_embedded_resource_file("source_serif_pro_regular_12.xml",
		                 &descriptorData);

		if ((imageSize < 0) || (descriptorSize < 0))
			return -1;

		engine->font = rdtk_embedded_font_new(engine, imageData, imageSize, descriptorData, descriptorSize);
	}

	return 1;
}
int rdtk_font_engine_uninit(rdtkEngine* engine)
{
	if (engine->font)
	{
		rdtk_font_free(engine->font);
		engine->font = NULL;
	}

	return 1;
}
