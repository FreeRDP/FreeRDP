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

static void log(const char* fmt, ...)
{
	wLog* log = WLog_Get(TAG);
	const DWORD log_level = WLOG_WARN;

	if (WLog_IsLevelActive(log, log_level))
	{
		char buffer[1024] = { 0 };

		va_list ap;
		va_start(ap, fmt);
		vsnprintf(buffer, sizeof(buffer), fmt, ap);
		va_end(ap);

		WLog_PrintMessage(log, WLOG_MESSAGE_TEXT, log_level, __LINE__, __FILE__, __FUNCTION__, "%s",
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
		const unsigned char eld_conf[] = { 0xF8, 0xE8, 0x50, 0x00 };
		ssize_t rc = fdk_aac_dsp_impl_config(context->fdkAacInstance, context->encoder, eld_conf,
		                                     sizeof(eld_conf), log);
		if (rc < 0)
			return FALSE;

		context->fdkSetup = TRUE;
	}

	const size_t expect =
	    fdk_aac_dsp_impl_stream_info(context->fdkAacInstance, context->encoder, log);
	if (!Stream_EnsureRemainingCapacity(out, expect))
		return FALSE;

	ssize_t encoded =
	    fdk_aac_dsp_impl_encode(context->fdkAacInstance, data, length, Stream_Pointer(out),
	                            Stream_GetRemainingCapacity(out), log);
	if (encoded < 0)
		return FALSE;
	return Stream_SafeSeek(out, encoded);
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
		const unsigned char eld_conf[] = { 0xF8, 0xE8, 0x50, 0x00 };
		ssize_t rc = fdk_aac_dsp_impl_config(context->fdkAacInstance, context->encoder, eld_conf,
		                                     sizeof(eld_conf), log);
		if (rc < 0)
			return FALSE;

		context->fdkSetup = TRUE;
	}

	if (fdk_aac_dsp_impl_decode_fill(context->fdkAacInstance, data, length, log) < 0)
		return FALSE;

	const size_t expect =
	    fdk_aac_dsp_impl_stream_info(context->fdkAacInstance, context->encoder, log);
	if (!Stream_EnsureRemainingCapacity(out, expect))
		return FALSE;

	if (fdk_aac_dsp_impl_decode_read(context->fdkAacInstance, Stream_Pointer(out), expect, log) <=
	    0)
		return FALSE;
	Stream_Seek(out, expect);
	return TRUE;
}

void fdk_aac_dsp_uninit(FREERDP_DSP_COMMON_CONTEXT* context)
{
	WINPR_ASSERT(context);

	fdk_aac_dsp_impl_uninit(&context->fdkAacInstance, context->encoder, log);
}

BOOL fdk_aac_dsp_init(FREERDP_DSP_COMMON_CONTEXT* context)
{
	WINPR_ASSERT(context);
	context->fdkSetup = FALSE;
	return fdk_aac_dsp_impl_init(&context->fdkAacInstance, context->encoder, log);
}
