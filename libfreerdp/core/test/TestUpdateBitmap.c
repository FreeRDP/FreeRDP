/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Bitmap update (TS_BITMAP_DATA) parsing unit test
 *
 * Copyright 2026 Cong Zhang
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

#include <winpr/crt.h>
#include <winpr/stream.h>

#include <freerdp/freerdp.h>

#include "../update.h"

/* One uncompressed 32 bpp rectangle; returns whether it was accepted */
static BOOL read_rectangle(rdpUpdate* update, UINT16 left, UINT16 top, UINT16 right, UINT16 bottom,
                           UINT16 width, UINT16 height)
{
	const size_t length = 4ull * width * height;
	wStream* s = Stream_New(nullptr, 2 + 18 + length);
	if (!s)
		return FALSE;

	Stream_Write_UINT16(s, 1); /* numberRectangles */
	Stream_Write_UINT16(s, left);
	Stream_Write_UINT16(s, top);
	Stream_Write_UINT16(s, right);
	Stream_Write_UINT16(s, bottom);
	Stream_Write_UINT16(s, width);
	Stream_Write_UINT16(s, height);
	Stream_Write_UINT16(s, 32); /* bitsPerPixel */
	Stream_Write_UINT16(s, 0);  /* flags: uncompressed */
	Stream_Write_UINT16(s, (UINT16)length);
	Stream_Zero(s, length);
	Stream_SealLength(s);
	Stream_ResetPosition(s);

	BITMAP_UPDATE* bitmapUpdate = update_read_bitmap_update(update, s);
	const BOOL accepted = (bitmapUpdate != nullptr);
	free_bitmap_update(update->context, bitmapUpdate);
	Stream_Free(s, TRUE);
	return accepted;
}

typedef struct
{
	const char* what;
	UINT16 left;
	UINT16 top;
	UINT16 right;
	UINT16 bottom;
	UINT16 width;
	UINT16 height;
	BOOL accepted;
} test_case;

int TestUpdateBitmap(WINPR_ATTR_UNUSED int argc, WINPR_ATTR_UNUSED char* argv[])
{
	/* destRight and destBottom are inclusive */
	const test_case cases[] = {
		{ "bitmap as wide as the rectangle", 0, 0, 11, 3, 12, 4, TRUE },
		/* xrdp: a 9 pixel wide rectangle, the bitmap padded to 12 */
		{ "bitmap wider than the rectangle (padded)", 0, 0, 8, 3, 12, 4, TRUE },
		{ "bitmap taller than the rectangle", 0, 0, 11, 2, 12, 4, TRUE },
		{ "rectangle wider than the bitmap", 0, 0, 12, 3, 12, 4, FALSE },
		{ "rectangle taller than the bitmap", 0, 0, 11, 4, 12, 4, FALSE },
		{ "inverted rectangle", 9, 0, 8, 3, 12, 4, FALSE },
		{ "inverted rectangle, vertically", 0, 4, 11, 3, 12, 4, FALSE },
	};
	int rc = -1;

	freerdp* instance = freerdp_new();
	if (!instance || !freerdp_context_new(instance))
		goto fail;

	for (size_t x = 0; x < ARRAYSIZE(cases); x++)
	{
		const test_case* c = &cases[x];
		const BOOL accepted = read_rectangle(instance->context->update, c->left, c->top, c->right,
		                                     c->bottom, c->width, c->height);
		if (accepted != c->accepted)
		{
			(void)fprintf(stderr, "%s: %s, expected %s\n", c->what,
			              accepted ? "accepted" : "rejected",
			              c->accepted ? "accepted" : "rejected");
			goto fail;
		}
	}

	rc = 0;
fail:
	if (instance)
		freerdp_context_free(instance);
	freerdp_free(instance);
	return rc;
}
