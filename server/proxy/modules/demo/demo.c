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

#include <stdio.h>

#include "modules_api.h"

static BOOL demo_filter_keyboard_event(moduleOperations* module, rdpContext* context, void* param)
{
	proxyKeyboardEventInfo* event_data = (proxyKeyboardEventInfo*) param;
	WINPR_UNUSED(event_data);

	return TRUE;
}

static BOOL demo_filter_mouse_event(moduleOperations* module, rdpContext* context, void* param)
{
 	proxyMouseEventInfo* event_data = (proxyMouseEventInfo*) param;

	if (event_data->x % 100 == 0)
	{
		module->AbortConnect(module, context);
		printf("filter_demo: mouse x is currently %"PRIu16"\n", event_data->x);
	}

	return TRUE;
}

BOOL module_init(moduleOperations* module)
{
	module->KeyboardEvent = demo_filter_keyboard_event;
	module->MouseEvent = demo_filter_mouse_event;

	return TRUE;
}

BOOL module_exit(moduleOperations* module)
{
	printf("bye bye\n");

	return TRUE;
}
