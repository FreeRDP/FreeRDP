/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * WebAuthn Virtual Channel Extension [MS-RDPEWA]
 * libfido2 authenticator integration
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

#include <fido.h>
#include <fido/credman.h>
#include <cbor.h>

#include <winpr/assert.h>
#include <winpr/endian.h>
#include <winpr/wlog.h>

#include <freerdp/channels/log.h>
#include <freerdp/channels/rdpewa.h>

#include "rdpewa_cbor.h"
#include "rdpewa_fido.h"

#include <winpr/thread.h>
#include <winpr/synch.h>

#define TAG CHANNELS_TAG("rdpewa.client")

/** @brief Args for running a FIDO operation on a background thread */
typedef struct
{
	fido_dev_t* dev;
	fido_cred_t* cred;
	fido_assert_t* assert;
	const char* pin;
	int result;
} RDPEWA_FIDO_ASYNC;

static DWORD WINAPI rdpewa_fido_makecred_thread(LPVOID arg)
{
	RDPEWA_FIDO_ASYNC* a = (RDPEWA_FIDO_ASYNC*)arg;
	a->result = fido_dev_make_cred(a->dev, a->cred, a->pin);
	return 0;
}

static DWORD WINAPI rdpewa_fido_getassert_thread(LPVOID arg)
{
	RDPEWA_FIDO_ASYNC* a = (RDPEWA_FIDO_ASYNC*)arg;
	a->result = fido_dev_get_assert(a->dev, a->assert, a->pin);
	return 0;
}

/**
 * Select a device by making all available keys blink and waiting for the user to touch one.
 * Uses fido_dev_get_touch_begin/fido_dev_get_touch_status for non-blocking multi-device selection.
 * Returns the index of the touched device, or -1 on failure/timeout.
 */
static int rdpewa_fido_select_device(rdpContext* context, fido_dev_t** devs, size_t ndevs,
                                     int timeout_sec)
{
	if (ndevs == 1)
		return 0; /* only one device, no selection needed */

	/* Start touch request on all devices */
	for (size_t i = 0; i < ndevs; i++)
	{
		if (devs[i])
		{
			int r = fido_dev_get_touch_begin(devs[i]);
			if (r != FIDO_OK)
				WLog_DBG(TAG, "Device %zu: fido_dev_get_touch_begin: %s", i, fido_strerr(r));
		}
	}

	{
		/* Ideally this would be a non-blocking message that doesn't require user
		 * input, but there's no facility to do that. */
		freerdp* instance = context->instance;
		if (instance && instance->AuthenticateEx)
		{
			char* u = nullptr;
			char* p = nullptr;
			char* d = nullptr;
			if (!instance->AuthenticateEx(instance, &u, &p, &d, AUTH_FIDO_TOUCH))
			{
				free(u);
				free(p);
				free(d);
				return -1;
			}
			free(u);
			free(p);
			free(d);
		}
	}

	/* Poll for touch with 200ms intervals */
	int selected = -1;
	int iterations = timeout_sec * 5; /* 200ms * 5 = 1 second */
	for (int iter = 0; iter < iterations && selected < 0; iter++)
	{
		for (size_t i = 0; i < ndevs; i++)
		{
			if (!devs[i])
				continue;

			int touched = 0;
			int r = fido_dev_get_touch_status(devs[i], &touched, 200);
			if (r == FIDO_OK && touched)
			{
				WLog_DBG(TAG, "Device %zu was touched", i);
				selected = WINPR_ASSERTING_INT_CAST(int, i);
				break;
			}
		}
	}

	return selected;
}

/**
 * Populate device info from an open FIDO device.
 */
static void rdpewa_fido_fill_device_info(fido_dev_t* dev, const char* path,
                                         const char* manufacturer, const char* product,
                                         RDPEWA_DEVICE_INFO* info)
{
	WINPR_ASSERT(dev);
	WINPR_ASSERT(info);

	*info = (RDPEWA_DEVICE_INFO){ .maxMsgSize = 1200,
		                          .maxSerializedLargeBlobArray = 1024,
		                          .providerType = "Hid",
		                          .providerName = "FreeRDPFidoProvider",
		                          .uvStatus = fido_dev_has_pin(dev) ? 1 : 0,
		                          .uvRetries = 3 };

	if (path)
		strncpy(info->devicePath, path, sizeof(info->devicePath) - 1);
	if (manufacturer && manufacturer[0])
		strncpy(info->manufacturer, manufacturer, sizeof(info->manufacturer) - 1);
	if (product && product[0])
		strncpy(info->product, product, sizeof(info->product) - 1);

	fido_cbor_info_t* ci = fido_cbor_info_new();
	if (ci && fido_dev_get_cbor_info(dev, ci) == FIDO_OK)
	{
		uint64_t msgsz = fido_cbor_info_maxmsgsiz(ci);
		if (msgsz > 0 && msgsz <= UINT32_MAX)
			info->maxMsgSize = (UINT32)msgsz;

		uint64_t lblob = fido_cbor_info_maxlargeblob(ci);
		if (lblob > 0 && lblob <= UINT32_MAX)
			info->maxSerializedLargeBlobArray = (UINT32)lblob;

		const unsigned char* aaguid = fido_cbor_info_aaguid_ptr(ci);
		size_t aaguidLen = fido_cbor_info_aaguid_len(ci);
		if (aaguid && aaguidLen == 16)
			memcpy(info->aaGuid, aaguid, 16);

		size_t ntransports = fido_cbor_info_transports_len(ci);
		char** transports = fido_cbor_info_transports_ptr(ci);
		for (size_t i = 0; i < ntransports; i++)
		{
			if (!transports[i])
				continue;

			if (strcmp(transports[i], "usb") == 0)
				info->transports |= 1; /* USB */
			else if (strcmp(transports[i], "nfc") == 0)
				info->transports |= 2; /* NFC */
			else if (strcmp(transports[i], "ble") == 0)
				info->transports |= 4; /* BLE */
		}
	}
	if (ci)
		fido_cbor_info_free(&ci);
}

/**
 * Try to find an appropriate FIDO device. Opens the device at the given index
 * (0-based). If devIndex < 0, opens the first available device.
 * If ndevs is non-nullptr, stores the total number of devices found.
 * Caller must call fido_dev_close() + fido_dev_free() when done.
 */
