/**
 * WinPR: Windows Portable Runtime
 * NCrypt pkcs11 provider
 *
 * Copyright 2021 David Fort <contact@hardening-consulting.com>
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

#include <stdlib.h>
#include <pkcs11-helper-1.0/pkcs11.h>

#include <winpr/library.h>
#include <winpr/assert.h>
#include <winpr/spec.h>
#include <winpr/smartcard.h>
#include <winpr/asn1.h>

#include "../log.h"
#include "ncrypt.h"

#define TAG WINPR_TAG("ncryptp11")

#define MAX_SLOTS 64
#define MAX_KEYS 64
#define MAX_KEYS_PER_SLOT 64

/** @brief ncrypt provider handle */
typedef struct
{
	NCryptBaseProvider baseProvider;

	HANDLE library;
	CK_FUNCTION_LIST_PTR p11;
} NCryptP11ProviderHandle;

/** @brief a handle returned by NCryptOpenKey */
typedef struct
{
	NCryptBaseHandle base;
	NCryptP11ProviderHandle* provider;
	CK_SLOT_ID slotId;
	CK_BYTE keyCertId[64];
	CK_ULONG keyCertIdLen;
} NCryptP11KeyHandle;

typedef struct
{
	CK_SLOT_ID slotId;
	CK_SLOT_INFO slotInfo;
	CK_KEY_TYPE keyType;
	CK_CHAR keyLabel[256];
	CK_ULONG idLen;
	CK_BYTE id[64];
} NCryptKeyEnum;

typedef struct
{
	CK_ULONG nslots;
	CK_SLOT_ID slots[MAX_SLOTS];
	CK_ULONG nKeys;
	NCryptKeyEnum keys[MAX_KEYS];
	CK_ULONG keyIndex;
} P11EnumKeysState;

typedef struct
{
	const char* label;
	BYTE tag[3];
} piv_cert_tags_t;
static const piv_cert_tags_t piv_cert_tags[] = {
	{ "Certificate for PIV Authentication", "\x5F\xC1\x05" },
	{ "Certificate for Digital Signature", "\x5F\xC1\x0A" },
	{ "Certificate for Key Management", "\x5F\xC1\x0B" },
	{ "Certificate for Card Authentication", "\x5F\xC1\x01" },
};

static const BYTE APDU_PIV_SELECT_AID[] = { 0x00, 0xA4, 0x04, 0x00, 0x09, 0xA0, 0x00, 0x00,
	                                        0x03, 0x08, 0x00, 0x00, 0x10, 0x00, 0x00 };
static const BYTE APDU_PIV_GET_CHUID[] = { 0x00, 0xCB, 0x3F, 0xFF, 0x05, 0x5C,
	                                       0x03, 0x5F, 0xC1, 0x02, 0x00 };
#define PIV_CONTAINER_NAME_LEN 36

static CK_OBJECT_CLASS object_class_public_key = CKO_PUBLIC_KEY;
static CK_BBOOL object_verify = CK_TRUE;
static CK_KEY_TYPE object_ktype_rsa = CKK_RSA;

static CK_ATTRIBUTE public_key_filter[] = {
	{ CKA_CLASS, &object_class_public_key, sizeof(object_class_public_key) },
	{ CKA_VERIFY, &object_verify, sizeof(object_verify) },
	{ CKA_KEY_TYPE, &object_ktype_rsa, sizeof(object_ktype_rsa) }
};

static SECURITY_STATUS NCryptP11StorageProvider_dtor(NCRYPT_HANDLE handle)
{
	NCryptP11ProviderHandle* provider = (NCryptP11ProviderHandle*)handle;
	CK_RV rv;

	WINPR_ASSERT(provider);
	rv = provider->p11->C_Finalize(NULL);
	if (rv != CKR_OK)
	{
	}

	if (provider->library)
		FreeLibrary(provider->library);

	return winpr_NCryptDefault_dtor(handle);
}

static void fix_padded_string(char* str, size_t maxlen)
{
	char* ptr = str + maxlen - 1;

	while (ptr > str && *ptr == ' ')
		ptr--;
	ptr++;
	*ptr = 0;
}

static BOOL attributes_have_unallocated_buffers(CK_ATTRIBUTE_PTR attributes, CK_ULONG count)
{
	CK_ULONG i;

	for (i = 0; i < count; i++)
	{
		if (!attributes[i].pValue && (attributes[i].ulValueLen != CK_UNAVAILABLE_INFORMATION))
			return TRUE;
	}

	return FALSE;
}

static BOOL attribute_allocate_attribute_array(CK_ATTRIBUTE_PTR attribute)
{
	attribute->pValue = calloc(attribute->ulValueLen, sizeof(void*));
	return !!attribute->pValue;
}

static BOOL attribute_allocate_ulong_array(CK_ATTRIBUTE_PTR attribute)
{
	attribute->pValue = calloc(attribute->ulValueLen, sizeof(CK_ULONG));
	return !!attribute->pValue;
}

static BOOL attribute_allocate_buffer(CK_ATTRIBUTE_PTR attribute)
{
	attribute->pValue = calloc(attribute->ulValueLen, 1);
	return !!attribute->pValue;
}

static BOOL attributes_allocate_buffers(CK_ATTRIBUTE_PTR attributes, CK_ULONG count)
{
	CK_ULONG i;
	BOOL ret = TRUE;

	for (i = 0; i < count; i++)
	{
		if (attributes[i].pValue || (attributes[i].ulValueLen == CK_UNAVAILABLE_INFORMATION))
			continue;

		switch (attributes[i].type)
		{
			case CKA_WRAP_TEMPLATE:
			case CKA_UNWRAP_TEMPLATE:
				ret &= attribute_allocate_attribute_array(&attributes[i]);
				break;

			case CKA_ALLOWED_MECHANISMS:
				ret &= attribute_allocate_ulong_array(&attributes[i]);
				break;

			default:
				ret &= attribute_allocate_buffer(&attributes[i]);
				break;
		}
	}

	return ret;
}

