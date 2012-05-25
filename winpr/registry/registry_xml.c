/**
 * WinPR: Windows Portable Runtime
 * Windows Registry
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include "registry_xml.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>

#ifndef _WIN32
#define PATH_SEPARATOR_STR	"/"
#define PATH_SEPARATOR_CHR	'/'
#define HOME_ENV_VARIABLE	"HOME"
#else
#define PATH_SEPARATOR_STR	"\\"
#define PATH_SEPARATOR_CHR	'\\'
#define HOME_ENV_VARIABLE	"HOME"
#endif

char* construct_path(char* base_path, char* relative_path)
{
	char* path;
	int length;
	int base_path_length;
	int relative_path_length;

	base_path_length = strlen(base_path);
	relative_path_length = strlen(relative_path);
	length = base_path_length + relative_path_length + 1;

	path = malloc(length + 1);
	sprintf(path, "%s" PATH_SEPARATOR_STR "%s", base_path, relative_path);

	return path;
}

char* find_registry_path()
{
	char* path;
	char* home_path;

	home_path = getenv("HOME");
	path = construct_path(home_path, "registry.xml");

	return path;
}

RegistryXml* registry_xml_new()
{
	char* filename;
	RegistryXml* registry;

	registry = (RegistryXml*) malloc(sizeof(RegistryXml));
	ZeroMemory(registry, sizeof(RegistryXml));

	filename = find_registry_path();
	registry->doc = xmlParseFile(filename);

	if (registry->doc == NULL)
	{
		fprintf(stderr, "xmlParseFile error\n");
		return NULL;
	}

	registry->node = xmlDocGetRootElement(registry->doc);

	if (registry->node == NULL)
	{
		fprintf(stderr, "empty document\n");
		xmlFreeDoc(registry->doc);
		return NULL;
	}

	if (xmlStrcmp(registry->node->name, (const xmlChar*) "registry"))
	{
		fprintf(stderr, "document of the wrong type, root node != registry");
		xmlFreeDoc(registry->doc);
		return NULL;
	}

	registry->node = registry->node->xmlChildrenNode;

	while (registry->node != NULL)
	{
		if ((!xmlStrcmp(registry->node->name, (const xmlChar*) "key")))
		{
			//read_registry_key(doc, node, 0);
		}

		registry->node = registry->node->next;
	}

	return NULL;
}

static RegistryXml* instance = NULL;

RegistryXml* registry_xml_open()
{
	if (instance == NULL)
		instance = registry_xml_new();

	return instance;
}

void registry_xml_close(RegistryXml* registry)
{
	xmlFreeDoc(registry->doc);
}
