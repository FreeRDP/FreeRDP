/**
 * FreeRDP: A Remote Desktop Protocol Client
 * FreeRDP Windows Server
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef WFREERDP_H
#define WFREERDP_H

#include <freerdp/freerdp.h>
#include <freerdp/codec/rfx.h>

struct wf_info
{
	HDC driverDC;
	boolean activated;
	void* changeBuffer;
	LPTSTR deviceKey;
	TCHAR deviceName[32];
	int subscribers;
	int threadCnt;
	int height;
	int width;
	int bitsPerPix;

	HANDLE mutex, encodeMutex;

	unsigned long lastUpdate;
	unsigned long nextUpdate;

	long invalid_x1;
	long invalid_y1;

	long invalid_x2;
	long invalid_y2;

	
};
typedef struct wf_info wfInfo;

struct wf_peer_context
{
	rdpContext _p;

	wfInfo* wfInfo;
	boolean activated;
	RFX_CONTEXT* rfx_context;
	STREAM* s;

};
typedef struct wf_peer_context wfPeerContext;

extern wfInfo * wfInfoSingleton;

#endif /* WFREERDP_H */
