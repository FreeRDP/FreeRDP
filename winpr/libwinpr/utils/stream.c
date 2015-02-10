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

void Stream_EnsureCapacity(wStream* s, size_t size)
{
	if (s->capacity < size)
	{
		size_t position;
		size_t old_capacity;
		size_t new_capacity;

		old_capacity = s->capacity;
		new_capacity = old_capacity;

		do
		{
			new_capacity *= 2;
		}
		while (new_capacity < size);

		s->capacity = new_capacity;
		s->length = new_capacity;

		position = Stream_GetPosition(s);

		s->buffer = (BYTE*) realloc(s->buffer, s->capacity);

		ZeroMemory(&s->buffer[old_capacity], s->capacity - old_capacity);

		Stream_SetPosition(s, position);
	}
}

void Stream_EnsureRemainingCapacity(wStream* s, size_t size)
{
	if (Stream_GetPosition(s) + size > Stream_Capacity(s))
		Stream_EnsureCapacity(s, Stream_Capacity(s) + size);
}

wStream* Stream_New(BYTE* buffer, size_t size)
{
	wStream* s;

	s = malloc(sizeof(wStream));

	if (!s)
		return NULL;


	if (buffer)
		s->buffer = buffer;
	else
		s->buffer = (BYTE*) malloc(size);

	if (!s->buffer)
	{
		free(s);
		return NULL;
	}

	s->pointer = s->buffer;
	s->capacity = size;
	s->length = size;

	s->pool = NULL;
	s->count = 0;

	return s;
}

void Stream_Free(wStream* s, BOOL bFreeBuffer)
{
	if (s)
	{
		if (bFreeBuffer)
		{
			if (s->buffer)
				free(s->buffer);
		}

		free(s);
	}
}
