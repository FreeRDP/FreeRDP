/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Proxy Server
 *
 * Copyright 2019 Mati Shabtay <matishabtay@gmail.com>
 * Copyright 2019 Kobi Mizrachi <kmizrachi18@gmail.com>
 * Copyright 2019 Idan Freiberg <speidy@gmail.com>
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

#include <assert.h>

#include <winpr/wlog.h>
#include <winpr/library.h>
#include <freerdp/api.h>

#include "pf_log.h"
#include "pf_filters.h"

#define TAG PROXY_TAG("filters")
#define FILTER_INIT_METHOD "filter_init"

static const char* FILTER_RESULT_STRINGS[] =
{
	"FILTER_PASS",
	"FILTER_DROP",
	"FILTER_IGNORE",
};

static const char* EVENT_TYPE_STRINGS[] =
{
	"KEYBOARD_EVENT",
	"MOUSE_EVENT",
};

static const char* pf_filters_get_filter_result_string(PF_FILTER_RESULT result)
{
	if (result >= FILTER_PASS && result <= FILTER_IGNORE)
		return FILTER_RESULT_STRINGS[result];
	else
		return "FILTER_UNKNOWN";
}

static const char* pf_filters_get_event_type_string(PF_FILTER_TYPE result)
{
	if (result >= FILTER_TYPE_KEYBOARD && result <= FILTER_TYPE_MOUSE)
		return EVENT_TYPE_STRINGS[result];
	else
		return "EVENT_UNKNOWN";
}

BOOL pf_filters_init(filters_list** list)
{
	if (list == NULL)
	{
		WLog_ERR(TAG, "pf_filters_init(): list == NULL");
		return FALSE;
	}

	*list = ArrayList_New(FALSE);

	if (*list == NULL)
	{
		WLog_ERR(TAG, "pf_filters_init(): ArrayList_New failed!");
		return FALSE;
	}

	return TRUE;
}

PF_FILTER_RESULT pf_filters_run_by_type(filters_list* list, PF_FILTER_TYPE type,
                                        connectionInfo* info,
                                        void* param)
{
	proxyFilter* filter;
	proxyEvents* events;
	PF_FILTER_RESULT result = FILTER_PASS;
	const size_t count = (size_t) ArrayList_Count(list);
	size_t index;

	for (index = 0; index < count; index++)
	{
		filter = (proxyFilter*) ArrayList_GetItem(list, index);
		events = filter->events;
		WLog_DBG(TAG, "pf_filters_run_by_type(): Running filter: %s", filter->name);

		switch (type)
		{
			case FILTER_TYPE_KEYBOARD:
				IFCALLRET(events->KeyboardEvent, result, info, param);
				break;

			case FILTER_TYPE_MOUSE:
				IFCALLRET(events->MouseEvent, result, info, param);
				break;
		}

		if (result != FILTER_PASS)
		{
			/* Filter returned FILTER_DROP or FILTER_IGNORE. There's no need to call next filters. */
			WLog_INFO(TAG, "Filter %s [%s] returned %s", filter->name,
			          pf_filters_get_event_type_string(type), pf_filters_get_filter_result_string(result));
			return result;
		}
	}

	/* all filters returned FILTER_PASS */
	return FILTER_PASS;
}

static void pf_filters_filter_free(proxyFilter* filter)
{
	if (!filter)
		return;

	if (filter->handle)
		FreeLibrary(filter->handle);

	free(filter->name);
	free(filter->events);
	free(filter);
}

void pf_filters_unregister_all(filters_list* list)
{
	size_t count;
	size_t index;

	if (list == NULL)
		return;
	
	count = (size_t) ArrayList_Count(list);

	for (index = 0; index < count; index++)
	{
		proxyFilter* filter = (proxyFilter*) ArrayList_GetItem(list, index);
		WLog_DBG(TAG, "pf_filters_unregister_all(): freeing filter: %s", filter->name);
		pf_filters_filter_free(filter);
	}

	ArrayList_Free(list);
}

BOOL pf_filters_register_new(filters_list* list, const char* module_path, const char* filter_name)
{
	proxyEvents* events = NULL;
	proxyFilter* filter = NULL;
	HMODULE handle = NULL;
	filterInitFn fn;

	assert(list != NULL);
	handle = LoadLibraryA(module_path);

	if (handle == NULL)
	{
		WLog_ERR(TAG, "pf_filters_register_new(): failed loading external module: %s", module_path);
		return FALSE;
	}

	if (!(fn = (filterInitFn) GetProcAddress(handle, FILTER_INIT_METHOD)))
	{
		WLog_ERR(TAG, "pf_filters_register_new(): GetProcAddress failed while loading %s", module_path);
		goto error;
	}

	filter = (proxyFilter*) malloc(sizeof(proxyFilter));

	if (filter == NULL)
	{
		WLog_ERR(TAG, "pf_filters_register_new(): malloc failed");
		goto error;
	}

	events = calloc(1, sizeof(proxyEvents));

	if (events == NULL)
	{
		WLog_ERR(TAG, "pf_filters_register_new(): calloc proxyEvents failed");
		goto error;
	}

	if (!fn(events))
	{
		WLog_ERR(TAG, "pf_filters_register_new(): failed calling external filter_init: %s", module_path);
		goto error;
	}

	filter->handle = handle;
	filter->name = _strdup(filter_name);
	filter->events = events;
	filter->enabled = TRUE;

	if (ArrayList_Add(list, filter) < 0)
	{
		WLog_ERR(TAG, "pf_filters_register_new(): failed adding filter to list: %s", module_path);
		goto error;
	}

	return TRUE;
error:

	if (handle)
		FreeLibrary(handle);

	pf_filters_filter_free(filter);
	return FALSE;
}
