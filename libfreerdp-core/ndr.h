/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Network Data Representation (NDR)
 *
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

#ifndef FREERDP_CORE_NDR_H
#define FREERDP_CORE_NDR_H

#include "rpc.h"

#include "config.h"

#include <freerdp/types.h>
#include <freerdp/wtypes.h>
#include <freerdp/utils/debug.h>
#include <freerdp/utils/stream.h>

/* Type Format Strings: http://msdn.microsoft.com/en-us/library/windows/desktop/aa379093/ */

#define FC_BYTE				0x01
#define FC_CHAR				0x02
#define FC_SMALL			0x03
#define FC_USMALL			0x04
#define FC_WCHAR			0x05
#define FC_SHORT			0x06
#define FC_USHORT			0x07
#define FC_LONG				0x08
#define FC_ULONG			0x09
#define FC_FLOAT			0x0A
#define FC_HYPER			0x0B
#define FC_DOUBLE			0x0C
#define FC_ENUM16			0x0D
#define FC_ENUM32			0x0E
#define FC_ERROR_STATUS_T		0x10
#define FC_INT3264			0xB8
#define FC_UINT3264			0xB9

#define NdrFcShort(s)	(byte)(s & 0xFF), (byte)(s >> 8)

#define NdrFcLong(s)	(byte)(s & 0xFF), (byte)((s & 0x0000FF00) >> 8), \
                        (byte)((s & 0x00FF0000) >> 16), (byte)(s >> 24)

#endif /* FREERDP_CORE_NDR_H */
