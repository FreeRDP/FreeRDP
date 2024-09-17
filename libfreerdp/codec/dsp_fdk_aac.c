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

#include "dsp_fdk_aac.h"
#include "dsp_fdk_impl.h"

#include <freerdp/log.h>
#define TAG FREERDP_TAG("dsp.fdk")

static void write_log(unsigned log_level, const char* fmt, ...)
{
	wLog* log = WLog_Get(TAG);

	if (WLog_IsLevelActive(log, log_level))
	{
		char buffer[1024] = { 0 };

		va_list ap = { 0 };
		va_start(ap, fmt);
		vsnprintf(buffer, sizeof(buffer), fmt, ap);
		va_end(ap);

		WLog_PrintMessage(log, WLOG_MESSAGE_TEXT, log_level, __LINE__, __FILE__, __func__, "%s",
		                  buffer);
	}
}

BOOL fdk_aac_dsp_encode(FREERDP_DSP_COMMON_CONTEXT* context, const AUDIO_FORMAT* srcFormat,
                        const BYTE* data, size_t length, wStream* out)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(srcFormat);

	if (srcFormat->wFormatTag != WAVE_FORMAT_PCM)
	{
		WLog_WARN(TAG, "Feeding %s format data to encoder function, but require %s",
		          audio_format_get_tag_string(srcFormat->wFormatTag),
		          audio_format_get_tag_string(WAVE_FORMAT_PCM));
		return FALSE;
	}

	if (!context->fdkSetup)
	{
		ssize_t rc = fdk_aac_dsp_impl_config(
		    context->fdkAacInstance, &context->buffersize, context->encoder,
		    context->format.nSamplesPerSec, context->format.nChannels,
		    context->format.nAvgBytesPerSec, context->frames_per_packet, write_log);
		if (rc < 0)
			return FALSE;

		context->fdkSetup = TRUE;
	}

	if (!Stream_EnsureRemainingCapacity(out, context->buffersize))
		return FALSE;

	{
		const ssize_t encoded =
		    fdk_aac_dsp_impl_encode(context->fdkAacInstance, data, length, Stream_Pointer(out),
		                            Stream_GetRemainingCapacity(out), write_log);
		if (encoded < 0)
			return FALSE;
		Stream_Seek(out, (size_t)encoded);
		return TRUE;
	}
}

BOOL fdk_aac_dsp_decode(FREERDP_DSP_COMMON_CONTEXT* context, const AUDIO_FORMAT* srcFormat,
                        const BYTE* data, size_t length, wStream* out)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(srcFormat);

	if (srcFormat->wFormatTag != WAVE_FORMAT_AAC_MS)
	{
		WLog_WARN(TAG, "Feeding %s format data to encoder function, but require %s",
		          audio_format_get_tag_string(srcFormat->wFormatTag),
		          audio_format_get_tag_string(WAVE_FORMAT_AAC_MS));
		return FALSE;
	}

	if (!context->fdkSetup)
	{
		ssize_t rc = fdk_aac_dsp_impl_config(
		    context->fdkAacInstance, &context->buffersize, context->encoder,
		    context->format.nSamplesPerSec, context->format.nChannels,
		    context->format.nAvgBytesPerSec, context->frames_per_packet, write_log);
		if (rc < 0)
			return FALSE;

		context->fdkSetup = TRUE;
	}

	ssize_t rest = 0;
	do
	{
		rest = fdk_aac_dsp_impl_decode_fill(context->fdkAacInstance, data, length, write_log);
		if (rest < 0)
		{
			WLog_WARN(TAG, "DecodeFill() failed");
			return FALSE;
		}

		ssize_t ret = -1;
		do
		{
			const size_t expect = context->buffersize;
			if (!Stream_EnsureRemainingCapacity(out, expect))
				return FALSE;

			ret = fdk_aac_dsp_impl_decode_read(context->fdkAacInstance, Stream_Pointer(out), expect,
			                                   write_log);
			if (ret < 0)
				return FALSE;

			Stream_Seek(out, (size_t)ret);
		} while (ret > 0);
	} while (rest > 0);

	return TRUE;
}

void fdk_aac_dsp_uninit(FREERDP_DSP_COMMON_CONTEXT* context)
{
	WINPR_ASSERT(context);

	fdk_aac_dsp_impl_uninit(&context->fdkAacInstance, context->encoder, write_log);
}

BOOL fdk_aac_dsp_init(FREERDP_DSP_COMMON_CONTEXT* context, size_t frames_per_packet)
{
	WINPR_ASSERT(context);
	context->fdkSetup = FALSE;
	WINPR_ASSERT(frames_per_packet <= UINT_MAX);
	context->frames_per_packet = (unsigned)frames_per_packet;
	return fdk_aac_dsp_impl_init(&context->fdkAacInstance, context->encoder, write_log);
}
