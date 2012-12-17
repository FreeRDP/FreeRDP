/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Unicode Utils
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <wctype.h>
#include <freerdp/types.h>
#include <winpr/print.h>

#include <freerdp/utils/unicode.h>

#include <winpr/crt.h>

int freerdp_AsciiToUnicodeAlloc(const CHAR* str, WCHAR** wstr, int length)
{
	int status;

	*wstr = NULL;

	if (!str)
		return 0;

	if (length < 1)
		length = -1;

	status = ConvertToUnicode(CP_UTF8, 0, str, length, wstr, 0);

	return status;
}
