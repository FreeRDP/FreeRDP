/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifndef FREERDP_SERVER_WIN_DXGI_H
#define FREERDP_SERVER_WIN_DXGI_H

#include "wf_interface.h"

int wf_dxgi_init(wfInfo* context);

int wf_dxgi_createDevice(wfInfo* context);

int wf_dxgi_getDuplication(wfInfo* context);

int wf_dxgi_cleanup(wfInfo* context);

int wf_dxgi_nextFrame(wfInfo* context, UINT timeout);

int wf_dxgi_getPixelData(wfInfo* context, BYTE** data, int* pitch, RECT* invalid);

int wf_dxgi_releasePixelData(wfInfo* context);

int wf_dxgi_getInvalidRegion(RECT* invalid);

#endif /* FREERDP_SERVER_WIN_DXGI_H */