static CK_RV object_load_attributes(NCryptP11ProviderHandle* provider, CK_SESSION_HANDLE session,
                                    CK_OBJECT_HANDLE object, CK_ATTRIBUTE_PTR attributes,
                                    CK_ULONG count)
{
	CK_RV rv;

	WINPR_ASSERT(provider);
	WINPR_ASSERT(provider->p11);
	WINPR_ASSERT(provider->p11->C_GetAttributeValue);

	rv = provider->p11->C_GetAttributeValue(session, object, attributes, count);

	switch (rv)
	{
		case CKR_OK:
			if (!attributes_have_unallocated_buffers(attributes, count))
				return rv;
			/* fallthrough */
		case CKR_ATTRIBUTE_SENSITIVE:
		case CKR_ATTRIBUTE_TYPE_INVALID:
		case CKR_BUFFER_TOO_SMALL:
			/* attributes need some buffers for the result value */
			if (!attributes_allocate_buffers(attributes, count))
				return CKR_HOST_MEMORY;

			rv = provider->p11->C_GetAttributeValue(session, object, attributes, count);
			break;
		default:
			return rv;
	}

	switch (rv)
	{
		case CKR_ATTRIBUTE_SENSITIVE:
		case CKR_ATTRIBUTE_TYPE_INVALID:
		case CKR_BUFFER_TOO_SMALL:
			WLog_ERR(TAG, "C_GetAttributeValue return %d even after buffer allocation", rv);
			break;
	}
	return rv;
}

static const char* CK_RV_error_string(CK_RV rv)
{
	static char generic_buffer[200];
#define ERR_ENTRY(X) \
	case X:          \
		return #X

	switch (rv)
	{
		ERR_ENTRY(CKR_OK);
		ERR_ENTRY(CKR_CANCEL);
		ERR_ENTRY(CKR_HOST_MEMORY);
		ERR_ENTRY(CKR_SLOT_ID_INVALID);
		ERR_ENTRY(CKR_GENERAL_ERROR);
		ERR_ENTRY(CKR_FUNCTION_FAILED);
		ERR_ENTRY(CKR_ARGUMENTS_BAD);
		ERR_ENTRY(CKR_NO_EVENT);
		ERR_ENTRY(CKR_NEED_TO_CREATE_THREADS);
		ERR_ENTRY(CKR_CANT_LOCK);
		ERR_ENTRY(CKR_ATTRIBUTE_READ_ONLY);
		ERR_ENTRY(CKR_ATTRIBUTE_SENSITIVE);
		ERR_ENTRY(CKR_ATTRIBUTE_TYPE_INVALID);
		ERR_ENTRY(CKR_ATTRIBUTE_VALUE_INVALID);
		ERR_ENTRY(CKR_DATA_INVALID);
		ERR_ENTRY(CKR_DATA_LEN_RANGE);
		ERR_ENTRY(CKR_DEVICE_ERROR);
		ERR_ENTRY(CKR_DEVICE_MEMORY);
		ERR_ENTRY(CKR_DEVICE_REMOVED);
		ERR_ENTRY(CKR_ENCRYPTED_DATA_INVALID);
		ERR_ENTRY(CKR_ENCRYPTED_DATA_LEN_RANGE);
		ERR_ENTRY(CKR_FUNCTION_CANCELED);
		ERR_ENTRY(CKR_FUNCTION_NOT_PARALLEL);
		ERR_ENTRY(CKR_FUNCTION_NOT_SUPPORTED);
		ERR_ENTRY(CKR_KEY_HANDLE_INVALID);
		ERR_ENTRY(CKR_KEY_SIZE_RANGE);
		ERR_ENTRY(CKR_KEY_TYPE_INCONSISTENT);
		ERR_ENTRY(CKR_KEY_NOT_NEEDED);
		ERR_ENTRY(CKR_KEY_CHANGED);
		ERR_ENTRY(CKR_KEY_NEEDED);
		ERR_ENTRY(CKR_KEY_INDIGESTIBLE);
		ERR_ENTRY(CKR_KEY_FUNCTION_NOT_PERMITTED);
		ERR_ENTRY(CKR_KEY_NOT_WRAPPABLE);
		ERR_ENTRY(CKR_KEY_UNEXTRACTABLE);
		ERR_ENTRY(CKR_MECHANISM_INVALID);
		ERR_ENTRY(CKR_MECHANISM_PARAM_INVALID);
		ERR_ENTRY(CKR_OBJECT_HANDLE_INVALID);
		ERR_ENTRY(CKR_OPERATION_ACTIVE);
		ERR_ENTRY(CKR_OPERATION_NOT_INITIALIZED);
		ERR_ENTRY(CKR_PIN_INCORRECT);
		ERR_ENTRY(CKR_PIN_INVALID);
		ERR_ENTRY(CKR_PIN_LEN_RANGE);
		ERR_ENTRY(CKR_PIN_EXPIRED);
		ERR_ENTRY(CKR_PIN_LOCKED);
		ERR_ENTRY(CKR_SESSION_CLOSED);
		ERR_ENTRY(CKR_SESSION_COUNT);
		ERR_ENTRY(CKR_SESSION_HANDLE_INVALID);
		ERR_ENTRY(CKR_SESSION_PARALLEL_NOT_SUPPORTED);
		ERR_ENTRY(CKR_SESSION_READ_ONLY);
		ERR_ENTRY(CKR_SESSION_EXISTS);
		ERR_ENTRY(CKR_SESSION_READ_ONLY_EXISTS);
		ERR_ENTRY(CKR_SESSION_READ_WRITE_SO_EXISTS);
		ERR_ENTRY(CKR_SIGNATURE_INVALID);
		ERR_ENTRY(CKR_SIGNATURE_LEN_RANGE);
		ERR_ENTRY(CKR_TEMPLATE_INCOMPLETE);
		ERR_ENTRY(CKR_TEMPLATE_INCONSISTENT);
		ERR_ENTRY(CKR_TOKEN_NOT_PRESENT);
		ERR_ENTRY(CKR_TOKEN_NOT_RECOGNIZED);
		ERR_ENTRY(CKR_TOKEN_WRITE_PROTECTED);
		ERR_ENTRY(CKR_UNWRAPPING_KEY_HANDLE_INVALID);
		ERR_ENTRY(CKR_UNWRAPPING_KEY_SIZE_RANGE);
		ERR_ENTRY(CKR_UNWRAPPING_KEY_TYPE_INCONSISTENT);
		ERR_ENTRY(CKR_USER_ALREADY_LOGGED_IN);
		ERR_ENTRY(CKR_USER_NOT_LOGGED_IN);
		ERR_ENTRY(CKR_USER_PIN_NOT_INITIALIZED);
		ERR_ENTRY(CKR_USER_TYPE_INVALID);
		ERR_ENTRY(CKR_USER_ANOTHER_ALREADY_LOGGED_IN);
		ERR_ENTRY(CKR_USER_TOO_MANY_TYPES);
		ERR_ENTRY(CKR_WRAPPED_KEY_INVALID);
		ERR_ENTRY(CKR_WRAPPED_KEY_LEN_RANGE);
		ERR_ENTRY(CKR_WRAPPING_KEY_HANDLE_INVALID);
		ERR_ENTRY(CKR_WRAPPING_KEY_SIZE_RANGE);
		ERR_ENTRY(CKR_WRAPPING_KEY_TYPE_INCONSISTENT);
		ERR_ENTRY(CKR_RANDOM_SEED_NOT_SUPPORTED);
		ERR_ENTRY(CKR_RANDOM_NO_RNG);
		ERR_ENTRY(CKR_DOMAIN_PARAMS_INVALID);
		ERR_ENTRY(CKR_BUFFER_TOO_SMALL);
		ERR_ENTRY(CKR_SAVED_STATE_INVALID);
		ERR_ENTRY(CKR_INFORMATION_SENSITIVE);
		ERR_ENTRY(CKR_STATE_UNSAVEABLE);
		ERR_ENTRY(CKR_CRYPTOKI_NOT_INITIALIZED);
		ERR_ENTRY(CKR_CRYPTOKI_ALREADY_INITIALIZED);
		ERR_ENTRY(CKR_MUTEX_BAD);
		ERR_ENTRY(CKR_MUTEX_NOT_LOCKED);
		ERR_ENTRY(CKR_FUNCTION_REJECTED);
		default:
			snprintf(generic_buffer, sizeof(generic_buffer), "unknown 0x%lx", rv);
			return generic_buffer;
	}
#undef ERR_ENTRY
}

