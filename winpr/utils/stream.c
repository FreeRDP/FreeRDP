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

#include <winpr/crt.h>
#include <winpr/stream.h>

PStream PStreamAlloc(size_t size)
{
	PStream s;

	s = malloc(sizeof(Stream));

	if (s != NULL)
	{
		if (size > 0)
		{
			s->buffer = (BYTE*) malloc(size);
			s->pointer = s->buffer;
			s->size = size;
			s->length = size;
		}
		else
		{
			ZeroMemory(s, sizeof(Stream));
		}
	}

	return s;
}

void StreamAlloc(PStream s, size_t size)
{
	if (s != NULL)
	{
		if (size > 0)
		{
			s->buffer = (BYTE*) malloc(size);
			s->pointer = s->buffer;
			s->size = size;
			s->length = size;
		}
		else
		{
			ZeroMemory(s, sizeof(Stream));
		}
	}
}

void StreamReAlloc(PStream s, size_t size)
{
	if (s != NULL)
	{
		if (size > 0)
		{
			size_t old_size;
			size_t offset_p;

			old_size = s->size;
			offset_p = s->pointer - s->buffer;

			s->buffer = (BYTE*) realloc(s->buffer, size);
			s->size = size;

			if (old_size <= size)
				s->pointer = s->buffer + offset_p;
			else
				s->pointer = s->buffer;
		}
		else
		{
			if (s->size > 0)
				free(s->buffer);

			ZeroMemory(s, sizeof(Stream));
		}
	}
}

PStream PStreamAllocAttach(BYTE* buffer, size_t size)
{
	PStream s;

	s = malloc(sizeof(Stream));

	if (s != NULL)
	{
		s->buffer = buffer;
		s->pointer = s->buffer;
		s->size = size;
		s->length = size;
	}

	return s;
}

void StreamAllocAttach(PStream s, BYTE* buffer, size_t size)
{
	if (s != NULL)
	{
		s->buffer = buffer;
		s->pointer = s->buffer;
		s->size = size;
		s->length = size;
	}
}

void PStreamFree(PStream s)
{
	if (s != NULL)
	{
		if (s->buffer != NULL)
			free(s->buffer);

		free(s);
	}
}

void StreamFree(PStream s)
{
	if (s != NULL)
	{
		if (s->buffer != NULL)
			free(s->buffer);
	}
}

void PStreamFreeDetach(PStream s)
{
	if (s != NULL)
	{
		s->buffer = NULL;
		s->pointer = s->buffer;
		s->size = 0;
		s->length = 0;
		free(s);
	}
}

void StreamFreeDetach(PStream s)
{
	if (s != NULL)
	{
		s->buffer = NULL;
		s->pointer = s->buffer;
		s->size = 0;
		s->length = 0;
	}
}

void StreamAttach(PStream s, BYTE* buffer, size_t size)
{
	if (s != NULL)
	{
		s->buffer = buffer;
		s->pointer = s->buffer;
		s->size = size;
		s->length = size;
	}
}

void StreamDetach(PStream s)
{
	if (s != NULL)
	{
		s->buffer = NULL;
		s->pointer = s->buffer;
		s->size = 0;
		s->length = 0;
	}
}
/* Modeline for vim. Don't delete */
/* vim: set cindent:noet:sw=8:ts=8 */
