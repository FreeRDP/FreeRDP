/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Compressed Bitmap
 *
 * Copyright 2011 Jay Sorg <jay.sorg@gmail.com>
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

#ifndef FREERDP_CODEC_BITMAP_H
#define FREERDP_CODEC_BITMAP_H

#include <freerdp/api.h>
#include <freerdp/types.h>

#include <freerdp/codec/color.h>

#include <winpr/crt.h>
#include <winpr/stream.h>

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_API SSIZE_T freerdp_bitmap_compress(const void* WINPR_RESTRICT srcData, UINT32 width,
	                                            UINT32 height, wStream* WINPR_RESTRICT s,
	                                            UINT32 bpp, UINT32 byte_limit, UINT32 start_line,
	                                            wStream* WINPR_RESTRICT temp_s, UINT32 e);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CODEC_BITMAP_H */