static fido_dev_t* rdpewa_fido_open_device(RDPEWA_DEVICE_INFO* devInfo, int devIndex,
                                           size_t* ndevsOut)
{
	WLog_DBG(TAG, "Enumerating FIDO2 devices...");

	fido_dev_t* dev = nullptr;
	fido_dev_info_t* devlist = fido_dev_info_new(64);
	if (!devlist)
		return nullptr;

	size_t ndevs = 0;
	int r = fido_dev_info_manifest(devlist, 64, &ndevs);
	if (r != FIDO_OK || ndevs == 0)
	{
		WLog_WARN(TAG, "No FIDO2 authenticators found (r=%d, ndevs=%zu)", r, ndevs);
		fido_dev_info_free(&devlist, 64);
		return nullptr;
	}

	if (ndevsOut)
		*ndevsOut = ndevs;

	size_t idx = (devIndex >= 0 && (size_t)devIndex < ndevs) ? (size_t)devIndex : 0;
	const fido_dev_info_t* di = fido_dev_info_ptr(devlist, idx);
	const char* path = fido_dev_info_path(di);

	WLog_DBG(TAG, "Found %zu FIDO2 device(s), opening device %zu: '%s'", ndevs, idx, path);

	/* Save path and device strings before freeing devlist */
	const char* mfr = fido_dev_info_manufacturer_string(di);
	const char* prod = fido_dev_info_product_string(di);
	char* savedPath = path ? _strdup(path) : nullptr;
	char* savedMfr = mfr ? _strdup(mfr) : nullptr;
	char* savedProd = prod ? _strdup(prod) : nullptr;

	if ((path && !savedPath) || (mfr && !savedMfr) || (prod && !savedProd))
	{
		fido_dev_info_free(&devlist, 64);
		goto cleanup;
	}

	dev = fido_dev_new();
	if (!dev)
	{
		fido_dev_info_free(&devlist, 64);
		goto cleanup;
	}

	r = fido_dev_open(dev, savedPath);
	fido_dev_info_free(&devlist, 64);

	if (r != FIDO_OK)
	{
		WLog_ERR(TAG, "Failed to open FIDO2 device '%s': %s", savedPath, fido_strerr(r));
		fido_dev_free(&dev);
		dev = nullptr;
		goto cleanup;
	}

	if (devInfo)
		rdpewa_fido_fill_device_info(dev, savedPath, savedMfr, savedProd, devInfo);

cleanup:
	free(savedPath);
	free(savedMfr);
	free(savedProd);
	return dev;
}

/**
 * Parse the inner CTAP CBOR request for MakeCredential (sub-command 0x01).
 * The request byte string has: first byte = 0x01, followed by a CBOR map with integer keys
 * per FIDO CTAP2 authenticatorMakeCredential (section 5.1).
 */