static SECURITY_STATUS collect_keys(NCryptP11ProviderHandle* provider, P11EnumKeysState* state)
{
	CK_RV rv;
	CK_ULONG i, j, nslotObjects;
	CK_OBJECT_HANDLE slotObjects[MAX_KEYS_PER_SLOT] = { 0 };
	const char* step = NULL;
	CK_FUNCTION_LIST_PTR p11;

	WINPR_ASSERT(provider);

	p11 = provider->p11;
	WINPR_ASSERT(p11);

	state->nKeys = 0;
	for (i = 0; i < state->nslots; i++)
	{
		CK_SESSION_HANDLE session = (CK_SESSION_HANDLE)NULL;
		CK_SLOT_INFO slotInfo;
		CK_TOKEN_INFO tokenInfo;

		WINPR_ASSERT(p11->C_GetSlotInfo);
		rv = p11->C_GetSlotInfo(state->slots[i], &slotInfo);
		if (rv != CKR_OK)
		{
			WLog_ERR(TAG, "unable to retrieve information for slot #%d(%d)", i, state->slots[i]);
			continue;
		}

		fix_padded_string((char*)slotInfo.slotDescription, sizeof(slotInfo.slotDescription));
		WLog_DBG(TAG, "%s: collecting keys for slot #%d(%lu) descr='%s' flags=0x%x", __FUNCTION__,
		         i, state->slots[i], slotInfo.slotDescription, slotInfo.flags);

		/* this is a safety guard as we're supposed to have listed only readers with tokens in them
		 */
		if (!(slotInfo.flags & CKF_TOKEN_PRESENT))
		{
			WLog_INFO(TAG, "token not present for slot #%d(%d)", i, state->slots[i]);
			continue;
		}

		WINPR_ASSERT(p11->C_GetTokenInfo);
		rv = p11->C_GetTokenInfo(state->slots[i], &tokenInfo);
		if (rv != CKR_OK)
		{
			WLog_INFO(TAG, "unable to retrieve token info for slot #%d(%d)", i, state->slots[i]);
		}
		else
		{
			fix_padded_string((char*)tokenInfo.label, sizeof(tokenInfo.label));
			WLog_DBG(TAG, "%s: token, label='%s' flags=0x%x", __FUNCTION__, tokenInfo.label,
			         tokenInfo.flags);
		}

		WINPR_ASSERT(p11->C_OpenSession);
		rv = p11->C_OpenSession(state->slots[i], CKF_SERIAL_SESSION, NULL, NULL, &session);
		if (rv != CKR_OK)
		{
			WLog_ERR(TAG, "unable to openSession for slot #%d(%d), session=%p rv=%s", i,
			         state->slots[i], session, CK_RV_error_string(rv));
			continue;
		}

		WINPR_ASSERT(p11->C_FindObjectsInit);
		rv = p11->C_FindObjectsInit(session, public_key_filter, ARRAYSIZE(public_key_filter));
		if (rv != CKR_OK)
		{
			// TODO: shall it be fatal ?
			WLog_ERR(TAG, "unable to initiate search for slot #%d(%d), rv=%s", i, state->slots[i],
			         CK_RV_error_string(rv));
			step = "C_FindObjectsInit";
			goto cleanup_FindObjectsInit;
		}

		WINPR_ASSERT(p11->C_FindObjects);
		rv = p11->C_FindObjects(session, &slotObjects[0], ARRAYSIZE(slotObjects), &nslotObjects);
		if (rv != CKR_OK)
		{
			WLog_ERR(TAG, "unable to findObjects for slot #%d(%d), rv=%s", i, state->slots[i],
			         CK_RV_error_string(rv));
			step = "C_FindObjects";
			goto cleanup_FindObjects;
		}

		WLog_DBG(TAG, "%s: slot has %d objects", __FUNCTION__, nslotObjects);
		for (j = 0; j < nslotObjects; j++)
		{
			NCryptKeyEnum* key = &state->keys[state->nKeys];
			CK_OBJECT_CLASS dataClass = CKO_PUBLIC_KEY;
			CK_ATTRIBUTE key_or_certAttrs[] = {
				{ CKA_ID, &key->id, sizeof(key->id) },
				{ CKA_CLASS, &dataClass, sizeof(dataClass) },
				{ CKA_LABEL, &key->keyLabel, sizeof(key->keyLabel) },
				{ CKA_KEY_TYPE, &key->keyType, sizeof(key->keyType) }
			};

			rv = object_load_attributes(provider, session, slotObjects[j], key_or_certAttrs,
			                            ARRAYSIZE(key_or_certAttrs));
			if (rv != CKR_OK)
			{
				WLog_ERR(TAG, "error getting attributes, rv=%s", CK_RV_error_string(rv));
				continue;
			}

			key->idLen = key_or_certAttrs[0].ulValueLen;
			key->slotId = state->slots[i];
			key->slotInfo = slotInfo;
			state->nKeys++;
		}

	cleanup_FindObjects:
		WINPR_ASSERT(p11->C_FindObjectsFinal);
		rv = p11->C_FindObjectsFinal(session);
		if (rv != CKR_OK)
		{
			WLog_ERR(TAG, "error during C_FindObjectsFinal for slot #%d(%d) (errorStep=%s), rv=%s",
			         i, state->slots[i], step, CK_RV_error_string(rv));
		}
	cleanup_FindObjectsInit:
		WINPR_ASSERT(p11->C_CloseSession);
		rv = p11->C_CloseSession(session);
		if (rv != CKR_OK)
		{
			WLog_ERR(TAG, "error closing session for slot #%d(%d) (errorStep=%s), rv=%s", i,
			         state->slots[i], step, CK_RV_error_string(rv));
		}
	}

	return ERROR_SUCCESS;
}

