/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Proxy Server
 *
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

#ifndef FREERDP_SERVER_PROXY_MODULES_H
#define FREERDP_SERVER_PROXY_MODULES_H

#include <winpr/wtypes.h>
#include <winpr/collections.h>

#include "modules/modules_api.h"

typedef wArrayList modules_list;
typedef struct proxy_module proxyModule;

typedef enum _PF_FILTER_TYPE PF_FILTER_TYPE;
enum _PF_FILTER_TYPE
{
	FILTER_TYPE_KEYBOARD,
	FILTER_TYPE_MOUSE
};

typedef enum _PF_HOOK_TYPE PF_HOOK_TYPE;
enum _PF_HOOK_TYPE
{
	HOOK_TYPE_CLIENT_PRE_CONNECT,
	HOOK_TYPE_SERVER_CHANNELS_INIT,
	HOOK_TYPE_SERVER_CHANNELS_FREE,
};

struct proxy_module
{
	/* Handle to the loaded library. Used for freeing the library */
	HMODULE handle;

	char* name;
	BOOL enabled;
	moduleOperations* ops;
};

BOOL pf_modules_init(void);
BOOL pf_modules_register_new(const char* module_path, const char* module_name);

BOOL pf_modules_run_filter(PF_FILTER_TYPE type, rdpContext* server, void* param);
BOOL pf_modules_run_hook(PF_HOOK_TYPE type, rdpContext* context);

void pf_modules_free(void);

#endif /* FREERDP_SERVER_PROXY_MODULES_H */
