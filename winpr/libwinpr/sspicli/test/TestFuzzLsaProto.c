/**
 * WinPR: Windows Portable Runtime
 * libFuzzer harness for the lsa4posix wire codec (lsa_proto)
 *
 * The daemon and the WinPR LSA client exchange length-prefixed messages parsed by
 * lsa_proto. This feeds arbitrary bytes to the reader (envelope, scalars and a
 * length-prefixed blob) to exercise the frame parser's bounds and allocation paths.
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

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <winpr/crt.h>
#include <winpr/stream.h>

#include "../lsa_proto.c"

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	wStream buffer = { 0 };
	wStream* s = Stream_StaticConstInit(&buffer, data, size);
	if (!s)
		return 0;

	LsaProtoReader reader = { 0 };
	LSA_MSG_HDR hdr = { 0 };
	if (lsa_proto_reader_init(&reader, s, &hdr))
	{
		UINT16 u16 = 0;
		UINT32 u32 = 0;
		UINT64 u64 = 0;
		BYTE* blob = nullptr;
		UINT32 blen = 0;

		(void)lsa_proto_read_u16(&reader, &u16);
		(void)lsa_proto_read_u32(&reader, &u32);
		(void)lsa_proto_read_u64(&reader, &u64);
		if (lsa_proto_read_blob(&reader, &blob, &blen))
			free(blob);

		lsa_proto_reader_done(&reader);
	}

	return 0;
}
