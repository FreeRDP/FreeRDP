/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * H.264 Bitmap Compression — VideoToolbox Backend (iOS/macOS)
 *
 * Copyright 2025 rdp-ios-bridge contributors
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

#include <freerdp/config.h>

#include <winpr/assert.h>
#include <winpr/sysinfo.h>
#include <freerdp/log.h>
#include <freerdp/codec/h264.h>

#include "h264.h"

#include <VideoToolbox/VideoToolbox.h>
#include <CoreVideo/CoreVideo.h>
#include <CoreMedia/CoreMedia.h>
#include <CoreFoundation/CoreFoundation.h>

#define TAG FREERDP_TAG("codec.h264.videotoolbox")

typedef struct
{
	VTCompressionSessionRef session;
	CFMutableDataRef outputData;
	uint32_t encodedWidth;
	uint32_t encodedHeight;
	uint64_t frameCount;
	uint8_t *nv12UVBuffer;      /* Interleaved UV plane for I420→NV12 conversion */
	size_t nv12UVBufferSize;
} H264_CONTEXT_VIDEOTOOLBOX;

/**
 * Compression output callback — converts AVCC format to Annex B.
 *
 * VideoToolbox outputs NALUs in AVCC format (4-byte length prefix).
 * RDP/FreeRDP expects Annex B format (0x00 0x00 0x00 0x01 start codes).
 * We also prepend SPS/PPS from the format description on keyframes.
 */
static void videotoolbox_compress_callback(void *outputCallbackRefCon,
                                           void *sourceFrameRefCon,
                                           OSStatus status,
                                           VTEncodeInfoFlags infoFlags,
                                           CMSampleBufferRef sampleBuffer)
{
	H264_CONTEXT_VIDEOTOOLBOX *sys = (H264_CONTEXT_VIDEOTOOLBOX *)outputCallbackRefCon;
	(void)sourceFrameRefCon;
	(void)infoFlags;

	if (status != noErr || !sampleBuffer)
		return;

	/* Reset output buffer for this frame */
	CFDataSetLength(sys->outputData, 0);

	/* On keyframes, prepend SPS and PPS */
	CFArrayRef attachments =
	    CMSampleBufferGetSampleAttachmentsArray(sampleBuffer, false);
	BOOL isKeyframe = FALSE;
	if (attachments && CFArrayGetCount(attachments) > 0)
	{
		CFDictionaryRef attachment =
		    (CFDictionaryRef)CFArrayGetValueAtIndex(attachments, 0);
		CFBooleanRef notSync = NULL;
		if (CFDictionaryGetValueIfPresent(attachment,
		                                  kCMSampleAttachmentKey_NotSync,
		                                  (const void **)&notSync))
		{
			isKeyframe = !CFBooleanGetValue(notSync);
		}
		else
		{
			isKeyframe = TRUE;
		}
	}

	if (isKeyframe)
	{
		CMFormatDescriptionRef fmt = CMSampleBufferGetFormatDescription(sampleBuffer);
		if (fmt)
		{
			static const uint8_t startCode[4] = { 0, 0, 0, 1 };
			size_t sparamCount = 0;
			size_t pparamCount = 0;

			CMVideoFormatDescriptionGetH264ParameterSetAtIndex(
			    fmt, 0, NULL, NULL, &sparamCount, NULL);

			for (size_t i = 0; i < sparamCount; i++)
			{
				const uint8_t *params = NULL;
				size_t paramSize = 0;
				OSStatus paramStatus =
				    CMVideoFormatDescriptionGetH264ParameterSetAtIndex(
				        fmt, i, &params, &paramSize, NULL, NULL);
				if (paramStatus == noErr && params && paramSize > 0)
				{
					CFDataAppendBytes(sys->outputData, startCode, 4);
					CFDataAppendBytes(sys->outputData, params, (CFIndex)paramSize);
				}
			}

			/* PPS parameter sets start after SPS count. On most encoders
			 * there is exactly 1 SPS + 1 PPS, but iterate to be safe. */
			CMVideoFormatDescriptionGetH264ParameterSetAtIndex(
			    fmt, sparamCount, NULL, NULL, &pparamCount, NULL);

			/* The above call may fail if pparamCount trick doesn't work.
			 * In that case pparamCount stays 0. Iterate SPS+PPS together
			 * instead using the total from the initial call. */
		}
	}

	/* Convert AVCC NALUs to Annex B */
	CMBlockBufferRef blockBuffer = CMSampleBufferGetDataBuffer(sampleBuffer);
	if (!blockBuffer)
		return;

	size_t totalLength = 0;
	char *dataPointer = NULL;
	OSStatus blockStatus = CMBlockBufferGetDataPointer(
	    blockBuffer, 0, NULL, &totalLength, &dataPointer);

	if (blockStatus != noErr || !dataPointer)
		return;

	/* Determine NALU length header size (usually 4) */
	CMFormatDescriptionRef fmt2 = CMSampleBufferGetFormatDescription(sampleBuffer);
	int naluLengthSize = 4;
	if (fmt2)
	{
		int headerSize = 0;
		CMVideoFormatDescriptionGetH264ParameterSetAtIndex(
		    fmt2, 0, NULL, NULL, NULL, &headerSize);
		if (headerSize > 0)
			naluLengthSize = headerSize;
	}

	static const uint8_t startCode[4] = { 0, 0, 0, 1 };
	size_t offset = 0;
	while (offset < totalLength)
	{
		/* Read NALU length (big-endian) */
		if (offset + (size_t)naluLengthSize > totalLength)
			break;

		uint32_t naluLen = 0;
		for (int i = 0; i < naluLengthSize; i++)
		{
			naluLen = (naluLen << 8) | (uint8_t)dataPointer[offset + i];
		}
		offset += (size_t)naluLengthSize;

		if (offset + naluLen > totalLength)
			break;

		CFDataAppendBytes(sys->outputData, startCode, 4);
		CFDataAppendBytes(sys->outputData, (const uint8_t *)&dataPointer[offset],
		                  (CFIndex)naluLen);
		offset += naluLen;
	}
}

