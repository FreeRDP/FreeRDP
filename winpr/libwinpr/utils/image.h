/**
 * WinPR: Windows Portable Runtime
 * Image Utils
 *
 * Copyright 2024 Armin Novak <armin.novak@thincast.com>
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

#ifndef WINPR_UTILS_IMAGE_H
#define WINPR_UTILS_IMAGE_H

#include <winpr/winpr.h>
#include <winpr/wtypes.h>

WINPR_LOCAL void* winpr_convert_to_png(const void* data, size_t size, UINT32 width, UINT32 height,
                                       UINT32 stride, UINT32 bpp, UINT32* pSize);
WINPR_LOCAL SSIZE_T winpr_convert_from_png(const char* comp_data, size_t comp_data_bytes,
                                           UINT32* width, UINT32* height, UINT32* bpp,
                                           char** ppdecomp_data);

WINPR_LOCAL void* winpr_convert_to_webp(const void* data, size_t size, UINT32 width, UINT32 height,
                                        UINT32 stride, UINT32 bpp, UINT32* pSize);
WINPR_LOCAL SSIZE_T winpr_convert_from_webp(const char* comp_data, size_t comp_data_bytes,
                                            UINT32* width, UINT32* height, UINT32* bpp,
                                            char** ppdecomp_data);

WINPR_LOCAL void* winpr_convert_to_jpeg(const void* data, size_t size, UINT32 width, UINT32 height,
                                        UINT32 stride, UINT32 bpp, UINT32* pSize);
WINPR_LOCAL SSIZE_T winpr_convert_from_jpeg(const char* comp_data, size_t comp_data_bytes,
                                            UINT32* width, UINT32* height, UINT32* bpp,
                                            char** ppdecomp_data);

#endif
