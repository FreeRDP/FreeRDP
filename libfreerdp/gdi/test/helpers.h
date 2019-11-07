/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * GDI Library Tests
 *
 * Copyright 2016 Armin Novak <armin.novak@thincast.com>
 * Copyright 2016 Thincast Technologies GmbH
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef GDI_TEST_HELPERS_H
#define GDI_TEST_HELPERS_H

#include <freerdp/codec/color.h>
#include <freerdp/gdi/bitmap.h>

HGDI_BITMAP test_convert_to_bitmap(const BYTE* src, UINT32 SrcFormat, UINT32 SrcStride, UINT32 xSrc,
                                   UINT32 ySrc, UINT32 DstFormat, UINT32 DstStride, UINT32 xDst,
                                   UINT32 yDst, UINT32 nWidth, UINT32 nHeight,

                                   const gdiPalette* hPalette);

void test_dump_bitmap(HGDI_BITMAP hBmp, const char* name);
BOOL test_assert_bitmaps_equal(HGDI_BITMAP hBmpActual, HGDI_BITMAP hBmpExpected, const char* name,
                               const gdiPalette* palette);

#endif /* __GDI_CORE_H */