static wStream* rdpewa_fido_make_credential(rdpContext* context, const BYTE* ctapData,
                                            size_t ctapLen, UINT32 flags)
{
	fido_dev_t* dev = nullptr;
	RDPEWA_DEVICE_INFO devInfo = WINPR_C_ARRAY_INIT;
	fido_cred_t* cred = nullptr;
	wStream* ret = nullptr;

	WINPR_ASSERT(ctapData);

	struct cbor_load_result result;
	cbor_item_t* root = cbor_load(ctapData, ctapLen, &result);
	if (!root || !cbor_isa_map(root))
	{
		WLog_ERR(TAG, "Invalid CTAP MakeCredential CBOR data");
		goto out;
	}

	cred = fido_cred_new();
	if (!cred)
		goto out;

	fido_cred_set_type(cred, COSE_ES256);

	const size_t mapSize = cbor_map_size(root);
	struct cbor_pair* pairs = cbor_map_handle(root);

	for (size_t i = 0; i < mapSize; i++)
	{
		if (!cbor_isa_uint(pairs[i].key))
			continue;

		uint64_t k = cbor_get_int(pairs[i].key);
		cbor_item_t* v = pairs[i].value;

		switch (k)
		{
			case CTAP_MAKECRED_CLIENT_DATA_HASH:
				if (cbor_isa_bytestring(v))
					fido_cred_set_clientdata_hash(cred, cbor_bytestring_handle(v),
					                              cbor_bytestring_length(v));
				break;

			case CTAP_MAKECRED_RP:
				if (cbor_isa_map(v))
				{
					char* rpId = nullptr;
					char* rpName = nullptr;
					const size_t rpSize = cbor_map_size(v);
					struct cbor_pair* rpPairs = cbor_map_handle(v);

					for (size_t j = 0; j < rpSize; j++)
					{
						if (!cbor_isa_string(rpPairs[j].key))
							continue;

						const char* rk = (const char*)cbor_string_handle(rpPairs[j].key);
						size_t rkl = cbor_string_length(rpPairs[j].key);
						if (rkl == 2 && memcmp(rk, "id", 2) == 0 &&
						    cbor_isa_string(rpPairs[j].value))
						{
							free(rpId);
							rpId = strndup((const char*)cbor_string_handle(rpPairs[j].value),
							               cbor_string_length(rpPairs[j].value));
						}
						else if (rkl == 4 && memcmp(rk, "name", 4) == 0 &&
						         cbor_isa_string(rpPairs[j].value))
						{
							free(rpName);
							rpName = strndup((const char*)cbor_string_handle(rpPairs[j].value),
							                 cbor_string_length(rpPairs[j].value));
						}
					}

					if (rpId)
						fido_cred_set_rp(cred, rpId, rpName);

					free(rpId);
					free(rpName);
				}
				break;

			case CTAP_MAKECRED_USER:
				if (cbor_isa_map(v))
				{
					const unsigned char* userId = nullptr;
					size_t userIdLen = 0;
					char* userName = nullptr;
					char* userDisplayName = nullptr;
					const size_t uSize = cbor_map_size(v);
					struct cbor_pair* uPairs = cbor_map_handle(v);

					for (size_t j = 0; j < uSize; j++)
					{
						if (!cbor_isa_string(uPairs[j].key))
							continue;

						const char* uk = (const char*)cbor_string_handle(uPairs[j].key);
						size_t ukl = cbor_string_length(uPairs[j].key);
						if (ukl == 2 && memcmp(uk, "id", 2) == 0 &&
						    cbor_isa_bytestring(uPairs[j].value))
						{
							userId = cbor_bytestring_handle(uPairs[j].value);
							userIdLen = cbor_bytestring_length(uPairs[j].value);
						}
						else if (ukl == 4 && memcmp(uk, "name", 4) == 0 &&
						         cbor_isa_string(uPairs[j].value))
						{
							free(userName);
							userName = strndup((const char*)cbor_string_handle(uPairs[j].value),
							                   cbor_string_length(uPairs[j].value));
						}
						else if (ukl == 11 && memcmp(uk, "displayName", 11) == 0 &&
						         cbor_isa_string(uPairs[j].value))
						{
							free(userDisplayName);
							userDisplayName =
							    strndup((const char*)cbor_string_handle(uPairs[j].value),
							            cbor_string_length(uPairs[j].value));
						}
					}

					if (userId && userIdLen > 0)
						fido_cred_set_user(cred, userId, userIdLen, userName, userDisplayName,
						                   nullptr);

					free(userName);
					free(userDisplayName);
				}
				break;

			case CTAP_MAKECRED_PUB_KEY_CRED_PARAMS:
				if (cbor_isa_array(v) && cbor_array_size(v) > 0)
				{
					cbor_item_t* first = cbor_array_get(v, 0);

					if (first && cbor_isa_map(first))
					{
						const size_t pSize = cbor_map_size(first);
						struct cbor_pair* pPairs = cbor_map_handle(first);

						for (size_t j = 0; j < pSize; j++)
						{
							if (!cbor_isa_string(pPairs[j].key))
								continue;

							const char* pk = (const char*)cbor_string_handle(pPairs[j].key);
							if (cbor_string_length(pPairs[j].key) == 3 &&
							    memcmp(pk, "alg", 3) == 0 && cbor_is_int(pPairs[j].value))
							{
								int alg =
								    WINPR_ASSERTING_INT_CAST(int, cbor_get_int(pPairs[j].value));
								if (cbor_isa_negint(pPairs[j].value))
									alg = -(alg + 1);
								fido_cred_set_type(cred, alg);
							}
						}
					}

					if (first)
						cbor_decref(&first);
				}
				break;

			case CTAP_MAKECRED_EXCLUDE_LIST:
				if (cbor_isa_array(v))
				{
					for (size_t j = 0; j < cbor_array_size(v); j++)
					{
						cbor_item_t* credDesc = cbor_array_get(v, j);
						if (credDesc && cbor_isa_map(credDesc))
						{
							const size_t cdSize = cbor_map_size(credDesc);
							struct cbor_pair* cdPairs = cbor_map_handle(credDesc);

							for (size_t m = 0; m < cdSize; m++)
							{
								if (!cbor_isa_string(cdPairs[m].key))
									continue;

								const char* ck = (const char*)cbor_string_handle(cdPairs[m].key);
								if (cbor_string_length(cdPairs[m].key) == 2 &&
								    memcmp(ck, "id", 2) == 0 &&
								    cbor_isa_bytestring(cdPairs[m].value))
								{
									fido_cred_exclude(cred,
									                  cbor_bytestring_handle(cdPairs[m].value),
									                  cbor_bytestring_length(cdPairs[m].value));
								}
							}
						}

						if (credDesc)
							cbor_decref(&credDesc);
					}
				}
				break;

			case CTAP_MAKECRED_OPTIONS:
				if (cbor_isa_map(v))
				{
					const size_t oSize = cbor_map_size(v);
					struct cbor_pair* oPairs = cbor_map_handle(v);

					for (size_t j = 0; j < oSize; j++)
					{
						if (!cbor_isa_string(oPairs[j].key))
							continue;

						const char* ok = (const char*)cbor_string_handle(oPairs[j].key);
						size_t okl = cbor_string_length(oPairs[j].key);
						if (okl == 2 && memcmp(ok, "rk", 2) == 0 &&
						    cbor_isa_float_ctrl(oPairs[j].value))
						{
							if (cbor_get_bool(oPairs[j].value))
								fido_cred_set_rk(cred, FIDO_OPT_TRUE);
						}
					}
				}
				break;

			default:
				break;
		}
	}

	/* Open all devices, let the user select by touch, then do the operation */
	size_t ndevs = 0;
	dev = rdpewa_fido_open_device(&devInfo, 0, &ndevs);
	if (!dev)
	{
		ret = rdpewa_cbor_encode_webauthn_response(E_FAIL, 0x01, nullptr, 0, &devInfo);
		goto out;
	}

	int r = FIDO_ERR_INTERNAL;
	if (ndevs > 1)
	{
		/* Multiple devices: open all and let user select by touch. */
		fido_dev_t** devs = (fido_dev_t**)calloc(ndevs, sizeof(*devs));
		RDPEWA_DEVICE_INFO* devInfos = calloc(ndevs, sizeof(*devInfos));
		if (!devs || !devInfos)
		{
			free((void*)devs);
			free(devInfos);
			ret = rdpewa_cbor_encode_webauthn_response(E_FAIL, 0x01, nullptr, 0, &devInfo);
			goto out;
		}
		devs[0] = dev;
		devInfos[0] = devInfo;

		for (size_t i = 1; i < ndevs; i++)
			devs[i] = rdpewa_fido_open_device(&devInfos[i], (int)i, nullptr);

		int sel = rdpewa_fido_select_device(context, devs, ndevs, 30);
		if (sel >= 0)
		{
			dev = devs[sel];
			devInfo = devInfos[sel];
		}
		else
			dev = nullptr;

		/* Cancel and close non-selected devices */
		for (size_t i = 0; i < ndevs; i++)
		{
			if (devs[i] && (int)i != sel)
			{
				fido_dev_cancel(devs[i]);
				fido_dev_close(devs[i]);
				fido_dev_free(&devs[i]);
			}
		}

		free((void*)devs);
		free(devInfos);

		if (dev == nullptr)
		{
			ret = rdpewa_cbor_encode_webauthn_response(E_FAIL, 0x01, nullptr, 0, &devInfo);
			goto out;
		}
	}

	{
		char* pin = nullptr;
		if (fido_dev_has_pin(dev))
		{
			freerdp* instance = context->instance;
			char* u = nullptr;
			char* p = nullptr;
			char* d = nullptr;
			if (!instance || !instance->AuthenticateEx ||
			    !instance->AuthenticateEx(instance, &u, &p, &d, AUTH_FIDO_PIN) || !p)
			{
				free(u);
				free(p);
				free(d);
				ret = rdpewa_cbor_encode_webauthn_response(E_FAIL, 0x01, nullptr, 0, &devInfo);
				goto out;
			}
			free(u);
			free(d);
			pin = p;
		}

		/* Run fido in background (key starts blinking), show prompt on this thread */
		RDPEWA_FIDO_ASYNC fta = { dev, cred, nullptr, pin, FIDO_ERR_INTERNAL };
		HANDLE ft = CreateThread(nullptr, 0, rdpewa_fido_makecred_thread, &fta, 0, nullptr);
		if (ft)
		{
			freerdp* instance = context->instance;
			if (instance && instance->AuthenticateEx)
			{
				char* tu = nullptr;
				char* tp = nullptr;
				char* td = nullptr;
				if (!instance->AuthenticateEx(instance, &tu, &tp, &td, AUTH_FIDO_TOUCH))
					WLog_WARN(TAG, "Touch prompt failed");
				free(tu);
				free(tp);
				free(td);
			}
			WaitForSingleObject(ft, INFINITE);
			CloseHandle(ft);
		}
		r = fta.result;
		if (pin)
			memset(pin, 0, strlen(pin));
		free(pin);
	}

	if (r != FIDO_OK)
	{
		WLog_ERR(TAG, "fido_dev_make_cred failed: %s", fido_strerr(r));
		ret = rdpewa_cbor_encode_webauthn_response(E_FAIL, 0x01, nullptr, 0, &devInfo);
		goto out;
	}

	/* Encode the CTAP authenticator response (authData + fmt + attStmt) as CBOR.
	 * Use raw data from libfido2 to preserve exact authenticator output. */
	cbor_item_t* respMap = cbor_new_definite_map(3);
	if (!respMap)
		goto out;

	/* key 1: fmt */
	cbor_item_t* fmtKey = cbor_build_uint8(1);
	const char* fmt = fido_cred_fmt(cred);
	cbor_item_t* fmtVal = cbor_build_string(fmt ? fmt : "none");
	if (!cbor_map_add(respMap, (struct cbor_pair){ .key = fmtKey, .value = fmtVal }))
	{
		cbor_decref(&fmtKey);
		cbor_decref(&fmtVal);
		cbor_decref(&respMap);
		goto out;
	}
	cbor_decref(&fmtKey);
	cbor_decref(&fmtVal);

	/* key 2: authData (raw bytes to preserve signature validity) */
	cbor_item_t* adKey = cbor_build_uint8(2);
	cbor_item_t* adVal =
	    cbor_build_bytestring(fido_cred_authdata_raw_ptr(cred), fido_cred_authdata_raw_len(cred));
	if (!cbor_map_add(respMap, (struct cbor_pair){ .key = adKey, .value = adVal }))
	{
		cbor_decref(&adKey);
		cbor_decref(&adVal);
		cbor_decref(&respMap);
		goto out;
	}
	cbor_decref(&adKey);
	cbor_decref(&adVal);

	/* key 3: attStmt, use raw attestation statement CBOR from authenticator.
	 * This preserves alg, sig, AND x5c certificate chain for packed attestation. */
	const unsigned char* rawAttStmt = fido_cred_attstmt_ptr(cred);
	size_t rawAttStmtLen = fido_cred_attstmt_len(cred);
	if (rawAttStmt && rawAttStmtLen > 0)
	{
		/* The raw attstmt is already a CBOR-encoded map. Parse and re-attach it. */
		struct cbor_load_result attResult;
		cbor_item_t* asKey = cbor_build_uint8(3);
		cbor_item_t* asVal = cbor_load(rawAttStmt, rawAttStmtLen, &attResult);
		if (!asVal || attResult.error.code != CBOR_ERR_NONE)
		{
			WLog_ERR(TAG, "Failed to parse raw attStmt CBOR");
			if (asVal)
				cbor_decref(&asVal);
			cbor_decref(&asKey);
			cbor_decref(&respMap);
			goto out;
		}

		if (!cbor_map_add(respMap, (struct cbor_pair){ .key = asKey, .value = asVal }))
		{
			cbor_decref(&asKey);
			cbor_decref(&asVal);
			cbor_decref(&respMap);
			goto out;
		}

		cbor_decref(&asKey);
		cbor_decref(&asVal);
	}
	else
	{
		/* No attestation statement - use empty map */
		cbor_item_t* asKey = cbor_build_uint8(3);
		cbor_item_t* asVal = cbor_new_definite_map(0);
		if (!cbor_map_add(respMap, (struct cbor_pair){ .key = asKey, .value = asVal }))
		{
			cbor_decref(&asKey);
			cbor_decref(&asVal);
			cbor_decref(&respMap);
			goto out;
		}

		cbor_decref(&asKey);
		cbor_decref(&asVal);
	}

	unsigned char* ctapBuf = nullptr;
	size_t ctapBufLen = 0;
	ctapBufLen = cbor_serialize_alloc(respMap, &ctapBuf, &ctapBufLen);
	cbor_decref(&respMap);

	if (ctapBufLen == 0 || !ctapBuf)
		goto out;

	ret = rdpewa_cbor_encode_webauthn_response(S_OK, 0x00, ctapBuf, ctapBufLen, &devInfo);
	free(ctapBuf);

out:
	if (root)
		cbor_decref(&root);
	if (cred)
		fido_cred_free(&cred);
	if (dev)
	{
		fido_dev_close(dev);
		fido_dev_free(&dev);
	}
	return ret;
}

