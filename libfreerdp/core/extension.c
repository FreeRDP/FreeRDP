/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>

#include <freerdp/freerdp.h>

#include "extension.h"

#ifdef _WIN32
#define DLOPEN(f) LoadLibraryA(f)
#define DLSYM(f, n) GetProcAddress(f, n)
#define DLCLOSE(f) FreeLibrary(f)
#define PATH_SEPARATOR '\\'
#define PLUGIN_EXT "dll"
#else
#include <dlfcn.h>
#define DLOPEN(f) dlopen(f, RTLD_LOCAL | RTLD_LAZY)
#define DLSYM(f, n) dlsym(f, n)
#define DLCLOSE(f) dlclose(f)
#define PATH_SEPARATOR '/'

#ifdef __APPLE__
#define PLUGIN_EXT "dylib"
#else
#define PLUGIN_EXT "so"
#endif 

#endif

static UINT32 FREERDP_CC extension_register_plugin(rdpExtPlugin* plugin)
{
	rdpExtension* ext = (rdpExtension*) plugin->ext;

	if (ext->num_plugins >= FREERDP_EXT_MAX_COUNT)
	{
		fprintf(stderr, "extension_register_extension: maximum number of plugins reached.\n");
		return 1;
	}

	ext->plugins[ext->num_plugins++] = plugin;
	return 0;
}

static UINT32 FREERDP_CC extension_register_pre_connect_hook(rdpExtPlugin* plugin, PFREERDP_EXTENSION_HOOK hook)
{
	rdpExtension* ext = (rdpExtension*) plugin->ext;

	if (ext->num_pre_connect_hooks >= FREERDP_EXT_MAX_COUNT)
	{
		fprintf(stderr, "extension_register_pre_connect_hook: maximum plugin reached.\n");
		return 1;
	}

	ext->pre_connect_hooks[ext->num_pre_connect_hooks] = hook;
	ext->pre_connect_hooks_instances[ext->num_pre_connect_hooks] = plugin;
	ext->num_pre_connect_hooks++;
	return 0;
}

static UINT32 FREERDP_CC extension_register_post_connect_hook(rdpExtPlugin* plugin, PFREERDP_EXTENSION_HOOK hook)
{
	rdpExtension* ext = (rdpExtension*) plugin->ext;

	if (ext->num_post_connect_hooks >= FREERDP_EXT_MAX_COUNT)
	{
		fprintf(stderr, "extension_register_post_connect_hook: maximum plugin reached.\n");
		return 1;
	}

	ext->post_connect_hooks[ext->num_post_connect_hooks] = hook;
	ext->post_connect_hooks_instances[ext->num_post_connect_hooks] = plugin;
	ext->num_post_connect_hooks++;

	return 0;
}

static int extension_load_plugins(rdpExtension* extension)
{
	int i;
	void* han;
	char path[256];
	rdpSettings* settings;
	PFREERDP_EXTENSION_ENTRY entry;
	FREERDP_EXTENSION_ENTRY_POINTS entryPoints;

	settings = extension->instance->settings;

	entryPoints.ext = extension;
	entryPoints.pRegisterExtension = extension_register_plugin;
	entryPoints.pRegisterPreConnectHook = extension_register_pre_connect_hook;
	entryPoints.pRegisterPostConnectHook = extension_register_post_connect_hook;

	for (i = 0; settings->extensions[i].name[0]; i++)
	{
		if (strchr(settings->extensions[i].name, PATH_SEPARATOR) == NULL)
			sprintf_s(path, sizeof(path), EXT_PATH "/%s." PLUGIN_EXT, settings->extensions[i].name);
		else
			sprintf_s(path, sizeof(path), "%s", settings->extensions[i].name);

		han = DLOPEN(path);
		fprintf(stderr, "extension_load_plugins: %s\n", path);

		if (han == NULL)
		{
			fprintf(stderr, "extension_load_plugins: failed to load %s\n", path);
			continue;
		}

		entry = (PFREERDP_EXTENSION_ENTRY) DLSYM(han, FREERDP_EXT_EXPORT_FUNC_NAME);
		if (entry == NULL)
		{
			DLCLOSE(han);
			fprintf(stderr, "extension_load_plugins: failed to find export function in %s\n", path);
			continue;
		}

		entryPoints.data = extension->instance->settings->extensions[i].data;
		if (entry(&entryPoints) != 0)
		{
			DLCLOSE(han);
			fprintf(stderr, "extension_load_plugins: %s entry returns error.\n", path);
			continue;
		}
	}

	return 0;
}

static int extension_init_plugins(rdpExtension* extension)
{
	int i;

	for (i = 0; i < extension->num_plugins; i++)
		extension->plugins[i]->init(extension->plugins[i], extension->instance);

	return 0;
}

static int extension_uninit_plugins(rdpExtension* extension)
{
	int i;

	for (i = 0; i < extension->num_plugins; i++)
		extension->plugins[i]->uninit(extension->plugins[i], extension->instance);

	return 0;
}

/** Gets through all registered pre-connect hooks and executes them.
 *  @param extension - pointer to a rdpExtension structure
 *  @return 0 always
 */
int extension_pre_connect(rdpExtension* extension)
{
	int i;

	for (i = 0; i < extension->num_pre_connect_hooks; i++)
		extension->pre_connect_hooks[i](extension->pre_connect_hooks_instances[i], extension->instance);

	return 0;
}

/** Gets through all registered post-connect hooks and executes them.
 *  @param extension - pointer to a rdpExtension structure
 *  @return 0 always
 */
int extension_post_connect(rdpExtension* ext)
{
	int i;

	for (i = 0; i < ext->num_post_connect_hooks; i++)
		ext->post_connect_hooks[i](ext->post_connect_hooks_instances[i], ext->instance);

	return 0;
}

void extension_load_and_init_plugins(rdpExtension* extension)
{
	extension_load_plugins(extension);
	extension_init_plugins(extension);
}

rdpExtension* extension_new(freerdp* instance)
{
	rdpExtension* extension = NULL;

	if (instance != NULL)
	{
		extension = (rdpExtension*) malloc(sizeof(rdpExtension));
		ZeroMemory(extension, sizeof(rdpExtension));

		extension->instance = instance;
	}

	return extension;
}

void extension_free(rdpExtension* extension)
{
	if (extension != NULL)
	{
		extension_uninit_plugins(extension);
		free(extension);
	}
}
