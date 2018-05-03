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

struct _FREERDP_DSP_COMMON_CONTEXT
{
	wStream* buffer;
	wStream* resample;
};

#endif /* FREERDP_LIB_CODEC_DSP_H */
