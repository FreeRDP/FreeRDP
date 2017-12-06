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

#include <freerdp/channels/geometry.h>

/**
 * Client Interface
 */
typedef struct _geometry_client_context GeometryClientContext;


typedef UINT (*pcMappedGeometryPacket)(GeometryClientContext* context, MAPPED_GEOMETRY_PACKET *packet);

struct _geometry_client_context
{
	void* handle;
	void* custom;

	pcMappedGeometryPacket MappedGeometryPacket;
};

#endif /* FREERDP_CHANNELS_CLIENT_GEOMETRY_H */