static BOOL convertKeyType(CK_KEY_TYPE k, LPWSTR dest, DWORD len, DWORD* outlen)
{
	DWORD retLen;
	const WCHAR* r = NULL;

#define ALGO_CASE(V, S) \
	case V:             \
		r = S;          \
		break
	switch (k)
	{
		ALGO_CASE(CKK_RSA, BCRYPT_RSA_ALGORITHM);
		ALGO_CASE(CKK_DSA, BCRYPT_DSA_ALGORITHM);
		ALGO_CASE(CKK_DH, BCRYPT_DH_ALGORITHM);
		ALGO_CASE(CKK_ECDSA, BCRYPT_ECDSA_ALGORITHM);
		ALGO_CASE(CKK_RC2, BCRYPT_RC2_ALGORITHM);
		ALGO_CASE(CKK_RC4, BCRYPT_RC4_ALGORITHM);
		ALGO_CASE(CKK_DES, BCRYPT_DES_ALGORITHM);
		ALGO_CASE(CKK_DES3, BCRYPT_3DES_ALGORITHM);
		case CKK_DES2:
		case CKK_X9_42_DH:
		case CKK_KEA:
		case CKK_GENERIC_SECRET:
		case CKK_CAST:
		case CKK_CAST3:
		case CKK_CAST128:
		case CKK_RC5:
		case CKK_IDEA:
		case CKK_SKIPJACK:
		case CKK_BATON:
		case CKK_JUNIPER:
		case CKK_CDMF:
		case CKK_AES:
		case CKK_BLOWFISH:
		case CKK_TWOFISH:
		default:
			break;
	}
#undef ALGO_CASE

	retLen = _wcslen(r);
	if (outlen)
		*outlen = retLen;

	if (!r)
	{
		if (dest && len > 0)
			dest[0] = 0;
		return FALSE;
	}
	else
	{
		if (retLen + 1 < len)
		{
			WLog_ERR(TAG, "target buffer is too small for algo name");
			return FALSE;
		}

		if (dest)
		{
			memcpy(dest, r, retLen * 2);
			dest[retLen] = 0;
		}
	}

	return TRUE;
}

static void wprintKeyName(LPWSTR str, CK_SLOT_ID slotId, CK_BYTE* id, CK_ULONG idLen)
{
	char asciiName[128] = { 0 };
	char* ptr = asciiName;
	const CK_BYTE* bytePtr;
	CK_ULONG i;

	*ptr = '\\';
	ptr++;

	bytePtr = ((CK_BYTE*)&slotId);
	for (i = 0; i < sizeof(slotId); i++, bytePtr++, ptr += 2)
		snprintf(ptr, 3, "%.2x", *bytePtr);

	*ptr = '\\';
	ptr++;

	for (i = 0; i < idLen; i++, id++, ptr += 2)
		snprintf(ptr, 3, "%.2x", *id);

	ConvertUtf8NToWChar(asciiName, ARRAYSIZE(asciiName), str,
	                    strnlen(asciiName, ARRAYSIZE(asciiName)) + 1);
}

static size_t parseHex(const char* str, const char* end, CK_BYTE* target)
{
	int ret = 0;

	for (ret = 0; str != end && *str; str++, ret++, target++)
	{
		CK_BYTE v = 0;
		if (*str <= '9' && *str >= '0')
		{
			v = (*str - '0');
		}
		else if (*str <= 'f' && *str >= 'a')
		{
			v = (10 + *str - 'a');
		}
		else if (*str <= 'F' && *str >= 'A')
		{
			v |= (10 + *str - 'A');
		}
		else
		{
			return 0;
		}
		v <<= 4;
		str++;

		if (!*str || str == end)
			return 0;

		if (*str <= '9' && *str >= '0')
		{
			v |= (*str - '0');
		}
		else if (*str <= 'f' && *str >= 'a')
		{
			v |= (10 + *str - 'a');
		}
		else if (*str <= 'F' && *str >= 'A')
		{
			v |= (10 + *str - 'A');
		}
		else
		{
			return 0;
		}

		*target = v;
	}
	return ret;
}

static SECURITY_STATUS parseKeyName(LPCWSTR pszKeyName, CK_SLOT_ID* slotId, CK_BYTE* id,
                                    CK_ULONG* idLen)
{
	char asciiKeyName[128] = { 0 };
	char* pos;

	if (ConvertWCharToUtf8(pszKeyName, asciiKeyName, ARRAYSIZE(asciiKeyName)) < 0)
		return NTE_BAD_KEY;

	if (*asciiKeyName != '\\')
		return NTE_BAD_KEY;

	pos = strchr(&asciiKeyName[1], '\\');
	if (!pos)
		return NTE_BAD_KEY;

	if ((size_t)(pos - &asciiKeyName[1]) > sizeof(CK_SLOT_ID) * 2ull)
		return NTE_BAD_KEY;

	*slotId = (CK_SLOT_ID)0;
	if (parseHex(&asciiKeyName[1], pos, (CK_BYTE*)slotId) != sizeof(CK_SLOT_ID))
		return NTE_BAD_KEY;

	*idLen = parseHex(pos + 1, NULL, id);
	if (!*idLen)
		return NTE_BAD_KEY;

	return ERROR_SUCCESS;
}

