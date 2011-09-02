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

#define RDP5_HISTORY_BUF_SIZE     65536

struct rdp_mppc
{
    uint8_t *history_buf;
    uint8_t *history_ptr;
};

// forward declarations
int decompress_rdp_5(rdpRdp *, uint8_t *, int, int, uint32_t *, uint32_t *);

#endif