/**
 * Create (or recreate) the VTCompressionSession for the given dimensions.
 */
static BOOL videotoolbox_create_session(H264_CONTEXT *h264,
                                        H264_CONTEXT_VIDEOTOOLBOX *sys,
                                        uint32_t width, uint32_t height)
{
	if (sys->session)
	{
		VTCompressionSessionInvalidate(sys->session);
		CFRelease(sys->session);
		sys->session = NULL;
	}

	/* Source pixel format: NV12 (biplanar YCbCr 4:2:0)
	 * iOS hardware encoder requires biplanar NV12, not planar I420.
	 * We convert I420→NV12 in the compress function. */
	CFMutableDictionaryRef pixelBufferAttrs = CFDictionaryCreateMutable(
	    kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
	    &kCFTypeDictionaryValueCallBacks);

	int32_t pixFmt = kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
	CFNumberRef pixFmtVal = CFNumberCreate(NULL, kCFNumberSInt32Type, &pixFmt);
	CFDictionarySetValue(pixelBufferAttrs,
	                     kCVPixelBufferPixelFormatTypeKey, pixFmtVal);
	CFRelease(pixFmtVal);

	OSStatus status = VTCompressionSessionCreate(
	    kCFAllocatorDefault,
	    (int32_t)width,
	    (int32_t)height,
	    kCMVideoCodecType_H264,
	    NULL,                /* encoderSpecification */
	    pixelBufferAttrs,    /* sourceImageBufferAttributes */
	    NULL,                /* compressedDataAllocator */
	    videotoolbox_compress_callback,
	    sys,                 /* outputCallbackRefCon */
	    &sys->session);

	CFRelease(pixelBufferAttrs);

	if (status != noErr)
	{
		WLog_ERR(TAG, "VTCompressionSessionCreate failed: %d", (int)status);
		return FALSE;
	}

	/* Configure encoder properties */

	/* Real-time encoding for low latency */
	VTSessionSetProperty(sys->session,
	                     kVTCompressionPropertyKey_RealTime,
	                     kCFBooleanTrue);

	/* No B-frames — reduces latency */
	VTSessionSetProperty(sys->session,
	                     kVTCompressionPropertyKey_AllowFrameReordering,
	                     kCFBooleanFalse);

	/* Profile: Main (good balance of quality and compatibility) */
	VTSessionSetProperty(sys->session,
	                     kVTCompressionPropertyKey_ProfileLevel,
	                     kVTProfileLevel_H264_Main_AutoLevel);

	/* CABAC entropy coding for better compression */
	VTSessionSetProperty(sys->session,
	                     kVTCompressionPropertyKey_H264EntropyMode,
	                     kVTH264EntropyMode_CABAC);

	/* Keyframe interval: every 2 seconds */
	int32_t kfInterval = (int32_t)(h264->FrameRate > 0 ? h264->FrameRate * 2 : 30);
	CFNumberRef kfIntervalVal = CFNumberCreate(NULL, kCFNumberSInt32Type, &kfInterval);
	VTSessionSetProperty(sys->session,
	                     kVTCompressionPropertyKey_MaxKeyFrameInterval,
	                     kfIntervalVal);
	CFRelease(kfIntervalVal);

	/* Frame rate */
	if (h264->FrameRate > 0)
	{
		float fps = (float)h264->FrameRate;
		CFNumberRef fpsVal = CFNumberCreate(NULL, kCFNumberFloat32Type, &fps);
		VTSessionSetProperty(sys->session,
		                     kVTCompressionPropertyKey_ExpectedFrameRate,
		                     fpsVal);
		CFRelease(fpsVal);
	}

	/* Rate control */
	switch (h264->RateControlMode)
	{
		case H264_RATECONTROL_VBR:
		{
			int32_t bitrate = (int32_t)h264->BitRate;
			CFNumberRef bitrateVal = CFNumberCreate(NULL, kCFNumberSInt32Type, &bitrate);
			VTSessionSetProperty(sys->session,
			                     kVTCompressionPropertyKey_AverageBitRate,
			                     bitrateVal);
			CFRelease(bitrateVal);
			break;
		}
		case H264_RATECONTROL_CQP:
		{
			/* VideoToolbox doesn't have direct QP control; map QP to Quality.
			 * QP 0 = best quality (1.0), QP 51 = worst (0.0) */
			float quality = 1.0f - ((float)h264->QP / 51.0f);
			if (quality < 0.0f) quality = 0.0f;
			if (quality > 1.0f) quality = 1.0f;
			CFNumberRef qualityVal = CFNumberCreate(NULL, kCFNumberFloat32Type, &quality);
			VTSessionSetProperty(sys->session,
			                     kVTCompressionPropertyKey_Quality,
			                     qualityVal);
			CFRelease(qualityVal);
			break;
		}
		default:
		{
			/* Default: 1 Mbps VBR */
			int32_t bitrate = 1000000;
			CFNumberRef bitrateVal = CFNumberCreate(NULL, kCFNumberSInt32Type, &bitrate);
			VTSessionSetProperty(sys->session,
			                     kVTCompressionPropertyKey_AverageBitRate,
			                     bitrateVal);
			CFRelease(bitrateVal);
			break;
		}
	}

	VTCompressionSessionPrepareToEncodeFrames(sys->session);

	sys->encodedWidth = width;
	sys->encodedHeight = height;

	WLog_INFO(TAG, "VideoToolbox H.264 encoder initialized: %ux%u @ %u fps, bitrate=%u",
	          width, height, h264->FrameRate, h264->BitRate);

	return TRUE;
}

