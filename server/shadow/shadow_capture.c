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

#include <freerdp/config.h>

#include <winpr/crt.h>
#include <winpr/print.h>

#include <freerdp/log.h>

#include "shadow_surface.h"

#include "shadow_capture.h"

#define TAG SERVER_TAG("shadow")

int shadow_capture_align_clip_rect(RECTANGLE_16* rect, const RECTANGLE_16* clip)
{
	int dx = 0;
	int dy = 0;
	dx = (rect->left % 16);

	if (dx != 0)
	{
		rect->left -= dx;
		rect->right += dx;
	}

	dx = (rect->right % 16);

	if (dx != 0)
	{
		rect->right += (16 - dx);
	}

	dy = (rect->top % 16);

	if (dy != 0)
	{
		rect->top -= dy;
		rect->bottom += dy;
	}

	dy = (rect->bottom % 16);

	if (dy != 0)
	{
		rect->bottom += (16 - dy);
	}

	if (rect->left < clip->left)
		rect->left = clip->left;

	if (rect->top < clip->top)
		rect->top = clip->top;

	if (rect->right > clip->right)
		rect->right = clip->right;

	if (rect->bottom > clip->bottom)
		rect->bottom = clip->bottom;

	return 1;
}

int shadow_capture_compare(const BYTE* pData1, UINT32 nStep1, UINT32 nWidth, UINT32 nHeight,
                           const BYTE* pData2, UINT32 nStep2, RECTANGLE_16* rect)
{
	BOOL allEqual = TRUE;
	UINT32 tw = 0;
	const UINT32 nrow = (nHeight + 15) / 16;
	const UINT32 ncol = (nWidth + 15) / 16;
	UINT32 l = ncol + 1;
	UINT32 t = nrow + 1;
	UINT32 r = 0;
	UINT32 b = 0;
	const RECTANGLE_16 empty = { 0 };
	WINPR_ASSERT(rect);

	*rect = empty;

	for (UINT32 ty = 0; ty < nrow; ty++)
	{
		BOOL rowEqual = TRUE;
		UINT32 th = ((ty + 1) == nrow) ? (nHeight % 16) : 16;

		if (!th)
			th = 16;

		for (UINT32 tx = 0; tx < ncol; tx++)
		{
			BOOL equal = TRUE;
			tw = ((tx + 1) == ncol) ? (nWidth % 16) : 16;

			if (!tw)
				tw = 16;

			const BYTE* p1 = &pData1[(ty * 16 * nStep1) + (tx * 16 * 4)];
			const BYTE* p2 = &pData2[(ty * 16 * nStep2) + (tx * 16 * 4)];

			for (UINT32 k = 0; k < th; k++)
			{
				if (memcmp(p1, p2, tw * 4) != 0)
				{
					equal = FALSE;
					break;
				}

				p1 += nStep1;
				p2 += nStep2;
			}

			if (!equal)
			{
				rowEqual = FALSE;
				if (l > tx)
					l = tx;

				if (r < tx)
					r = tx;
			}
		}

		if (!rowEqual)
		{
			allEqual = FALSE;

			if (t > ty)
				t = ty;

			if (b < ty)
				b = ty;
		}
	}

	if (allEqual)
		return 0;

	WINPR_ASSERT(l * 16 <= UINT16_MAX);
	WINPR_ASSERT(t * 16 <= UINT16_MAX);
	WINPR_ASSERT((r + 1) * 16 <= UINT16_MAX);
	WINPR_ASSERT((b + 1) * 16 <= UINT16_MAX);
	rect->left = (UINT16)l * 16;
	rect->top = (UINT16)t * 16;
	rect->right = (UINT16)(r + 1) * 16;
	rect->bottom = (UINT16)(b + 1) * 16;

	WINPR_ASSERT(nWidth <= UINT16_MAX);
	if (rect->right > nWidth)
		rect->right = (UINT16)nWidth;

	WINPR_ASSERT(nHeight <= UINT16_MAX);
	if (rect->bottom > nHeight)
		rect->bottom = (UINT16)nHeight;

	return 1;
}

static BOOL color_equal(UINT32 colorA, UINT32 formatA, UINT32 colorB, UINT32 formatB)
{
	BYTE ar = 0;
	BYTE ag = 0;
	BYTE ab = 0;
	BYTE aa = 0;
	BYTE br = 0;
	BYTE bg = 0;
	BYTE bb = 0;
	BYTE ba = 0;
	FreeRDPSplitColor(colorA, formatA, &ar, &ag, &ab, &aa, NULL);
	FreeRDPSplitColor(colorB, formatB, &br, &bg, &bb, &ba, NULL);

	if (ar != br)
		return FALSE;
	if (ag != bg)
		return FALSE;
	if (ab != bb)
		return FALSE;
	if (aa != ba)
		return FALSE;
	return TRUE;
}

