/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Time Zone Redirection
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

#ifndef __LOCALE_TIMEZONE_H
#define __LOCALE_TIMEZONE_H

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/settings.h>

FREERDP_API void freerdp_time_zone_detect(TIME_ZONE_INFO* clientTimeZone);

#endif /* __LOCALE_TIMEZONE_H */
