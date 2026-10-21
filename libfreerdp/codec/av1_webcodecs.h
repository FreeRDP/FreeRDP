/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * AV1 decoding with the browser's WebCodecs API (Emscripten builds)
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

#ifndef FREERDP_LIB_CODEC_AV1_WEBCODECS_H
#define FREERDP_LIB_CODEC_AV1_WEBCODECS_H

#include <winpr/wtypes.h>

/** @return a decoder handle (> 0), or 0 if the browser has no usable VideoDecoder */
int av1_wc_open(void);
void av1_wc_close(int handle);

#define AV1_WC_UNSUPPORTED (-100) /**< destination format not supported by VideoFrame.copyTo() */

/** Decodes one AV1 temporal unit straight into the RGB destination surface.
 *  @return 1 if a picture was written, 0 if none was ready, AV1_WC_UNSUPPORTED if the
 *  destination format needs the software decoder, other values < 0 on error */
INT32 av1_wc_decode(int handle, const BYTE* pSrcData, UINT32 SrcSize, BYTE* pDstData,
                    DWORD DstFormat, UINT32 nDstStep, UINT32 nDstWidth, UINT32 nDstHeight);

#endif /* FREERDP_LIB_CODEC_AV1_WEBCODECS_H */
