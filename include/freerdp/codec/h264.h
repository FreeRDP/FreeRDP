/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * H.264 Bitmap Compression
 *
 * Copyright 2014 Mike McDonald <Mike.McDonald@software.dell.com>
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

#ifndef FREERDP_CODEC_H264_H
#define FREERDP_CODEC_H264_H

#include <freerdp/api.h>
#include <freerdp/types.h>

#ifdef WITH_LIBAVCODEC
#ifdef WITH_OPENH264
#undef WITH_OPENH264
#endif
#endif

#ifdef WITH_OPENH264
#include "wels/codec_def.h"
#include "wels/codec_api.h"
#endif

#ifdef WITH_LIBAVCODEC
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#endif

struct _H264_CONTEXT
{
	BOOL Compressor;

	BYTE* data;
	UINT32 size;
	UINT32 width;
	UINT32 height;
	int scanline;

#ifdef WITH_OPENH264
	ISVCDecoder* pDecoder;
#endif

#ifdef WITH_LIBAVCODEC
	AVCodec* codec;
	AVCodecContext* codecContext;
	AVCodecParserContext* codecParser;
	AVFrame* videoFrame;
#endif
};
typedef struct _H264_CONTEXT H264_CONTEXT;

#ifdef __cplusplus
extern "C" {
#endif

FREERDP_API int h264_compress(H264_CONTEXT* h264, BYTE* pSrcData, UINT32 SrcSize, BYTE** ppDstData, UINT32* pDstSize);

FREERDP_API int h264_decompress(H264_CONTEXT* h264, BYTE* pSrcData, UINT32 SrcSize,
		BYTE** ppDstData, DWORD DstFormat, int nDstStep, int nXDst, int nYDst, int nWidth, int nHeight);

FREERDP_API void h264_context_reset(H264_CONTEXT* h264);

FREERDP_API H264_CONTEXT* h264_context_new(BOOL Compressor);
FREERDP_API void h264_context_free(H264_CONTEXT* h264);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CODEC_H264_H */

