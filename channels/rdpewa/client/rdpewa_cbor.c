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

#include <freerdp/config.h>

#include <string.h>
#include <stdlib.h>

#include <cbor.h>

#include <winpr/assert.h>
#include <winpr/endian.h>

#include <freerdp/channels/log.h>

#include "rdpewa_cbor.h"

#define TAG CHANNELS_TAG("rdpewa.client")

BOOL rdpewa_cbor_decode_request(const BYTE* data, size_t length, RDPEWA_REQUEST* out)
{
	struct cbor_load_result result;
	BOOL ret = FALSE;

	WINPR_ASSERT(data);
	WINPR_ASSERT(out);

	memset(out, 0, sizeof(*out));

	cbor_item_t* root = cbor_load(data, length, &result);
	if (!root || result.error.code != CBOR_ERR_NONE)
	{
		WLog_ERR(TAG, "Failed to decode CBOR request, error %d at position %" PRIuz,
		         result.error.code, result.error.position);
		goto out;
	}

	if (!cbor_isa_map(root))
	{
		WLog_ERR(TAG, "CBOR request is not a map");
		goto out;
	}

	const size_t mapSize = cbor_map_size(root);
	struct cbor_pair* pairs = cbor_map_handle(root);

	WLog_DBG(TAG, "Request CBOR map has %zu entries", mapSize);

	for (size_t i = 0; i < mapSize; i++)
	{
		cbor_item_t* key = pairs[i].key;
		cbor_item_t* value = pairs[i].value;

		if (!cbor_isa_string(key))
			continue;

		const char* keyStr = (const char*)cbor_string_handle(key);
		const size_t keyLen = cbor_string_length(key);

		WLog_DBG(TAG, "  key[%" PRIuz "]: \"%.*s\" type=%d", i,
		         WINPR_ASSERTING_INT_CAST(int, keyLen), keyStr, cbor_typeof(value));

		switch (keyLen)
		{
			case 4:
				if (memcmp(keyStr, "rpId", 4) == 0)
				{
					if (cbor_isa_string(value))
					{
						size_t slen = cbor_string_length(value);
						if (slen >= sizeof(out->rpId))
							slen = sizeof(out->rpId) - 1;
						memcpy(out->rpId, cbor_string_handle(value), slen);
						out->rpId[slen] = '\0';
					}
				}
				break;
			case 5:
				if (memcmp(keyStr, "flags", 5) == 0)
				{
					if (!cbor_isa_uint(value))
					{
						WLog_ERR(TAG, "\"flags\"is not an unsigned int");
						goto out;
					}
					out->flags = WINPR_ASSERTING_INT_CAST(UINT32, cbor_get_int(value));
				}
				break;
			case 7:
				if (memcmp(keyStr, "command", 7) == 0)
				{
					if (!cbor_isa_uint(value))
					{
						WLog_ERR(TAG, "\"command\"is not an unsigned int");
						goto out;
					}
					out->command = WINPR_ASSERTING_INT_CAST(UINT32, cbor_get_int(value));
				}
				else if (memcmp(keyStr, "request", 7) == 0)
				{
					if (!cbor_isa_bytestring(value))
					{
						WLog_ERR(TAG, "\"request\"is not a byte string");
						goto out;
					}
					out->requestLen = cbor_bytestring_length(value);
					out->request = malloc(out->requestLen);
					if (!out->request)
					{
						out->requestLen = 0;
						goto out;
					}
					memcpy(out->request, cbor_bytestring_handle(value), out->requestLen);
				}
				else if (memcmp(keyStr, "timeout", 7) == 0)
				{
					if (!cbor_isa_uint(value))
					{
						WLog_ERR(TAG, "\"timeout\"is not an unsigned int");
						goto out;
					}
					out->timeout = WINPR_ASSERTING_INT_CAST(UINT32, cbor_get_int(value));
				}
				break;
			case 13:
				if (memcmp(keyStr, "transactionId", 13) == 0)
				{
					if (!cbor_isa_bytestring(value) || cbor_bytestring_length(value) != 16)
					{
						WLog_ERR(TAG, "\"transactionId\"is not a 16-byte byte string");
						goto out;
					}
					memcpy(out->transactionId, cbor_bytestring_handle(value), 16);
					out->hasTransactionId = TRUE;
				}
				break;
			default:
				break;
		}
	}

	if (out->command == 0)
	{
		WLog_ERR(TAG, "Missing or invalid \"command\" in request");
		goto out;
	}

	ret = TRUE;
out:
	if (!ret)
	{
		free(out->request);
		out->request = nullptr;
		out->requestLen = 0;
	}
	if (root)
		cbor_decref(&root);
	return ret;
}

