/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Windows Clipboard Redirection
 *
 * Copyright 2012 Jason Champion
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>

#include <freerdp/utils/event.h>
#include <winpr/stream.h>
#include <freerdp/client/cliprdr.h>

#include "wf_cliprdr.h"

void wf_cliprdr_init(wfInfo* wfi, rdpChannels* chanman)
{

}

void wf_cliprdr_uninit(wfInfo* wfi)
{

}

static void wf_cliprdr_process_cb_monitor_ready_event(wfInfo* wfi)
{

}

static void wf_cliprdr_process_cb_data_request_event(wfInfo* wfi, RDP_CB_DATA_REQUEST_EVENT* event)
{

}

static void wf_cliprdr_process_cb_format_list_event(wfInfo* wfi, RDP_CB_FORMAT_LIST_EVENT* event)
{

}

static void wf_cliprdr_process_cb_data_response_event(wfInfo* wfi, RDP_CB_DATA_RESPONSE_EVENT* event)
{

}

void wf_process_cliprdr_event(wfInfo* wfi, RDP_EVENT* event)
{
	switch (event->event_type)
	{
		case RDP_EVENT_TYPE_CB_MONITOR_READY:
			wf_cliprdr_process_cb_monitor_ready_event(wfi);
			break;

		case RDP_EVENT_TYPE_CB_FORMAT_LIST:
			wf_cliprdr_process_cb_format_list_event(wfi, (RDP_CB_FORMAT_LIST_EVENT*) event);
			break;

		case RDP_EVENT_TYPE_CB_DATA_REQUEST:
			wf_cliprdr_process_cb_data_request_event(wfi, (RDP_CB_DATA_REQUEST_EVENT*) event);
			break;

		case RDP_EVENT_TYPE_CB_DATA_RESPONSE:
			wf_cliprdr_process_cb_data_response_event(wfi, (RDP_CB_DATA_RESPONSE_EVENT*) event);
			break;

		default:
			break;
	}
}

BOOL wf_cliprdr_process_selection_notify(wfInfo* wfi, HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	return TRUE;
}

BOOL wf_cliprdr_process_selection_request(wfInfo* wfi, HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	return TRUE;
}

BOOL wf_cliprdr_process_selection_clear(wfInfo* wfi, HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	return TRUE;
}

BOOL wf_cliprdr_process_property_notify(wfInfo* wfi, HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	return TRUE;
}

void wf_cliprdr_check_owner(wfInfo* wfi)
{

}
