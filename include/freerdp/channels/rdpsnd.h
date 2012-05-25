/**
 * FreeRDP: A Remote Desktop Protocol client.
 * Audio Virtual Channel Types
 *
 * Copyright 2012 Vic Lee
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __FREERDP_RDPSND_H
#define __FREERDP_RDPSND_H

#include <freerdp/api.h>
#include <freerdp/types.h>

typedef struct rdpsnd_format rdpsndFormat;
struct rdpsnd_format
{
	uint16 wFormatTag;
	uint16 nChannels;
	uint32 nSamplesPerSec;
	uint16 nBlockAlign;
	uint16 wBitsPerSample;
	uint16 cbSize;
	uint8* data;
};

#define SNDC_CLOSE         1
#define SNDC_WAVE          2
#define SNDC_SETVOLUME     3
#define SNDC_SETPITCH      4
#define SNDC_WAVECONFIRM   5
#define SNDC_TRAINING      6
#define SNDC_FORMATS       7
#define SNDC_CRYPTKEY      8
#define SNDC_WAVEENCRYPT   9
#define SNDC_UDPWAVE       10
#define SNDC_UDPWAVELAST   11
#define SNDC_QUALITYMODE   12

#define TSSNDCAPS_ALIVE  1
#define TSSNDCAPS_VOLUME 2
#define TSSNDCAPS_PITCH  4

#define DYNAMIC_QUALITY  0x0000
#define MEDIUM_QUALITY   0x0001
#define HIGH_QUALITY     0x0002

#endif
