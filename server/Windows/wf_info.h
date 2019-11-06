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

#ifndef FREERDP_SERVER_WIN_INFO_H
#define FREERDP_SERVER_WIN_INFO_H

#include "wf_interface.h"

#define FREERDP_SERVER_WIN_INFO_DEFAULT_FPS 24
#define FREERDP_SERVER_WIN_INFO_MAXPEERS 32

BOOL wf_info_lock(wfInfo* wfi);
BOOL wf_info_try_lock(wfInfo* wfi, DWORD dwMilliseconds);
BOOL wf_info_unlock(wfInfo* wfi);

wfInfo* wf_info_get_instance(void);
BOOL wf_info_peer_register(wfInfo* wfi, wfPeerContext* context);
void wf_info_peer_unregister(wfInfo* wfi, wfPeerContext* context);

BOOL wf_info_have_updates(wfInfo* wfi);
void wf_info_update_changes(wfInfo* wfi);
void wf_info_find_invalid_region(wfInfo* wfi);
void wf_info_clear_invalid_region(wfInfo* wfi);
void wf_info_invalidate_full_screen(wfInfo* wfi);
BOOL wf_info_have_invalid_region(wfInfo* wfi);
void wf_info_getScreenData(wfInfo* wfi, long* width, long* height, BYTE** pBits, int* pitch);
BOOL CALLBACK wf_info_monEnumCB(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor,
                                LPARAM dwData);

#endif /* FREERDP_SERVER_WIN_INFO_H */
