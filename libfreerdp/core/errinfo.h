/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Error Info
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

#ifndef FREERDP_LIB_CORE_ERRINFO_H
#define FREERDP_LIB_CORE_ERRINFO_H

#include <freerdp/freerdp.h>
#include <freerdp/api.h>

typedef struct
{
	UINT32 code;
	const char* name;
	const char* info;
	const char* category;
} ERRINFO;

FREERDP_LOCAL void rdp_print_errinfo(UINT32 code);

#endif /* FREERDP_LIB_CORE_ERRINFO_H */