static int videotoolbox_compress(H264_CONTEXT *WINPR_RESTRICT h264,
                                 const BYTE **WINPR_RESTRICT pYUVData,
                                 const UINT32 *WINPR_RESTRICT iStride,
                                 BYTE **WINPR_RESTRICT ppDstData,
                                 UINT32 *WINPR_RESTRICT pDstSize)
{
	WINPR_ASSERT(h264);
	WINPR_ASSERT(pYUVData);
	WINPR_ASSERT(iStride);
	WINPR_ASSERT(ppDstData);
	WINPR_ASSERT(pDstSize);

	H264_CONTEXT_VIDEOTOOLBOX *sys =
	    (H264_CONTEXT_VIDEOTOOLBOX *)h264->pSystemData;
	WINPR_ASSERT(sys);

	if (!pYUVData[0] || !pYUVData[1] || !pYUVData[2])
		return -1;

	/* Create or recreate session if dimensions changed */
	if (!sys->session ||
	    sys->encodedWidth != h264->width ||
	    sys->encodedHeight != h264->height)
	{
		if (!videotoolbox_create_session(h264, sys, h264->width, h264->height))
			return -1;
	}

	/* Convert I420 (Y + U + V) to NV12 (Y + interleaved UV) for VideoToolbox.
	 * iOS hardware encoder only accepts biplanar NV12, not planar I420. */
	uint32_t uvWidth = (h264->width + 1) / 2;
	uint32_t uvHeight = (h264->height + 1) / 2;
	size_t uvStride = uvWidth * 2;  /* Interleaved UV: 2 bytes per sample pair */
	size_t uvBufSize = uvStride * uvHeight;

	/* (Re)allocate the interleaved UV buffer if needed */
	if (!sys->nv12UVBuffer || sys->nv12UVBufferSize < uvBufSize)
	{
		free(sys->nv12UVBuffer);
		sys->nv12UVBuffer = (uint8_t *)malloc(uvBufSize);
		if (!sys->nv12UVBuffer)
		{
			WLog_ERR(TAG, "Failed to allocate NV12 UV buffer (%zu bytes)", uvBufSize);
			sys->nv12UVBufferSize = 0;
			return -1;
		}
		sys->nv12UVBufferSize = uvBufSize;
	}

	/* Interleave U and V planes into UVUVUV... */
	const BYTE *uPlane = pYUVData[1];
	const BYTE *vPlane = pYUVData[2];
	for (uint32_t row = 0; row < uvHeight; row++)
	{
		uint8_t *dst = sys->nv12UVBuffer + row * uvStride;
		const BYTE *uRow = uPlane + row * iStride[1];
		const BYTE *vRow = vPlane + row * iStride[2];
		for (uint32_t col = 0; col < uvWidth; col++)
		{
			dst[col * 2]     = uRow[col];
			dst[col * 2 + 1] = vRow[col];
		}
	}

	/* Create NV12 CVPixelBuffer via CVPixelBufferCreate and copy data in.
	 * Using CVPixelBufferCreate ensures proper IOSurface-backed buffers
	 * that the hardware encoder can access directly. */
	CVPixelBufferRef pixelBuffer = NULL;

	CFDictionaryRef ioSurfaceProps = CFDictionaryCreate(
	    kCFAllocatorDefault, NULL, NULL, 0,
	    &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFMutableDictionaryRef pbAttrs = CFDictionaryCreateMutable(
	    kCFAllocatorDefault, 1,
	    &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFDictionarySetValue(pbAttrs, kCVPixelBufferIOSurfacePropertiesKey, ioSurfaceProps);
	CFRelease(ioSurfaceProps);

	CVReturn cvRet = CVPixelBufferCreate(
	    kCFAllocatorDefault,
	    h264->width,
	    h264->height,
	    kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange,
	    pbAttrs,
	    &pixelBuffer);

	CFRelease(pbAttrs);

	if (cvRet != kCVReturnSuccess || !pixelBuffer)
	{
		WLog_ERR(TAG, "CVPixelBufferCreate (NV12) failed: %d", (int)cvRet);
		return -1;
	}

	CVPixelBufferLockBaseAddress(pixelBuffer, 0);

	/* Copy Y plane */
	uint8_t *yDst = (uint8_t *)CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 0);
	size_t yDstStride = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 0);
	const BYTE *ySrc = pYUVData[0];
	for (uint32_t row = 0; row < h264->height; row++)
	{
		memcpy(yDst + row * yDstStride, ySrc + row * iStride[0], h264->width);
	}

	/* Copy interleaved UV plane */
	uint8_t *uvDst = (uint8_t *)CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 1);
	size_t uvDstStride = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 1);
	for (uint32_t row = 0; row < uvHeight; row++)
	{
		memcpy(uvDst + row * uvDstStride,
		       sys->nv12UVBuffer + row * uvStride,
		       uvWidth * 2);
	}

	CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);

	/* Encode frame synchronously */
	CMTime pts = CMTimeMake((int64_t)sys->frameCount, (int32_t)(h264->FrameRate > 0 ? h264->FrameRate : 30));
	CMTime dur = CMTimeMake(1, (int32_t)(h264->FrameRate > 0 ? h264->FrameRate : 30));

	OSStatus encStatus = VTCompressionSessionEncodeFrame(
	    sys->session,
	    pixelBuffer,
	    pts,
	    dur,
	    NULL,  /* frameProperties */
	    NULL,  /* sourceFrameRefCon */
	    NULL); /* infoFlagsOut */

	CVPixelBufferRelease(pixelBuffer);

	if (encStatus != noErr)
	{
		WLog_ERR(TAG, "VTCompressionSessionEncodeFrame failed: %d", (int)encStatus);
		return -1;
	}

	/* Flush to ensure the callback has been called */
	VTCompressionSessionCompleteFrames(sys->session, kCMTimeInvalid);

	sys->frameCount++;

	/* Return pointer to the encoded data */
	CFIndex dataLen = CFDataGetLength(sys->outputData);
	if (dataLen <= 0)
	{
		WLog_WARN(TAG, "Encoder produced no output for frame %" PRIu64, sys->frameCount - 1);
		*pDstSize = 0;
		return -1;
	}

	*ppDstData = (BYTE *)CFDataGetMutableBytePtr(sys->outputData);
	*pDstSize = (UINT32)dataLen;

	return 1;
}

