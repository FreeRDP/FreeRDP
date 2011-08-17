/**
 * FreeRDP: A Remote Desktop Protocol client.
 * Digital Sound Processing
 *
 * Copyright 2010-2011 Vic Lee
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

#ifndef __DSP_UTILS_H
#define __DSP_UTILS_H

#include <freerdp/api.h>

struct _ADPCM
{
	sint16 last_sample[2];
	sint16 last_step[2];
};
typedef struct _ADPCM ADPCM;

FREERDP_API uint8* dsp_resample(uint8* src, int bytes_per_sample,
	uint32 schan, uint32 srate, int sframes,
	uint32 rchan, uint32 rrate, int * prframes);

FREERDP_API uint8* dsp_decode_ima_adpcm(ADPCM* adpcm,
	uint8* src, int size, int channels, int block_size, int* out_size);
FREERDP_API uint8* dsp_encode_ima_adpcm(ADPCM* adpcm,
	uint8* src, int size, int channels, int block_size, int* out_size);

#endif /* __DSP_UTILS_H */