/**
 * Parse the inner CTAP CBOR request for GetAssertion (sub-command 0x02).
 */
static wStream* rdpewa_fido_get_assertion(rdpContext* context, const BYTE* ctapData, size_t ctapLen,
                                          UINT32 flags)
{
	fido_dev_t* dev = nullptr;
	RDPEWA_DEVICE_INFO devInfo = WINPR_C_ARRAY_INIT;
	fido_assert_t* assert = nullptr;
	wStream* ret = nullptr;

	WINPR_ASSERT(ctapData);

	struct cbor_load_result result;
	cbor_item_t* root = cbor_load(ctapData, ctapLen, &result);
	if (!root || !cbor_isa_map(root))
	{
		WLog_ERR(TAG, "Invalid CTAP GetAssertion CBOR data");
		goto out;
	}

	assert = fido_assert_new();
	if (!assert)
		goto out;

	const size_t mapSize = cbor_map_size(root);
	struct cbor_pair* pairs = cbor_map_handle(root);

	for (size_t i = 0; i < mapSize; i++)
	{
		if (!cbor_isa_uint(pairs[i].key))
			continue;

		uint64_t k = cbor_get_int(pairs[i].key);
		cbor_item_t* v = pairs[i].value;

		switch (k)
		{
			case CTAP_GETASSERT_RP_ID:
				if (cbor_isa_string(v))
				{
					char* rpId = strndup((const char*)cbor_string_handle(v), cbor_string_length(v));
					if (rpId)
					{
						fido_assert_set_rp(assert, rpId);
						free(rpId);
					}
				}
				break;

			case CTAP_GETASSERT_CLIENT_DATA_HASH:
				if (cbor_isa_bytestring(v))
					fido_assert_set_clientdata_hash(assert, cbor_bytestring_handle(v),
					                                cbor_bytestring_length(v));
				break;

			case CTAP_GETASSERT_ALLOW_LIST:
				if (cbor_isa_array(v))
				{
					for (size_t j = 0; j < cbor_array_size(v); j++)
					{
						cbor_item_t* credDesc = cbor_array_get(v, j);
						if (credDesc && cbor_isa_map(credDesc))
						{
							const size_t cdSize = cbor_map_size(credDesc);
							struct cbor_pair* cdPairs = cbor_map_handle(credDesc);

							for (size_t m = 0; m < cdSize; m++)
							{
								if (!cbor_isa_string(cdPairs[m].key))
									continue;

								const char* ck = (const char*)cbor_string_handle(cdPairs[m].key);
								if (cbor_string_length(cdPairs[m].key) == 2 &&
								    memcmp(ck, "id", 2) == 0 &&
								    cbor_isa_bytestring(cdPairs[m].value))
								{
									fido_assert_allow_cred(
									    assert, cbor_bytestring_handle(cdPairs[m].value),
									    cbor_bytestring_length(cdPairs[m].value));
								}
							}
						}

						if (credDesc)
							cbor_decref(&credDesc);
					}
				}
				break;

			case CTAP_GETASSERT_OPTIONS:
				if (cbor_isa_map(v))
				{
					const size_t oSize = cbor_map_size(v);
					struct cbor_pair* oPairs = cbor_map_handle(v);

					for (size_t j = 0; j < oSize; j++)
					{
						if (!cbor_isa_string(oPairs[j].key))
							continue;

						const char* ok = (const char*)cbor_string_handle(oPairs[j].key);
						size_t okl = cbor_string_length(oPairs[j].key);
						if (okl == 2 && memcmp(ok, "up", 2) == 0 &&
						    cbor_isa_float_ctrl(oPairs[j].value))
						{
							fido_assert_set_up(assert, cbor_get_bool(oPairs[j].value)
							                               ? FIDO_OPT_TRUE
							                               : FIDO_OPT_FALSE);
						}
					}
				}
				break;

			default:
				break;
		}
	}

	/* Open all devices, let the user select by touch, then do the operation */
	size_t ndevs = 0;
	int r = FIDO_ERR_NO_CREDENTIALS;
	dev = rdpewa_fido_open_device(&devInfo, 0, &ndevs);
	if (!dev)
	{
		ret = rdpewa_cbor_encode_webauthn_response(E_FAIL, 0x01, nullptr, 0, &devInfo);
		goto out;
	}

	if (ndevs > 1)
	{
		fido_dev_t** devs = (fido_dev_t**)calloc(ndevs, sizeof(*devs));
		RDPEWA_DEVICE_INFO* devInfos = calloc(ndevs, sizeof(*devInfos));
		if (!devs || !devInfos)
		{
			free((void*)devs);
			free(devInfos);
			ret = rdpewa_cbor_encode_webauthn_response(E_FAIL, 0x01, nullptr, 0, &devInfo);
			goto out;
		}

		devs[0] = dev;
		devInfos[0] = devInfo;
		for (size_t i = 1; i < ndevs; i++)
			devs[i] = rdpewa_fido_open_device(&devInfos[i], (int)i, nullptr);

		int sel = rdpewa_fido_select_device(context, devs, ndevs, 30);
		if (sel >= 0)
		{
			dev = devs[sel];
			devInfo = devInfos[sel];
		}
		else
			dev = nullptr;

		/* Cancel and close non-selected devices */
		for (size_t i = 0; i < ndevs; i++)
		{
			if (devs[i] && (int)i != sel)
			{
				fido_dev_cancel(devs[i]);
				fido_dev_close(devs[i]);
				fido_dev_free(&devs[i]);
			}
		}

		free((void*)devs);
		free(devInfos);

		if (dev == nullptr)
		{
			ret = rdpewa_cbor_encode_webauthn_response(E_FAIL, 0x01, nullptr, 0, &devInfo);
			goto out;
		}
	}

	{
		char* pin = nullptr;
		if (fido_dev_has_pin(dev))
		{
			freerdp* instance = context->instance;
			char* u = nullptr;
			char* p = nullptr;
			char* d = nullptr;
			if (!instance || !instance->AuthenticateEx ||
			    !instance->AuthenticateEx(instance, &u, &p, &d, AUTH_FIDO_PIN) || !p)
			{
				free(u);
				free(p);
				free(d);
				ret = rdpewa_cbor_encode_webauthn_response(E_FAIL, 0x01, nullptr, 0, &devInfo);
				goto out;
			}
			free(u);
			free(d);
			pin = p;
		}

		/* Run fido in background (key starts blinking), show prompt on this thread */
		RDPEWA_FIDO_ASYNC fta = { dev, nullptr, assert, pin, FIDO_ERR_INTERNAL };
		HANDLE ft = CreateThread(nullptr, 0, rdpewa_fido_getassert_thread, &fta, 0, nullptr);
		if (ft)
		{
			freerdp* instance = context->instance;
			if (instance && instance->AuthenticateEx)
			{
				char* tu = nullptr;
				char* tp = nullptr;
				char* td = nullptr;
				if (!instance->AuthenticateEx(instance, &tu, &tp, &td, AUTH_FIDO_TOUCH))
					WLog_WARN(TAG, "Touch prompt failed");
				free(tu);
				free(tp);
				free(td);
			}
			WaitForSingleObject(ft, INFINITE);
			CloseHandle(ft);
		}
		r = fta.result;
		if (pin)
			memset(pin, 0, strlen(pin));
		free(pin);
	}

	if (r != FIDO_OK)
	{
		WLog_ERR(TAG, "fido_dev_get_assert failed: %s", fido_strerr(r));
		ret = rdpewa_cbor_encode_webauthn_response(E_FAIL, 0x01, nullptr, 0, &devInfo);
		goto out;
	}

	if (fido_assert_count(assert) == 0)
	{
		ret = rdpewa_cbor_encode_webauthn_response(E_FAIL, 0x01, nullptr, 0, &devInfo);
		goto out;
	}

	/* Encode CTAP assertion response as CBOR map with integer keys per FIDO CTAP section 6.2 */
	size_t nkeys = 3; /* credential(1) + authData(2) + signature(3) */
	if (fido_assert_user_id_len(assert, 0) > 0)
		nkeys = 4; /* + user(4) */
	cbor_item_t* respMap = cbor_new_definite_map(nkeys);
	if (!respMap)
		goto out;

	/* key 1: credential {id, type} */
	if (fido_assert_id_len(assert, 0) > 0)
	{
		cbor_item_t* credKey = cbor_build_uint8(1);
		cbor_item_t* credMap = cbor_new_definite_map(2);
		cbor_item_t* cidKey = cbor_build_string("id");
		cbor_item_t* cidVal =
		    cbor_build_bytestring(fido_assert_id_ptr(assert, 0), fido_assert_id_len(assert, 0));
		cbor_item_t* ctypeKey = cbor_build_string("type");
		cbor_item_t* ctypeVal = cbor_build_string("public-key");

		if (!cbor_map_add(credMap, (struct cbor_pair){ .key = cidKey, .value = cidVal }) ||
		    !cbor_map_add(credMap, (struct cbor_pair){ .key = ctypeKey, .value = ctypeVal }) ||
		    !cbor_map_add(respMap, (struct cbor_pair){ .key = credKey, .value = credMap }))
		{
			cbor_decref(&cidKey);
			cbor_decref(&cidVal);
			cbor_decref(&ctypeKey);
			cbor_decref(&ctypeVal);
			cbor_decref(&credKey);
			cbor_decref(&credMap);
			cbor_decref(&respMap);
			goto out;
		}

		cbor_decref(&cidKey);
		cbor_decref(&cidVal);
		cbor_decref(&ctypeKey);
		cbor_decref(&ctypeVal);
		cbor_decref(&credKey);
		cbor_decref(&credMap);
	}

	/* key 2: authData (raw) */
	cbor_item_t* adKey = cbor_build_uint8(2);
	cbor_item_t* adVal = cbor_build_bytestring(fido_assert_authdata_raw_ptr(assert, 0),
	                                           fido_assert_authdata_raw_len(assert, 0));
	if (!cbor_map_add(respMap, (struct cbor_pair){ .key = adKey, .value = adVal }))
	{
		cbor_decref(&adKey);
		cbor_decref(&adVal);
		cbor_decref(&respMap);
		goto out;
	}
	cbor_decref(&adKey);
	cbor_decref(&adVal);

	/* key 3: signature */
	cbor_item_t* sigKey = cbor_build_uint8(3);
	cbor_item_t* sigVal =
	    cbor_build_bytestring(fido_assert_sig_ptr(assert, 0), fido_assert_sig_len(assert, 0));
	if (!cbor_map_add(respMap, (struct cbor_pair){ .key = sigKey, .value = sigVal }))
	{
		cbor_decref(&sigKey);
		cbor_decref(&sigVal);
		cbor_decref(&respMap);
		goto out;
	}
	cbor_decref(&sigKey);
	cbor_decref(&sigVal);

	/* key 4: user -- id only */
	if (fido_assert_user_id_len(assert, 0) > 0)
	{
		cbor_item_t* uKey = cbor_build_uint8(4);
		cbor_item_t* uMap = cbor_new_definite_map(1);
		cbor_item_t* uidKey = cbor_build_string("id");
		cbor_item_t* uidVal = cbor_build_bytestring(fido_assert_user_id_ptr(assert, 0),
		                                            fido_assert_user_id_len(assert, 0));

		if (!cbor_map_add(uMap, (struct cbor_pair){ .key = uidKey, .value = uidVal }))
		{
			cbor_decref(&uidKey);
			cbor_decref(&uidVal);
			cbor_decref(&uKey);
			cbor_decref(&uMap);
			cbor_decref(&respMap);
			goto out;
		}
		cbor_decref(&uidKey);
		cbor_decref(&uidVal);

		if (!cbor_map_add(respMap, (struct cbor_pair){ .key = uKey, .value = uMap }))
		{
			cbor_decref(&uKey);
			cbor_decref(&uMap);
			cbor_decref(&respMap);
			goto out;
		}
		cbor_decref(&uKey);
		cbor_decref(&uMap);
	}

	unsigned char* ctapBuf = nullptr;
	size_t ctapBufLen = 0;
	ctapBufLen = cbor_serialize_alloc(respMap, &ctapBuf, &ctapBufLen);
	cbor_decref(&respMap);

	if (ctapBufLen == 0 || !ctapBuf)
		goto out;

	ret = rdpewa_cbor_encode_webauthn_response(S_OK, 0x00, ctapBuf, ctapBufLen, &devInfo);
	free(ctapBuf);

out:
	if (root)
		cbor_decref(&root);
	if (assert)
		fido_assert_free(&assert);
	if (dev)
	{
		fido_dev_close(dev);
		fido_dev_free(&dev);
	}
	return ret;
}

