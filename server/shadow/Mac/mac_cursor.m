/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#import <AppKit/AppKit.h>

#include <math.h>

#include <winpr/assert.h>

#include "mac_cursor.h"

BOOL mac_cursor_get(macCursorImage *image, UINT32 maxSize)
{
	WINPR_ASSERT(image);
	memset(image, 0, sizeof(*image));

	@autoreleasepool
	{
		/* the cursor of whatever application is in front, not only ours */
		NSCursor *cursor = [NSCursor currentSystemCursor];
		NSImage *picture = cursor.image;
		if (!picture)
			return FALSE;

		const NSSize size = picture.size;
		const NSPoint hot = cursor.hotSpot;
		const double width = ceil(size.width);
		const double height = ceil(size.height);
		if ((width < 1.0) || (height < 1.0) || (width > maxSize) || (height > maxSize))
			return FALSE;

		/* the representation drawn at the size in points: on a Retina display
		 * that is the 2x one, scaled down like the screen itself */
		NSRect rect = NSMakeRect(0, 0, width, height);
		CGImageRef cgImage = [picture CGImageForProposedRect:&rect context:nil hints:nil];
		if (!cgImage)
			return FALSE;

		const size_t w = (size_t)width;
		const size_t h = (size_t)height;
		BYTE *pixels = calloc(w * h, 4);
		if (!pixels)
			return FALSE;

		/* premultiplied ARGB in a little-endian word: B, G, R, A in memory */
		CGColorSpaceRef space = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
		CGContextRef context =
		    CGBitmapContextCreate(pixels, w, h, 8, w * 4, space,
		                          kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);
		CGColorSpaceRelease(space);
		if (!context)
		{
			free(pixels);
			return FALSE;
		}
		CGContextDrawImage(context, CGRectMake(0, 0, width, height), cgImage);
		CGContextRelease(context);

		image->width = (UINT32)w;
		image->height = (UINT32)h;
		image->xHot = (UINT32)MIN(MAX(hot.x, 0.0), width - 1.0);
		image->yHot = (UINT32)MIN(MAX(hot.y, 0.0), height - 1.0);
		image->pixels = pixels;
	}
	return TRUE;
}

void mac_cursor_image_free(macCursorImage *image)
{
	if (!image)
		return;
	free(image->pixels);
	image->pixels = NULL;
}
