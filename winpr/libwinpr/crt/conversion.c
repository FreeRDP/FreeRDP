/**
 * WinPR: Windows Portable Runtime
 * Data Conversion
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
#include <winpr/string.h>

/* Data Conversion: http://msdn.microsoft.com/en-us/library/0heszx3w/ */

#ifndef _WIN32

errno_t _itoa_s(int value, char* buffer, size_t sizeInCharacters, int radix)
{
	int length;

	length = sprintf_s(NULL, 0, "%d", value);

	if (length < 0)
		return -1;

	if (sizeInCharacters < (size_t)length)
		return -1;

	sprintf_s(buffer, length + 1, "%d", value);

	return 0;
}

#endif
