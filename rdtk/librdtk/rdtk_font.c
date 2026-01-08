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

#include <rdtk/config.h>

#include <errno.h>

#include <winpr/config.h>
#include <winpr/wtypes.h>
#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/cast.h>
#include <winpr/path.h>
#include <winpr/file.h>
#include <winpr/print.h>

#include "rdtk_engine.h"
#include "rdtk_resources.h"
#include "rdtk_surface.h"

#include "rdtk_font.h"

#if defined(WINPR_WITH_PNG)
#define FILE_EXT "png"
#else
#define FILE_EXT "bmp"
#endif

static int rdtk_font_draw_glyph(rdtkSurface* surface, uint16_t nXDst, uint16_t nYDst,
                                rdtkFont* font, rdtkGlyph* glyph)
{
	WINPR_ASSERT(surface);
	WINPR_ASSERT(font);
	WINPR_ASSERT(glyph);

	nXDst += glyph->offsetX;
	nYDst += glyph->offsetY;
	const size_t nXSrc = WINPR_ASSERTING_INT_CAST(size_t, glyph->rectX);
	const size_t nYSrc = WINPR_ASSERTING_INT_CAST(size_t, glyph->rectY);
	const size_t nWidth = WINPR_ASSERTING_INT_CAST(size_t, glyph->rectWidth);
	const size_t nHeight = WINPR_ASSERTING_INT_CAST(size_t, glyph->rectHeight);
	const uint32_t nSrcStep = font->image->scanline;
	const uint8_t* pSrcData = font->image->data;
	uint8_t* pDstData = surface->data;
	const uint32_t nDstStep = surface->scanline;

	for (size_t y = 0; y < nHeight; y++)
	{
		const uint8_t* pSrcPixel = &pSrcData[((1ULL * nYSrc + y) * nSrcStep) + (4ULL * nXSrc)];
		uint8_t* pDstPixel = &pDstData[((1ULL * nYDst + y) * nDstStep) + (4ULL * nXDst)];

		for (size_t x = 0; x < nWidth; x++)
		{
			uint8_t B = pSrcPixel[0];
			uint8_t G = pSrcPixel[1];
			uint8_t R = pSrcPixel[2];
			uint8_t A = pSrcPixel[3];
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

int rdtk_font_draw_text(rdtkSurface* surface, uint16_t nXDst, uint16_t nYDst, rdtkFont* font,
                        const char* text)
{
	WINPR_ASSERT(surface);
	WINPR_ASSERT(font);
	WINPR_ASSERT(text);

	const size_t length = strlen(text);
	for (size_t index = 0; index < length; index++)
	{
		rdtkGlyph* glyph = &font->glyphs[text[index] - 32];
		rdtk_font_draw_glyph(surface, nXDst, nYDst, font, glyph);
		nXDst += (glyph->width + 1);
	}

	return 1;
}

int rdtk_font_text_draw_size(rdtkFont* font, uint16_t* width, uint16_t* height, const char* text)
{
	WINPR_ASSERT(font);
	WINPR_ASSERT(width);
	WINPR_ASSERT(height);
	WINPR_ASSERT(text);

	*width = 0;
	*height = 0;
	const size_t length = strlen(text);
	for (size_t index = 0; index < length; index++)
	{
		const size_t glyphIndex = WINPR_ASSERTING_INT_CAST(size_t, text[index] - 32);

		if (glyphIndex < font->glyphCount)
		{
			rdtkGlyph* glyph = &font->glyphs[glyphIndex];
			*width += (glyph->width + 1);
		}
	}

	*height = font->height + 2;
	return 1;
}

WINPR_ATTR_MALLOC(free, 1)
static char* rdtk_font_load_descriptor_file(const char* filename, size_t* pSize)
{
	WINPR_ASSERT(filename);
	WINPR_ASSERT(pSize);

	union
	{
		size_t s;
		INT64 i64;
	} fileSize;
	FILE* fp = winpr_fopen(filename, "r");

	if (!fp)
		return NULL;

	if (_fseeki64(fp, 0, SEEK_END) != 0)
		goto fail;
	fileSize.i64 = _ftelli64(fp);
	if (_fseeki64(fp, 0, SEEK_SET) != 0)
		goto fail;

	if (fileSize.i64 < 1)
		goto fail;

	{
		char* buffer = (char*)calloc(fileSize.s + 4, sizeof(char));
		if (!buffer)
			goto fail;

		{
			size_t readSize = fread(buffer, fileSize.s, 1, fp);
			if (readSize == 0)
			{
				if (!ferror(fp))
					readSize = fileSize.s;
			}

			(void)fclose(fp);

			if (readSize < 1)
			{
				free(buffer);
				return NULL;
			}
		}

		buffer[fileSize.s] = '\0';
		buffer[fileSize.s + 1] = '\0';
		*pSize = fileSize.s;
		return buffer;
	}

fail:
	(void)fclose(fp);
	return NULL;
}

static int rdtk_font_convert_descriptor_code_to_utf8(const char* str, uint8_t* utf8)
{
	WINPR_ASSERT(str);
	WINPR_ASSERT(utf8);

	const size_t len = strlen(str);
	*((uint32_t*)utf8) = 0;

	if (len < 1)
		return 1;

	if (len == 1)
	{
		if ((str[0] > 31) && (str[0] < 127))
		{
			utf8[0] = WINPR_ASSERTING_INT_CAST(uint8_t, str[0] & 0xFF);
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

static int rdtk_font_parse_descriptor_buffer(rdtkFont* font, char* buffer,
                                             WINPR_ATTR_UNUSED size_t size)
{
	int rc = -1;

	WINPR_ASSERT(font);

	const char xmlversion[] = "<?xml version=\"1.0\" encoding=\"utf-8\"?>";
	const char xmlfont[] = "<Font ";

	char* p = strstr(buffer, xmlversion);

	if (!p)
		goto fail;

	p += sizeof(xmlversion) - 1;
	p = strstr(p, xmlfont);

	if (!p)
		goto fail;

	p += sizeof(xmlfont) - 1;

	/* find closing font tag */
	{
		char* end = strstr(p, "</Font>");
		if (!end)
			goto fail;

		/* parse font size */
		p = strstr(p, "size=\"");

		if (!p)
			goto fail;

		p += sizeof("size=\"") - 1;

		{
			char* q = strchr(p, '"');
			if (!q)
				goto fail;

			*q = '\0';
			errno = 0;
			{
				long val = strtol(p, NULL, 0);

				if ((errno != 0) || (val == 0) || (val > UINT32_MAX))
					goto fail;

				font->size = (UINT32)val;
			}
			*q = '"';

			if (font->size <= 0)
				goto fail;

			p = q + 1;
		}
		/* parse font family */
		p = strstr(p, "family=\"");

		if (!p)
			goto fail;

		p += sizeof("family=\"") - 1;
		{
			char* q = strchr(p, '"');

			if (!q)
				goto fail;

			*q = '\0';
			font->family = _strdup(p);
			*q = '"';

			if (!font->family)
				goto fail;

			p = q + 1;
		}
		/* parse font height */
		p = strstr(p, "height=\"");

		if (!p)
			goto fail;

		p += sizeof("height=\"") - 1;
		{
			char* q = strchr(p, '"');

			if (!q)
				goto fail;

			*q = '\0';
			errno = 0;
			{
				const unsigned long val = strtoul(p, NULL, 0);

				if ((errno != 0) || (val > UINT16_MAX))
					goto fail;

				font->height = (uint16_t)val;
			}
			*q = '"';

			if (font->height <= 0)
				goto fail;

			p = q + 1;
		}

		/* parse font style */
		p = strstr(p, "style=\"");

		if (!p)
			goto fail;

		p += sizeof("style=\"") - 1;
		{
			char* q = strchr(p, '"');

			if (!q)
				goto fail;

			*q = '\0';
			font->style = _strdup(p);
			*q = '"';

			if (!font->style)
				goto fail;

			p = q + 1;
		}

		// printf("size: %d family: %s height: %d style: %s\n",
		//		font->size, font->family, font->height, font->style);
		{
			char* beg = p;
			size_t count = 0;

			while (p < end)
			{
				p = strstr(p, "<Char ");

				if (!p)
					goto fail;

				p += sizeof("<Char ") - 1;
				char* r = strstr(p, "/>");

				if (!r)
					goto fail;

				*r = '\0';
				p = r + sizeof("/>");
				*r = '/';
				count++;
			}

			if (count > UINT16_MAX)
				goto fail;

			font->glyphCount = (uint16_t)count;
			font->glyphs = NULL;

			if (count > 0)
				font->glyphs = (rdtkGlyph*)calloc(font->glyphCount, sizeof(rdtkGlyph));

			if (!font->glyphs)
				goto fail;

			p = beg;
		}

		{
			size_t index = 0;
			while (p < end)
			{
				p = strstr(p, "<Char ");

				if (!p)
					goto fail;

				p += sizeof("<Char ") - 1;
				char* r = strstr(p, "/>");

				if (!r)
					goto fail;

				*r = '\0';
				/* start parsing glyph */
				if (index >= font->glyphCount)
					goto fail;

				rdtkGlyph* glyph = &font->glyphs[index];
				/* parse glyph width */
				p = strstr(p, "width=\"");

				if (!p)
					goto fail;

				p += sizeof("width=\"") - 1;
				{
					char* q = strchr(p, '"');

					if (!q)
						goto fail;

					*q = '\0';
					errno = 0;
					{
						long val = strtol(p, NULL, 0);

						if ((errno != 0) || (val < INT32_MIN) || (val > INT32_MAX))
							goto fail;

						glyph->width = (INT32)val;
					}
					*q = '"';

					if (glyph->width < 0)
						goto fail;

					p = q + 1;
				}

				/* parse glyph offset x,y */
				p = strstr(p, "offset=\"");

				if (!p)
					goto fail;

				p += sizeof("offset=\"") - 1;

				char* tok[4] = { 0 };
				{
					char* q = strchr(p, '"');

					if (!q)
						goto fail;

					*q = '\0';
					tok[0] = p;
					p = strchr(tok[0] + 1, ' ');

					if (!p)
						goto fail;

					*p = 0;
					tok[1] = p + 1;
					errno = 0;
					{
						long val = strtol(tok[0], NULL, 0);

						if ((errno != 0) || (val < INT32_MIN) || (val > INT32_MAX))
							goto fail;

						glyph->offsetX = (INT32)val;
					}
					{
						long val = strtol(tok[1], NULL, 0);

						if ((errno != 0) || (val < INT32_MIN) || (val > INT32_MAX))
							goto fail;

						glyph->offsetY = (INT32)val;
					}
					*q = '"';
					p = q + 1;
				}
				/* parse glyph rect x,y,w,h */
				p = strstr(p, "rect=\"");

				if (!p)
					goto fail;

				p += sizeof("rect=\"") - 1;
				{
					char* q = strchr(p, '"');

					if (!q)
						goto fail;

					*q = '\0';

					tok[0] = p;
					p = strchr(tok[0] + 1, ' ');

					if (!p)
						goto fail;

					*p = 0;
					tok[1] = p + 1;
					p = strchr(tok[1] + 1, ' ');

					if (!p)
						goto fail;

					*p = 0;
					tok[2] = p + 1;
					p = strchr(tok[2] + 1, ' ');

					if (!p)
						goto fail;

					*p = 0;
					tok[3] = p + 1;
					errno = 0;
					{
						long val = strtol(tok[0], NULL, 0);

						if ((errno != 0) || (val < INT32_MIN) || (val > INT32_MAX))
							goto fail;

						glyph->rectX = (INT32)val;
					}
					{
						long val = strtol(tok[1], NULL, 0);

						if ((errno != 0) || (val < INT32_MIN) || (val > INT32_MAX))
							goto fail;

						glyph->rectY = (INT32)val;
					}
					{
						long val = strtol(tok[2], NULL, 0);

						if ((errno != 0) || (val < INT32_MIN) || (val > INT32_MAX))
							goto fail;

						glyph->rectWidth = (INT32)val;
					}
					{
						long val = strtol(tok[3], NULL, 0);

						if ((errno != 0) || (val < INT32_MIN) || (val > INT32_MAX))
							goto fail;

						glyph->rectHeight = (INT32)val;
					}
					*q = '"';
					p = q + 1;
				}
				/* parse code */
				p = strstr(p, "code=\"");

				if (!p)
					goto fail;

				p += sizeof("code=\"") - 1;
				{
					char* q = strchr(p, '"');

					if (!q)
						goto fail;

					*q = '\0';
					rdtk_font_convert_descriptor_code_to_utf8(p, glyph->code);
					*q = '"';
				}
				/* finish parsing glyph */
				p = r + sizeof("/>");
				*r = '/';
				index++;
			}
		}
	}

	rc = 1;

fail:
	free(buffer);
	return rc;
}

static int rdtk_font_load_descriptor(rdtkFont* font, const char* filename)
{
	size_t size = 0;

	WINPR_ASSERT(font);
	char* buffer = rdtk_font_load_descriptor_file(filename, &size);

	if (!buffer)
		return -1;

	return rdtk_font_parse_descriptor_buffer(font, buffer, size);
}

rdtkFont* rdtk_font_new(rdtkEngine* engine, const char* path, const char* file)
{
	size_t length = 0;
	rdtkFont* font = NULL;
	char* fontImageFile = NULL;
	char* fontDescriptorFile = NULL;

	WINPR_ASSERT(engine);
	WINPR_ASSERT(path);
	WINPR_ASSERT(file);

	char* fontBaseFile = GetCombinedPath(path, file);
	if (!fontBaseFile)
		goto cleanup;

	winpr_asprintf(&fontImageFile, &length, "%s." FILE_EXT, fontBaseFile);
	if (!fontImageFile)
		goto cleanup;

	winpr_asprintf(&fontDescriptorFile, &length, "%s.xml", fontBaseFile);
	if (!fontDescriptorFile)
		goto cleanup;

	if (!winpr_PathFileExists(fontImageFile))
		goto cleanup;

	if (!winpr_PathFileExists(fontDescriptorFile))
		goto cleanup;

	font = (rdtkFont*)calloc(1, sizeof(rdtkFont));

	if (!font)
		goto cleanup;

	font->engine = engine;
	font->image = winpr_image_new();

	if (!font->image)
		goto cleanup;

	{
		const int status = winpr_image_read(font->image, fontImageFile);
		if (status < 0)
			goto cleanup;
	}

	{
		const int status2 = rdtk_font_load_descriptor(font, fontDescriptorFile);
		if (status2 < 0)
			goto cleanup;
	}

	free(fontBaseFile);
	free(fontImageFile);
	free(fontDescriptorFile);
	return font;
cleanup:
	free(fontBaseFile);
	free(fontImageFile);
	free(fontDescriptorFile);

	rdtk_font_free(font);
	return NULL;
}

static rdtkFont* rdtk_embedded_font_new(rdtkEngine* engine, const uint8_t* imageData,
                                        size_t imageSize, const uint8_t* descriptorData,
                                        size_t descriptorSize)
{
	size_t size = 0;

	WINPR_ASSERT(engine);

	rdtkFont* font = (rdtkFont*)calloc(1, sizeof(rdtkFont));

	if (!font)
		return NULL;

	font->engine = engine;
	font->image = winpr_image_new();

	if (!font->image)
	{
		free(font);
		return NULL;
	}

	const int status = winpr_image_read_buffer(font->image, imageData, imageSize);
	if (status < 0)
	{
		winpr_image_free(font->image, TRUE);
		free(font);
		return NULL;
	}

	size = descriptorSize;
	char* buffer = (char*)calloc(size + 4, sizeof(char));

	if (!buffer)
		goto fail;

	CopyMemory(buffer, descriptorData, size);
	{
		const int status2 = rdtk_font_parse_descriptor_buffer(font, buffer, size);
		if (status2 < 0)
			goto fail;
	}

	return font;

fail:
	rdtk_font_free(font);
	return NULL;
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
	WINPR_ASSERT(engine);
	if (!engine->font)
	{
		const uint8_t* imageData = NULL;
		const uint8_t* descriptorData = NULL;
		const SSIZE_T imageSize =
		    rdtk_get_embedded_resource_file("source_serif_pro_regular_12." FILE_EXT, &imageData);
		const SSIZE_T descriptorSize =
		    rdtk_get_embedded_resource_file("source_serif_pro_regular_12.xml", &descriptorData);

		if ((imageSize < 0) || (descriptorSize < 0))
			return -1;

		engine->font = rdtk_embedded_font_new(engine, imageData, (size_t)imageSize, descriptorData,
		                                      (size_t)descriptorSize);
		if (!engine->font)
			return -1;
	}

	return 1;
}

int rdtk_font_engine_uninit(rdtkEngine* engine)
{
	WINPR_ASSERT(engine);
	if (engine->font)
	{
		rdtk_font_free(engine->font);
		engine->font = NULL;
	}

	return 1;
}
