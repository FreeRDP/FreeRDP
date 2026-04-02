/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * WebAuthn Virtual Channel Extension [MS-RDPEWA]
 * CBOR encoding/decoding
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

#ifndef FREERDP_CHANNEL_RDPEWA_CLIENT_CBOR_H
#define FREERDP_CHANNEL_RDPEWA_CLIENT_CBOR_H

#include <winpr/stream.h>
#include <winpr/wtypes.h>

#include <freerdp/channels/rdpewa.h>

/** @brief Decoded MS-RDPEWA request message */
typedef struct
{
	UINT32 command;
	UINT32 flags;
	BYTE* request;
	size_t requestLen;
	UINT32 timeout;
	BYTE transactionId[16];
	BOOL hasTransactionId;
	char rpId[256];
} RDPEWA_REQUEST;

/** @brief Device info for the response map */
typedef struct
{
	UINT32 maxMsgSize;
	UINT32 maxSerializedLargeBlobArray;
	char providerType[16];
	char providerName[64];
	char devicePath[256];
	char manufacturer[64];
	char product[64];
	BYTE aaGuid[16];
	BYTE uvStatus;
	BYTE uvRetries;
	UINT32 transports;
} RDPEWA_DEVICE_INFO;

/** @brief Parse a CBOR-encoded MS-RDPEWA request message.
 *
 *  @param data     raw CBOR bytes
 *  @param length   size of @p data
 *  @param out      filled on success; @p out->request points into @p data and must not be freed
 *  @return TRUE on success
 */
BOOL rdpewa_cbor_decode_request(const BYTE* data, size_t length, RDPEWA_REQUEST* out);

/** @brief Encode a CBOR response for CTAPCBOR_RPC_COMMAND_WEB_AUTHN (command 5).
 *
 *  Builds a 4-byte LE HRESULT prefix followed by CBOR-encoded response map.
 *
 *  @param hresult    HRESULT status code
 *  @param ctapStatus single CTAP status byte (0x00 = success)
 *  @param ctapData   CTAP authenticator response (CBOR map), may be nullptr
 *  @param ctapLen    length of @p ctapData
 *  @param devInfo    device info for the response map
 *  @return a new wStream on success (caller frees with Stream_Free(s, TRUE)), nullptr on failure
 */
wStream* rdpewa_cbor_encode_webauthn_response(HRESULT hresult, BYTE ctapStatus,
                                              const BYTE* ctapData, size_t ctapLen,
                                              const RDPEWA_DEVICE_INFO* devInfo);

/** @brief Encode a simple 4-byte LE value response (IUVPAA / API_VERSION).
 *
 *  @param hresult  HRESULT status code
 *  @param value    the 4-byte value to append
 *  @return a new wStream on success (caller frees with Stream_Free(s, TRUE)), nullptr on failure
 */
wStream* rdpewa_cbor_encode_simple_response(HRESULT hresult, UINT32 value);

/** @brief Encode an HRESULT-only response (e.g. for CANCEL_CUR_OP).
 *
 *  @param hresult  HRESULT status code
 *  @return a new wStream on success (caller frees with Stream_Free(s, TRUE)), nullptr on failure
 */
wStream* rdpewa_cbor_encode_hresult_response(HRESULT hresult);

#endif /* FREERDP_CHANNEL_RDPEWA_CLIENT_CBOR_H */
