/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Authentication redirection virtual channel, in-process servicing
 *
 * Copyright 2023 David Fort <contact@hardening-consulting.com>
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
#include <krb5.h>

#include <winpr/assert.h>
#include <winpr/wtypes.h>
#include <winpr/crt.h>
#include <winpr/wlog.h>
#include <winpr/stream.h>
#include <winpr/sspi.h>

#include <rdpear-common/rdpear_kerb.h>

#include <freerdp/config.h>
#include <freerdp/freerdp.h>
#include <freerdp/channels/log.h>

#include "rdpear_inproc.h"
#include "rdpear_packet.h"

#define TAG CHANNELS_TAG("rdpear.client")

/* The packet is unwrapped with the NLA (CredSSP) context of the connection, dispatched to the
 * shared handlers, and the reply is wrapped with the same context. */
struct rdpear_inproc
{
	rdpContext* rdp_context;
	krb5_context krbContext;
};

void rdpear_inproc_free(rdpearInproc* inproc)
{
	if (!inproc)
		return;
	krb5_free_context(inproc->krbContext);
	free(inproc);
}

rdpearInproc* rdpear_inproc_new(rdpContext* context)
{
	WINPR_ASSERT(context);

	rdpearInproc* inproc = calloc(1, sizeof(rdpearInproc));
	if (!inproc)
		return nullptr;
	inproc->rdp_context = context;
	if (krb5_init_context(&inproc->krbContext))
	{
		free(inproc);
		return nullptr;
	}
	return inproc;
}

UINT rdpear_inproc_receive(rdpearInproc* inproc, IWTSVirtualChannel* channel, wStream* s)
{
	WINPR_ASSERT(inproc);
	WINPR_ASSERT(channel);
	UINT ret = ERROR_INVALID_DATA;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, RDPEAR_PACKET_HEADER_LENGTH))
		return ERROR_INVALID_DATA;

	UINT32 protocolMagic = 0;
	UINT32 Length = 0;
	UINT32 Version = 0;
	Stream_Read_UINT32(s, protocolMagic);
	if (protocolMagic != RDPEAR_PROTOCOL_MAGIC)
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, Length);

	Stream_Read_UINT32(s, Version);
	if (Version != 0x00000000)
		return ERROR_INVALID_DATA;

	Stream_Seek(s, 4); /* Reserved (4 bytes) */
	Stream_Seek(s, 8); /* TsPkgContext (8 bytes) */

	if (!Stream_CheckAndLogRequiredLength(TAG, s, Length))
		return ERROR_INVALID_DATA;

	SecBuffer inBuffer = { Length, SECBUFFER_TOKEN, Stream_PointerAs(s, void) };
	SecBuffer decrypted = WINPR_C_ARRAY_INIT;
	SecBuffer cryptedBuffer = WINPR_C_ARRAY_INIT;
	wStream* respPlain = nullptr;
	wStream* finalStream = nullptr;

	if (!freerdp_nla_decrypt(inproc->rdp_context, &inBuffer, &decrypted))
		goto out;

	respPlain = rdpear_kerb_service(inproc->krbContext, decrypted.pvBuffer, decrypted.cbBuffer);
	if (!respPlain)
		goto out;

	const size_t respLen = Stream_GetPosition(respPlain);
	if (respLen > UINT32_MAX)
		goto out;

	SecBuffer plainResp = { (ULONG)respLen, SECBUFFER_DATA, Stream_Buffer(respPlain) };
	if (!freerdp_nla_encrypt(inproc->rdp_context, &plainResp, &cryptedBuffer))
		goto out;

	finalStream = Stream_New(nullptr, RDPEAR_PACKET_HEADER_LENGTH + cryptedBuffer.cbBuffer);
	if (!finalStream)
		goto out;
	Stream_Write_UINT32(finalStream, RDPEAR_PROTOCOL_MAGIC);  /* ProtocolMagic (4 bytes) */
	Stream_Write_UINT32(finalStream, cryptedBuffer.cbBuffer); /* Length (4 bytes) */
	Stream_Write_UINT32(finalStream, 0x00000000);             /* Version (4 bytes) */
	Stream_Write_UINT32(finalStream, 0x00000000);             /* Reserved (4 bytes) */
	Stream_Write_UINT64(finalStream, 0);                      /* TsPkgContext (8 bytes) */
	Stream_Write(finalStream, cryptedBuffer.pvBuffer, cryptedBuffer.cbBuffer);

	const size_t pos = Stream_GetPosition(finalStream);
	if (pos > UINT32_MAX)
		goto out;

	ret = channel->Write(channel, (ULONG)pos, Stream_Buffer(finalStream), nullptr);

out:
	sspi_SecBufferFree(&decrypted);
	sspi_SecBufferFree(&cryptedBuffer);
	Stream_Free(respPlain, TRUE);
	Stream_Free(finalStream, TRUE);
	return ret;
}
