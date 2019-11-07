/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Event Handling
 *
 * Copyright 2009-2011 Jay Sorg
 * Copyright 2010-2011 Vic Lee
 * Copyright 2010-2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_CLIENT_WIN_EVENT_H
#define FREERDP_CLIENT_WIN_EVENT_H

#include "wf_client.h"
#include <freerdp/log.h>

LRESULT CALLBACK wf_ll_kbd_proc(int nCode, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK wf_event_proc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

void wf_event_focus_in(wfContext* wfc);

#define KBD_TAG CLIENT_TAG("windows")
#ifdef WITH_DEBUG_KBD
#define DEBUG_KBD(...) WLog_DBG(KBD_TAG, __VA_ARGS__)
#else
#define DEBUG_KBD(...) \
	do                 \
	{                  \
	} while (0)
#endif

#endif /* FREERDP_CLIENT_WIN_EVENT_H */
