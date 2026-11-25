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

#ifndef WINPR_TIME_ZONE_IANA_ALIASES_H_
#define WINPR_TIME_ZONE_IANA_ALIASES_H_

#include <winpr/wtypes.h>

/* A zone IANA has renamed: its current name and CLDR's (legacy) name for it */
typedef struct
{
	char* Current;
	char* Legacy;
} TimeZoneIanaAliasEntry;

/* The other spelling of a renamed zone, or nullptr */
WINPR_ATTR_NODISCARD
const char* TimeZoneIanaAlias(const char* iana);

#endif /* WINPR_TIME_ZONE_IANA_ALIASES_H_ */
