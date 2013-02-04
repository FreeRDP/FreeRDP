/**
 * WinPR: Windows Portable Runtime
 * Buffer Manipulation
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>

/* Buffer Manipulation: http://msdn.microsoft.com/en-us/library/b3893xdy/ */

#ifndef _WIN32

#include <string.h>

errno_t memmove_s(void* dest, size_t numberOfElements, const void* src, size_t count)
{
	if (count > numberOfElements)
		return -1;

	memmove(dest, src, count);

	return 0;
}

errno_t wmemmove_s(WCHAR* dest, size_t numberOfElements, const WCHAR* src, size_t count)
{
	if (count * 2 > numberOfElements)
		return -1;

	memmove(dest, src, count * 2);

	return 0;
}

#endif
