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

#ifndef FREERDP_SERVER_PROXY_FILTERS_H
#define FREERDP_SERVER_PROXY_FILTERS_H

#include <winpr/wtypes.h>
#include <winpr/collections.h>

#include "filters/filters_api.h"

/* filter init method */
typedef BOOL (*filterInitFn)(proxyEvents* events);

typedef wArrayList filters_list;
typedef struct proxy_filter proxyFilter;

typedef enum _PF_FILTER_TYPE PF_FILTER_TYPE;
enum _PF_FILTER_TYPE
{
	FILTER_TYPE_KEYBOARD,
	FILTER_TYPE_MOUSE
};

struct proxy_filter
{
	/* Handle to the loaded library. Used for freeing the library */
	HMODULE handle;

	char* name;
	BOOL enabled;
	proxyEvents* events;
};

BOOL pf_filters_init(filters_list** list);
BOOL pf_filters_register_new(filters_list* list, const char* module_path, const char* filter_name);
PF_FILTER_RESULT pf_filters_run_by_type(filters_list* list, PF_FILTER_TYPE type,
                                        connectionInfo* info,
                                        void* param);
void pf_filters_unregister_all(filters_list* list);

#define RUN_FILTER(_filters,_type,_conn_info,_event_info,_ret,_cb,...) do { \
			switch(pf_filters_run_by_type(_filters,_type,_conn_info,_event_info)) { \
				case FILTER_PASS:           \
					_ret = _cb(__VA_ARGS__);       \
					break; \
				case FILTER_IGNORE:       \
					_ret = TRUE;   \
					break; \
				case FILTER_DROP:       \
				default: \
					_ret = FALSE;   \
			} \
		} while(0)

#endif /* FREERDP_SERVER_PROXY_FILTERS_H */
