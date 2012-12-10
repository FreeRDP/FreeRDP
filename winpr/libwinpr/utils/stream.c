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

PStream PStream_Alloc(size_t size)
{
	PStream s;

	s = malloc(sizeof(Stream));

	if (s != NULL)
	{
		if (size > 0)
		{
			s->buffer = (BYTE*) malloc(size);
			s->pointer = s->buffer;
			s->capacity = size;
			s->length = size;
		}
		else
		{
			ZeroMemory(s, sizeof(Stream));
		}
	}

	return s;
}

void Stream_Alloc(PStream s, size_t size)
{
	if (s != NULL)
	{
		if (size > 0)
		{
			s->buffer = (BYTE*) malloc(size);
			s->pointer = s->buffer;
			s->capacity = size;
			s->length = size;
		}
		else
		{
			ZeroMemory(s, sizeof(Stream));
		}
	}
}

void Stream_Realloc(PStream s, size_t size)
{
	if (s != NULL)
	{
		if (size > 0)
		{
			size_t old_size;
			size_t offset_p;

			old_size = s->capacity;
			offset_p = s->pointer - s->buffer;

			s->buffer = (BYTE*) realloc(s->buffer, size);
			s->capacity = size;

			if (old_size <= size)
				s->pointer = s->buffer + offset_p;
			else
				s->pointer = s->buffer;
		}
		else
		{
			if (s->capacity > 0)
				free(s->buffer);

			ZeroMemory(s, sizeof(Stream));
		}
	}
}

PStream PStream_AllocAttach(BYTE* buffer, size_t size)
{
	PStream s;

	s = malloc(sizeof(Stream));

	if (s != NULL)
	{
		s->buffer = buffer;
		s->pointer = s->buffer;
		s->capacity = size;
		s->length = size;
	}

	return s;
}

void Stream_AllocAttach(PStream s, BYTE* buffer, size_t size)
{
	if (s != NULL)
	{
		s->buffer = buffer;
		s->pointer = s->buffer;
		s->capacity = size;
		s->length = size;
	}
}

void PStream_Free(PStream s)
{
	if (s != NULL)
	{
		if (s->buffer != NULL)
			free(s->buffer);

		free(s);
	}
}

void Stream_Free(PStream s)
{
	if (s != NULL)
	{
		if (s->buffer != NULL)
			free(s->buffer);
	}
}

void PStream_FreeDetach(PStream s)
{
	if (s != NULL)
	{
		s->buffer = NULL;
		s->pointer = s->buffer;
		s->capacity = 0;
		s->length = 0;
		free(s);
	}
}

void Stream_FreeDetach(PStream s)
{
	if (s != NULL)
	{
		s->buffer = NULL;
		s->pointer = s->buffer;
		s->capacity = 0;
		s->length = 0;
	}
}

void Stream_Attach(PStream s, BYTE* buffer, size_t size)
{
	if (s != NULL)
	{
		s->buffer = buffer;
		s->pointer = s->buffer;
		s->capacity = size;
		s->length = size;
	}
}

void Stream_Detach(PStream s)
{
	if (s != NULL)
	{
		s->buffer = NULL;
		s->pointer = s->buffer;
		s->capacity = 0;
		s->length = 0;
	}
}
