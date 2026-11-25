/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Digital Sound Processing - Android MediaCodec AAC backend
 *
 * Copyright 2026 Ibrahim Sevinc <ibrahim.sevinc.mail@gmail.com>
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

#ifndef FREERDP_LIB_CODEC_DSP_MEDIACODEC_H
#define FREERDP_LIB_CODEC_DSP_MEDIACODEC_H

#include <freerdp/config.h>

#include <freerdp/codec/audio.h>
#include <winpr/stream.h>

#include "dsp.h"

WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL mediacodec_aac_dsp_init(FREERDP_DSP_COMMON_CONTEXT* context,
                                                                size_t frames_per_packet);

FREERDP_LOCAL void mediacodec_aac_dsp_uninit(FREERDP_DSP_COMMON_CONTEXT* context);

WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL
mediacodec_aac_dsp_encode(FREERDP_DSP_COMMON_CONTEXT* context, const AUDIO_FORMAT* srcFormat,
                          const BYTE* data, size_t length, wStream* out);

WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL
mediacodec_aac_dsp_supports_format(const AUDIO_FORMAT* format, BOOL encode);

WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL
mediacodec_aac_dsp_decode(FREERDP_DSP_COMMON_CONTEXT* context, const AUDIO_FORMAT* srcFormat,
                          const BYTE* data, size_t length, wStream* out);

#endif /* FREERDP_LIB_CODEC_DSP_MEDIACODEC_H */
