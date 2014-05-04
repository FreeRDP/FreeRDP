/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Bulk Data Compression
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_CODEC_BULK_H
#define FREERDP_CODEC_BULK_H

#include <freerdp/api.h>
#include <freerdp/types.h>

/* Level-2 Compression Flags */

#define PACKET_COMPRESSED		0x20
#define PACKET_AT_FRONT			0x40
#define PACKET_FLUSHED			0x80

/* Level-1 Compression Flags */

#define L1_PACKET_AT_FRONT		0x04
#define L1_NO_COMPRESSION		0x02
#define L1_COMPRESSED			0x01
#define L1_INNER_COMPRESSION		0x10

#endif /* FREERDP_CODEC_BULK_H */

