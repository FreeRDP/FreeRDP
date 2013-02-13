/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDP Constants
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_CONSTANTS_H
#define FREERDP_CONSTANTS_H

/**
 * Codec IDs
 */
enum RDP_CODEC_ID
{
	RDP_CODEC_ID_NONE = 0x00,
	RDP_CODEC_ID_NSCODEC = 0x01,
	RDP_CODEC_ID_JPEG = 0x02,
	RDP_CODEC_ID_REMOTEFX = 0x03,
	RDP_CODEC_ID_IMAGE_REMOTEFX = 0x04
};

/**
 * Static Virtual Channel Flags
 */
enum RDP_SVC_CHANNEL_FLAG
{
	CHANNEL_FLAG_MIDDLE = 0,
	CHANNEL_FLAG_FIRST = 0x01,
	CHANNEL_FLAG_LAST = 0x02,
	CHANNEL_FLAG_ONLY = (CHANNEL_FLAG_FIRST | CHANNEL_FLAG_LAST),
	CHANNEL_FLAG_SHOW_PROTOCOL = 0x10,
	CHANNEL_FLAG_SUSPEND = 0x20,
	CHANNEL_FLAG_RESUME = 0x40,
	CHANNEL_FLAG_FAIL = 0x100
};

/**
 * Static Virtual Channel Events
 */
enum RDP_SVC_CHANNEL_EVENT
{
	CHANNEL_EVENT_INITIALIZED = 0,
	CHANNEL_EVENT_CONNECTED = 1,
	CHANNEL_EVENT_V1_CONNECTED = 2,
	CHANNEL_EVENT_DISCONNECTED = 3,
	CHANNEL_EVENT_TERMINATED = 4,
	CHANNEL_EVENT_DATA_RECEIVED = 10,
	CHANNEL_EVENT_WRITE_COMPLETE = 11,
	CHANNEL_EVENT_WRITE_CANCELLED = 12,
	CHANNEL_EVENT_USER = 1000
};

/**
 * Pixel format
 */
enum RDP_PIXEL_FORMAT
{
	RDP_PIXEL_FORMAT_B8G8R8A8,
	RDP_PIXEL_FORMAT_R8G8B8A8,
	RDP_PIXEL_FORMAT_B8G8R8,
	RDP_PIXEL_FORMAT_R8G8B8,
	RDP_PIXEL_FORMAT_B5G6R5_LE,
	RDP_PIXEL_FORMAT_R5G6B5_LE,
	RDP_PIXEL_FORMAT_P4_PLANER,
	RDP_PIXEL_FORMAT_P8
};
typedef enum RDP_PIXEL_FORMAT RDP_PIXEL_FORMAT;

/**
 * Virtual Channel Constants
 */
#define CHANNEL_CHUNK_LENGTH 1600

/**
 * CPU Optimization flags
 */
#define CPU_SSE2			0x1

/**
 * OSMajorType
 */
#define OSMAJORTYPE_UNSPECIFIED			0x0000
#define OSMAJORTYPE_WINDOWS			0x0001
#define OSMAJORTYPE_OS2				0x0002
#define OSMAJORTYPE_MACINTOSH			0x0003
#define OSMAJORTYPE_UNIX			0x0004

/**
 * OSMinorType
 */
#define OSMINORTYPE_UNSPECIFIED			0x0000
#define OSMINORTYPE_WINDOWS_31X			0x0001
#define OSMINORTYPE_WINDOWS_95			0x0002
#define OSMINORTYPE_WINDOWS_NT			0x0003
#define OSMINORTYPE_OS2_V21			0x0004
#define OSMINORTYPE_POWER_PC			0x0005
#define OSMINORTYPE_MACINTOSH			0x0006
#define OSMINORTYPE_NATIVE_XSERVER		0x0007
#define OSMINORTYPE_PSEUDO_XSERVER		0x0008

#endif /* FREERDP_CONSTANTS_H */
