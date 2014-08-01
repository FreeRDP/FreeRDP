/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifndef FREERDP_CODEC_DSP_H
#define FREERDP_CODEC_DSP_H

#include <freerdp/api.h>

union _ADPCM
{
	struct
	{
		INT16 last_sample[2];
		INT16 last_step[2];
	} ima;
	struct
	{
		BYTE predictor[2];
		INT32 delta[2];
		INT32 sample1[2];
		INT32 sample2[2];
	} ms;
};
typedef union _ADPCM ADPCM;

typedef struct _FREERDP_DSP_CONTEXT FREERDP_DSP_CONTEXT;

struct _FREERDP_DSP_CONTEXT
{
	BYTE* resampled_buffer;
	UINT32 resampled_size;
	UINT32 resampled_frames;
	UINT32 resampled_maxlength;

	BYTE* adpcm_buffer;
	UINT32 adpcm_size;
	UINT32 adpcm_maxlength;

	ADPCM adpcm;

	BOOL (*resample)(FREERDP_DSP_CONTEXT* context,
		const BYTE* src, int bytes_per_sample,
		UINT32 schan, UINT32 srate, int sframes,
		UINT32 rchan, UINT32 rrate);

	BOOL (*decode_ima_adpcm)(FREERDP_DSP_CONTEXT* context,
		const BYTE* src, int size, int channels, int block_size);
	BOOL (*encode_ima_adpcm)(FREERDP_DSP_CONTEXT* context,
		const BYTE* src, int size, int channels, int block_size);

	BOOL (*decode_ms_adpcm)(FREERDP_DSP_CONTEXT* context,
		const BYTE* src, int size, int channels, int block_size);
	BOOL (*encode_ms_adpcm)(FREERDP_DSP_CONTEXT* context,
		const BYTE* src, int size, int channels, int block_size);
};

#ifdef __cplusplus
extern "C" {
#endif

FREERDP_API FREERDP_DSP_CONTEXT* freerdp_dsp_context_new(void);
FREERDP_API void freerdp_dsp_context_free(FREERDP_DSP_CONTEXT* context);
#define freerdp_dsp_context_reset_adpcm(_c) memset(&_c->adpcm, 0, sizeof(ADPCM))

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CODEC_DSP_H */

