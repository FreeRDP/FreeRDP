/**
 * WinPR: Windows Portable Runtime
 * Renamed IANA time zones
 *
 * Copyright 2026 Island Technology, Inc.
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
#include <winpr/config.h>

#include <string.h>

#include "TimeZoneIanaAliases.h"

#include "TimeZoneIanaAliases_static.h"

const char* TimeZoneIanaAlias(const char* iana)
{
	if (!iana)
		return nullptr;

	for (size_t x = 0; x < TimeZoneIanaAliasesSize; x++)
	{
		const TimeZoneIanaAliasEntry* alias = &TimeZoneIanaAliases[x];
		if (strcmp(iana, alias->Current) == 0)
			return alias->Legacy;
		if (strcmp(iana, alias->Legacy) == 0)
			return alias->Current;
	}
	return nullptr;
}
