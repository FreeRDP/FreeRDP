/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * YUV decoder
 *
 * Copyright 2017 David Fort <contact@hardening-consulting.com>
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

#ifndef FREERDP_CODEC_YUV_H
#define FREERDP_CODEC_YUV_H

typedef struct _YUV_CONTEXT YUV_CONTEXT;

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/freerdp.h>
#include <freerdp/constants.h>


#ifdef __cplusplus
extern "C" {
#endif


FREERDP_API BOOL yuv_context_decode(YUV_CONTEXT* context, const BYTE* pYUVData[3], UINT32 iStride[3],
		DWORD DstFormat, BYTE *dest, UINT32 nDstStep);

FREERDP_API void yuv_context_reset(YUV_CONTEXT* context, UINT32 width, UINT32 height);

FREERDP_API YUV_CONTEXT* yuv_context_new(BOOL encoder);
FREERDP_API void yuv_context_free(YUV_CONTEXT* context);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CODEC_YUV_H */
