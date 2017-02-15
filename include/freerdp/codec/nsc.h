/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * NSCodec Codec
 *
 * Copyright 2011 Samsung, Author Jiten Pathy
 * Copyright 2012 Vic Lee
 * Copyright 2016 Armin Novak <armin.novak@thincast.com>
 * Copyright 2016 Thincast Technologies GmbH
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FREERDP_CODEC_NSCODEC_H
#define FREERDP_CODEC_NSCODEC_H

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/constants.h>

#include <winpr/stream.h>

#ifdef __cplusplus
extern "C" {
#endif

struct _NSC_MESSAGE
{
	UINT32 x;
	UINT32 y;
	UINT32 width;
	UINT32 height;
	const BYTE* data;
	UINT32 scanline;
	BYTE* PlaneBuffer;
	UINT32 MaxPlaneSize;
	BYTE* PlaneBuffers[5];
	UINT32 OrgByteCount[4];

	UINT32 LumaPlaneByteCount;
	UINT32 OrangeChromaPlaneByteCount;
	UINT32 GreenChromaPlaneByteCount;
	UINT32 AlphaPlaneByteCount;
	UINT32 ColorLossLevel;
	UINT32 ChromaSubsamplingLevel;
};
typedef struct _NSC_MESSAGE NSC_MESSAGE;

typedef struct _NSC_CONTEXT_PRIV NSC_CONTEXT_PRIV;

typedef struct _NSC_CONTEXT NSC_CONTEXT;

struct _NSC_CONTEXT
{
	UINT32 OrgByteCount[4];
	UINT32 format;
	UINT16 width;
	UINT16 height;
	BYTE* BitmapData;
	UINT32 BitmapDataLength;

	BYTE* Planes;
	UINT32 PlaneByteCount[4];
	UINT32 ColorLossLevel;
	UINT32 ChromaSubsamplingLevel;
	BOOL DynamicColorFidelity;

	/* color palette allocated by the application */
	const BYTE* palette;

	void (*decode)(NSC_CONTEXT* context);
	void (*encode)(NSC_CONTEXT* context, const BYTE* BitmapData,
	               UINT32 rowstride);

	NSC_CONTEXT_PRIV* priv;
};

FREERDP_API BOOL nsc_context_set_pixel_format(NSC_CONTEXT* context,
        UINT32 pixel_format);
FREERDP_API BOOL nsc_process_message(NSC_CONTEXT* context, UINT16 bpp,
                                     UINT32 width, UINT32 height,
                                     const BYTE* data, UINT32 length,
                                     BYTE* pDstData, UINT32 DstFormat,
                                     UINT32 nDstStride, UINT32 nXDst, UINT32 nYDst,
                                     UINT32 nWidth, UINT32 nHeight, UINT32 flip);
FREERDP_API BOOL nsc_compose_message(NSC_CONTEXT* context, wStream* s,
                                     const BYTE* bmpdata,
                                     UINT32 width, UINT32 height, UINT32 rowstride);

FREERDP_API NSC_MESSAGE* nsc_encode_messages(NSC_CONTEXT* context,
        const BYTE* data,
        UINT32 x, UINT32 y,
        UINT32 width, UINT32 height, UINT32 scanline,
        UINT32* numMessages, UINT32 maxDataSize);
FREERDP_API BOOL nsc_write_message(NSC_CONTEXT* context, wStream* s,
                                   NSC_MESSAGE* message);
FREERDP_API void nsc_message_free(NSC_CONTEXT* context, NSC_MESSAGE* message);

FREERDP_API BOOL nsc_context_reset(NSC_CONTEXT* context, UINT32 width,
                                   UINT32 height);

FREERDP_API NSC_CONTEXT* nsc_context_new(void);
FREERDP_API void nsc_context_free(NSC_CONTEXT* context);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CODEC_NSCODEC_H */
