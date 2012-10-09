/*
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Memory Utils
 *
 * Copyright 2009-2011 Jay Sorg
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>

#include <freerdp/utils/memory.h>

/**
 * Allocate memory initialized to zero.
 * This function is used to secure a calloc call.
 * It verifies its return value, and logs an error if the allocation failed.
 *
 * @param size - number of bytes to allocate. If the size is < 1, it will default to 1.
 *
 * @return a pointer to the allocated and zeroed buffer. NULL if the allocation failed.
 */
void* xzalloc(size_t size)
{
	void* mem;

	if (size < 1)
		size = 1;

	mem = calloc(1, size);

	if (mem == NULL)
	{
		perror("xzalloc");
		printf("xzalloc: failed to allocate memory of size: %d\n", (int) size);
	}

	return mem;
}

