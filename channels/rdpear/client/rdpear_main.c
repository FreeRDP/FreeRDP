/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Authentication redirection virtual channel
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
#include <freerdp/addin.h>
#include <freerdp/client/channels.h>
#include <freerdp/channels/log.h>
#include <freerdp/channels/rdpear.h>

#define TAG CHANNELS_TAG("rdpear.client")

/* The redirected [MS-RDPEAR] calls are serviced in process here: the packet is unwrapped with the
 * NLA (CredSSP) context of the connection, dispatched to the shared Kerberos handlers, and the
 * reply is wrapped with the same context. This is the path used when the lsa4posix daemon is not
 * built; otherwise rdpear_sspi.c forwards the calls to the daemon. */
typedef struct
{
	GENERIC_DYNVC_PLUGIN base;
	rdpContext* rdp_context;
	krb5_context krbContext;
} RDPEAR_PLUGIN;

/* [MS-RDPEAR] 2.2.1.1 TSRemoteGuardPacket header: ProtocolMagic, Length, Version, Reserved,
 * TsPkgContext. */
#define RDPEAR_PACKET_HEADER_LENGTH 24

/**
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpear_on_data_received(IWTSVirtualChannelCallback* pChannelCallback, wStream* s)
{
	GENERIC_CHANNEL_CALLBACK* callback = (GENERIC_CHANNEL_CALLBACK*)pChannelCallback;
	WINPR_ASSERT(callback);
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

	RDPEAR_PLUGIN* rdpear = (RDPEAR_PLUGIN*)callback->plugin;
	WINPR_ASSERT(rdpear);

	SecBuffer inBuffer = { Length, SECBUFFER_TOKEN, Stream_PointerAs(s, void) };
	SecBuffer decrypted = WINPR_C_ARRAY_INIT;
	SecBuffer cryptedBuffer = WINPR_C_ARRAY_INIT;
	wStream* respPlain = nullptr;
	wStream* finalStream = nullptr;

	if (!freerdp_nla_decrypt(rdpear->rdp_context, &inBuffer, &decrypted))
		goto out;

	respPlain = rdpear_kerb_service(rdpear->krbContext, decrypted.pvBuffer, decrypted.cbBuffer);
	if (!respPlain)
		goto out;

	const size_t respLen = Stream_GetPosition(respPlain);
	if (respLen > UINT32_MAX)
		goto out;

	SecBuffer plainResp = { (ULONG)respLen, SECBUFFER_DATA, Stream_Buffer(respPlain) };
	if (!freerdp_nla_encrypt(rdpear->rdp_context, &plainResp, &cryptedBuffer))
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

	ret = callback->channel->Write(callback->channel, (ULONG)pos, Stream_Buffer(finalStream),
	                               nullptr);
out:
	sspi_SecBufferFree(&decrypted);
	sspi_SecBufferFree(&cryptedBuffer);
	Stream_Free(respPlain, TRUE);
	Stream_Free(finalStream, TRUE);
	return ret;
}

/**
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpear_on_open(IWTSVirtualChannelCallback* pChannelCallback)
{
	WINPR_UNUSED(pChannelCallback);
	return CHANNEL_RC_OK;
}

/**
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpear_on_close(IWTSVirtualChannelCallback* pChannelCallback)
{
	WINPR_UNUSED(pChannelCallback);
	return CHANNEL_RC_OK;
}

static void terminate_plugin_cb(GENERIC_DYNVC_PLUGIN* base)
{
	WINPR_ASSERT(base);

	RDPEAR_PLUGIN* rdpear = (RDPEAR_PLUGIN*)base;
	krb5_free_context(rdpear->krbContext);
}

static UINT init_plugin_cb(GENERIC_DYNVC_PLUGIN* base, rdpContext* rcontext, rdpSettings* settings)
{
	WINPR_ASSERT(base);
	WINPR_UNUSED(settings);

	RDPEAR_PLUGIN* rdpear = (RDPEAR_PLUGIN*)base;
	rdpear->rdp_context = rcontext;
	if (krb5_init_context(&rdpear->krbContext))
		return CHANNEL_RC_INITIALIZATION_ERROR;
	return CHANNEL_RC_OK;
}

static const IWTSVirtualChannelCallback rdpear_callbacks = { rdpear_on_data_received,
	                                                         rdpear_on_open, rdpear_on_close,
	                                                         nullptr };

/**
 * @return 0 on success, otherwise a Win32 error code
 */
FREERDP_ENTRY_POINT(UINT rdpear_DVCPluginEntry(IDRDYNVC_ENTRY_POINTS* pEntryPoints))
{
	return freerdp_generic_DVCPluginEntry(pEntryPoints, TAG, RDPEAR_DVC_CHANNEL_NAME,
	                                      sizeof(RDPEAR_PLUGIN), sizeof(GENERIC_CHANNEL_CALLBACK),
	                                      &rdpear_callbacks, init_plugin_cb, terminate_plugin_cb);
}
