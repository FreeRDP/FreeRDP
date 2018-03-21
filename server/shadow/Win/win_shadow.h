/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2011-2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_SERVER_SHADOW_WIN_H
#define FREERDP_SERVER_SHADOW_WIN_H

#include <freerdp/assistance.h>

#include <freerdp/server/shadow.h>

typedef struct win_shadow_subsystem winShadowSubsystem;

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/stream.h>
#include <winpr/collections.h>

#include "win_rdp.h"
#include "win_wds.h"
#include "win_dxgi.h"

struct win_shadow_subsystem
{
	rdpShadowSubsystem base;

	int bpp;
	int width;
	int height;

#ifdef WITH_WDS_API
	HWND hWnd;
	shwContext* shw;
	HANDLE RdpUpdateEnterEvent;
	HANDLE RdpUpdateLeaveEvent;
	rdpAssistanceFile* pAssistanceFile;
	_IRDPSessionEvents* pSessionEvents;
	IRDPSRAPISharingSession* pSharingSession;
	IRDPSRAPIInvitation* pInvitation;
	IRDPSRAPIInvitationManager* pInvitationMgr;
	IRDPSRAPISessionProperties* pSessionProperties;
	IRDPSRAPIVirtualChannelManager* pVirtualChannelMgr;
	IRDPSRAPIApplicationFilter* pApplicationFilter;
	IRDPSRAPIAttendeeManager* pAttendeeMgr;
#endif

#ifdef WITH_DXGI_1_2
	UINT pendingFrames;
	BYTE* MetadataBuffer;
	UINT MetadataBufferSize;
	BOOL dxgiSurfaceMapped;
	BOOL dxgiFrameAcquired;
	ID3D11Device* dxgiDevice;
	IDXGISurface* dxgiSurface;
	ID3D11Texture2D* dxgiStage;
	IDXGIResource* dxgiResource;
	D3D_FEATURE_LEVEL featureLevel;
	ID3D11Texture2D* dxgiDesktopImage;
	DXGI_OUTDUPL_FRAME_INFO dxgiFrameInfo;
	ID3D11DeviceContext* dxgiDeviceContext;
	IDXGIOutputDuplication* dxgiOutputDuplication;
#endif
};

#ifdef __cplusplus
extern "C" {
#endif



#ifdef __cplusplus
}
#endif

#endif /* FREERDP_SERVER_SHADOW_WIN_H */