static SECURITY_STATUS NCryptP11EnumKeys(NCRYPT_PROV_HANDLE hProvider, LPCWSTR pszScope,
                                         NCryptKeyName** ppKeyName, PVOID* ppEnumState,
                                         DWORD dwFlags)
{
	SECURITY_STATUS ret;
	NCryptP11ProviderHandle* provider = (NCryptP11ProviderHandle*)hProvider;
	P11EnumKeysState* state = (P11EnumKeysState*)*ppEnumState;
	CK_RV rv = { 0 };
	CK_SLOT_ID currentSlot = { 0 };
	CK_SESSION_HANDLE currentSession = (CK_SESSION_HANDLE)NULL;
	char slotFilterBuffer[65] = { 0 };
	char* slotFilter = NULL;
	size_t slotFilterLen = 0;

	ret = checkNCryptHandle((NCRYPT_HANDLE)hProvider, WINPR_NCRYPT_PROVIDER);
	if (ret != ERROR_SUCCESS)
		return ret;

	if (pszScope)
	{
		/*
		 * check whether pszScope is of the form \\.\<reader name>\ for filtering by
		 * card reader
		 */
		char asciiScope[128 + 6] = { 0 };
		size_t asciiScopeLen;

		if (ConvertWCharToUtf8(pszScope, asciiScope, ARRAYSIZE(asciiScope)) < 0)
			return NTE_INVALID_PARAMETER;

		if (strstr(asciiScope, "\\\\.\\") != asciiScope)
			return NTE_INVALID_PARAMETER;

		asciiScopeLen = strnlen(asciiScope, sizeof(asciiScope));
		if (asciiScope[asciiScopeLen - 1] != '\\')
			return NTE_INVALID_PARAMETER;

		asciiScope[asciiScopeLen - 1] = 0;

		strncpy(slotFilterBuffer, &asciiScope[4], sizeof(slotFilterBuffer));
		slotFilter = slotFilterBuffer;
		slotFilterLen = asciiScopeLen - 5;
	}

	if (!state)
	{
		state = (P11EnumKeysState*)calloc(1, sizeof(*state));
		if (!state)
			return NTE_NO_MEMORY;

		WINPR_ASSERT(provider->p11->C_GetSlotList);
		rv = provider->p11->C_GetSlotList(CK_TRUE, NULL, &state->nslots);
		if (rv != CKR_OK)
		{
			/* TODO: perhaps convert rv to NTE_*** errors */
			return NTE_FAIL;
		}

		if (state->nslots > MAX_SLOTS)
			state->nslots = MAX_SLOTS;

		rv = provider->p11->C_GetSlotList(CK_TRUE, state->slots, &state->nslots);
		if (rv != CKR_OK)
		{
			free(state);
			/* TODO: perhaps convert rv to NTE_*** errors */
			return NTE_FAIL;
		}

		ret = collect_keys(provider, state);
		if (ret != ERROR_SUCCESS)
		{
			free(state);
			return ret;
		}

		*ppEnumState = state;
	}

	for (; state->keyIndex < state->nKeys; state->keyIndex++)
	{
		NCryptKeyName* keyName = NULL;
		NCryptKeyEnum* key = &state->keys[state->keyIndex];
		CK_OBJECT_CLASS oclass = CKO_CERTIFICATE;
		CK_CERTIFICATE_TYPE ctype = CKC_X_509;
		CK_ATTRIBUTE certificateFilter[] = { { CKA_CLASS, &oclass, sizeof(oclass) },
			                                 { CKA_CERTIFICATE_TYPE, &ctype, sizeof(ctype) },
			                                 { CKA_ID, key->id, key->idLen } };
		CK_ULONG ncertObjects;
		CK_OBJECT_HANDLE certObject;

		/* check the reader filter if any */
		if (slotFilter && memcmp(key->slotInfo.slotDescription, slotFilter, slotFilterLen) != 0)
			continue;

		if (!currentSession || (currentSlot != key->slotId))
		{
			/* if the current session doesn't match the current key's slot, open a new one
			 */
			if (currentSession)
			{
				WINPR_ASSERT(provider->p11->C_CloseSession);
				rv = provider->p11->C_CloseSession(currentSession);
				currentSession = (CK_SESSION_HANDLE)NULL;
			}

			WINPR_ASSERT(provider->p11->C_OpenSession);
			rv = provider->p11->C_OpenSession(key->slotId, CKF_SERIAL_SESSION, NULL, NULL,
			                                  &currentSession);
			if (rv != CKR_OK)
			{
				WLog_ERR(TAG, "unable to openSession for slot %d", key->slotId);
				continue;
			}
			currentSlot = key->slotId;
		}

		/* look if we can find a certificate that matches the key's id */
		WINPR_ASSERT(provider->p11->C_FindObjectsInit);
		rv = provider->p11->C_FindObjectsInit(currentSession, certificateFilter,
		                                      ARRAYSIZE(certificateFilter));
		if (rv != CKR_OK)
		{
			WLog_ERR(TAG, "unable to initiate search for slot %d", key->slotId);
			continue;
		}

		WINPR_ASSERT(provider->p11->C_FindObjects);
		rv = provider->p11->C_FindObjects(currentSession, &certObject, 1, &ncertObjects);
		if (rv != CKR_OK)
		{
			WLog_ERR(TAG, "unable to findObjects for slot %d", currentSlot);
			goto cleanup_FindObjects;
		}

		if (ncertObjects)
		{
			/* sizeof keyName struct + "\<slotId>\<certId>" + keyName->pszAlgid */
			DWORD algoSz;
			size_t KEYNAME_SZ =
			    (1 + (sizeof(key->slotId) * 2) /*slotId*/ + 1 + (key->idLen * 2) + 1) * 2;

			convertKeyType(key->keyType, NULL, 0, &algoSz);
			KEYNAME_SZ += (algoSz + 1) * 2;

			keyName = calloc(1, sizeof(*keyName) + KEYNAME_SZ);
			if (!keyName)
			{
				WLog_ERR(TAG, "unable to allocate keyName");
				goto cleanup_FindObjects;
			}
			keyName->dwLegacyKeySpec = AT_KEYEXCHANGE | AT_SIGNATURE;
			keyName->dwFlags = NCRYPT_MACHINE_KEY_FLAG;
			keyName->pszName = (LPWSTR)(keyName + 1);
			wprintKeyName(keyName->pszName, key->slotId, key->id, key->idLen);

			keyName->pszAlgid = keyName->pszName + _wcslen(keyName->pszName) + 1;
			convertKeyType(key->keyType, keyName->pszAlgid, algoSz + 1, NULL);
		}

	cleanup_FindObjects:
		WINPR_ASSERT(provider->p11->C_FindObjectsFinal);
		rv = provider->p11->C_FindObjectsFinal(currentSession);

		if (keyName)
		{
			*ppKeyName = keyName;
			state->keyIndex++;
			return ERROR_SUCCESS;
		}
	}

	return NTE_NO_MORE_ITEMS;
}

