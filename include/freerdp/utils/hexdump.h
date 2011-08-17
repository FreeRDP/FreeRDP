/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Hex Dump Utils
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

#ifndef __UTILS_HEXDUMP_H
#define __UTILS_HEXDUMP_H

#include <freerdp/api.h>

#define FREERDP_HEXDUMP_LINE_LENGTH	16

FREERDP_API void freerdp_hexdump(uint8* data, int length);

#endif /* __UTILS_HEXDUMP_H */
