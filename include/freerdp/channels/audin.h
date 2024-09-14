/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Audio Input Redirection Virtual Channel
 *
 * Copyright 2010-2011 Vic Lee
 * Copyright 2023 Pascal Nowack <Pascal.Nowack@gmx.de>
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

#ifndef FREERDP_CHANNEL_AUDIN_H
#define FREERDP_CHANNEL_AUDIN_H

#include <freerdp/api.h>
#include <freerdp/codec/audio.h>
#include <freerdp/dvc.h>
#include <freerdp/types.h>

/** The command line name of the channel
 *
 *  \since version 3.0.0
 */
#define AUDIN_CHANNEL_NAME "audin"

/** The name of the channel (protocol internal)
 *
 *  \since version 3.0.0
 */
#define AUDIN_DVC_CHANNEL_NAME "AUDIO_INPUT"

typedef struct
{
	BYTE MessageId;
} SNDIN_PDU;

typedef enum
{
	SNDIN_VERSION_Version_1 = 0x00000001,
	SNDIN_VERSION_Version_2 = 0x00000002,
} SNDIN_VERSION_Version;

typedef struct
{
	SNDIN_PDU Header;
	SNDIN_VERSION_Version Version;
} SNDIN_VERSION;

typedef struct
{
	SNDIN_PDU Header;
	UINT32 NumFormats;
	UINT32 cbSizeFormatsPacket;
	AUDIO_FORMAT* SoundFormats;
	size_t ExtraDataSize;
} SNDIN_FORMATS;

typedef enum
{
	SPEAKER_FRONT_LEFT = 0x00000001,
	SPEAKER_FRONT_RIGHT = 0x00000002,
	SPEAKER_FRONT_CENTER = 0x00000004,
	SPEAKER_LOW_FREQUENCY = 0x00000008,
	SPEAKER_BACK_LEFT = 0x00000010,
	SPEAKER_BACK_RIGHT = 0x00000020,
	SPEAKER_FRONT_LEFT_OF_CENTER = 0x00000040,
	SPEAKER_FRONT_RIGHT_OF_CENTER = 0x00000080,
	SPEAKER_BACK_CENTER = 0x00000100,
	SPEAKER_SIDE_LEFT = 0x00000200,
	SPEAKER_SIDE_RIGHT = 0x00000400,
	SPEAKER_TOP_CENTER = 0x00000800,
	SPEAKER_TOP_FRONT_LEFT = 0x00001000,
	SPEAKER_TOP_FRONT_CENTER = 0x00002000,
	SPEAKER_TOP_FRONT_RIGHT = 0x00004000,
	SPEAKER_TOP_BACK_LEFT = 0x00008000,
	SPEAKER_TOP_BACK_CENTER = 0x00010000,
	SPEAKER_TOP_BACK_RIGHT = 0x00020000,
} AUDIN_SPEAKER;

typedef struct
{
	union
	{
		UINT16 wValidBitsPerSample;
		UINT16 wSamplesPerBlock;
		UINT16 wReserved;
	} Samples;
	AUDIN_SPEAKER dwChannelMask;
	GUID SubFormat;
} WAVEFORMAT_EXTENSIBLE;

typedef struct
{
	SNDIN_PDU Header;
	UINT32 FramesPerPacket;
	UINT32 initialFormat;
	AUDIO_FORMAT captureFormat;
	WAVEFORMAT_EXTENSIBLE* ExtraFormatData;
} SNDIN_OPEN;

typedef struct
{
	SNDIN_PDU Header;
	UINT32 Result;
} SNDIN_OPEN_REPLY;

typedef struct
{
	SNDIN_PDU Header;
} SNDIN_DATA_INCOMING;

typedef struct
{
	SNDIN_PDU Header;
	wStream* Data;
} SNDIN_DATA;

typedef struct
{
	SNDIN_PDU Header;
	UINT32 NewFormat;
} SNDIN_FORMATCHANGE;

#endif /* FREERDP_CHANNEL_AUDIN_H */