static SECURITY_STATUS get_piv_container_name(NCryptP11KeyHandle* key, const BYTE* piv_tag,
                                              BYTE* output, size_t output_len)
{
	CK_SLOT_INFO slot_info = { 0 };
	CK_FUNCTION_LIST_PTR p11 = NULL;
	WCHAR* reader = NULL;
	SCARDCONTEXT context = 0;
	SCARDHANDLE card = 0;
	DWORD proto = 0;
	const SCARD_IO_REQUEST* pci = NULL;
	BYTE buf[258] = { 0 };
	char container_name[PIV_CONTAINER_NAME_LEN + 1] = { 0 };
	DWORD buf_len = 0;
	SECURITY_STATUS ret = NTE_BAD_KEY;
	WinPrAsn1Decoder dec = { 0 };
	WinPrAsn1Decoder dec2 = { 0 };
	size_t len = 0;
	BYTE tag = 0;
	BYTE* p = NULL;
	wStream s = { 0 };

	WINPR_ASSERT(key);
	WINPR_ASSERT(piv_tag);

	WINPR_ASSERT(key->provider);
	p11 = key->provider->p11;
	WINPR_ASSERT(p11);

	/* Get the reader the card is in */
	WINPR_ASSERT(p11->C_GetSlotInfo);
	if (p11->C_GetSlotInfo(key->slotId, &slot_info) != CKR_OK)
		return NTE_BAD_KEY;

	fix_padded_string((char*)slot_info.slotDescription, sizeof(slot_info.slotDescription));
	reader = ConvertUtf8NToWCharAlloc((char*)slot_info.slotDescription,
	                                  ARRAYSIZE(slot_info.slotDescription), NULL);
	ret = NTE_NO_MEMORY;
	if (!reader)
		goto out;

	ret = NTE_BAD_KEY;
	if (SCardEstablishContext(SCARD_SCOPE_USER, NULL, NULL, &context) != SCARD_S_SUCCESS)
		goto out;

	if (SCardConnectW(context, reader, SCARD_SHARE_SHARED, SCARD_PROTOCOL_Tx, &card, &proto) !=
	    SCARD_S_SUCCESS)
		goto out;
	pci = (proto == SCARD_PROTOCOL_T0) ? SCARD_PCI_T0 : SCARD_PCI_T1;

	buf_len = sizeof(buf);
	if (SCardTransmit(card, pci, APDU_PIV_SELECT_AID, sizeof(APDU_PIV_SELECT_AID), NULL, buf,
	                  &buf_len) != SCARD_S_SUCCESS)
		goto out;
	if ((buf[buf_len - 2] != 0x90 || buf[buf_len - 1] != 0) && buf[buf_len - 2] != 0x61)
		goto out;

	buf_len = sizeof(buf);
	if (SCardTransmit(card, pci, APDU_PIV_GET_CHUID, sizeof(APDU_PIV_GET_CHUID), NULL, buf,
	                  &buf_len) != SCARD_S_SUCCESS)
		goto out;
	if ((buf[buf_len - 2] != 0x90 || buf[buf_len - 1] != 0) && buf[buf_len - 2] != 0x61)
		goto out;

	/* Find the GUID field in the CHUID data object */
	WinPrAsn1Decoder_InitMem(&dec, WINPR_ASN1_BER, buf, buf_len);
	if (!WinPrAsn1DecReadTagAndLen(&dec, &tag, &len) || tag != 0x53)
		goto out;
	while (WinPrAsn1DecReadTagLenValue(&dec, &tag, &len, &dec2) && tag != 0x34)
		;
	if (tag != 0x34 || len != 16)
		goto out;

	s = WinPrAsn1DecGetStream(&dec2);
	p = Stream_Buffer(&s);

	/* Construct the value Windows would use for a PIV key's container name */
	snprintf(container_name, PIV_CONTAINER_NAME_LEN + 1,
	         "%.2x%.2x%.2x%.2x-%.2x%.2x-%.2x%.2x-%.2x%.2x-%.2x%.2x%.2x%.2x%.2x%.2x", p[3], p[2],
	         p[1], p[0], p[5], p[4], p[7], p[6], p[8], p[9], p[10], p[11], p[12], piv_tag[0],
	         piv_tag[1], piv_tag[2]);

	/* And convert it to UTF-16 */
	union
	{
		WCHAR* wc;
		BYTE* b;
	} cnv;
	cnv.b = output;
	if (ConvertUtf8NToWChar(container_name, ARRAYSIZE(container_name), cnv.wc,
	                        output_len / sizeof(WCHAR)) > 0)
		ret = ERROR_SUCCESS;

out:
	free(reader);
	if (card)
		SCardDisconnect(card, SCARD_LEAVE_CARD);
	if (context)
		SCardReleaseContext(context);
	return ret;
}

static SECURITY_STATUS check_for_piv_container_name(NCryptP11KeyHandle* key, BYTE* pbOutput,
                                                    DWORD cbOutput, DWORD* pcbResult, char* label,
                                                    size_t label_len)
{
	for (size_t i = 0; i < ARRAYSIZE(piv_cert_tags); i++)
	{
		const piv_cert_tags_t* cur = &piv_cert_tags[i];
		if (strncmp(label, cur->label, label_len) == 0)
		{
			*pcbResult = (PIV_CONTAINER_NAME_LEN + 1) * sizeof(WCHAR);
			if (!pbOutput)
				return ERROR_SUCCESS;
			else if (cbOutput < (PIV_CONTAINER_NAME_LEN + 1) * sizeof(WCHAR))
				return NTE_NO_MEMORY;
			else
				return get_piv_container_name(key, cur->tag, pbOutput, cbOutput);
		}
	}
	return NTE_NOT_FOUND;
}