static int videotoolbox_decompress(H264_CONTEXT *WINPR_RESTRICT h264,
                                   const BYTE *WINPR_RESTRICT pSrcData,
                                   UINT32 SrcSize)
{
	(void)h264;
	(void)pSrcData;
	(void)SrcSize;

	/* Decode not implemented — this backend is encoder-only (server side) */
	WLog_ERR(TAG, "VideoToolbox decoder not implemented (encoder-only backend)");
	return -1;
}

static BOOL videotoolbox_init(H264_CONTEXT *h264)
{
	WINPR_ASSERT(h264);

	H264_CONTEXT_VIDEOTOOLBOX *sys =
	    (H264_CONTEXT_VIDEOTOOLBOX *)calloc(1, sizeof(H264_CONTEXT_VIDEOTOOLBOX));

	if (!sys)
	{
		WLog_ERR(TAG, "Failed to allocate VideoToolbox context");
		return FALSE;
	}

	sys->outputData = CFDataCreateMutable(kCFAllocatorDefault, 0);
	if (!sys->outputData)
	{
		free(sys);
		return FALSE;
	}

	h264->pSystemData = sys;
	h264->numSystemData = 1;

	/* Session creation is deferred to the first Compress call,
	 * because width/height are not yet known at Init time. */
	WLog_INFO(TAG, "VideoToolbox H.264 encoder backend loaded (session deferred)");

	return TRUE;
}

static void videotoolbox_uninit(H264_CONTEXT *h264)
{
	WINPR_ASSERT(h264);

	H264_CONTEXT_VIDEOTOOLBOX *sys =
	    (H264_CONTEXT_VIDEOTOOLBOX *)h264->pSystemData;

	if (sys)
	{
		if (sys->session)
		{
			VTCompressionSessionInvalidate(sys->session);
			CFRelease(sys->session);
			sys->session = NULL;
		}

		if (sys->outputData)
		{
			CFRelease(sys->outputData);
			sys->outputData = NULL;
		}

		free(sys->nv12UVBuffer);
		sys->nv12UVBuffer = NULL;

		free(sys);
		h264->pSystemData = NULL;
	}
}

const H264_CONTEXT_SUBSYSTEM g_Subsystem_VideoToolbox = {
	"VideoToolbox",
	videotoolbox_init,
	videotoolbox_uninit,
	videotoolbox_decompress,
	videotoolbox_compress
};
