/**
 * FreeRDP: A Remote Desktop Protocol client.
 * NSCodec Codec
 *
 * Copyright 2011 Samsung, Author Jiten Pathy
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

#ifndef __NSC_H
#define __NSC_H

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/utils/stream.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BYTESUM(_bs) (_bs[0] + _bs[1] + _bs[2] + _bs[3])
#define ROUND_UP_TO(_b, _n) (_b + ((~(_b & (_n-1)) + 0x1) & (_n-1)))

/* NSCODEC_BITMAP_STREAM */
struct _NSC_STREAM
{
	uint32 PlaneByteCount[4];
	uint8 ColorLossLevel;
	uint8 ChromaSubSamplingLevel;
	uint16 Reserved;
	uint8* Planes;
};
typedef struct _NSC_STREAM NSC_STREAM;

typedef struct _NSC_CONTEXT_PRIV NSC_CONTEXT_PRIV;

struct _NSC_CONTEXT
{
	uint32 OrgByteCount[4];	/* original byte length of luma, chroma orange, chroma green, alpha variable in order */
	NSC_STREAM nsc_stream;
	uint16 bpp;
	uint16 width;
	uint16 height;
	uint8* bmpdata;     /* final argb values in little endian order */
	uint32 bmpdata_length; /* the maximum length of the buffer that bmpdata points to */
	NSC_CONTEXT_PRIV* priv;
};
typedef struct _NSC_CONTEXT NSC_CONTEXT;

FREERDP_API NSC_CONTEXT* nsc_context_new(void);
FREERDP_API void nsc_process_message(NSC_CONTEXT* context, uint16 bpp,
	uint16 width, uint16 height, uint8* data, uint32 length);
FREERDP_API void nsc_context_free(NSC_CONTEXT* context);

#ifdef __cplusplus
}
#endif

#endif /* __NSC_H */
