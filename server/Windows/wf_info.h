/**
 * FreeRDP: A Remote Desktop Protocol Client
 * FreeRDP Windows Server
 *
 * Copyright 2012 Corey Clayton <can.of.tuna@gmail.com>
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

#ifndef WF_INFO_H
#define WF_INFO_H


#define CINTERFACE
#include <D3D11.h>
#include <dxgi1_2.h>

#include "wf_interface.h"
#include <freerdp/codec/rfx.h>

struct wf_peer_context;
typedef struct wf_peer_context wfPeerContext;

struct wf_dxgi_info
{
	ID3D11Device* Device;
	ID3D11DeviceContext* DeviceContext;
	IDXGIOutputDuplication* DeskDupl;
	ID3D11Texture2D* AcquiredDesktopImage;
};
typedef struct wf_dxgi_info wfDxgiInfo;

struct wf_info
{
	STREAM* s;
	HDC driverDC;
	BOOL activated;
	void* changeBuffer;
	LPTSTR deviceKey;
	TCHAR deviceName[32];
	int subscribers;
	int threadCount;
	int height;
	int width;
	int bitsPerPix;

	RECT invalid;
	HANDLE mutex;
	BOOL updatePending;
	HANDLE updateEvent;
	RFX_CONTEXT* rfx_context;
	unsigned long lastUpdate;
	unsigned long nextUpdate;

	wfDxgiInfo dxgi;
};
typedef struct wf_info wfInfo;

int wf_info_lock(wfInfo* wfi);
int wf_info_try_lock(wfInfo* wfi, DWORD dwMilliseconds);
int wf_info_unlock(wfInfo* wfi);

void wf_info_get_screen_info(wfInfo* wfi);

wfInfo* wf_info_get_instance();
void wf_info_peer_register(wfInfo* wfi, wfPeerContext* context);
void wf_info_peer_unregister(wfInfo* wfi, wfPeerContext* context);

BOOL wf_info_have_updates(wfInfo* wfi);
void wf_info_update_changes(wfInfo* wfi);
void wf_info_find_invalid_region(wfInfo* wfi);
void wf_info_clear_invalid_region(wfInfo* wfi);
void wf_info_invalidate_full_screen(wfInfo* wfi);
BOOL wf_info_have_invalid_region(wfInfo* wfi);

#endif /* WF_INFO_H */
