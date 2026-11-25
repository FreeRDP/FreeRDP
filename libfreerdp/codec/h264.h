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
		WINPR_ATTR_NODISCARD pfnH264SubsystemInit Init;
		pfnH264SubsystemUninit Uninit;
		WINPR_ATTR_NODISCARD pfnH264SubsystemDecompress Decompress;
		WINPR_ATTR_NODISCARD pfnH264SubsystemCompress Compress;
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
		UINT32 hwAccel;
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

		UINT32 YUVWidth;
		UINT32 YUVHeight;

		BOOL hevc;    /**< decode H.265/HEVC instead of H.264 */
		BOOL hevc444; /**< the HEVC stream is 4:4:4, needs a subsystem with SupportsHEVC444 */
		BOOL yuv444;  /**< set by the subsystem: pYUVData holds a full resolution 4:4:4 picture
		               *   (Y, Cb, Cr) instead of 4:2:0 */
	};

	WINPR_ATTR_NODISCARD
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
#ifdef WITH_APPLE_VIDEOTOOLBOX
	extern const H264_CONTEXT_SUBSYSTEM g_Subsystem_VideoToolbox;
#endif
#ifdef WITH_VIDEO_FFMPEG
	extern const H264_CONTEXT_SUBSYSTEM g_Subsystem_libavcodec;
#endif
#ifdef WITH_H264_WEBCODECS
	extern const H264_CONTEXT_SUBSYSTEM g_Subsystem_WebCodecs;
#endif

	WINPR_ATTR_NODISCARD
	FREERDP_LOCAL INT32 h26X_decompress(H264_CONTEXT* h264, const BYTE* pSrcData, UINT32 SrcSize,
	                                    BYTE* pDstData, DWORD DstFormat, UINT32 nDstStep,
	                                    WINPR_ATTR_UNUSED UINT32 nDstWidth,
	                                    WINPR_ATTR_UNUSED UINT32 nDstHeight,
	                                    const RECTANGLE_16* regionRects, UINT32 numRegionRects);

	WINPR_ATTR_MALLOC(h264_context_free, 1)
	H264_CONTEXT* h264_context_new_ex(BOOL Compressor, BOOL hevc, BOOL hevc444);

	WINPR_ATTR_NODISCARD
	BOOL avc444_ensure_buffer(H264_CONTEXT* h264, DWORD nDstHeight);

	WINPR_ATTR_NODISCARD
	BOOL allocate_h264_metablock(UINT32 QP, RECTANGLE_16* rectangles, RDPGFX_H264_METABLOCK* meta,
	                             size_t count);

	WINPR_ATTR_NODISCARD
	BOOL detect_changes(BOOL firstFrameDone, UINT32 QP, const RECTANGLE_16* regionRect,
	                    BYTE* pYUVData[3], BYTE* pOldYUVData[3], UINT32 const iStride[3],
	                    UINT32 v2AuxWidth, RDPGFX_H264_METABLOCK* meta);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_LIB_CODEC_H264_H */
