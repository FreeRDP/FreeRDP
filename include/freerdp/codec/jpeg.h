/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Compressed Bitmap
 *
 * Copyright 2012 Jay Sorg <jay.sorg@gmail.com>
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

#ifndef FREERDP_CODEC_JPEG_H
#define FREERDP_CODEC_JPEG_H

#include <freerdp/api.h>
#include <freerdp/types.h>

FREERDP_API BOOL jpeg_decompress(BYTE* input, BYTE* output, int width, int height, int size, int bpp);

#endif /* FREERDP_CODEC_JPEG_H */
