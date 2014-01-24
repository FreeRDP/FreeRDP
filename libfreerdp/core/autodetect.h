/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Auto-Detect PDUs
 *
 * Copyright 2014 Dell Software <Mike.McDonald@software.dell.com>
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

#ifndef __AUTODETECT_H
#define __AUTODETECT_H

typedef struct rdp_autodetect rdpAutoDetect;

#include "rdp.h"

#include <freerdp/freerdp.h>

#include <winpr/stream.h>
#include <winpr/sysinfo.h>

#define TYPE_ID_AUTODETECT_REQUEST	0x00
#define TYPE_ID_AUTODETECT_RESPONSE	0x01

struct rdp_autodetect
{
	/* Bandwidth measurement */
	UINT32 bandwidthMeasureStartTime;
	UINT32 bandwidthMeasureByteCount;

	/* Network characteristics (as reported by server) */
	UINT32 netCharBandwidth;
	UINT32 netCharBaseRTT;
	UINT32 netCharAverageRTT;
};

int rdp_recv_autodetect_packet(rdpRdp* rdp, wStream* s);

rdpAutoDetect* autodetect_new(void);
void autodetect_free(rdpAutoDetect* autodetect);

#ifdef WITH_DEBUG_AUTODETECT
#define DEBUG_AUTODETECT(fmt, ...) DEBUG_CLASS(AUTODETECT, fmt, ## __VA_ARGS__)
#else
#define DEBUG_AUTODETECT(fmt, ...) DEBUG_NULL(fmt, ## __VA_ARGS__)
#endif

#endif /* __AUTODETECT_H */
