/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Remote Applications Integrated Locally (RAIL)
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <freerdp/utils/stream.h>
#include <freerdp/utils/memory.h>

#include <freerdp/rail/rail.h>

static void rail_WindowCreate(rdpUpdate* update, WINDOW_ORDER_INFO* orderInfo, WINDOW_STATE_ORDER* window_state)
{
	rdpRail* rail;
	rail = (rdpRail*) update->rail;
	window_list_create(rail->list, orderInfo, window_state);
}

static void rail_WindowUpdate(rdpUpdate* update, WINDOW_ORDER_INFO* orderInfo, WINDOW_STATE_ORDER* window_state)
{
	rdpRail* rail;
	rail = (rdpRail*) update->rail;
	window_list_update(rail->list, orderInfo, window_state);
}

static void rail_WindowDelete(rdpUpdate* update, WINDOW_ORDER_INFO* orderInfo)
{
	rdpRail* rail;
	rail = (rdpRail*) update->rail;
	window_list_delete(rail->list, orderInfo);
}

void rail_register_update_callbacks(rdpRail* rail, rdpUpdate* update)
{
	update->WindowCreate = rail_WindowCreate;
	update->WindowUpdate = rail_WindowUpdate;
	update->WindowDelete = rail_WindowDelete;
}

rdpRail* rail_new()
{
	rdpRail* rail;

	rail = (rdpRail*) xzalloc(sizeof(rdpRail));

	if (rail != NULL)
	{
		rail->list = window_list_new(rail);
		rail->uniconv = freerdp_uniconv_new();
	}

	return rail;
}

void rail_free(rdpRail* rail)
{
	if (rail != NULL)
	{
		window_list_free(rail->list);
		freerdp_uniconv_free(rail->uniconv);
		xfree(rail);
	}
}
