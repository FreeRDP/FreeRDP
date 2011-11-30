/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Registry Utils
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

#include <freerdp/utils/file.h>

#include <freerdp/utils/registry.h>

static const char registry_dir[] = "freerdp";
static const char registry_file[] = "config.txt";

static REG_SECTION global[] =
{
	{ REG_TYPE_SECTION, "global",			0,  NULL },
	{ REG_TYPE_BOOLEAN, "fast_path",		1,  "1" },
	{ REG_TYPE_STRING,  "resolution",		8,  "1024x768" },
	{ REG_TYPE_INTEGER, "performance_flags",	4,  "0xFFFF" },
	{ REG_TYPE_NONE,    "",				0,  NULL }
};

static REG_SECTION licensing[] =
{
	{ REG_TYPE_SECTION, "licensing",	0,  NULL },
	{ REG_TYPE_STRING,  "platform_id",	1,  "0x000201" },
	{ REG_TYPE_STRING,  "hardware_id",	16, "0xe107d9d372bb6826bd81d3542a419d6" },
	{ REG_TYPE_NONE,    "",			0,  NULL }
};

static REG_SECTION* sections[] =
{
	(REG_SECTION*) &global,
	(REG_SECTION*) &licensing,
	(REG_SECTION*) NULL
};

void registry_print_entry(REG_ENTRY* entry, FILE* fp)
{
	uint8* value;
	value = (uint8*) entry->value;
	fprintf(fp, "%s = %s\n", entry->name, value);
}

void registry_print_section(REG_SECTION* section, FILE* fp)
{
	int i = 0;
	REG_ENTRY* entries = (REG_ENTRY*) &section[1];

	fprintf(fp, "\n");
	fprintf(fp, "[%s]\n", section->name);

	while (entries[i].type != REG_TYPE_NONE)
	{
		registry_print_entry(&entries[i], fp);
		i++;
	}
}

void registry_print(rdpRegistry* registry, FILE* fp)
{
	int i = 0;

	fprintf(fp, "# FreeRDP Configuration Registry\n");

	while (sections[i] != NULL)
	{
		registry_print_section(sections[i], fp);
		i++;
	}

	fprintf(fp, "\n");
}

void registry_create(rdpRegistry* registry)
{
	registry->fp = fopen((char*)registry->file, "w+");

	if (registry->fp == NULL)
	{
		printf("registry_create: error opening [%s] for writing\n", registry->file);
		return;
	}

	registry_print(registry, registry->fp);
	fflush(registry->fp);
}

void registry_load(rdpRegistry* registry)
{
	registry->fp = fopen((char*) registry->file, "r+");
}

void registry_open(rdpRegistry* registry)
{
	struct stat stat_info;

	if (stat((char*)registry->file, &stat_info) != 0)
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

	if (registry->settings->home_path == NULL)
		home_path = getenv("HOME");
	else
		home_path = registry->settings->home_path;

	if (home_path == NULL)
	{
		printf("could not get home path\n");
		registry->available = false;
		return;
	}

	registry->available = true;

	if (home_path == NULL)
	{
		printf("could not get home path\n");
		registry->available = false;
		return;
	}

	registry->home = (char*) xstrdup(home_path);
	printf("home path: %s\n", registry->home);

	registry->path = (char*) xmalloc(strlen(registry->home) + strlen("/.") + strlen(registry_dir) + 1);
	sprintf(registry->path, "%s/.%s", registry->home, registry_dir);
	printf("registry path: %s\n", registry->path);

	if (stat(registry->path, &stat_info) != 0)
	{
		freerdp_mkdir(registry->path);
		printf("creating directory %s\n", registry->path);
	}

	length = strlen(registry->path);
	registry->file = (char*) xmalloc(strlen(registry->path) + strlen("/") + strlen(registry_file) + 1);
	sprintf(registry->file, "%s/%s", registry->path, registry_file);
	printf("registry file: %s\n", registry->file);

	registry_open(registry);
}

rdpRegistry* registry_new(rdpSettings* settings)
{
	rdpRegistry* registry = (rdpRegistry*) xzalloc(sizeof(rdpRegistry));

	if (registry != NULL)
	{
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
