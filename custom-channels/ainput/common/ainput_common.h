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

#include <winpr/string.h>

#include <freerdp/channels/ainput.h>

static INLINE const char* ainput_flags_to_string(UINT64 flags, char* buffer, size_t size)
{
	char number[32] = { 0 };

	if (flags & AINPUT_FLAGS_HAVE_REL)
		winpr_str_append("AINPUT_FLAGS_HAVE_REL", buffer, size, "|");
	if (flags & AINPUT_FLAGS_WHEEL)
		winpr_str_append("AINPUT_FLAGS_WHEEL", buffer, size, "|");
	if (flags & AINPUT_FLAGS_MOVE)
		winpr_str_append("AINPUT_FLAGS_MOVE", buffer, size, "|");
	if (flags & AINPUT_FLAGS_DOWN)
		winpr_str_append("AINPUT_FLAGS_DOWN", buffer, size, "|");
	if (flags & AINPUT_FLAGS_REL)
		winpr_str_append("AINPUT_FLAGS_REL", buffer, size, "|");
	if (flags & AINPUT_FLAGS_BUTTON1)
		winpr_str_append("AINPUT_FLAGS_BUTTON1", buffer, size, "|");
	if (flags & AINPUT_FLAGS_BUTTON2)
		winpr_str_append("AINPUT_FLAGS_BUTTON2", buffer, size, "|");
	if (flags & AINPUT_FLAGS_BUTTON3)
		winpr_str_append("AINPUT_FLAGS_BUTTON3", buffer, size, "|");
	if (flags & AINPUT_XFLAGS_BUTTON1)
		winpr_str_append("AINPUT_XFLAGS_BUTTON1", buffer, size, "|");
	if (flags & AINPUT_XFLAGS_BUTTON2)
		winpr_str_append("AINPUT_XFLAGS_BUTTON2", buffer, size, "|");

	_snprintf(number, sizeof(number), "[0x%08" PRIx64 "]", flags);
	winpr_str_append(number, buffer, size, " ");

	return buffer;
}

#endif /* FREERDP_INT_AINPUT_COMMON_H */
