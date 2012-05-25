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

#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

struct _registry_xml
{
	xmlDocPtr doc;
	xmlNodePtr node;
};
typedef struct _registry_xml RegistryXml;

RegistryXml* registry_xml_new();
RegistryXml* registry_xml_open();
void registry_xml_close(RegistryXml* registry);
