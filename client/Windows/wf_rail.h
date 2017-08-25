/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2013-2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_CLIENT_WIN_RAIL_H
#define FREERDP_CLIENT_WIN_RAIL_H

typedef struct wf_rail_window wfRailWindow;

#include "wf_client.h"

#include <freerdp/client/rail.h>

struct wf_rail_window
{
	wfContext* wfc;

	HWND hWnd;

	DWORD dwStyle;
	DWORD dwExStyle;

	int x;
	int y;
	int width;
	int height;
	char* title;
};

BOOL wf_rail_init(wfContext* wfc, RailClientContext* rail);
void wf_rail_uninit(wfContext* wfc, RailClientContext* rail);

void wf_rail_invalidate_region(wfContext* wfc, REGION16* invalidRegion);

#endif /* FREERDP_CLIENT_WIN_RAIL_H */
