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

#include <freerdp/freerdp.h>
#include <freerdp/extension.h>

#include "rdp.h"

#define FREERDP_EXT_MAX_COUNT 16

struct rdp_ext
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
typedef struct rdp_ext rdpExt;

int freerdp_ext_pre_connect(rdpExt* ext);
int freerdp_ext_post_connect(rdpExt* ext);

rdpExt* freerdp_ext_new(rdpRdp* rdp);
void freerdp_ext_free(rdpExt* ext);

#endif /* __EXTENSION_H */

