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

#include <winpr/crt.h>
#include <winpr/print.h>

#include <freerdp/log.h>

#include "shadow_surface.h"

#include "shadow_capture.h"

#define TAG SERVER_TAG("shadow")

int shadow_capture_align_clip_rect(RECTANGLE_16* rect, RECTANGLE_16* clip)
{
	int dx, dy;

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

int shadow_capture_compare(BYTE* pData1, int nStep1, int nWidth, int nHeight, BYTE* pData2, int nStep2, RECTANGLE_16* rect)
{
	BOOL equal;
	BOOL allEqual;
	int tw, th;
	int tx, ty, k;
	int nrow, ncol;
	int l, t, r, b;
	BYTE *p1, *p2;
	BOOL rows[1024];
#ifdef WITH_DEBUG_SHADOW_CAPTURE
	BOOL cols[1024];
#endif

	allEqual = TRUE;
	ZeroMemory(rect, sizeof(RECTANGLE_16));
	FillMemory(rows, sizeof(rows), 0xFF);
#ifdef WITH_DEBUG_SHADOW_CAPTURE
	FillMemory(cols, sizeof(cols), 0xFF);
#endif

	nrow = (nHeight + 15) / 16;
	ncol = (nWidth + 15) / 16;

	l = ncol + 1;
	r = -1;

	t = nrow + 1;
	b = -1;

	for (ty = 0; ty < nrow; ty++)
	{
		th = ((ty + 1) == nrow) ? (nHeight % 16) : 16;

		if (!th)
			th = 16;

		for (tx = 0; tx < ncol; tx++)
		{
			equal = TRUE;

			tw = ((tx + 1) == ncol) ? (nWidth % 16) : 16;

			if (!tw)
				tw = 16;

			p1 = &pData1[(ty * 16 * nStep1) + (tx * 16 * 4)];
			p2 = &pData2[(ty * 16 * nStep2) + (tx * 16 * 4)];

			for (k = 0; k < th; k++)
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
				rows[ty] = FALSE;
#ifdef WITH_DEBUG_SHADOW_CAPTURE
				cols[tx] = FALSE;
#endif

				if (l > tx)
					l = tx;

				if (r < tx)
					r = tx;
			}
		}

		if (!rows[ty])
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

	rect->left = l * 16;
	rect->top = t * 16;
	rect->right = (r + 1) * 16;
	rect->bottom = (b + 1) * 16;

	if (rect->right > nWidth)
		rect->right = nWidth;

	if (rect->bottom > nHeight)
		rect->bottom = nHeight;

#ifdef WITH_DEBUG_SHADOW_CAPTURE
	char *col_str = calloc(ncol + 1, sizeof(char));
	if (!col_str)
	{
		WLog_ERR(TAG, "calloc failed!");
		return 1;
	}

	for (tx = 0; tx < ncol; tx++)
		sprintf(&col_str[tx], "-");
	WLog_INFO(TAG, "%s", col_str);

	for (tx = 0; tx < ncol; tx++)
		sprintf(&col_str[tx], "%c", cols[tx] ? 'O' : 'X');
	WLog_INFO(TAG, "%s", col_str);

	for (tx = 0; tx < ncol; tx++)
		sprintf(&col_str[tx], "-");
	WLog_INFO(TAG, "%s", col_str);

	for (ty = 0; ty < nrow; ty++)
	{
		for (tx = 0; tx < ncol; tx++)
			sprintf(&col_str[tx], "%c", cols[tx] ? 'O' : 'X');
		WLog_INFO(TAG, "%s", col_str);
		WLog_INFO(TAG, "|%s|", rows[ty] ? "O" : "X");
	}

	WLog_INFO(TAG, "left: %d top: %d right: %d bottom: %d ncol: %d nrow: %d",
			l, t, r, b, ncol, nrow);
	free(col_str);
#endif

	return 1;
}

rdpShadowCapture* shadow_capture_new(rdpShadowServer* server)
{
	rdpShadowCapture* capture;

	capture = (rdpShadowCapture*) calloc(1, sizeof(rdpShadowCapture));

	if (!capture)
		return NULL;

	capture->server = server;

	if (!InitializeCriticalSectionAndSpinCount(&(capture->lock), 4000))
	{
		free(capture);
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