wStream* rdpewa_fido_webauthn(rdpContext* context, const RDPEWA_REQUEST* request)
{
	WINPR_ASSERT(request);

	if (!request->request || request->requestLen < 1)
	{
		WLog_ERR(TAG, "Empty request payload for WEB_AUTHN command");
		return nullptr;
	}

	BYTE subCommand = request->request[0];
	const BYTE* ctapData = request->request + 1;
	size_t ctapLen = request->requestLen - 1;

	WLog_DBG(TAG, "WebAuthn sub-command 0x%02" PRIx8 " ctapLen=%" PRIuz, subCommand, ctapLen);

	switch (subCommand)
	{
		case CTAPCBOR_CMD_MAKE_CREDENTIAL:
			return rdpewa_fido_make_credential(context, ctapData, ctapLen, request->flags);

		case CTAPCBOR_CMD_GET_ASSERTION:
			return rdpewa_fido_get_assertion(context, ctapData, ctapLen, request->flags);

		default:
			WLog_ERR(TAG, "Unknown WebAuthn sub-command 0x%02" PRIx8, subCommand);
			return nullptr;
	}
}

wStream* rdpewa_fido_is_uvpaa(void)
{
	WLog_DBG(TAG, "IUVPAA: checking for platform authenticators");

	fido_dev_info_t* devlist = fido_dev_info_new(64);
	if (!devlist)
		return rdpewa_cbor_encode_simple_response(S_OK, 0);

	size_t ndevs = 0;
	int r = fido_dev_info_manifest(devlist, 64, &ndevs);
	fido_dev_info_free(&devlist, 64);

	/* Report TRUE if at least one authenticator is found */
	UINT32 available = (r == FIDO_OK && ndevs > 0) ? 1 : 0;
	WLog_DBG(TAG, "IUVPAA: ndevs=%zu available=%" PRIu32, ndevs, available);
	return rdpewa_cbor_encode_simple_response(S_OK, available);
}

