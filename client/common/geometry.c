/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Geometry tracking Virtual Channel Extension
 *
 * Copyright 2017 David Fort <contact@hardening-consulting.com>
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

#include <freerdp/config.h>

#include <freerdp/client/geometry.h>
#include <winpr/interlocked.h>

void mappedGeometryRef(MAPPED_GEOMETRY* g)
{
	InterlockedIncrement(&g->refCounter);
}

void mappedGeometryUnref(MAPPED_GEOMETRY* g)
{
	if (!g)
		return;

	if (InterlockedDecrement(&g->refCounter))
		return;

	g->MappedGeometryUpdate = NULL;
	g->MappedGeometryClear = NULL;
	g->custom = NULL;
	free(g->geometry.rects);
	free(g);
}
