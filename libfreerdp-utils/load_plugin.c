/**
 * FreeRDP: A Remote Desktop Protocol Client
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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

/**
 * UNUSED
 * This function opens a handle on the specified library in order to load symbols from it.
 * It is just a wrapper over dlopen(), but provides some logs in case of error.
 *
 * The returned pointer can be used to load a symbol from the library, using the freerdp_get_library_symbol() call.
 * The returned pointer should be closed using the freerdp_close_library() call.
 *
 * @see freerdp_get_library_symbol
 * @see freerdp_close_library
 *
 * @param file [IN]		- library name
 * @return Pointer to the loaded library. NULL if an error occurs.
 */
void* freerdp_open_library(const char* file)
{
	void* library;

	library = DLOPEN(file);

	if (library == NULL)
	{
		printf("freerdp_load_library: failed to open %s: %s\n", file, DLERROR());
		return NULL;
	}

	return library;
}

/**
 * UNUSED
 * This function retrieves a pointer to the specified symbol from the given (loaded) library.
 * It is a wrapper over the dlsym() function, but provides some logs in case of error.
 *
 * @see freerdp_open_library
 * @see freerdp_close_library
 *
 * @param library [IN]		- a valid pointer to the opened library.
 * 							  This pointer should come from a successful call to freerdp_open_library()
 * @param name [IN]			- name of the symbol that must be loaded
 *
 * @return A pointer to the loaded symbol. NULL if an error occured.
 */
void* freerdp_get_library_symbol(void* library, const char* name)
{
	void* symbol;

	symbol = DLSYM(library, name);

	if (symbol == NULL)
	{
		printf("freerdp_get_library_symbol: failed to load %s: %s\n", name, DLERROR());
		return NULL;
	}

	return symbol;
}

/**
 * UNUSED
 * This function closes a library handle that was previously opened by freerdp_open_library().
 * It is a wrapper over dlclose(), but provides logs in case of error.
 *
 * @see freerdp_open_library
 * @see freerdp_get_library_symbol
 *
 * @return true if the close succeeded. false otherwise.
 */
boolean freerdp_close_library(void* library)
{
	int status;

	status = DLCLOSE(library);

#ifdef _WIN32
	if (status != 0)
#else
	if (status == 0)
#endif
	{
		printf("freerdp_free_library: failed to close: %s\n", DLERROR());
		return false;
	}

	return true;
}

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

	suffixed_name = freerdp_append_shared_library_suffix((char*) name);

	if (!freerdp_path_contains_separator(suffixed_name))
	{
		/* no explicit path given, use default path */
		path = freerdp_construct_path(FREERDP_PLUGIN_PATH, suffixed_name);
	}
	else
	{
		/* explicit path given, use it instead of default path */
		path = xstrdup(suffixed_name);
	}

	entry = freerdp_load_library_symbol(path, entry_name);

	xfree(suffixed_name);
	xfree(path);

	if (entry == NULL)
	{
		printf("freerdp_load_plugin: failed to load %s/%s\n", name, entry_name);
		return NULL;
	}

	return entry;
}

/**
 * UNUSED
 * This function was meant to be used to load channel plugins.
 * It was initially called from freerdp_channels_load_plugin() but now we use the freerdp_load_plugin() function
 * which is more generic.
 *
 */
void* freerdp_load_channel_plugin(rdpSettings* settings, const char* name, const char* entry_name)
{
	char* path;
	void* entry;
	char* suffixed_name;

	suffixed_name = freerdp_append_shared_library_suffix((char*) name);

	if (!freerdp_path_contains_separator(suffixed_name))
	{
		/* no explicit path given, use default path */

		if (!settings->development_mode)
		{
			path = freerdp_construct_path(FREERDP_PLUGIN_PATH, suffixed_name);
		}
		else
		{
			char* dot;
			char* plugin_name;
			char* channels_path;
			char* channel_subpath;

			dot = strrchr(suffixed_name, '.');
			plugin_name = xmalloc((dot - suffixed_name) + 1);
			strncpy(plugin_name, suffixed_name, (dot - suffixed_name));
			plugin_name[(dot - suffixed_name)] = '\0';

			channels_path = freerdp_construct_path(settings->development_path, "channels");
			channel_subpath = freerdp_construct_path(channels_path, plugin_name);

			path = freerdp_construct_path(channel_subpath, suffixed_name);

			xfree(plugin_name);
			xfree(channels_path);
			xfree(channel_subpath);
		}
	}
	else
	{
		/* explicit path given, use it instead of default path */
		path = xstrdup(suffixed_name);
	}

	entry = freerdp_load_library_symbol(path, entry_name);

	xfree(suffixed_name);
	xfree(path);

	if (entry == NULL)
	{
		printf("freerdp_load_channel_plugin: failed to load %s/%s\n", name, entry_name);
		return NULL;
	}

	return entry;
}
