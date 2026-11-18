/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * AV1 decoding with Apple VideoToolbox
 *
 * Copyright 2026 Cong Zhang <congzhangzh@gmail.com>
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

#ifndef FREERDP_LIB_CODEC_AV1_VIDEOTOOLBOX_H
#define FREERDP_LIB_CODEC_AV1_VIDEOTOOLBOX_H

#include <winpr/wtypes.h>
#include <winpr/wlog.h>

typedef struct S_AV1_VT AV1_VT;

/** A decoded picture: 8 bit planar YUV, valid until the next call on the decoder. */
typedef struct
{
	const BYTE* planes[3];
	UINT32 strides[3];
	UINT32 width;
	UINT32 height;
	BOOL i444; /**< 4:4:4, else 4:2:0 */
} AV1_VT_PICTURE;

/** The stream (its sequence header) is one VideoToolbox can't decode here: use software. */
#define AV1_VT_UNSUPPORTED (-100)
/** The input is no valid temporal unit (or comes before any sequence header): refused, the
 * decoder is as it was and keeps the stream. */
#define AV1_VT_INVALID (-101)

/** @return a decoder, or nullptr when this machine has no hardware AV1 decoder. */
AV1_VT* av1_vt_new(wLog* log);
void av1_vt_free(AV1_VT* vt);

/** Forget the stream: the next temporal unit must start a new one (sequence header). */
void av1_vt_reset(AV1_VT* vt);

/** Decode one temporal unit (low overhead bitstream format, as libaom writes it).
 *
 * @return TRUE and @p pic filled when a frame was shown, FALSE when none was,
 *         AV1_VT_UNSUPPORTED when VideoToolbox can't decode this stream (no frame of it was
 *         decoded, so a software decoder can start with the same temporal unit),
 *         AV1_VT_INVALID for bad input (nothing changed), or another negative value when
 *         VideoToolbox failed in the middle of the stream.
 */
INT32 av1_vt_decode(AV1_VT* vt, const BYTE* pSrcData, UINT32 SrcSize, AV1_VT_PICTURE* pic);

#endif /* FREERDP_LIB_CODEC_AV1_VIDEOTOOLBOX_H */
