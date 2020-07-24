/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * IO Update Interface API
 *
 * Copyright 2020 Gluzskiy Alexandr <sss at sss dot chaoslab dot ru>
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

#ifndef FREERDP_UPDATE_IO_H
#define FREERDP_UPDATE_IO_H

#include <freerdp/types.h>

typedef int (*pRead)(rdpContext* context, const uint8_t* buf, size_t buf_size);
typedef int (*pWrite)(rdpContext* context, const uint8_t* buf, size_t buf_size);
typedef int (*pDataHandler)(rdpContext* context, const uint8_t* buf, size_t buf_size);

struct rdp_io_update
{
	rdpContext* context;     /* 0 */
	UINT32 paddingA[16 - 1]; /* 1 */

	/* switchable read
	 * used to read bytes from IO backend */
	pWrite Read; /* 16 */

	/* switchable write
	 * used to write bytes to IO backend */
	pWrite Write; /* 17 */

	/* switchable data handler
	 * used if IO backed doing internal polling and reading
	 * and just passing recieved data to freerdp */
	pDataHandler DataHandler; /* 18 */
	UINT32 paddingB[32 - 19]; /* 19 */
};
typedef struct rdp_io_update rdpIoUpdate;

#endif /* FREERDP_UPDATE_IO_H */
