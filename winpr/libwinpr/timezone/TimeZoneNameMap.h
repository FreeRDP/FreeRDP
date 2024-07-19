/**
 * WinPR: Windows Portable Runtime
 * Time Zone Name Map
 *
 * Copyright 2024 Armin Novak <anovak@thincast.com>
 * Copyright 2024 Thincast Technologies GmbH
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

#ifndef WINPR_TIME_NAME_MAP_H_
#define WINPR_TIME_NAME_MAP_H_

#include <winpr/wtypes.h>

typedef enum
{
	TIME_ZONE_NAME_ID,
	TIME_ZONE_NAME_STANDARD,
	TIME_ZONE_NAME_DISPLAY,
	TIME_ZONE_NAME_DAYLIGHT,
	TIME_ZONE_NAME_IANA,
} TimeZoneNameType;

typedef struct
{
	char* Id;
	char* StandardName;
	char* DisplayName;
	char* DaylightName;
	char* Iana;
} TimeZoneNameMapEntry;

const TimeZoneNameMapEntry* TimeZoneGetAt(size_t index);
const char* TimeZoneIanaToWindows(const char* iana, TimeZoneNameType type);

#endif