wStream* rdpewa_cbor_encode_webauthn_response(HRESULT hresult, BYTE ctapStatus,
                                              const BYTE* ctapData, size_t ctapLen,
                                              const RDPEWA_DEVICE_INFO* devInfo)
{
	wStream* s = nullptr;
	cbor_item_t* root = nullptr;
	unsigned char* cborBuf = nullptr;
	size_t cborLen = 0;

	WINPR_ASSERT(devInfo);

	/* Build the inner "Response" byte string: status byte + CTAP CBOR data */
	size_t responseLen = 1 + ctapLen;
	BYTE* responseData = malloc(responseLen);
	if (!responseData)
		return FALSE;

	responseData[0] = ctapStatus;
	if (ctapData && ctapLen > 0)
		memcpy(responseData + 1, ctapData, ctapLen);

	root = cbor_new_definite_map(3);
	if (!root)
		goto out;

	/* "deviceInfo" key */
	cbor_item_t* diKey = cbor_build_string("deviceInfo");
	cbor_item_t* diMap = cbor_new_definite_map(11);
	if (!diKey || !diMap)
	{
		if (diKey)
			cbor_decref(&diKey);
		if (diMap)
			cbor_decref(&diMap);
		goto out;
	}
	{
		/* The casing is all over the place, but that's how it's defined in the spec */
		cbor_item_t* pairs[][2] = {
			{ cbor_build_string("maxMsgSize"), cbor_build_uint16(devInfo->maxMsgSize) },
			{ cbor_build_string("maxSerializedLargeBlobArray"),
			  cbor_build_uint16(devInfo->maxSerializedLargeBlobArray) },
			{ cbor_build_string("providerType"), cbor_build_string(devInfo->providerType) },
			{ cbor_build_string("providerName"), cbor_build_string(devInfo->providerName) },
			{ cbor_build_string("devicePath"), cbor_build_string(devInfo->devicePath) },
			{ cbor_build_string("Manufacturer"), cbor_build_string(devInfo->manufacturer) },
			{ cbor_build_string("Product"), cbor_build_string(devInfo->product) },
			{ cbor_build_string("aaGuid"),
			  cbor_build_bytestring(devInfo->aaGuid, sizeof(devInfo->aaGuid)) },
			{ cbor_build_string("uvStatus"), cbor_build_uint8(devInfo->uvStatus) },
			{ cbor_build_string("uvRetries"), cbor_build_uint8(devInfo->uvRetries) },
			{ cbor_build_string("transports"), cbor_build_uint8(devInfo->transports) },
		};
		for (size_t i = 0; i < 11; i++)
		{
			cbor_map_add(diMap, (struct cbor_pair){ .key = pairs[i][0], .value = pairs[i][1] });
			cbor_decref(&pairs[i][0]);
			cbor_decref(&pairs[i][1]);
		}
	}
	cbor_map_add(root, (struct cbor_pair){ .key = diKey, .value = diMap });
	cbor_decref(&diKey);
	cbor_decref(&diMap);

	/* "status" key (lowercase per spec example hex) */
	cbor_item_t* statusKey = cbor_build_string("status");
	cbor_item_t* statusVal = cbor_build_uint8(0);
	cbor_map_add(root, (struct cbor_pair){ .key = statusKey, .value = statusVal });
	cbor_decref(&statusKey);
	cbor_decref(&statusVal);

	/* "response" key (lowercase per spec example hex) */
	cbor_item_t* respKey = cbor_build_string("response");
	cbor_item_t* respVal = cbor_build_bytestring(responseData, responseLen);
	cbor_map_add(root, (struct cbor_pair){ .key = respKey, .value = respVal });
	cbor_decref(&respKey);
	cbor_decref(&respVal);

	cborLen = cbor_serialize_alloc(root, &cborBuf, &cborLen);
	if (cborLen == 0 || !cborBuf)
		goto out;

	/* Response: 4-byte LE HRESULT + CBOR map */
	s = Stream_New(nullptr, 4 + cborLen);
	if (!s)
		goto out;

	Stream_Write_UINT32(s, (UINT32)hresult);
	Stream_Write(s, cborBuf, cborLen);

out:
	free(responseData);
	free(cborBuf);
	if (root)
		cbor_decref(&root);
	return s;
}

wStream* rdpewa_cbor_encode_simple_response(HRESULT hresult, UINT32 value)
{
	wStream* s = Stream_New(nullptr, 8);
	if (!s)
		return nullptr;

	Stream_Write_UINT32(s, (UINT32)hresult);
	Stream_Write_UINT32(s, value);
	return s;
}

wStream* rdpewa_cbor_encode_hresult_response(HRESULT hresult)
{
	wStream* s = Stream_New(nullptr, 4);
	if (!s)
		return nullptr;

	Stream_Write_UINT32(s, (UINT32)hresult);
	return s;
}
