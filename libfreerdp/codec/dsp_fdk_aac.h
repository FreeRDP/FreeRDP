/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Digital Sound Processing
 *
 * Copyright 2022 Armin Novak <anovak@thincast.com>
 * Copyright 2022 Thincast Technologies GmbH
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

#ifndef FREERDP_DSP_FDK_AAC_H_
#define FREERDP_DSP_FDK_AAC_H_

#include <winpr/stream.h>
#include <freerdp/codec/audio.h>

#include "dsp.h"

BOOL fdk_aac_dsp_init(FREERDP_DSP_COMMON_CONTEXT* context, size_t frames_per_packet);
void fdk_aac_dsp_uninit(FREERDP_DSP_COMMON_CONTEXT* context);

BOOL fdk_aac_dsp_encode(FREERDP_DSP_COMMON_CONTEXT* context, const AUDIO_FORMAT* srcFormat,
                        const BYTE* data, size_t length, wStream* out);

BOOL fdk_aac_dsp_decode(FREERDP_DSP_COMMON_CONTEXT* context, const AUDIO_FORMAT* srcFormat,
                        const BYTE* data, size_t length, wStream* out);

#endif
