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

rdpRail* rail_new()
{
	rdpRail* rail;

	rail = (rdpRail*) xzalloc(sizeof(rdpRail));

	if (rail != NULL)
	{
		rail->list = window_list_new();
	}

	return rail;
}

void rail_free(rdpRail* rail)
{
	if (rail != NULL)
	{
		xfree(rail);
	}
}
