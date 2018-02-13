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

#ifndef FREERDP_CHANNEL_GEOMETRY_H
#define FREERDP_CHANNEL_GEOMETRY_H

#include <winpr/wtypes.h>
#include <freerdp/types.h>

#define GEOMETRY_DVC_CHANNEL_NAME "Microsoft::Windows::RDS::Geometry::v08.01"

enum
{
	GEOMETRY_UPDATE	= 1,
	GEOMETRY_CLEAR	= 2
};

enum
{
	RDH_RECTANGLE = 1
};

struct _FREERDP_RGNDATA
{
	RDP_RECT boundingRect;
	UINT32 nRectCount;
	RDP_RECT *rects;
};

typedef struct _FREERDP_RGNDATA FREERDP_RGNDATA;


struct _MAPPED_GEOMETRY_PACKET
{
	UINT32 version;
	UINT64 mappingId;
	UINT32 updateType;
	UINT64 topLevelId;
	INT32 left, top, right, bottom;
	INT32 topLevelLeft, topLevelTop, topLevelRight, topLevelBottom;
	UINT32 geometryType;

	FREERDP_RGNDATA geometry;
};

typedef struct _MAPPED_GEOMETRY_PACKET MAPPED_GEOMETRY_PACKET;

#endif /* FREERDP_CHANNEL_GEOMETRY_H */
