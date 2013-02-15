/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Mac OS X Server
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

#ifndef MF_INFO_H
#define MF_INFO_H

#define MF_INFO_DEFAULT_FPS 1
#define MF_INFO_MAXPEERS 1



#include <winpr/wtypes.h>
#include <freerdp/codec/rfx.h>

#include "mf_interface.h"

int mf_info_lock(mfInfo* mfi);
int mf_info_try_lock(mfInfo* mfi, UINT32 ms);
int mf_info_unlock(mfInfo* mfi);

mfInfo* mf_info_get_instance(void);
void mf_info_peer_register(mfInfo* mfi, mfPeerContext* context);
void mf_info_peer_unregister(mfInfo* mfi, mfPeerContext* context);

BOOL mf_info_have_updates(mfInfo* mfi);
void mf_info_update_changes(mfInfo* mfi);
void mf_info_find_invalid_region(mfInfo* mfi);
void mf_info_clear_invalid_region(mfInfo* mfi);
void mf_info_invalidate_full_screen(mfInfo* mfi);
BOOL mf_info_have_invalid_region(mfInfo* mfi);
void mf_info_getScreenData(mfInfo* mfi, long* width, long* height, BYTE** pBits, int* pitch);
//BOOL CALLBACK mf_info_monEnumCB(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData);

#endif /* mf_info_H */
