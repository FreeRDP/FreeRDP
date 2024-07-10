/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Digital Sound Processing - backend
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

#ifndef FREERDP_LIB_CODEC_DSP_H
#define FREERDP_LIB_CODEC_DSP_H

#include <freerdp/api.h>
#include <freerdp/codec/audio.h>
#include <freerdp/codec/dsp.h>

typedef struct
{
	ALIGN64 AUDIO_FORMAT format;
	ALIGN64 BOOL encoder;
	ALIGN64 wStream* buffer;
	ALIGN64 wStream* resample;
	ALIGN64 wStream* channelmix;
#if defined(WITH_FDK_AAC)
	ALIGN64 BOOL fdkSetup;
	ALIGN64 void* fdkAacInstance;
	ALIGN64 size_t buffersize;
	ALIGN64 unsigned frames_per_packet;
#endif
} FREERDP_DSP_COMMON_CONTEXT;

BOOL freerdp_dsp_common_context_init(FREERDP_DSP_COMMON_CONTEXT* context, BOOL encode);
void freerdp_dsp_common_context_uninit(FREERDP_DSP_COMMON_CONTEXT* context);

#endif /* FREERDP_LIB_CODEC_DSP_H */