static SECURITY_STATUS NCryptP11KeyGetProperties(NCryptP11KeyHandle* keyHandle,
                                                 NCryptKeyGetPropertyEnum property, PBYTE pbOutput,
                                                 DWORD cbOutput, DWORD* pcbResult, DWORD dwFlags)
{
	SECURITY_STATUS ret = NTE_FAIL;
	CK_RV rv;
	CK_SESSION_HANDLE session;
	CK_OBJECT_HANDLE objectHandle;
	CK_ULONG objectCount;
	NCryptP11ProviderHandle* provider;
	CK_OBJECT_CLASS oclass = CKO_CERTIFICATE;
	CK_CERTIFICATE_TYPE ctype = CKC_X_509;
	CK_ATTRIBUTE certificateFilter[] = { { CKA_CLASS, &oclass, sizeof(oclass) },
		                                 { CKA_CERTIFICATE_TYPE, &ctype, sizeof(ctype) },
		                                 { CKA_ID, keyHandle->keyCertId,
		                                   keyHandle->keyCertIdLen } };
	CK_ATTRIBUTE* objectFilter = certificateFilter;
	CK_ULONG objectFilterLen = ARRAYSIZE(certificateFilter);

	WINPR_ASSERT(keyHandle);
	provider = keyHandle->provider;
	WINPR_ASSERT(provider);

	switch (property)

	{
		case NCRYPT_PROPERTY_CERTIFICATE:
		case NCRYPT_PROPERTY_NAME:
			break;
		case NCRYPT_PROPERTY_READER:
		{
			CK_SLOT_INFO slotInfo;

			WINPR_ASSERT(provider->p11->C_GetSlotInfo);
			rv = provider->p11->C_GetSlotInfo(keyHandle->slotId, &slotInfo);
			if (rv != CKR_OK)
				return NTE_BAD_KEY;

#define SLOT_DESC_SZ sizeof(slotInfo.slotDescription)
			fix_padded_string((char*)slotInfo.slotDescription, SLOT_DESC_SZ);
			*pcbResult = 2 * (strnlen((char*)slotInfo.slotDescription, SLOT_DESC_SZ) + 1);
			if (pbOutput)
			{
				union
				{
					WCHAR* wc;
					BYTE* b;
				} cnv;
				cnv.b = pbOutput;
				if (cbOutput < *pcbResult)
					return NTE_NO_MEMORY;

				if (ConvertUtf8ToWChar((char*)slotInfo.slotDescription, cnv.wc,
				                       cbOutput / sizeof(WCHAR)) < 0)
					return NTE_NO_MEMORY;
			}
			return ERROR_SUCCESS;
		}
		case NCRYPT_PROPERTY_SLOTID:
		{
			*pcbResult = 4;
			if (pbOutput)
			{
				UINT32* ptr = (UINT32*)pbOutput;

				if (cbOutput < 4)
					return NTE_NO_MEMORY;

				*ptr = keyHandle->slotId;
			}
			return ERROR_SUCCESS;
		}
		case NCRYPT_PROPERTY_UNKNOWN:
		default:
			return NTE_NOT_SUPPORTED;
	}

	WINPR_ASSERT(provider->p11->C_OpenSession);
	rv = provider->p11->C_OpenSession(keyHandle->slotId, CKF_SERIAL_SESSION, NULL, NULL, &session);
	if (rv != CKR_OK)
	{
		WLog_ERR(TAG, "error opening session on slot %d", keyHandle->slotId);
		return NTE_FAIL;
	}

	WINPR_ASSERT(provider->p11->C_FindObjectsInit);
	rv = provider->p11->C_FindObjectsInit(session, objectFilter, objectFilterLen);
	if (rv != CKR_OK)
	{
		WLog_ERR(TAG, "unable to initiate search for slot %d", keyHandle->slotId);
		goto out;
	}

	WINPR_ASSERT(provider->p11->C_FindObjects);
	rv = provider->p11->C_FindObjects(session, &objectHandle, 1, &objectCount);
	if (rv != CKR_OK)
	{
		WLog_ERR(TAG, "unable to findObjects for slot %d", keyHandle->slotId);
		goto out_final;
	}
	if (!objectCount)
	{
		ret = NTE_NOT_FOUND;
		goto out_final;
	}

	switch (property)
	{
		case NCRYPT_PROPERTY_CERTIFICATE:
		{
			CK_ATTRIBUTE certValue = { CKA_VALUE, pbOutput, cbOutput };

			WINPR_ASSERT(provider->p11->C_GetAttributeValue);
			rv = provider->p11->C_GetAttributeValue(session, objectHandle, &certValue, 1);
			if (rv != CKR_OK)
			{
				// TODO: do a kind of translation from CKR_* to NTE_*
			}

			*pcbResult = certValue.ulValueLen;
			ret = ERROR_SUCCESS;
			break;
		}
		case NCRYPT_PROPERTY_NAME:
		{
			CK_ATTRIBUTE attr = { CKA_LABEL, NULL, 0 };
			char* label = NULL;

			WINPR_ASSERT(provider->p11->C_GetAttributeValue);
			rv = provider->p11->C_GetAttributeValue(session, objectHandle, &attr, 1);
			if (rv == CKR_OK)
			{
				label = calloc(1, attr.ulValueLen);
				if (!label)
				{
					ret = NTE_NO_MEMORY;
					break;
				}

				attr.pValue = label;
				rv = provider->p11->C_GetAttributeValue(session, objectHandle, &attr, 1);
			}

			if (rv == CKR_OK)
			{
				/* Check if we have a PIV card */
				ret = check_for_piv_container_name(keyHandle, pbOutput, cbOutput, pcbResult, label,
				                                   attr.ulValueLen);

				/* Otherwise, at least for GIDS cards the label will be the correct value */
				if (ret == NTE_NOT_FOUND)
				{
					union
					{
						WCHAR* wc;
						BYTE* b;
					} cnv;
					const size_t olen = pbOutput ? cbOutput / sizeof(WCHAR) : 0;
					cnv.b = pbOutput;
					SSIZE_T size = ConvertUtf8NToWChar(label, attr.ulValueLen, cnv.wc, olen);
					if (size < 0)
						ret = ERROR_CONVERT_TO_LARGE;
					else
						ret = ERROR_SUCCESS;
				}
			}

			free(label);
			break;
		}
		default:
			ret = NTE_NOT_SUPPORTED;
			break;
	}

out_final:
	WINPR_ASSERT(provider->p11->C_FindObjectsFinal);
	rv = provider->p11->C_FindObjectsFinal(session);
	if (rv != CKR_OK)
	{
		WLog_ERR(TAG, "error in C_FindObjectsFinal() for slot %d", keyHandle->slotId);
	}
out:
	WINPR_ASSERT(provider->p11->C_CloseSession);
	rv = provider->p11->C_CloseSession(session);
	if (rv != CKR_OK)
	{
		WLog_ERR(TAG, "error in C_CloseSession() for slot %d", keyHandle->slotId);
	}
	return ret;
}

