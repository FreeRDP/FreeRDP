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

#ifndef FREERDP_CHANNELS_CLIENT_GEOMETRY_H
#define FREERDP_CHANNELS_CLIENT_GEOMETRY_H

#include <winpr/collections.h>
#include <freerdp/api.h>
#include <freerdp/channels/geometry.h>

/**
 * Client Interface
 */
typedef struct _geometry_client_context GeometryClientContext;

typedef struct _MAPPED_GEOMETRY MAPPED_GEOMETRY;
typedef BOOL (*pcMappedGeometryAdded)(GeometryClientContext* context, MAPPED_GEOMETRY *geometry);
typedef BOOL (*pcMappedGeometryUpdate)(MAPPED_GEOMETRY *geometry);
typedef BOOL (*pcMappedGeometryClear)(MAPPED_GEOMETRY *geometry);

/** @brief a geometry record tracked by the geometry channel */
struct _MAPPED_GEOMETRY
{
	volatile LONG refCounter;
	UINT64 mappingId;
	UINT64 topLevelId;
	INT32 left, top, right, bottom;
	INT32 topLevelLeft, topLevelTop, topLevelRight, topLevelBottom;
	FREERDP_RGNDATA geometry;

	void *custom;
	pcMappedGeometryUpdate MappedGeometryUpdate;
	pcMappedGeometryClear MappedGeometryClear;
};


/** @brief the geometry context for client channel */
struct _geometry_client_context
{
	wHashTable *geometries;
	void* handle;
	void* custom;

	pcMappedGeometryAdded MappedGeometryAdded;
};

#ifdef __cplusplus
extern "C" {
#endif

FREERDP_API void mappedGeometryRef(MAPPED_GEOMETRY *g);
FREERDP_API void mappedGeometryUnref(MAPPED_GEOMETRY *g);

#ifdef __cplusplus
}
#endif


#endif /* FREERDP_CHANNELS_CLIENT_GEOMETRY_H */

