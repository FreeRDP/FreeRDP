/**
 * FreeRDP: A Remote Desktop Protocol Client
 * FreeRDP Extension Plugin Interface
 *
 * Copyright 2010-2011 Vic Lee
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

#ifndef __EXTENSION_H
#define __EXTENSION_H

#include <freerdp/api.h>
#include <freerdp/freerdp.h>
#include <freerdp/extension.h>

#ifdef _WIN32
#include <windows.h>
#endif

#define FREERDP_EXT_MAX_COUNT 16

struct rdp_extension
{
	freerdp* instance;
	rdpExtPlugin* plugins[FREERDP_EXT_MAX_COUNT];
	int num_plugins;
	PFREERDP_EXTENSION_HOOK pre_connect_hooks[FREERDP_EXT_MAX_COUNT];
	rdpExtPlugin* pre_connect_hooks_instances[FREERDP_EXT_MAX_COUNT];
	int num_pre_connect_hooks;
	PFREERDP_EXTENSION_HOOK post_connect_hooks[FREERDP_EXT_MAX_COUNT];
	rdpExtPlugin* post_connect_hooks_instances[FREERDP_EXT_MAX_COUNT];
	int num_post_connect_hooks;
};
typedef struct rdp_extension rdpExtension;

FREERDP_API int extension_pre_connect(rdpExtension* extension);
FREERDP_API int extension_post_connect(rdpExtension* extension);

FREERDP_API rdpExtension* extension_new(freerdp* instance);
FREERDP_API void extension_free(rdpExtension* extension);

#endif /* __EXTENSION_H */