static SECURITY_STATUS NCryptP11GetProperty(NCRYPT_HANDLE hObject, NCryptKeyGetPropertyEnum prop,
                                            PBYTE pbOutput, DWORD cbOutput, DWORD* pcbResult,
                                            DWORD dwFlags)
{
	NCryptBaseHandle* base = (NCryptBaseHandle*)hObject;

	WINPR_ASSERT(base);
	switch (base->type)
	{
		case WINPR_NCRYPT_PROVIDER:
			return ERROR_CALL_NOT_IMPLEMENTED;
		case WINPR_NCRYPT_KEY:
			return NCryptP11KeyGetProperties((NCryptP11KeyHandle*)hObject, prop, pbOutput, cbOutput,
			                                 pcbResult, dwFlags);
		default:
			return ERROR_INVALID_HANDLE;
	}
	return ERROR_SUCCESS;
}

static SECURITY_STATUS NCryptP11OpenKey(NCRYPT_PROV_HANDLE hProvider, NCRYPT_KEY_HANDLE* phKey,
                                        LPCWSTR pszKeyName, DWORD dwLegacyKeySpec, DWORD dwFlags)
{
	SECURITY_STATUS ret;
	CK_SLOT_ID slotId;
	CK_BYTE keyCertId[64] = { 0 };
	CK_ULONG keyCertIdLen;
	NCryptP11KeyHandle* keyHandle;

	ret = parseKeyName(pszKeyName, &slotId, keyCertId, &keyCertIdLen);
	if (ret != ERROR_SUCCESS)
		return ret;

	keyHandle = (NCryptP11KeyHandle*)ncrypt_new_handle(
	    WINPR_NCRYPT_KEY, sizeof(*keyHandle), NCryptP11GetProperty, winpr_NCryptDefault_dtor);
	if (!keyHandle)
		return NTE_NO_MEMORY;

	keyHandle->provider = (NCryptP11ProviderHandle*)hProvider;
	keyHandle->slotId = slotId;
	memcpy(keyHandle->keyCertId, keyCertId, sizeof(keyCertId));
	keyHandle->keyCertIdLen = keyCertIdLen;
	*phKey = (NCRYPT_KEY_HANDLE)keyHandle;
	return ERROR_SUCCESS;
}

static SECURITY_STATUS initialize_pkcs11(HANDLE handle,
                                         CK_RV (*c_get_function_list)(CK_FUNCTION_LIST_PTR_PTR),
                                         NCRYPT_PROV_HANDLE* phProvider)
{
	SECURITY_STATUS status = ERROR_SUCCESS;
	NCryptP11ProviderHandle* ret;
	CK_RV rv;

	WINPR_ASSERT(c_get_function_list);
	WINPR_ASSERT(phProvider);

	ret = (NCryptP11ProviderHandle*)ncrypt_new_handle(
	    WINPR_NCRYPT_PROVIDER, sizeof(*ret), NCryptP11GetProperty, NCryptP11StorageProvider_dtor);
	if (!ret)
	{
		if (handle)
			FreeLibrary(handle);
		return NTE_NO_MEMORY;
	}

	ret->library = handle;
	ret->baseProvider.enumKeysFn = NCryptP11EnumKeys;
	ret->baseProvider.openKeyFn = NCryptP11OpenKey;

	rv = c_get_function_list(&ret->p11);
	if (rv != CKR_OK)
	{
		status = NTE_PROVIDER_DLL_FAIL;
		goto fail;
	}

	WINPR_ASSERT(ret->p11->C_Initialize);
	rv = ret->p11->C_Initialize(NULL);
	if (rv != CKR_OK)
	{
		status = NTE_PROVIDER_DLL_FAIL;
		goto fail;
	}

	*phProvider = (NCRYPT_PROV_HANDLE)ret;

fail:
	if (status != ERROR_SUCCESS)
		ret->baseProvider.baseHandle.releaseFn((NCRYPT_HANDLE)ret);
	return status;
}

SECURITY_STATUS NCryptOpenP11StorageProviderEx(NCRYPT_PROV_HANDLE* phProvider,
                                               LPCWSTR pszProviderName, DWORD dwFlags,
                                               LPCSTR* modulePaths)
{
	SECURITY_STATUS status = ERROR_INVALID_PARAMETER;
#if defined(__LP64__) || (defined(__x86_64__) && !defined(__ILP32__)) || defined(_M_X64) || \
    defined(__ia64) || defined(_M_IA64) || defined(__aarch64__) || defined(__powerpc64__)
#define LIBS64
#endif

	LPCSTR openscPaths[] = { "opensc-pkcs11.so", /* In case winpr is installed in system paths */
#ifdef __APPLE__
		                     "/usr/local/lib/pkcs11/opensc-pkcs11.so",
#else
	/* linux and UNIXes */
#ifdef LIBS64
		                     "/usr/lib/x86_64-linux-gnu/pkcs11/opensc-pkcs11.so", /* Ubuntu/debian
		                                                                           */
		                     "/lib64/pkcs11/opensc-pkcs11.so",                    /* Fedora */
#else
		                     "/usr/lib/i386-linux-gnu/opensc-pkcs11.so", /* debian */
		                     "/lib32/pkcs11/opensc-pkcs11.so",           /* Fedora */
#endif
#endif
		                     NULL };

	if (!phProvider)
		return ERROR_INVALID_PARAMETER;

#if defined(WITH_OPENSC_PKCS11_LINKED)
	if (!modulePaths)
		return initialize_pkcs11(NULL, C_GetFunctionList, phProvider);
#endif

	if (!modulePaths)
		modulePaths = openscPaths;

	while (*modulePaths)
	{
		HANDLE library = LoadLibrary(*modulePaths);
		typedef CK_RV (*c_get_function_list_t)(CK_FUNCTION_LIST_PTR_PTR);
		c_get_function_list_t c_get_function_list;

		WLog_DBG(TAG, "Trying pkcs11-helper module '%s'", *modulePaths);
		if (!library)
		{
			status = NTE_PROV_DLL_NOT_FOUND;
			goto out_load_library;
		}

		c_get_function_list = (c_get_function_list_t)GetProcAddress(library, "C_GetFunctionList");
		if (!c_get_function_list)
		{
			status = NTE_PROV_TYPE_ENTRY_BAD;
			goto out_load_library;
		}

		status = initialize_pkcs11(library, c_get_function_list, phProvider);
		if (status != ERROR_SUCCESS)
		{
			status = NTE_PROVIDER_DLL_FAIL;
			goto out_load_library;
		}

		WLog_DBG(TAG, "module '%s' loaded", *modulePaths);
		return ERROR_SUCCESS;

	out_load_library:
		modulePaths++;
	}

	return status;
}
