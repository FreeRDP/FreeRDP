/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Proxy Server
 *
 * Copyright 2019 Kobi Mizrachi <kmizrachi18@gmail.com>
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

#include "filters_api.h"

static PF_FILTER_RESULT demo_filter_keyboard_event(connectionInfo* info, void* param)
{
	proxyKeyboardEventInfo* event_data = (proxyKeyboardEventInfo*) param;
	WINPR_UNUSED(event_data);

	return FILTER_PASS;
}

static PF_FILTER_RESULT demo_filter_mouse_event(connectionInfo* info, void* param)
{
 	proxyMouseEventInfo* event_data = (proxyMouseEventInfo*) param;

	if (event_data->x % 100 == 0)
	{
		return FILTER_DROP;
	}

	return FILTER_PASS;
}

BOOL filter_init(proxyEvents* events)
{
	events->KeyboardEvent = demo_filter_keyboard_event;
	events->MouseEvent = demo_filter_mouse_event;

	return TRUE;
}
