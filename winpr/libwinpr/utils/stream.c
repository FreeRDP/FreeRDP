/*
 * WinPR: Windows Portable Runtime
 * Stream Utils
 *
 * Copyright 2011 Vic Lee
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
#include <winpr/stream.h>

wStream* Stream_New(BYTE* buffer, size_t size)
{
	wStream* s;

	s = malloc(sizeof(wStream));

	if (s != NULL)
	{
		if (buffer)
			s->buffer = buffer;
		else
			s->buffer = (BYTE*) malloc(size);

		s->pointer = s->buffer;
		s->capacity = size;
		s->length = size;
	}

	return s;
}

void Stream_Free(wStream* s, BOOL bFreeBuffer)
{
	if (s != NULL)
	{
		if (bFreeBuffer)
		{
			if (s->buffer != NULL)
				free(s->buffer);
		}

		free(s);
	}
}

void Stream_FreeDetach(wStream* s)
{
	if (s != NULL)
		free(s);
}
