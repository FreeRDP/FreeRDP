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
 * CPU Optimization flags
 */
#define CPU_SSE2 0x1

/**
 * OSMajorType
 */
#define OSMAJORTYPE_UNSPECIFIED 0x0000
#define OSMAJORTYPE_WINDOWS 0x0001
#define OSMAJORTYPE_OS2 0x0002
#define OSMAJORTYPE_MACINTOSH 0x0003
#define OSMAJORTYPE_UNIX 0x0004

/**
 * OSMinorType
 */
#define OSMINORTYPE_UNSPECIFIED 0x0000
#define OSMINORTYPE_WINDOWS_31X 0x0001
#define OSMINORTYPE_WINDOWS_95 0x0002
#define OSMINORTYPE_WINDOWS_NT 0x0003
#define OSMINORTYPE_OS2_V21 0x0004
#define OSMINORTYPE_POWER_PC 0x0005
#define OSMINORTYPE_MACINTOSH 0x0006
#define OSMINORTYPE_NATIVE_XSERVER 0x0007
#define OSMINORTYPE_PSEUDO_XSERVER 0x0008
#define OSMINORTYPE_NATIVE_WAYLAND 0x0009

#endif /* FREERDP_CONSTANTS_H */
