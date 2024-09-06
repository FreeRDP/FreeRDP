/*
 * WinPR: Windows Portable Runtime
 * Image Utils
 *
 * Copyright 2024 Armin Novak <anovak@thincast.com>
 * Copyright 2024 Thincast Technologies GmbH
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

#ifndef LIBWINPR_UTILS_IMAGE_H
#define LIBWINPR_UTILS_IMAGE_H

#include <winpr/wtypes.h>
#include <winpr/stream.h>
#include <winpr/image.h>

BOOL readBitmapFileHeader(wStream* s, WINPR_BITMAP_FILE_HEADER* bf);
BOOL writeBitmapFileHeader(wStream* s, const WINPR_BITMAP_FILE_HEADER* bf);

BOOL readBitmapInfoHeader(wStream* s, WINPR_BITMAP_INFO_HEADER* bi, size_t* poffset);
BOOL writeBitmapInfoHeader(wStream* s, const WINPR_BITMAP_INFO_HEADER* bi);

#endif /* LIBWINPR_UTILS_IMAGE_H */
