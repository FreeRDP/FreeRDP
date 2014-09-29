/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#include "shadow.h"

#include "shadow_font.h"

#define TEST_FONT_IMAGE		"source_serif_pro_regular_12.png"
#define TEST_FONT_DESCRIPTOR	"source_serif_pro_regular_12.xml"

char* shadow_font_load_descriptor_file(const char* filename, int* pSize)
{
	BYTE* buffer;
	FILE* fp = NULL;
	size_t readSize;
	size_t fileSize;

	fp = fopen(filename, "r");

	if (!fp)
		return NULL;

	fseek(fp, 0, SEEK_END);
	fileSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

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

int shadow_font_convert_descriptor_code_to_utf8(const char* str, BYTE* utf8)
{
	int len = strlen(str);

	*((UINT32*) utf8) = 0;

	if (len == 1)
	{
		if ((str[0] > 31) && (str[0] < 127))
		{
			utf8[0] = str[0];
		}
	}

	return 1;
}

int shadow_font_load_descriptor(rdpShadowFont* font, const char* filename)
{
	char* p;
	char* q;
	char* r;
	char* beg;
	char* end;
	char* tok[4];
	int index;
	int count;
	int size;
	char* buffer;
	rdpShadowGlyph* glyph;

	buffer = shadow_font_load_descriptor_file(filename, &size);

	if (!buffer)
		return -1;

	p = strstr(buffer, "<?xml version=\"1.0\" encoding=\"utf-8\"?>");

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
	font->size = atoi(p);
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
	font->height = atoi(p);
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

	printf("size: %d family: %s height: %d style: %s\n",
			font->size, font->family, font->height, font->style);

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
	font->glyphs = (rdpShadowGlyph*) calloc(font->glyphCount, sizeof(rdpShadowGlyph));

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
		glyph->width = atoi(p);
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

		glyph->offsetX = atoi(tok[0]);
		glyph->offsetY = atoi(tok[1]);

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

		glyph->rectX = atoi(tok[0]);
		glyph->rectY = atoi(tok[1]);
		glyph->rectWidth = atoi(tok[2]);
		glyph->rectHeight = atoi(tok[3]);

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
		shadow_font_convert_descriptor_code_to_utf8(p, glyph->code);
		*q = '"';

		p = q + 1;

		/* finish parsing glyph */

		p = r + sizeof("/>");
		*r = '/';

		index++;
	}

	return 1;
}

rdpShadowFont* shadow_font_new(const char* filename)
{
	int status;
	rdpShadowFont* font;

	font = (rdpShadowFont*) calloc(1, sizeof(rdpShadowFont));

	if (!font)
		return NULL;

	font->image = winpr_image_new();

	if (!font->image)
		return NULL;

	status = winpr_image_read(font->image, TEST_FONT_IMAGE);

	if (status < 0)
		return NULL;

	status = shadow_font_load_descriptor(font, TEST_FONT_DESCRIPTOR);

	return font;
}

void shadow_font_free(rdpShadowFont* font)
{
	if (!font)
		return;

	free(font);
}
