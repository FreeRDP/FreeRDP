/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * NSCodec Codec
 *
 * Copyright 2011 Samsung, Author Jiten Pathy
 * Copyright 2012 Vic Lee
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

/* NSCODEC_BITMAP_STREAM */
struct _NSC_STREAM
{
	UINT32 PlaneByteCount[4];
	BYTE ColorLossLevel;
	BYTE ChromaSubSamplingLevel;
	UINT16 Reserved;
	BYTE* Planes;
};
typedef struct _NSC_STREAM NSC_STREAM;

struct _NSC_MESSAGE
{
	int x;
	int y;
	UINT32 width;
	UINT32 height;
	BYTE* data;
	int scanline;
	BYTE* PlaneBuffer;
	UINT32 MaxPlaneSize;
	BYTE* PlaneBuffers[5];
	UINT32 OrgByteCount[4];
	UINT32 PlaneByteCount[4];
};
typedef struct _NSC_MESSAGE NSC_MESSAGE;

typedef struct _NSC_CONTEXT_PRIV NSC_CONTEXT_PRIV;

typedef struct _NSC_CONTEXT NSC_CONTEXT;

struct _NSC_CONTEXT
{
	UINT32 OrgByteCount[4];	/* original byte length of luma, chroma orange, chroma green, alpha variable in order */
	NSC_STREAM nsc_stream;
	UINT16 bpp;
	UINT16 width;
	UINT16 height;
	BYTE* BitmapData;     /* final argb values in little endian order */
	UINT32 BitmapDataLength; /* the maximum length of the buffer that bmpdata points to */
	RDP_PIXEL_FORMAT pixel_format;

	/* color palette allocated by the application */
	const BYTE* palette;

	void (*decode)(NSC_CONTEXT* context);
	void (*encode)(NSC_CONTEXT* context, BYTE* BitmapData, int rowstride);

	NSC_CONTEXT_PRIV* priv;
};

FREERDP_API NSC_CONTEXT* nsc_context_new(void);
FREERDP_API void nsc_context_set_pixel_format(NSC_CONTEXT* context, RDP_PIXEL_FORMAT pixel_format);
FREERDP_API void nsc_process_message(NSC_CONTEXT* context, UINT16 bpp,
	UINT16 width, UINT16 height, BYTE* data, UINT32 length);
FREERDP_API void nsc_compose_message(NSC_CONTEXT* context, wStream* s,
	BYTE* bmpdata, int width, int height, int rowstride);
FREERDP_API void nsc_context_free(NSC_CONTEXT* context);

FREERDP_API NSC_MESSAGE* nsc_encode_messages(NSC_CONTEXT* context, BYTE* data, int x, int y,
		int width, int height, int scanline, int* numMessages, int maxDataSize);
FREERDP_API int nsc_write_message(NSC_CONTEXT* context, wStream* s, NSC_MESSAGE* message);
FREERDP_API int nsc_message_free(NSC_CONTEXT* context, NSC_MESSAGE* message);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CODEC_NSCODEC_H */
