/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RemoteFX Codec Library - Decode
 *
 * Copyright 2018 Armin Novak <anovak@thincast.com>
 * Copyright 2018 Thincast Technologies GmbH
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

#ifndef FREERDP_LIB_CODEC_H264_H
#define FREERDP_LIB_CODEC_H264_H

#include <freerdp/api.h>
#include <freerdp/config.h>
#include <freerdp/codec/h264.h>

#ifdef __cplusplus
extern "C"
{
#endif

	typedef BOOL (*pfnH264SubsystemInit)(H264_CONTEXT* h264);
	typedef void (*pfnH264SubsystemUninit)(H264_CONTEXT* h264);

	typedef int (*pfnH264SubsystemDecompress)(H264_CONTEXT* h264, const BYTE* pSrcData,
	                                          UINT32 SrcSize);
	typedef int (*pfnH264SubsystemCompress)(H264_CONTEXT* h264, const BYTE** pSrcYuv,
	                                        const UINT32* pStride, BYTE** ppDstData,
	                                        UINT32* pDstSize);

	struct S_H264_CONTEXT_SUBSYSTEM
	{
		const char* name;
		pfnH264SubsystemInit Init;
		pfnH264SubsystemUninit Uninit;
		pfnH264SubsystemDecompress Decompress;
		pfnH264SubsystemCompress Compress;
	};

	FREERDP_LOCAL BOOL avc420_ensure_buffer(H264_CONTEXT* h264, UINT32 stride, UINT32 width,
	                                        UINT32 height);

#ifdef WITH_MEDIACODEC
	extern const H264_CONTEXT_SUBSYSTEM g_Subsystem_mediacodec;
#endif
#if defined(_WIN32) && defined(WITH_MEDIA_FOUNDATION)
	extern const H264_CONTEXT_SUBSYSTEM g_Subsystem_MF;
#endif
#ifdef WITH_OPENH264
	extern const H264_CONTEXT_SUBSYSTEM g_Subsystem_OpenH264;
#endif
#ifdef WITH_VIDEO_FFMPEG
	extern const H264_CONTEXT_SUBSYSTEM g_Subsystem_libavcodec;
#endif

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_LIB_CODEC_H264_H */
