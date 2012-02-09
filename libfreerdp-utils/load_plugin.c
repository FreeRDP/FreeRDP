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

void* freerdp_load_plugin(const char* name, const char* entry_name)
{
	char* path;
	void* module;
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

	module = DLOPEN(path);

	if (module == NULL)
	{
		printf("freerdp_load_plugin: failed to open %s: %s\n", path, DLERROR());
		xfree(suffixed_name);
		xfree(path);
		return NULL;
	}

	entry = DLSYM(module, entry_name);

	if (entry == NULL)
	{
		printf("freerdp_load_plugin: failed to load %s: %s\n", path, DLERROR());
		xfree(suffixed_name);
		xfree(path);
		return NULL;
	}

	xfree(suffixed_name);
	xfree(path);

	return entry;
}
