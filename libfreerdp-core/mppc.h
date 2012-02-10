/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Implements Microsoft Point to Point Compression (MPPC) protocol
 *
 * Copyright 2011 Laxmikant Rashinkar <LK.Rashinkar@gmail.com>
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

#ifndef __MPPC_H
#define __MPPC_H

#include <stdint.h>

#define RDP6_HISTORY_BUF_SIZE     65536
#define RDP6_OFFSET_CACHE_SIZE     4

struct rdp_mppc
{
	uint8 *history_buf;
	uint16 *offset_cache;
	uint8 *history_buf_end;
	uint8 *history_ptr;
};

// forward declarations
int decompress_rdp(rdpRdp *, uint8 *, int, int, uint32 *, uint32 *);
int decompress_rdp_4(rdpRdp *, uint8 *, int, int, uint32 *, uint32 *);
int decompress_rdp_5(rdpRdp *, uint8 *, int, int, uint32 *, uint32 *);
int decompress_rdp_6(rdpRdp *, uint8 *, int, int, uint32 *, uint32 *);
int decompress_rdp_61(rdpRdp *, uint8 *, int, int, uint32 *, uint32 *);
struct rdp_mppc *mppc_new(rdpRdp *rdp);
void mppc_free(rdpRdp *rdp);

#endif
