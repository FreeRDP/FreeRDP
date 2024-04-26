/**
 * WinPR: Windows Portable Runtime
 * Time Zone
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

#ifndef WINPR_TIMEZONE_IANA_ABBREV
#define WINPR_TIMEZONE_IANA_ABBREV

#include <winpr/wtypes.h>

/**! \brief returns a list of IANA names for a short timezone name
 *
 * \param abbrev The short name to look for
 * \param list The list to hold the const IANA names
 * \param listsize The size of the \b list. Set to 0 to only get the required size.
 *
 * \return The number of mappings found
 */
size_t TimeZoneIanaAbbrevGet(const char* abbrev, const char** list, size_t listsize);

#endif
