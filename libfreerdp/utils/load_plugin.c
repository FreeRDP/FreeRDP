/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Plugin Loading Utils
 *
 * Copyright 2011 Vic Lee
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
#include <winpr/windows.h>

#include <freerdp/utils/file.h>
#include <freerdp/utils/print.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/load_plugin.h>

#ifdef _WIN32

#define DLOPEN(f) LoadLibraryA(f)
#define DLSYM(f, n) GetProcAddress(f, n)
#define DLCLOSE(f) FreeLibrary(f)
#define DLERROR() ""

#else

#include <dlfcn.h>
#include <unistd.h>
#define DLOPEN(f) dlopen(f, RTLD_LOCAL | RTLD_LAZY)
#define DLSYM(f, n) dlsym(f, n)
#define DLCLOSE(f) dlclose(f)
#define DLERROR() dlerror()

#endif

#define MAX_STATIC_PLUGINS 50

struct static_plugin
{
	const char* name;
	const char* entry_name;
	void* entry_addr;
};
typedef struct static_plugin staticPlugin;

static staticPlugin g_static_plugins[MAX_STATIC_PLUGINS];
static int g_static_plugins_count;

/**
 * This function will load the specified library, retrieve the specified symbol in it, and return a pointer to it.
 * It is used in freerdp_load_plugin() and freerdp_load_channel_plugin().
 * It seems there is no way to unload the library once this call is made. Now since this is used for plugins,
 * we probably don't need to take care of unloading them, as it will be done only at shutdown.
 *
 * @param file [IN]	- library name
 * @param name [IN]	- symbol name to find in the library
 *
 * @return pointer to the referenced symbol. NULL if an error occured.
 */
void* freerdp_load_library_symbol(const char* file, const char* name)
{
	void* library;
	void* symbol;

	library = DLOPEN(file);

	if (library == NULL)
	{
		printf("freerdp_load_library_symbol: failed to open %s: %s\n", file, DLERROR());
		return NULL;
	}

	symbol = DLSYM(library, name);

	if (symbol == NULL)
	{
		printf("freerdp_load_library_symbol: failed to load %s: %s\n", file, DLERROR());
		return NULL;
	}

	return symbol;
}

/**
 * This function is used to load plugins. It will load the specified library, retrieve the specified symbol,
 * and return a pointer to it.
 * There is no way to unload the library once it has been loaded this way - since it is used for plugins only,
 * maybe it's not a must.
 *
 * @param name [IN]			- name of the library to load
 * @param entry_name [IN]	- name of the symbol to find and load from the library
 *
 * @return Pointer to the loaded symbol. NULL if an error occurred.
 */
void* freerdp_load_plugin(const char* name, const char* entry_name)
{
	char* path;
	void* entry;
	char* suffixed_name;

	/* first attempt to load a static plugin */
	entry = freerdp_load_static_plugin(name, entry_name);

	if (entry != NULL)
		return entry;

	suffixed_name = freerdp_append_shared_library_suffix((char*) name);

	if (!freerdp_path_contains_separator(suffixed_name))
	{
		/* no explicit path given, use default path */
		path = freerdp_construct_path(FREERDP_PLUGIN_PATH, suffixed_name);
	}
	else
	{
		/* explicit path given, use it instead of default path */
		path = _strdup(suffixed_name);
	}

	entry = freerdp_load_library_symbol(path, entry_name);

	free(suffixed_name);
	free(path);

	if (entry == NULL)
	{
		printf("freerdp_load_plugin: failed to load %s/%s\n", name, entry_name);
		return NULL;
	}

	return entry;
}

/**
 * This function is used to register a static plugin so that it can be loaded later on using freerdp_load_plugin.
 *
 * @param name [IN]		- name of the library to load
 * @param entry_name [IN]	- name of the symbol to register for later loading
 * @param entry [IN]		- function pointer to the entry function
 *
 * @return TRUE on success, otherwise FALSE.
 */
boolean freerdp_register_static_plugin(const char* name, const char* entry_name, void* entry_addr)
{
	staticPlugin* plugin;
  
	if (g_static_plugins_count >= MAX_STATIC_PLUGINS)
	{
		printf("freerdp_register_static_plugin: cannot register %s/%s", name, entry_name);
		return FALSE;
	}
  
	/* add the static plugin to the vector */
	plugin = &g_static_plugins[g_static_plugins_count++];
	plugin->name = name;
	plugin->entry_name = entry_name;
	plugin->entry_addr = entry_addr;
  
	return TRUE;
}

/**
 * This is a helper function, used by freerdp_load_plugin to return a function pointer from the static plugin table
 *
 * @param name [IN]		- name of the library to load
 * @param entry_name [IN]	- name of the symbol to register for later loading
 *
 * @return Pointer to the entry function, NULL if no matching plugin could be found
 */

void* freerdp_load_static_plugin(const char* name, const char* entry_name)
{
	int i;
	staticPlugin* plugin;

	for (i = 0; i < g_static_plugins_count; i++)
	{
		plugin = &g_static_plugins[i];

		if (!strcmp(plugin->name, name) && !strcmp(plugin->entry_name, entry_name))
		{
			return plugin->entry_addr;
		}
	}
  
	return NULL;
}
