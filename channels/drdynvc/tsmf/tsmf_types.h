/**
 * FreeRDP: A Remote Desktop Protocol client.
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

#ifndef __TSMF_TYPES_H
#define __TSMF_TYPES_H

#include <freerdp/types.h>

typedef struct _TS_AM_MEDIA_TYPE
{
	int MajorType;
	int SubType;
	int FormatType;

	uint32 Width;
	uint32 Height;	
	uint32 BitRate;
	struct
	{
		uint32 Numerator;
		uint32 Denominator;
	} SamplesPerSecond;
	uint32 Channels;
	uint32 BitsPerSample;
	uint32 BlockAlign;
	const uint8* ExtraData;
	uint32 ExtraDataSize;
} TS_AM_MEDIA_TYPE;

#endif