wStream* rdpewa_fido_api_version(void)
{
	WLog_DBG(TAG, "API_VERSION: reporting version 4");

	/* Report API version 4 (consistent with Windows 11 / Server 2025) */
	return rdpewa_cbor_encode_simple_response(S_OK, 4);
}

wStream* rdpewa_fido_get_authenticator_list(void)
{
	WLog_DBG(TAG, "GET_AUTHENTICATOR_LIST: enumerating authenticators");

	fido_dev_info_t* devlist = fido_dev_info_new(64);
	if (!devlist)
		return rdpewa_cbor_encode_hresult_response(S_OK);

	size_t ndevs = 0;
	int r = fido_dev_info_manifest(devlist, 64, &ndevs);
	if (r != FIDO_OK || ndevs == 0)
	{
		fido_dev_info_free(&devlist, 64);
		return rdpewa_cbor_encode_hresult_response(S_OK);
	}

	/* Build CBOR array of authenticator maps per spec section 2.2.2.3 */
	cbor_item_t* arr = cbor_new_definite_array(ndevs);
	if (!arr)
	{
		fido_dev_info_free(&devlist, 64);
		return nullptr;
	}

	for (size_t i = 0; i < ndevs; i++)
	{
		const fido_dev_info_t* di = fido_dev_info_ptr(devlist, i);
		cbor_item_t* entry = cbor_new_definite_map(4);
		if (!entry)
			continue;

		/* key 1: version */
		cbor_item_t* k1 = cbor_build_uint8(1);
		cbor_item_t* v1 = cbor_build_uint8(1);
		if (!cbor_map_add(entry, (struct cbor_pair){ .key = k1, .value = v1 }))
		{
			cbor_decref(&k1);
			cbor_decref(&v1);
			cbor_decref(&entry);
			continue;
		}
		cbor_decref(&k1);
		cbor_decref(&v1);

		/* key 3: authenticator name */
		const char* prod = fido_dev_info_product_string(di);
		cbor_item_t* k3 = cbor_build_uint8(3);
		cbor_item_t* v3 = cbor_build_string(prod ? prod : "FIDO2 Key");
		if (!cbor_map_add(entry, (struct cbor_pair){ .key = k3, .value = v3 }))
		{
			cbor_decref(&k3);
			cbor_decref(&v3);
			cbor_decref(&entry);
			continue;
		}
		cbor_decref(&k3);
		cbor_decref(&v3);

		/* key 4: authenticator logo (empty) */
		cbor_item_t* k4 = cbor_build_uint8(4);
		cbor_item_t* v4 = cbor_build_bytestring(nullptr, 0);
		if (!cbor_map_add(entry, (struct cbor_pair){ .key = k4, .value = v4 }))
		{
			cbor_decref(&k4);
			cbor_decref(&v4);
			cbor_decref(&entry);
			continue;
		}
		cbor_decref(&k4);
		cbor_decref(&v4);

		/* key 5: isLocked */
		cbor_item_t* k5 = cbor_build_uint8(5);
		cbor_item_t* v5 = cbor_build_bool(false);
		if (!cbor_map_add(entry, (struct cbor_pair){ .key = k5, .value = v5 }))
		{
			cbor_decref(&k5);
			cbor_decref(&v5);
			cbor_decref(&entry);
			continue;
		}
		cbor_decref(&k5);
		cbor_decref(&v5);

		if (!cbor_array_push(arr, entry))
		{
			cbor_decref(&entry);
			continue;
		}
		cbor_decref(&entry);
	}

	fido_dev_info_free(&devlist, 64);

	unsigned char* cborBuf = nullptr;
	size_t cborLen = 0;
	cborLen = cbor_serialize_alloc(arr, &cborBuf, &cborLen);
	cbor_decref(&arr);

	if (cborLen == 0 || !cborBuf)
		return nullptr;

	/* Response: 4-byte LE HRESULT + CBOR array */
	wStream* s = Stream_New(nullptr, 4 + cborLen);
	if (!s)
	{
		free(cborBuf);
		return nullptr;
	}

	Stream_Write_UINT32(s, (UINT32)S_OK);
	Stream_Write(s, cborBuf, cborLen);
	free(cborBuf);
	return s;
}

