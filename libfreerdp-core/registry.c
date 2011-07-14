/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Configuration Registry
 *
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

#include "registry.h"

static uint8 registry_dir[] = "freerdp";
static uint8 registry_file[] = "config.txt";

REG_ENTRY resolution =
{
	REG_TYPE_STRING, "resolution", 8, "1024x768"
};

REG_ENTRY fast_path =
{
	REG_TYPE_BOOLEAN, "fast_path", 1, "1"
};

REG_ENTRY performance_flags =
{
	REG_TYPE_INTEGER, "performance_flags", 4, "0xFFFF"
};

REG_SECTION root =
{
	REG_TYPE_SECTION, "root", 0, NULL
};

void registry_print_entry(REG_ENTRY* entry)
{
	uint8* value;
	value = (uint8*) entry->value;
	printf("%s = %s\n", entry->name, value);
}

void registry_print_section(REG_SECTION* section)
{
	printf("[%s]\n", section->name);
	registry_print_entry(&resolution);
	registry_print_entry(&fast_path);
	registry_print_entry(&performance_flags);
}

void registry_print(rdpRegistry* registry)
{
	registry_print_section(registry->root);
}

void registry_create(rdpRegistry* registry)
{
	registry->fp = fopen(registry->file, "w+");
}

void registry_load(rdpRegistry* registry)
{
	registry->fp = fopen(registry->file, "r+");
}

void registry_open(rdpRegistry* registry)
{
	struct stat stat_info;

	if (stat(registry->file, &stat_info) != 0)
		registry_create(registry);
	else
		registry_load(registry);
}

void registry_close(rdpRegistry* registry)
{
	if (registry->fp != NULL)
		fclose(registry->fp);
}

void registry_init(rdpRegistry* registry)
{
	int length;
	char* home_path;
	struct stat stat_info;

	home_path = getenv("HOME");
	registry->available = True;

	if (home_path == NULL)
	{
		printf("could not get home path\n");
		registry->available = False;
		return;
	}

	length = strlen(home_path);
	registry->home = (uint8*) xmalloc(length);
	memcpy(registry->home, home_path, length);
	printf("home path: %s\n", registry->home);

	registry->path = (uint8*) xmalloc(length + sizeof(registry_dir) + 2);
	sprintf(registry->path, "%s/.%s", registry->home, registry_dir);
	printf("registry path: %s\n", registry->path);

	if (stat(registry->path, &stat_info) != 0)
	{
		mkdir(registry->path, S_IRUSR | S_IWUSR | S_IXUSR);
		printf("creating directory %s\n", registry->path);
	}

	length = strlen(registry->path);
	registry->file = (uint8*) xmalloc(length + sizeof(registry_file) + 1);
	sprintf(registry->file, "%s/%s", registry->path, registry_file);
	printf("registry file: %s\n", registry->file);

	registry_open(registry);

	registry_print(registry);
}

rdpRegistry* registry_new(rdpSettings* settings)
{
	rdpRegistry* registry = (rdpRegistry*) xzalloc(sizeof(rdpRegistry));

	if (registry != NULL)
	{
		registry->root = &root;
		registry->settings = settings;
		registry_init(registry);
	}

	return registry;
}

void registry_free(rdpRegistry* registry)
{
	if (registry != NULL)
	{
		registry_close(registry);
		xfree(registry->path);
		xfree(registry->file);
		xfree(registry->home);
		xfree(registry);
	}
}
