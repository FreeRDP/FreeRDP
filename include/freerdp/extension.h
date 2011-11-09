/**
 * FreeRDP: A Remote Desktop Protocol Client
 * RDP Extensions
 *
 * Copyright 2010-2011 Vic Lee
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

#ifndef __RDP_EXTENSION_H
#define __RDP_EXTENSION_H

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/freerdp.h>

#define FREERDP_EXT_EXPORT_FUNC_NAME	"FreeRDPExtensionEntry"

typedef struct rdp_ext_plugin rdpExtPlugin;

struct rdp_ext_plugin
{
	void* ext;
	int (*init) (rdpExtPlugin* plugin, freerdp* instance);
	int (*uninit) (rdpExtPlugin* plugin, freerdp* instance);
};

typedef uint32 (FREERDP_CC* PFREERDP_EXTENSION_HOOK)(rdpExtPlugin* plugin, freerdp* instance);

typedef uint32 (FREERDP_CC* PREGISTEREXTENSION)(rdpExtPlugin* plugin);
typedef uint32 (FREERDP_CC* PREGISTERPRECONNECTHOOK)(rdpExtPlugin* plugin, PFREERDP_EXTENSION_HOOK hook);
typedef uint32 (FREERDP_CC* PREGISTERPOSTCONNECTHOOK)(rdpExtPlugin* plugin, PFREERDP_EXTENSION_HOOK hook);

struct _FREERDP_EXTENSION_ENTRY_POINTS
{
	void* ext; /* Reference to internal instance */
	PREGISTEREXTENSION pRegisterExtension;
	PREGISTERPRECONNECTHOOK pRegisterPreConnectHook;
	PREGISTERPOSTCONNECTHOOK pRegisterPostConnectHook;
	void* data;
};

typedef struct _FREERDP_EXTENSION_ENTRY_POINTS FREERDP_EXTENSION_ENTRY_POINTS;
typedef FREERDP_EXTENSION_ENTRY_POINTS* PFREERDP_EXTENSION_ENTRY_POINTS;

typedef int (FREERDP_CC* PFREERDP_EXTENSION_ENTRY)(PFREERDP_EXTENSION_ENTRY_POINTS pEntryPoints);

#endif /* __RDP_EXTENSION_H */

