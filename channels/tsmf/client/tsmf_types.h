/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Video Redirection Virtual Channel - Types
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

#ifndef FREERDP_CHANNEL_TSMF_CLIENT_TYPES_H
#define FREERDP_CHANNEL_TSMF_CLIENT_TYPES_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <freerdp/dvc.h>
#include <freerdp/types.h>
#include <freerdp/channels/log.h>

#define TAG CHANNELS_TAG("tsmf.client")

#ifdef WITH_DEBUG_TSMF
#define DEBUG_TSMF(...) WLog_DBG(TAG, __VA_ARGS__)
#else
#define DEBUG_TSMF(...) \
	do                  \
	{                   \
	} while (0)
#endif

typedef struct _TS_AM_MEDIA_TYPE
{
	int MajorType;
	int SubType;
	int FormatType;

	UINT32 Width;
	UINT32 Height;
	UINT32 BitRate;
	struct
	{
		UINT32 Numerator;
		UINT32 Denominator;
	} SamplesPerSecond;
	UINT32 Channels;
	UINT32 BitsPerSample;
	UINT32 BlockAlign;
	const BYTE* ExtraData;
	UINT32 ExtraDataSize;
} TS_AM_MEDIA_TYPE;

#endif /* FREERDP_CHANNEL_TSMF_CLIENT_TYPES_H */
