/**
 * FreeRDP: A Remote Desktop Protocol client.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/load_plugin.h>

#include "drdynvc_types.h"
#include "tsmf_types.h"
#include "tsmf_constants.h"
#include "tsmf_decoder.h"

static ITSMFDecoder* tsmf_load_decoder_by_name(const char* name, TS_AM_MEDIA_TYPE* media_type)
{
	ITSMFDecoder* decoder;
	TSMF_DECODER_ENTRY entry;
	char* fullname;

	if (strrchr(name, '.') != NULL)
		entry = (TSMF_DECODER_ENTRY) freerdp_load_plugin(name, TSMF_DECODER_EXPORT_FUNC_NAME);
	else
	{
		fullname = xzalloc(strlen(name) + 6);
		strcpy(fullname, "tsmf_");
		strcat(fullname, name);
		entry = (TSMF_DECODER_ENTRY) freerdp_load_plugin(fullname, TSMF_DECODER_EXPORT_FUNC_NAME);
		xfree(fullname);
	}
	if (entry == NULL)
	{
		return NULL;
	}

	decoder = entry();
	if (decoder == NULL)
	{
		DEBUG_WARN("failed to call export function in %s", name);
		return NULL;
	}
	if (!decoder->SetFormat(decoder, media_type))
	{
		decoder->Free(decoder);
		decoder = NULL;
	}
	return decoder;
}

ITSMFDecoder* tsmf_load_decoder(const char* name, TS_AM_MEDIA_TYPE* media_type)
{
	ITSMFDecoder* decoder;

	if (name)
	{
		decoder = tsmf_load_decoder_by_name(name, media_type);
	}
	else
	{
		decoder = tsmf_load_decoder_by_name("ffmpeg", media_type);
	}

	return decoder;
}

