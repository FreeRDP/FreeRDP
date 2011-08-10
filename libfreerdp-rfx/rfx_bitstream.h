/**
 * FreeRDP: A Remote Desktop Protocol client.
 * RemoteFX Codec Library - Bit Stream
 *
 * Copyright 2011 Vic Lee
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

#ifndef __RFX_BITSTREAM_H
#define __RFX_BITSTREAM_H

#include <freerdp/rfx.h>

struct _RFX_BITSTREAM
{
	uint8* buffer;
	int nbytes;
	int byte_pos;
	int bits_left;
};
typedef struct _RFX_BITSTREAM RFX_BITSTREAM;

void rfx_bitstream_attach(RFX_BITSTREAM* bs, uint8* buffer, int nbytes);
uint16 rfx_bitstream_get_bits(RFX_BITSTREAM* bs, int nbits);
void rfx_bitstream_put_bits(RFX_BITSTREAM* bs, uint16 bits, int nbits);

#define rfx_bitstream_eos(_bs) ((_bs)->byte_pos >= (_bs)->nbytes)
#define rfx_bitstream_left(_bs) ((_bs)->byte_pos >= (_bs)->nbytes ? 0 : ((_bs)->nbytes - (_bs)->byte_pos - 1) * 8 + (_bs)->bits_left)
#define rfx_bitstream_get_processed_bytes(_bs) ((_bs)->bits_left < 8 ? (_bs)->byte_pos + 1 : (_bs)->byte_pos)

#endif /* __RFX_BITSTREAM_H */