static BOOL pixel_equal(const BYTE* a, UINT32 formatA, const BYTE* b, UINT32 formatB, size_t count)
{
	const size_t bppA = FreeRDPGetBytesPerPixel(formatA);
	const size_t bppB = FreeRDPGetBytesPerPixel(formatB);

	for (size_t x = 0; x < count; x++)
	{
		const UINT32 colorA = FreeRDPReadColor(&a[bppA * x], formatA);
		const UINT32 colorB = FreeRDPReadColor(&b[bppB * x], formatB);
		if (!color_equal(colorA, formatA, colorB, formatB))
			return FALSE;
	}

	return TRUE;
}

int shadow_capture_compare_with_format(const BYTE* pData1, UINT32 format1, UINT32 nStep1,
                                       UINT32 nWidth, UINT32 nHeight, const BYTE* pData2,
                                       UINT32 format2, UINT32 nStep2, RECTANGLE_16* rect)
{
	if (format1 == format2)
		return shadow_capture_compare(pData1, nStep1, nWidth, nHeight, pData2, nStep2, rect);

	BOOL allEqual = TRUE;
	UINT32 tw = 0;
	const UINT32 nrow = (nHeight + 15) / 16;
	const UINT32 ncol = (nWidth + 15) / 16;
	UINT32 l = ncol + 1;
	UINT32 t = nrow + 1;
	UINT32 r = 0;
	UINT32 b = 0;
	const size_t bppA = FreeRDPGetBytesPerPixel(format1);
	const size_t bppB = FreeRDPGetBytesPerPixel(format2);
	const RECTANGLE_16 empty = { 0 };
	WINPR_ASSERT(rect);

	*rect = empty;

	for (size_t ty = 0; ty < nrow; ty++)
	{
		BOOL rowEqual = TRUE;
		size_t th = ((ty + 1) == nrow) ? (nHeight % 16) : 16;

		if (!th)
			th = 16;

		for (size_t tx = 0; tx < ncol; tx++)
		{
			BOOL equal = TRUE;
			tw = ((tx + 1) == ncol) ? (nWidth % 16) : 16;

			if (!tw)
				tw = 16;

			const BYTE* p1 = &pData1[(ty * 16 * nStep1) + (tx * 16ull * bppA)];
			const BYTE* p2 = &pData2[(ty * 16 * nStep2) + (tx * 16ull * bppB)];

			for (size_t k = 0; k < th; k++)
			{
				if (!pixel_equal(p1, format1, p2, format2, tw))
				{
					equal = FALSE;
					break;
				}

				p1 += nStep1;
				p2 += nStep2;
			}

			if (!equal)
			{
				rowEqual = FALSE;
				if (l > tx)
					l = tx;

				if (r < tx)
					r = tx;
			}
		}

		if (!rowEqual)
		{
			allEqual = FALSE;

			if (t > ty)
				t = ty;

			if (b < ty)
				b = ty;
		}
	}

	if (allEqual)
		return 0;

	WINPR_ASSERT(l * 16 <= UINT16_MAX);
	WINPR_ASSERT(t * 16 <= UINT16_MAX);
	WINPR_ASSERT((r + 1) * 16 <= UINT16_MAX);
	WINPR_ASSERT((b + 1) * 16 <= UINT16_MAX);
	rect->left = (UINT16)l * 16;
	rect->top = (UINT16)t * 16;
	rect->right = (UINT16)(r + 1) * 16;
	rect->bottom = (UINT16)(b + 1) * 16;

	WINPR_ASSERT(nWidth <= UINT16_MAX);
	if (rect->right > nWidth)
		rect->right = (UINT16)nWidth;

	WINPR_ASSERT(nHeight <= UINT16_MAX);
	if (rect->bottom > nHeight)
		rect->bottom = (UINT16)nHeight;

	return 1;
}

rdpShadowCapture* shadow_capture_new(rdpShadowServer* server)
{
	WINPR_ASSERT(server);

	rdpShadowCapture* capture = (rdpShadowCapture*)calloc(1, sizeof(rdpShadowCapture));

	if (!capture)
		return NULL;

	capture->server = server;

	if (!InitializeCriticalSectionAndSpinCount(&(capture->lock), 4000))
	{
		WINPR_PRAGMA_DIAG_PUSH
		WINPR_PRAGMA_DIAG_IGNORED_MISMATCHED_DEALLOC
		shadow_capture_free(capture);
		WINPR_PRAGMA_DIAG_POP
		return NULL;
	}

	return capture;
}

void shadow_capture_free(rdpShadowCapture* capture)
{
	if (!capture)
		return;

	DeleteCriticalSection(&(capture->lock));
	free(capture);
}
