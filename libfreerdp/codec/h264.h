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

	typedef int (*pfnH264SubsystemDecompress)(H264_CONTEXT* WINPR_RESTRICT h264,
	                                          const BYTE* WINPR_RESTRICT pSrcData, UINT32 SrcSize);
	typedef int (*pfnH264SubsystemCompress)(H264_CONTEXT* WINPR_RESTRICT h264,
	                                        const BYTE** WINPR_RESTRICT pSrcYuv,
	                                        const UINT32* WINPR_RESTRICT pStride,
	                                        BYTE** WINPR_RESTRICT ppDstData,
	                                        UINT32* WINPR_RESTRICT pDstSize);

	struct S_H264_CONTEXT_SUBSYSTEM
	{
		const char* name;
		pfnH264SubsystemInit Init;
		pfnH264SubsystemUninit Uninit;
		pfnH264SubsystemDecompress Decompress;
		pfnH264SubsystemCompress Compress;
	};

	struct S_H264_CONTEXT
	{
		BOOL Compressor;

		UINT32 width;
		UINT32 height;

		H264_RATECONTROL_MODE RateControlMode;
		UINT32 BitRate;
		UINT32 FrameRate;
		UINT32 QP;
		UINT32 UsageType;
		UINT32 NumberOfThreads;

		UINT32 iStride[3];
		BYTE* pOldYUVData[3];
		BYTE* pYUVData[3];

		UINT32 iYUV444Size[3];
		UINT32 iYUV444Stride[3];
		BYTE* pOldYUV444Data[3];
		BYTE* pYUV444Data[3];

		UINT32 numSystemData;
		void* pSystemData;
		const H264_CONTEXT_SUBSYSTEM* subsystem;
		YUV_CONTEXT* yuv;

		BOOL encodingBuffer;
		BOOL firstLumaFrameDone;
		BOOL firstChromaFrameDone;

		void* lumaData;
		wLog* log;
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
