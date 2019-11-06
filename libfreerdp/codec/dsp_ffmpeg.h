/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Digital Sound Processing - FFMPEG backend
 *
 * Copyright 2018 Armin Novak <armin.novak@thincast.com>
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

#ifndef FREERDP_LIB_CODEC_DSP_FFMPEG_H
#define FREERDP_LIB_CODEC_DSP_FFMPEG_H

#include <freerdp/api.h>
#include <freerdp/codec/audio.h>
#include <freerdp/codec/dsp.h>

#include <libavcodec/version.h>

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(57, 48, 101)
#error \
    "DSP module requires libavcodec version >= 57.48.101. Upgrade or set WITH_DSP_FFMPEG=OFF to continue"
#endif

FREERDP_DSP_CONTEXT* freerdp_dsp_ffmpeg_context_new(BOOL encode);
BOOL freerdp_dsp_ffmpeg_supports_format(const AUDIO_FORMAT* format, BOOL encode);
BOOL freerdp_dsp_ffmpeg_encode(FREERDP_DSP_CONTEXT* context, const AUDIO_FORMAT* srcFormat,
                               const BYTE* data, size_t length, wStream* out);
BOOL freerdp_dsp_ffmpeg_decode(FREERDP_DSP_CONTEXT* context, const AUDIO_FORMAT* srcFormat,
                               const BYTE* data, size_t length, wStream* out);
void freerdp_dsp_ffmpeg_context_free(FREERDP_DSP_CONTEXT* context);
BOOL freerdp_dsp_ffmpeg_context_reset(FREERDP_DSP_CONTEXT* context,
                                      const AUDIO_FORMAT* targetFormat);

#endif /* FREERDP_LIB_CODEC_DSP_FFMPEG_H */