wStream* rdpewa_fido_get_credentials(rdpContext* context, const char* rpId)
{
	WINPR_UNUSED(context);

	if (!rpId || !rpId[0])
	{
		WLog_WARN(TAG, "GET_CREDENTIALS: no rpId provided");
		return rdpewa_cbor_encode_hresult_response(S_OK);
	}

	WLog_DBG(TAG, "GET_CREDENTIALS: querying credentials for rpId '%s'", rpId);

	/* Iterate all devices and collect credentials from each */
	size_t ndevs = 0;
	RDPEWA_DEVICE_INFO devInfo = WINPR_C_ARRAY_INIT;
	fido_dev_t* dev = rdpewa_fido_open_device(&devInfo, 0, &ndevs);
	if (!dev)
		return rdpewa_cbor_encode_hresult_response(S_OK);

	cbor_item_t* arr = cbor_new_definite_array(0);
	if (!arr)
	{
		fido_dev_close(dev);
		fido_dev_free(&dev);
		return nullptr;
	}

	for (size_t devIdx = 0; devIdx < ndevs; devIdx++)
	{
		if (devIdx > 0)
		{
			fido_dev_close(dev);
			fido_dev_free(&dev);
			dev = rdpewa_fido_open_device(&devInfo, (int)devIdx, nullptr);
			if (!dev)
				continue;
		}

		if (!fido_dev_supports_credman(dev))
		{
			WLog_DBG(TAG, "GET_CREDENTIALS: device %zu does not support credential management",
			         devIdx);
			continue;
		}

		/* Credential management requires a PIN token. We don't prompt for PIN here
		 * since the platform handles credential selection differently. The server
		 * sends allowList in command 5 and the authenticator handles it at touch time.
		 * Only enumerate credentials from devices that don't require a PIN, to avoid
		 * spamming the user with PIN prompts. */
		if (fido_dev_has_pin(dev))
		{
			WLog_DBG(TAG, "GET_CREDENTIALS: device %zu requires PIN, skipping", devIdx);
			continue;
		}

		const char* pin = nullptr;

		fido_credman_rk_t* rk = fido_credman_rk_new();
		if (!rk)
			continue;

		int r = fido_credman_get_dev_rk(dev, rpId, rk, pin);
		if (r != FIDO_OK)
		{
			WLog_DBG(TAG, "GET_CREDENTIALS: device %zu: %s", devIdx, fido_strerr(r));
			fido_credman_rk_free(&rk);
			continue;
		}

		size_t ncreds = fido_credman_rk_count(rk);
		WLog_DBG(TAG, "GET_CREDENTIALS: device %zu: found %zu credentials", devIdx, ncreds);

		for (size_t i = 0; i < ncreds; i++)
		{
			const fido_cred_t* cred = fido_credman_rk(rk, i);
			if (!cred)
				continue;

			cbor_item_t* entry = cbor_new_definite_map(4);
			if (!entry)
				continue;

			cbor_item_t* k0 = cbor_build_uint8(0);
			cbor_item_t* v0 = cbor_build_uint8(4);
			if (!cbor_map_add(entry, (struct cbor_pair){ .key = k0, .value = v0 }))
			{
				cbor_decref(&k0);
				cbor_decref(&v0);
				cbor_decref(&entry);
				continue;
			}
			cbor_decref(&k0);
			cbor_decref(&v0);

			cbor_item_t* k1 = cbor_build_uint8(1);
			cbor_item_t* v1 = cbor_build_bytestring(fido_cred_id_ptr(cred), fido_cred_id_len(cred));
			if (!cbor_map_add(entry, (struct cbor_pair){ .key = k1, .value = v1 }))
			{
				cbor_decref(&k1);
				cbor_decref(&v1);
				cbor_decref(&entry);
				continue;
			}
			cbor_decref(&k1);
			cbor_decref(&v1);

			cbor_item_t* k2 = cbor_build_uint8(2);
			cbor_item_t* rpMap = cbor_new_definite_map(1);
			{
				cbor_item_t* rk2 = cbor_build_string("id");
				cbor_item_t* rv2 = cbor_build_string(rpId);
				if (!cbor_map_add(rpMap, (struct cbor_pair){ .key = rk2, .value = rv2 }))
				{
					cbor_decref(&rk2);
					cbor_decref(&rv2);
					cbor_decref(&k2);
					cbor_decref(&rpMap);
					cbor_decref(&entry);
					continue;
				}
				cbor_decref(&rk2);
				cbor_decref(&rv2);
			}

			if (!cbor_map_add(entry, (struct cbor_pair){ .key = k2, .value = rpMap }))
			{
				cbor_decref(&k2);
				cbor_decref(&rpMap);
				cbor_decref(&entry);
				continue;
			}
			cbor_decref(&k2);
			cbor_decref(&rpMap);

			cbor_item_t* k3 = cbor_build_uint8(3);
			cbor_item_t* userMap = cbor_new_definite_map(2);
			{
				cbor_item_t* uk = cbor_build_string("id");
				cbor_item_t* uv =
				    cbor_build_bytestring(fido_cred_user_id_ptr(cred), fido_cred_user_id_len(cred));
				if (!cbor_map_add(userMap, (struct cbor_pair){ .key = uk, .value = uv }))
				{
					cbor_decref(&uk);
					cbor_decref(&uv);
					cbor_decref(&k3);
					cbor_decref(&userMap);
					cbor_decref(&entry);
					continue;
				}
				cbor_decref(&uk);
				cbor_decref(&uv);

				const char* uname = fido_cred_user_name(cred);
				if (uname)
				{
					cbor_item_t* unk = cbor_build_string("name");
					cbor_item_t* unv = cbor_build_string(uname);
					if (!cbor_map_add(userMap, (struct cbor_pair){ .key = unk, .value = unv }))
					{
						cbor_decref(&unk);
						cbor_decref(&unv);
						cbor_decref(&k3);
						cbor_decref(&userMap);
						cbor_decref(&entry);
						continue;
					}
					cbor_decref(&unk);
					cbor_decref(&unv);
				}
			}

			if (!cbor_map_add(entry, (struct cbor_pair){ .key = k3, .value = userMap }))
			{
				cbor_decref(&k3);
				cbor_decref(&userMap);
				cbor_decref(&entry);
				continue;
			}
			cbor_decref(&k3);
			cbor_decref(&userMap);

			if (!cbor_array_push(arr, entry))
			{
				cbor_decref(&entry);
				continue;
			}
			cbor_decref(&entry);
		}

		fido_credman_rk_free(&rk);
	}

	if (dev)
	{
		fido_dev_close(dev);
		fido_dev_free(&dev);
	}

	unsigned char* cborBuf = nullptr;
	size_t cborLen = 0;
	cborLen = cbor_serialize_alloc(arr, &cborBuf, &cborLen);
	cbor_decref(&arr);

	if (cborLen == 0 || !cborBuf)
		return nullptr;

	wStream* s = Stream_New(nullptr, 4 + cborLen);
	if (!s)
	{
		free(cborBuf);
		return nullptr;
	}

	Stream_Write_UINT32(s, (UINT32)S_OK);
	Stream_Write(s, cborBuf, cborLen);
	free(cborBuf);
	return s;
}
