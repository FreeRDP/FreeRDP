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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freerdp/utils/file.h>
#include <freerdp/utils/print.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/load_plugin.h>

#ifdef _WIN32

#include <windows.h>
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

void* freerdp_load_plugin(const char* name, const char* entry_name)
{
	char* path;
	void* entry;
	char* suffixed_name;

	suffixed_name = freerdp_append_shared_library_suffix((char*) name);

	if (!freerdp_path_contains_separator(suffixed_name))
	{
		/* no explicit path given, use default path */
		path = freerdp_construct_path(PLUGIN_PATH, suffixed_name);
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
			path = freerdp_construct_path(PLUGIN_PATH, suffixed_name);
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
