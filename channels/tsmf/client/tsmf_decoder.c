/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Video Redirection Virtual Channel - Decoder
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <freerdp/addin.h>
#include <freerdp/client/channels.h>

#include "tsmf_types.h"
#include "tsmf_constants.h"
#include "tsmf_decoder.h"

static ITSMFDecoder* tsmf_load_decoder_by_name(const char *name)
{
	ITSMFDecoder* decoder;
	TSMF_DECODER_ENTRY entry;

	entry = (TSMF_DECODER_ENTRY) freerdp_load_channel_addin_entry("tsmf", (LPSTR) name, "decoder", 0);

	if (!entry)
		return NULL;

	decoder = entry();

	if (!decoder)
	{
		WLog_ERR(TAG, "failed to call export function in %s", name);
		return NULL;
	}

	return decoder;
}

static BOOL tsmf_decoder_set_format(ITSMFDecoder *decoder, TS_AM_MEDIA_TYPE* media_type)
{
	if (decoder->SetFormat(decoder, media_type))
		return TRUE;
	else
		return FALSE;
}

ITSMFDecoder* tsmf_load_decoder(const char* name, TS_AM_MEDIA_TYPE* media_type)
{
	ITSMFDecoder* decoder = NULL;

	if (name)
	{
		decoder = tsmf_load_decoder_by_name(name);
	}

#if defined(WITH_GSTREAMER_1_0) || defined(WITH_GSTREAMER_0_10)
	if (!decoder)
		decoder = tsmf_load_decoder_by_name("gstreamer");
#endif

#if defined(WITH_FFMPEG)
	if (!decoder)
		decoder = tsmf_load_decoder_by_name("ffmpeg");
#endif

	if (decoder)
	{
		if (!tsmf_decoder_set_format(decoder, media_type))
		{
			decoder->Free(decoder);
			decoder = NULL;
		}
	}

	return decoder;
}

BOOL tsmf_check_decoder_available(const char* name)
{
	ITSMFDecoder* decoder = NULL;
	BOOL retValue = FALSE;

	if (name)
	{
		decoder = tsmf_load_decoder_by_name(name);
	}
#if defined(WITH_GSTREAMER_1_0) || defined(WITH_GSTREAMER_0_10)
        if (!decoder)
                decoder = tsmf_load_decoder_by_name("gstreamer");
#endif

#if defined(WITH_FFMPEG)
	if (!decoder)
		decoder = tsmf_load_decoder_by_name("ffmpeg");
#endif

	if (decoder)
	{
		decoder->Free(decoder);
		decoder = NULL;
		retValue = TRUE;
	}

	return retValue;
}

