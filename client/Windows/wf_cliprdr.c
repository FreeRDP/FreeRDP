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

void wf_cliprdr_init(wfContext* wfc, rdpChannels* channels)
{

}

void wf_cliprdr_uninit(wfContext* wfc)
{

}

static void wf_cliprdr_process_cb_monitor_ready_event(wfContext* wfc)
{

}

static void wf_cliprdr_process_cb_data_request_event(wfContext* wfc, RDP_CB_DATA_REQUEST_EVENT* event)
{

}

static void wf_cliprdr_process_cb_format_list_event(wfContext* wfc, RDP_CB_FORMAT_LIST_EVENT* event)
{

}

static void wf_cliprdr_process_cb_data_response_event(wfContext* wfc, RDP_CB_DATA_RESPONSE_EVENT* event)
{

}

void wf_process_cliprdr_event(wfContext* wfc, wMessage* event)
{
	switch (GetMessageType(event->id))
	{
		case CliprdrChannel_MonitorReady:
			wf_cliprdr_process_cb_monitor_ready_event(wfc);
			break;

		case CliprdrChannel_FormatList:
			wf_cliprdr_process_cb_format_list_event(wfc, (RDP_CB_FORMAT_LIST_EVENT*) event);
			break;

		case CliprdrChannel_DataRequest:
			wf_cliprdr_process_cb_data_request_event(wfc, (RDP_CB_DATA_REQUEST_EVENT*) event);
			break;

		case CliprdrChannel_DataResponse:
			wf_cliprdr_process_cb_data_response_event(wfc, (RDP_CB_DATA_RESPONSE_EVENT*) event);
			break;

		default:
			break;
	}
}

BOOL wf_cliprdr_process_selection_notify(wfContext* wfc, HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	return TRUE;
}

BOOL wf_cliprdr_process_selection_request(wfContext* wfc, HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	return TRUE;
}

BOOL wf_cliprdr_process_selection_clear(wfContext* wfc, HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	return TRUE;
}

BOOL wf_cliprdr_process_property_notify(wfContext* wfc, HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	return TRUE;
}

void wf_cliprdr_check_owner(wfContext* wfc)
{

}
