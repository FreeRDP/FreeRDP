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
#include <freerdp/utils/load_plugin.h>

#ifdef _WIN32

#include <windows.h>
#define DLOPEN(f) LoadLibrary(f)
#define DLSYM(f, n) GetProcAddress(f, n)
#define DLCLOSE(f) FreeLibrary(f)
#define PATH_SEPARATOR '\\'
#define PLUGIN_EXT "dll"

#else

#include <dlfcn.h>
#include <unistd.h>
#define DLOPEN(f) dlopen(f, RTLD_LOCAL | RTLD_LAZY)
#define DLSYM(f, n) dlsym(f, n)
#define DLCLOSE(f) dlclose(f)
#define PATH_SEPARATOR '/'
#define PLUGIN_EXT "so"

#endif

void* freerdp_load_plugin(const char* name, const char* entry_name)
{
	char path[255];
	void* module;
	void* entry;

	if (strchr(name, PATH_SEPARATOR) == NULL)
	{
		snprintf(path, sizeof(path), PLUGIN_PATH "%c%s." PLUGIN_EXT, PATH_SEPARATOR, name);
	}
	else
	{
		strncpy(path, name, sizeof(path));
	}

	module = DLOPEN(path);
	if (module == NULL)
	{
		printf("freerdp_load_plugin: failed to open %s.\n", path);
		return NULL;
	}
	entry = DLSYM(module, entry_name);
	if (entry == NULL)
	{
		printf("freerdp_load_plugin: failed to load %s.\n", path);
		return NULL;
	}
	return entry;
}
