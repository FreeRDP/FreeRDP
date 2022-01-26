/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Audio Input Redirection Virtual Channel
 *
 * Copyright 2022 Armin Novak <anovak@thincast.com>
 * Copyright 2022 Thincast Technologies GmbH
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

#ifndef FREERDP_INT_AINPUT_COMMON_H
#define FREERDP_INT_AINPUT_COMMON_H

#include <assert.h>
#include <winpr/string.h>

#include <freerdp/channels/ainput.h>

static INLINE void ainput_append(char* buffer, size_t size, const char* what, BOOL separator)
{
	size_t have;
	size_t toadd;

	assert(buffer || (size == 0));
	assert(what);

	have = strnlen(buffer, size);
	toadd = strlen(what);
	if (have > 0)
		toadd += 1;

	if (size - have < toadd + 1)
		return;

	if (have > 0)
		strcat(buffer, separator ? "|" : " ");
	strcat(buffer, what);
}

static INLINE const char* ainput_flags_to_string(UINT64 flags, char* buffer, size_t size)
{
	char number[32] = { 0 };

	if (flags & AINPUT_FLAGS_HAVE_REL)
		ainput_append(buffer, size, "AINPUT_FLAGS_HAVE_REL", TRUE);
	if (flags & AINPUT_FLAGS_WHEEL)
		ainput_append(buffer, size, "AINPUT_FLAGS_WHEEL", TRUE);
	if (flags & AINPUT_FLAGS_MOVE)
		ainput_append(buffer, size, "AINPUT_FLAGS_MOVE", TRUE);
	if (flags & AINPUT_FLAGS_DOWN)
		ainput_append(buffer, size, "AINPUT_FLAGS_DOWN", TRUE);
	if (flags & AINPUT_FLAGS_REL)
		ainput_append(buffer, size, "AINPUT_FLAGS_REL", TRUE);
	if (flags & AINPUT_FLAGS_BUTTON1)
		ainput_append(buffer, size, "AINPUT_FLAGS_BUTTON1", TRUE);
	if (flags & AINPUT_FLAGS_BUTTON2)
		ainput_append(buffer, size, "AINPUT_FLAGS_BUTTON2", TRUE);
	if (flags & AINPUT_FLAGS_BUTTON3)
		ainput_append(buffer, size, "AINPUT_FLAGS_BUTTON3", TRUE);
	if (flags & AINPUT_XFLAGS_BUTTON1)
		ainput_append(buffer, size, "AINPUT_XFLAGS_BUTTON1", TRUE);
	if (flags & AINPUT_XFLAGS_BUTTON2)
		ainput_append(buffer, size, "AINPUT_XFLAGS_BUTTON2", TRUE);

	_snprintf(number, sizeof(number), "[0x%08" PRIx64 "]", flags);
	ainput_append(buffer, size, number, FALSE);

	return buffer;
}

#endif /* FREERDP_INT_AINPUT_COMMON_H */
