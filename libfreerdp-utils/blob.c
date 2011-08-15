/**
 * FreeRDP: A Remote Desktop Protocol Client
 * BLOB Utils
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

#include <freerdp/utils/memory.h>

#include <freerdp/utils/blob.h>

/**
 * Allocate memory for data blob.
 * @param blob blob structure
 * @param length memory length
 */

void freerdp_blob_alloc(rdpBlob* blob, int length)
{
	blob->data = xmalloc(length);
	blob->length = length;
}

/**
 * Free memory allocated for data blob.
 * @param blob
 */

void freerdp_blob_free(rdpBlob* blob)
{
	if (blob->data)
		xfree(blob->data);
	
	blob->length = 0;
}
