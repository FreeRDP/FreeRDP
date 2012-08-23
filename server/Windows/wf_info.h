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

struct wf_peer_context;
typedef struct wf_peer_context wfPeerContext;

struct wf_info
{
	HDC driverDC;
	BOOL activated;
	void* changeBuffer;
	LPTSTR deviceKey;
	TCHAR deviceName[32];
	int subscribers;
	int threadCnt;
	int height;
	int width;
	int bitsPerPix;

	HANDLE mutex, encodeMutex, can_send_mutex;

	unsigned long lastUpdate;
	unsigned long nextUpdate;

	long invalid_x1;
	long invalid_y1;

	long invalid_x2;
	long invalid_y2;

	BOOL enc_data;
};
typedef struct wf_info wfInfo;

wfInfo* wf_info_init(wfInfo* wfi);
void wf_info_mirror_init(wfInfo* wfi, wfPeerContext* context);
void wf_info_subscriber_release(wfInfo* wfi, wfPeerContext* context);

int wf_info_get_thread_count(wfInfo* wfi);
void wf_info_set_thread_count(wfInfo* wfi, int count);

BOOL wf_info_has_subscribers(wfInfo* wfi);
BOOL wf_info_have_updates(wfInfo* wfi);
void wf_info_updated(wfInfo* wfi);
void wf_info_update_changes(wfInfo* wfi);
void wf_info_find_invalid_region(wfInfo* wfi);
void wf_info_clear_invalid_region(wfInfo* wfi);
BOOL wf_info_have_invalid_region(wfInfo* wfi);

int wf_info_get_height(wfInfo* wfi);
int wf_info_get_width(wfInfo* wfi);

#endif
