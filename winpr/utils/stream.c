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
			s->data = (BYTE*) malloc(size);
			s->p = s->data;
			s->end = s->p;
			s->size = size;
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
			s->data = (BYTE*) malloc(size);
			s->p = s->data;
			s->end = s->p;
			s->size = size;
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
			size_t offset_end;

			old_size = s->size;
			offset_p = s->p - s->data;
			offset_end = s->end - s->data;

			s->data = (BYTE*) realloc(s->data, size);
			s->size = size;

			if (old_size <= size)
			{
				s->p = s->data + offset_p;
				s->end = s->data + offset_end;
			}
			else
			{
				s->p = s->data;
				s->end = s->p;
			}
		}
		else
		{
			if (s->size > 0)
				free(s->data);

			ZeroMemory(s, sizeof(Stream));
		}
	}
}

PStream PStreamAllocAttach(BYTE* data, size_t size)
{
	PStream s;

	s = malloc(sizeof(Stream));

	if (s != NULL)
	{
		s->data = data;
		s->p = s->data;
		s->end = s->p;
		s->size = size;
	}

	return s;
}

void StreamAllocAttach(PStream s, BYTE* data, size_t size)
{
	if (s != NULL)
	{
		s->data = data;
		s->p = s->data;
		s->end = s->p;
		s->size = size;
	}
}

void PStreamFree(PStream s)
{
	if (s != NULL)
	{
		if (s->data != NULL)
			free(s->data);

		free(s);
	}
}

void StreamFree(PStream s)
{
	if (s != NULL)
	{
		if (s->data != NULL)
			free(s->data);
	}
}

void PStreamFreeDetach(PStream s)
{
	if (s != NULL)
	{
		s->data = NULL;
		s->p = s->data;
		s->end = s->p;
		s->size = 0;
		free(s);
	}
}

void StreamFreeDetach(PStream s)
{
	if (s != NULL)
	{
		s->data = NULL;
		s->p = s->data;
		s->end = s->p;
		s->size = 0;
	}
}

void StreamAttach(PStream s, BYTE* data, size_t size)
{
	if (s != NULL)
	{
		s->data = data;
		s->p = s->data;
		s->end = s->p;
		s->size = size;
	}
}

void StreamDetach(PStream s)
{
	if (s != NULL)
	{
		s->data = NULL;
		s->p = s->data;
		s->end = s->p;
		s->size = 0;
	}
}
