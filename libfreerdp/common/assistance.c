/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Remote Assistance
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <errno.h>

#include <winpr/wtypes.h>
#include <winpr/collections.h>
#include <winpr/string.h>
#include <winpr/crt.h>
#include <winpr/crypto.h>
#include <winpr/print.h>
#include <winpr/windows.h>
#include <winpr/ssl.h>
#include <winpr/file.h>

#include <freerdp/log.h>
#include <freerdp/client/file.h>
#include <freerdp/client/cmdline.h>

#include <freerdp/assistance.h>

#include "../core/settings.h"

#define TAG FREERDP_TAG("common")

struct rdp_assistance_file
{
	UINT32 Type;

	char* Username;
	char* LHTicket;
	char* RCTicket;
	char* PassStub;
	UINT32 DtStart;
	UINT32 DtLength;
	BOOL LowSpeed;
	BOOL RCTicketEncrypted;

	char* ConnectionString1;
	char* ConnectionString2;

	BYTE* EncryptedPassStub;
	size_t EncryptedPassStubLength;

	BYTE* EncryptedLHTicket;
	size_t EncryptedLHTicketLength;

	wArrayList* MachineAddresses;
	wArrayList* MachinePorts;
	wArrayList* MachineUris;

	char* RASessionId;
	char* RASpecificParams;
	char* RASpecificParams2;

	char* filename;
	char* password;
};

static const char* strrstr(const char* haystack, size_t len, const char* needle)
{
	if (*needle == '\0')
		return haystack;

	char* result = NULL;
	for (;;)
	{
		char* p = strstr(haystack, needle);
		if (p == NULL)
			break;
		if (p > haystack + len)
			return NULL;

		result = p;
		haystack = p + 1;
	}

	return result;
}

static BOOL update_option(char** opt, const char* val, size_t len)
{
	WINPR_ASSERT(opt);
	free(*opt);
	*opt = NULL;

	if (!val && (len != 0))
		return FALSE;
	else if (!val && (len == 0))
		return TRUE;
	*opt = strndup(val, len);
	return *opt != NULL;
}

static BOOL update_name(rdpAssistanceFile* file, const char* name)
{
	WINPR_ASSERT(file);

	if (!name)
	{
		WLog_ERR(TAG, "ASSISTANCE file %s invalid name", name);
		return FALSE;
	}

	free(file->filename);
	file->filename = _strdup(name);
	return file->filename != NULL;
}

static BOOL update_password(rdpAssistanceFile* file, const char* password)
{
	WINPR_ASSERT(file);
	free(file->password);
	file->password = NULL;
	if (!password)
		return TRUE;
	file->password = _strdup(password);
	return file->password != NULL;
}

static BOOL update_connectionstring2_nocopy(rdpAssistanceFile* file, char* str)
{
	WINPR_ASSERT(file);
	free(file->ConnectionString2);
	file->ConnectionString2 = NULL;
	if (!str)
		return TRUE;
	file->ConnectionString2 = str;
	return file->ConnectionString2 != NULL;
}

static BOOL update_connectionstring2(rdpAssistanceFile* file, const char* str, size_t len)
{
	char* strc = NULL;
	if (!str && (len != 0))
		return FALSE;

	if (str && (len > 0))
	{
		strc = strndup(str, len);
		if (!strc)
			return FALSE;
	}
	return update_connectionstring2_nocopy(file, strc);
}

static BOOL update_connectionstring2_wchar(rdpAssistanceFile* file, const WCHAR* str, size_t len)
{
	char* strc = NULL;

	if (!str && (len != 0))
		return FALSE;

	if (str && (len > 0))
	{
		strc = ConvertWCharNToUtf8Alloc(str, len, NULL);
		if (!strc)
			return FALSE;
	}
	return update_connectionstring2_nocopy(file, strc);
}

/**
 * Password encryption in establishing a remote assistance session of type 1:
 * http://blogs.msdn.com/b/openspecification/archive/2011/10/31/password-encryption-in-establishing-a-remote-assistance-session-of-type-1.aspx
 *
 * Creation of PassStub for the Remote Assistance Ticket:
 * http://social.msdn.microsoft.com/Forums/en-US/6316c3f4-ea09-4343-a4a1-9cca46d70d28/creation-of-passstub-for-the-remote-assistance-ticket?forum=os_windowsprotocols
 */

/**
 * CryptDeriveKey Function:
 * http://msdn.microsoft.com/en-us/library/windows/desktop/aa379916/
 *
 * Let n be the required derived key length, in bytes.
 * The derived key is the first n bytes of the hash value after the hash computation
 * has been completed by CryptDeriveKey. If the hash is not a member of the SHA-2
 * family and the required key is for either 3DES or AES, the key is derived as follows:
 *
 * Form a 64-byte buffer by repeating the constant 0x36 64 times.
 * Let k be the length of the hash value that is represented by the input parameter hBaseData.
 * Set the first k bytes of the buffer to the result of an XOR operation of the first k bytes
 * of the buffer with the hash value that is represented by the input parameter hBaseData.
 *
 * Form a 64-byte buffer by repeating the constant 0x5C 64 times.
 * Set the first k bytes of the buffer to the result of an XOR operation of the first k bytes
 * of the buffer with the hash value that is represented by the input parameter hBaseData.
 *
 * Hash the result of step 1 by using the same hash algorithm as that used to compute the hash
 * value that is represented by the hBaseData parameter.
 *
 * Hash the result of step 2 by using the same hash algorithm as that used to compute the hash
 * value that is represented by the hBaseData parameter.
 *
 * Concatenate the result of step 3 with the result of step 4.
 * Use the first n bytes of the result of step 5 as the derived key.
 */

static BOOL freerdp_assistance_crypt_derive_key_sha1(const BYTE* hash, size_t hashLength, BYTE* key,
                                                     size_t keyLength)
{
	BOOL rc = FALSE;
	BYTE pad1[64] = { 0 };
	BYTE pad2[64] = { 0 };

	if (hashLength == 0)
		return FALSE;

	memset(pad1, 0x36, sizeof(pad1));
	memset(pad2, 0x5C, sizeof(pad2));

	for (size_t i = 0; i < hashLength; i++)
	{
		pad1[i] ^= hash[i];
		pad2[i] ^= hash[i];
	}

	BYTE* buffer = (BYTE*)calloc(hashLength, 2);

	if (!buffer)
		goto fail;

	if (!winpr_Digest(WINPR_MD_SHA1, pad1, 64, buffer, hashLength))
		goto fail;

	if (!winpr_Digest(WINPR_MD_SHA1, pad2, 64, &buffer[hashLength], hashLength))
		goto fail;

	CopyMemory(key, buffer, keyLength);
	rc = TRUE;
fail:
	free(buffer);
	return rc;
}

static BOOL append_address_to_list(wArrayList* MachineAddresses, const char* str, size_t len)
{
	char* copy = NULL;
	if (len > 0)
		copy = strndup(str, len);
	if (!copy)
		return FALSE;

	const BOOL rc = ArrayList_Append(MachineAddresses, copy);
	if (!rc)
		free(copy);
	// NOLINTNEXTLINE(clang-analyzer-unix.Malloc): ArrayList_Append takes ownership of copy
	return rc;
}

static BOOL append_address(rdpAssistanceFile* file, const char* host, const char* port)
{
	WINPR_ASSERT(file);

	errno = 0;
	unsigned long p = strtoul(port, NULL, 0);

	if ((errno != 0) || (p == 0) || (p > UINT16_MAX))
	{
		WLog_ERR(TAG, "Failed to parse ASSISTANCE file: ConnectionString2 invalid port value %s",
		         port);
		return FALSE;
	}

	if (!append_address_to_list(file->MachineAddresses, host, host ? strlen(host) : 0))
		return FALSE;
	return ArrayList_Append(file->MachinePorts, (void*)(uintptr_t)p);
}

static BOOL freerdp_assistance_parse_address_list(rdpAssistanceFile* file, char* list)
{
	WINPR_ASSERT(file);

	WLog_DBG(TAG, "freerdp_assistance_parse_address_list list=%s", list);

	BOOL rc = FALSE;

	if (!list)
		return FALSE;

	char* strp = list;
	char* s = ";";

	// get the first token
	char* saveptr = NULL;
	char* token = strtok_s(strp, s, &saveptr);

	// walk through other tokens
	while (token != NULL)
	{
		char* port = strchr(token, ':');
		if (!port)
			goto out;
		*port = '\0';
		port++;

		if (!append_address(file, token, port))
			goto out;

		token = strtok_s(NULL, s, &saveptr);
	}
	rc = TRUE;
out:
	return rc;
}

static BOOL freerdp_assistance_parse_connection_string1(rdpAssistanceFile* file)
{
	char* tokens[8] = { 0 };
	BOOL rc = FALSE;

	WINPR_ASSERT(file);

	if (!file->RCTicket)
		return FALSE;

	/**
	 * <ProtocolVersion>,<protocolType>,<machineAddressList>,<assistantAccountPwd>,
	 * <RASessionID>,<RASessionName>,<RASessionPwd>,<protocolSpecificParms>
	 */
	char* str = _strdup(file->RCTicket);

	if (!str)
		goto error;

	const size_t length = strlen(str);

	int count = 1;
	for (size_t i = 0; i < length; i++)
	{
		if (str[i] == ',')
			count++;
	}

	if (count != 8)
		goto error;

	count = 0;
	tokens[count++] = str;

	for (size_t i = 0; i < length; i++)
	{
		if (str[i] == ',')
		{
			str[i] = '\0';
			tokens[count++] = &str[i + 1];
		}
	}

	if (strcmp(tokens[0], "65538") != 0)
		goto error;

	if (strcmp(tokens[1], "1") != 0)
		goto error;

	if (strcmp(tokens[3], "*") != 0)
		goto error;

	if (strcmp(tokens[5], "*") != 0)
		goto error;

	if (strcmp(tokens[6], "*") != 0)
		goto error;

	file->RASessionId = _strdup(tokens[4]);

	if (!file->RASessionId)
		goto error;

	file->RASpecificParams = _strdup(tokens[7]);

	if (!file->RASpecificParams)
		goto error;

	if (!freerdp_assistance_parse_address_list(file, tokens[2]))
		goto error;

	rc = TRUE;
error:
	free(str);
	return rc;
}

/**
 * Decrypted Connection String 2:
 *
 * <E>
 * <A KH="BNRjdu97DyczQSRuMRrDWoue+HA="
 * ID="+ULZ6ifjoCa6cGPMLQiGHRPwkg6VyJqGwxMnO6GcelwUh9a6/FBq3It5ADSndmLL"/> <C> <T ID="1" SID="0"> <L
 * P="49228" N="fe80::1032:53d9:5a01:909b%3"/> <L P="49229" N="fe80::3d8f:9b2d:6b4e:6aa%6"/> <L
 * P="49230" N="192.168.1.200"/> <L P="49231" N="169.254.6.170"/>
 * </T>
 * </C>
 * </E>
 */

static BOOL freerdp_assistance_parse_attr(const char** opt, size_t* plength, const char* key,
                                          const char* tag)
{
	WINPR_ASSERT(opt);
	WINPR_ASSERT(plength);
	WINPR_ASSERT(key);

	*opt = NULL;
	*plength = 0;
	if (!tag)
		return FALSE;

	char bkey[128] = { 0 };
	const int rc = _snprintf(bkey, sizeof(bkey), "%s=\"", key);
	WINPR_ASSERT(rc > 0);
	WINPR_ASSERT((size_t)rc < sizeof(bkey));
	if ((rc <= 0) || ((size_t)rc >= sizeof(bkey)))
		return FALSE;

	char* p = strstr(tag, bkey);
	if (!p)
		return TRUE;

	p += strlen(bkey);
	char* q = strchr(p, '"');

	if (!q)
	{
		WLog_ERR(TAG, "Failed to parse ASSISTANCE file: ConnectionString2 invalid field '%s=%s'",
		         key, p);
		return FALSE;
	}

	if (p > q)
	{
		WLog_ERR(TAG,
		         "Failed to parse ASSISTANCE file: ConnectionString2 invalid field "
		         "order for '%s'",
		         key);
		return FALSE;
	}
	const size_t length = q - p;
	*opt = p;
	*plength = length;

	return TRUE;
}

static BOOL freerdp_assistance_parse_attr_str(char** opt, const char* key, const char* tag)
{
	const char* copt = NULL;
	size_t size = 0;
	if (!freerdp_assistance_parse_attr(&copt, &size, key, tag))
		return FALSE;
	return update_option(opt, copt, size);
}

static BOOL freerdp_assistance_parse_attr_bool(BOOL* opt, const char* key, const char* tag)
{
	const char* copt = NULL;
	size_t size = 0;

	WINPR_ASSERT(opt);
	*opt = FALSE;

	if (!freerdp_assistance_parse_attr(&copt, &size, key, tag))
		return FALSE;
	if (size != 1)
		return TRUE;

	*opt = (copt[0] == '1');
	return TRUE;
}

static BOOL freerdp_assistance_parse_attr_uint32(UINT32* opt, const char* key, const char* tag)
{
	const char* copt = NULL;
	size_t size = 0;

	WINPR_ASSERT(opt);
	*opt = 0;

	if (!freerdp_assistance_parse_attr(&copt, &size, key, tag))
		return FALSE;

	char buffer[64] = { 0 };
	if (size >= sizeof(buffer))
	{
		WLog_WARN(TAG, "Invalid UINT32 string '%s' [%" PRIuz "]", copt, size);
		return FALSE;
	}

	strncpy(buffer, copt, size);
	errno = 0;
	unsigned long val = strtoul(buffer, NULL, 0);

	if ((errno != 0) || (val > UINT32_MAX))
	{
		WLog_ERR(TAG, "Failed to parse ASSISTANCE file: Invalid value %s", buffer);
		return FALSE;
	}

	*opt = val;

	return TRUE;
}

static char* freerdp_assistance_contains_element(char* input, size_t ilen, const char* key,
                                                 size_t* plen, char** pdata, size_t* pdlen)
{
	WINPR_ASSERT(input);
	WINPR_ASSERT(key);
	WINPR_ASSERT(plen);

	char bkey[128] = { 0 };
	const int rc = _snprintf(bkey, sizeof(bkey), "<%s", key);
	WINPR_ASSERT(rc > 0);
	WINPR_ASSERT((size_t)rc < sizeof(bkey));
	if ((rc < 0) || ((size_t)rc >= sizeof(bkey)))
		return NULL;

	char* tag = strstr(input, bkey);
	if (!tag || (tag > input + ilen))
		return NULL;

	char* data = tag + strnlen(bkey, sizeof(bkey));

	/* Ensure there is a valid delimiter following our token */
	switch (data[0])
	{
		case '>':
		case '/':
		case ' ':
		case '\t':
			break;
		default:
			WLog_ERR(TAG,
			         "Failed to parse ASSISTANCE file: ConnectionString2 missing delimiter after "
			         "field %s",
			         bkey);
			return NULL;
	}

	char* start = strstr(tag, ">");

	if (!start || (start > input + ilen))
	{
		WLog_ERR(TAG, "Failed to parse ASSISTANCE file: ConnectionString2 missing field %s", bkey);
		return NULL;
	}

	const char* end = start;
	const char* dend = start - 1;
	if (*dend != '/')
	{
		char ekey[128] = { 0 };
		const int erc = _snprintf(ekey, sizeof(ekey), "</%s>", key);
		WINPR_ASSERT(erc > 0);
		WINPR_ASSERT((size_t)erc < sizeof(ekey));
		if ((erc <= 0) || ((size_t)erc >= sizeof(ekey)))
			return NULL;
		const size_t offset = start - tag;
		dend = end = strrstr(start, ilen - offset, ekey);
		if (end)
			end += strnlen(ekey, sizeof(ekey));
	}

	if (!end)
	{
		WLog_ERR(TAG,
		         "Failed to parse ASSISTANCE file: ConnectionString2 missing end tag for field %s",
		         key);
		return NULL;
	}
	if (plen)
		*plen = end - tag;

	if (pdata)
		*pdata = data;
	if (pdlen)
		*pdlen = dend - data;
	return tag;
}

/**! \brief this function returns a XML element identified by \b key
 * The input string will be manipulated, so that the element found is '\0' terminated.
 *
 * This function can not find multiple elements on the same level as the input string is changed!
 */
static BOOL freerdp_assistance_consume_input_and_get_element(char* input, const char* key,
                                                             char** element, size_t* elen)
{
	WINPR_ASSERT(input);
	WINPR_ASSERT(key);
	WINPR_ASSERT(element);
	WINPR_ASSERT(elen);

	size_t len = 0;
	size_t dlen = 0;
	char* data = NULL;
	char* tag = freerdp_assistance_contains_element(input, strlen(input), key, &len, &data, &dlen);
	if (!tag)
		return FALSE;

	char* end = data + dlen;
	*tag = '\0';
	*end = '\0';
	*element = data;
	*elen = dlen + 1;
	return TRUE;
}

static BOOL freerdp_assistance_get_element(char* input, size_t ilen, const char* key,
                                           char** element, size_t* elen)
{
	WINPR_ASSERT(input);
	WINPR_ASSERT(key);
	WINPR_ASSERT(element);
	WINPR_ASSERT(elen);

	size_t len = 0;
	size_t dlen = 0;
	char* data = NULL;
	char* tag = freerdp_assistance_contains_element(input, ilen, key, &len, &data, &dlen);
	if (!tag)
		return FALSE;

	if (tag + len > input + ilen)
		return FALSE;

	char* end = tag + len;
	*element = data;
	*elen = end - data + 1;
	return TRUE;
}

static BOOL freerdp_assistance_parse_all_elements_of(rdpAssistanceFile* file, char* data,
                                                     size_t len, const char* key,
                                                     BOOL (*fkt)(rdpAssistanceFile* file,
                                                                 char* data, size_t len))
{
	char* val = NULL;
	size_t vlen = 0;

	while (freerdp_assistance_get_element(data, len, key, &val, &vlen))
	{
		data = val + vlen;
		len = strnlen(data, len);
		if (vlen > 0)
		{
			val[vlen - 1] = '\0';

			if (!fkt(file, val, vlen))
				return FALSE;
		}
	}

	return TRUE;
}

static BOOL freerdp_assistance_parse_all_elements_of_l(rdpAssistanceFile* file, char* data,
                                                       size_t len)
{
	UINT32 p = 0;
	const char* n = NULL;
	const char* u = NULL;
	size_t nlen = 0;
	size_t ulen = 0;
	if (!freerdp_assistance_parse_attr_uint32(&p, "P", data))
		return FALSE;
	if (!freerdp_assistance_parse_attr(&n, &nlen, "N", data))
		return FALSE;
	if (!freerdp_assistance_parse_attr(&u, &ulen, "U", data))
		return FALSE;

	if (n && (nlen > 0))
	{
		if (!append_address_to_list(file->MachineAddresses, n, nlen))
			return FALSE;
		if (!ArrayList_Append(file->MachinePorts, (void*)(uintptr_t)p))
			return FALSE;
	}
	if (u && (ulen > 0))
	{
		if (!append_address_to_list(file->MachineAddresses, u, ulen))
			return FALSE;
		if (!ArrayList_Append(file->MachinePorts, (void*)(uintptr_t)p))
			return FALSE;
	}
	return TRUE;
}

static BOOL freerdp_assistance_parse_all_elements_of_t(rdpAssistanceFile* file, char* data,
                                                       size_t len)
{
	UINT32 id = 0;
	UINT32 sid = 0;
	if (!freerdp_assistance_parse_attr_uint32(&id, "ID", data))
		return FALSE;
	if (!freerdp_assistance_parse_attr_uint32(&sid, "SID", data))
		return FALSE;
	WLog_DBG(TAG, "transport id=%" PRIu32 ", sid=%" PRIu32, id, sid);
	return freerdp_assistance_parse_all_elements_of(file, data, len, "L",
	                                                freerdp_assistance_parse_all_elements_of_l);
}

static BOOL freerdp_assistance_parse_all_elements_of_c(rdpAssistanceFile* file, char* data,
                                                       size_t len)
{
	return freerdp_assistance_parse_all_elements_of(file, data, len, "T",
	                                                freerdp_assistance_parse_all_elements_of_t);
}

static BOOL freerdp_assistance_parse_find_elements_of_c(rdpAssistanceFile* file, char* data,
                                                        size_t len)
{
	return freerdp_assistance_parse_all_elements_of(file, data, len, "C",
	                                                freerdp_assistance_parse_all_elements_of_c);
}

static BOOL freerdp_assistance_parse_connection_string2(rdpAssistanceFile* file)
{
	BOOL rc = FALSE;

	WINPR_ASSERT(file);

	if (!file->ConnectionString2)
		return FALSE;

	char* str = _strdup(file->ConnectionString2);
	if (!str)
		goto out_fail;

	char* e = NULL;
	size_t elen = 0;
	if (!freerdp_assistance_consume_input_and_get_element(str, "E", &e, &elen))
		goto out_fail;

	if (!e || (elen == 0))
		goto out_fail;

	char* a = NULL;
	size_t alen = 0;
	if (!freerdp_assistance_get_element(e, elen, "A", &a, &alen))
		goto out_fail;

	if (!a || (alen == 0))
		goto out_fail;

	if (!freerdp_assistance_parse_find_elements_of_c(file, e, elen))
		goto out_fail;

	/* '\0' terminate the detected XML elements so
	 * the parser can continue with terminated strings
	 */
	a[alen] = '\0';
	if (!freerdp_assistance_parse_attr_str(&file->RASpecificParams, "KH", a))
		goto out_fail;

	if (!freerdp_assistance_parse_attr_str(&file->RASpecificParams2, "KH2", a))
		goto out_fail;

	if (!freerdp_assistance_parse_attr_str(&file->RASessionId, "ID", a))
		goto out_fail;

	rc = TRUE;
out_fail:
	free(str);
	return rc;
}

char* freerdp_assistance_construct_expert_blob(const char* name, const char* pass)
{
	if (!name || !pass)
		return NULL;

	const size_t nameLength = strlen(name) + strlen("NAME=");
	const size_t passLength = strlen(pass) + strlen("PASS=");
	const size_t size = nameLength + passLength + 64;
	char* ExpertBlob = (char*)calloc(1, size);

	if (!ExpertBlob)
		return NULL;

	(void)sprintf_s(ExpertBlob, size, "%" PRIuz ";NAME=%s%" PRIuz ";PASS=%s", nameLength, name,
	                passLength, pass);
	return ExpertBlob;
}

char* freerdp_assistance_generate_pass_stub(DWORD flags)
{
	UINT32 nums[14];
	char* passStub = NULL;
	char set1[64] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789*_";
	char set2[12] = "!@#$&^*()-+=";
	char set3[10] = "0123456789";
	char set4[26] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	char set5[26] = "abcdefghijklmnopqrstuvwxyz";
	passStub = (char*)malloc(15);

	if (!passStub)
		return NULL;

	/**
	 * PassStub generation:
	 *
	 * Characters 0 and 5-13 are from the set A-Z a-z 0-9 * _
	 * Character 1 is from the set !@#$&^*()-+=
	 * Character 2 is from the set 0-9
	 * Character 3 is from the set A-Z
	 * Character 4 is from the set a-z
	 *
	 * Example: WB^6HsrIaFmEpi
	 */
	winpr_RAND(nums, sizeof(nums));
	passStub[0] = set1[nums[0] % sizeof(set1)];   /* character 0 */
	passStub[1] = set2[nums[1] % sizeof(set2)];   /* character 1 */
	passStub[2] = set3[nums[2] % sizeof(set3)];   /* character 2 */
	passStub[3] = set4[nums[3] % sizeof(set4)];   /* character 3 */
	passStub[4] = set5[nums[4] % sizeof(set5)];   /* character 4 */
	passStub[5] = set1[nums[5] % sizeof(set1)];   /* character 5 */
	passStub[6] = set1[nums[6] % sizeof(set1)];   /* character 6 */
	passStub[7] = set1[nums[7] % sizeof(set1)];   /* character 7 */
	passStub[8] = set1[nums[8] % sizeof(set1)];   /* character 8 */
	passStub[9] = set1[nums[9] % sizeof(set1)];   /* character 9 */
	passStub[10] = set1[nums[10] % sizeof(set1)]; /* character 10 */
	passStub[11] = set1[nums[11] % sizeof(set1)]; /* character 11 */
	passStub[12] = set1[nums[12] % sizeof(set1)]; /* character 12 */
	passStub[13] = set1[nums[13] % sizeof(set1)]; /* character 13 */
	passStub[14] = '\0';
	return passStub;
}

BYTE* freerdp_assistance_encrypt_pass_stub(const char* password, const char* passStub,
                                           size_t* pEncryptedSize)
{
	BOOL rc = 0;
	size_t cbPasswordW = 0;
	size_t cbPassStubW = 0;
	size_t EncryptedSize = 0;
	BYTE PasswordHash[WINPR_MD5_DIGEST_LENGTH];
	WINPR_CIPHER_CTX* rc4Ctx = NULL;
	BYTE* pbIn = NULL;
	BYTE* pbOut = NULL;
	size_t cbOut = 0;
	size_t cbIn = 0;
	size_t cbFinal = 0;
	WCHAR* PasswordW = ConvertUtf8ToWCharAlloc(password, &cbPasswordW);
	WCHAR* PassStubW = ConvertUtf8ToWCharAlloc(passStub, &cbPassStubW);

	if (!PasswordW || !PassStubW)
		goto fail;

	cbPasswordW = (cbPasswordW) * sizeof(WCHAR);
	cbPassStubW = (cbPassStubW) * sizeof(WCHAR);
	if (!winpr_Digest(WINPR_MD_MD5, (BYTE*)PasswordW, cbPasswordW, (BYTE*)PasswordHash,
	                  sizeof(PasswordHash)))
		goto fail;

	EncryptedSize = cbPassStubW + 4;
	pbIn = (BYTE*)calloc(1, EncryptedSize);
	pbOut = (BYTE*)calloc(1, EncryptedSize);

	if (!pbIn || !pbOut)
		goto fail;

	*((UINT32*)pbIn) = (UINT32)cbPassStubW;
	CopyMemory(&pbIn[4], PassStubW, cbPassStubW);
	rc4Ctx = winpr_Cipher_New(WINPR_CIPHER_ARC4_128, WINPR_ENCRYPT, PasswordHash, NULL);

	if (!rc4Ctx)
	{
		WLog_ERR(TAG, "winpr_Cipher_New failure");
		goto fail;
	}

	cbOut = cbFinal = 0;
	cbIn = EncryptedSize;
	rc = winpr_Cipher_Update(rc4Ctx, pbIn, cbIn, pbOut, &cbOut);

	if (!rc)
	{
		WLog_ERR(TAG, "winpr_Cipher_Update failure");
		goto fail;
	}

	if (!winpr_Cipher_Final(rc4Ctx, pbOut + cbOut, &cbFinal))
	{
		WLog_ERR(TAG, "winpr_Cipher_Final failure");
		goto fail;
	}

	winpr_Cipher_Free(rc4Ctx);
	free(pbIn);
	free(PasswordW);
	free(PassStubW);
	*pEncryptedSize = EncryptedSize;
	return pbOut;
fail:
	winpr_Cipher_Free(rc4Ctx);
	free(PasswordW);
	free(PassStubW);
	free(pbIn);
	free(pbOut);
	return NULL;
}

static BOOL freerdp_assistance_decrypt2(rdpAssistanceFile* file)
{
	BOOL rc = FALSE;
	int status = 0;
	size_t cbPasswordW = 0;
	size_t cchOutW = 0;
	WINPR_CIPHER_CTX* aesDec = NULL;
	WCHAR* PasswordW = NULL;
	BYTE* pbIn = NULL;
	BYTE* pbOut = NULL;
	size_t cbOut = 0;
	size_t cbIn = 0;
	size_t cbFinal = 0;
	BYTE DerivedKey[WINPR_AES_BLOCK_SIZE] = { 0 };
	BYTE InitializationVector[WINPR_AES_BLOCK_SIZE] = { 0 };
	BYTE PasswordHash[WINPR_SHA1_DIGEST_LENGTH] = { 0 };

	WINPR_ASSERT(file);

	if (!file->password)
		return FALSE;

	PasswordW = ConvertUtf8ToWCharAlloc(file->password, &cbPasswordW);
	if (!PasswordW)
	{
		WLog_ERR(TAG, "Failed to parse ASSISTANCE file: Conversion from UCS2 to UTF8 failed");
		return FALSE;
	}

	cbPasswordW = (cbPasswordW) * sizeof(WCHAR);

	if (!winpr_Digest(WINPR_MD_SHA1, (BYTE*)PasswordW, cbPasswordW, PasswordHash,
	                  sizeof(PasswordHash)))
		goto fail;

	if (!freerdp_assistance_crypt_derive_key_sha1(PasswordHash, sizeof(PasswordHash), DerivedKey,
	                                              sizeof(DerivedKey)))
		goto fail;

	aesDec =
	    winpr_Cipher_New(WINPR_CIPHER_AES_128_CBC, WINPR_DECRYPT, DerivedKey, InitializationVector);

	if (!aesDec)
		goto fail;

	cbOut = cbFinal = 0;
	cbIn = file->EncryptedLHTicketLength;
	pbIn = file->EncryptedLHTicket;
	pbOut = (BYTE*)calloc(1, cbIn + WINPR_AES_BLOCK_SIZE + 2);

	if (!pbOut)
		goto fail;

	if (!winpr_Cipher_Update(aesDec, pbIn, cbIn, pbOut, &cbOut))
		goto fail;

	if (!winpr_Cipher_Final(aesDec, pbOut + cbOut, &cbFinal))
	{
		WLog_ERR(TAG, "winpr_Cipher_Final failure");
		goto fail;
	}

	cbOut += cbFinal;
	cbFinal = 0;

	union
	{
		const WCHAR* wc;
		const BYTE* b;
	} cnv;

	cnv.b = pbOut;
	cchOutW = cbOut / sizeof(WCHAR);

	if (!update_connectionstring2_wchar(file, cnv.wc, cchOutW))
	{
		WLog_ERR(TAG, "Failed to parse ASSISTANCE file: Conversion from UCS2 to UTF8 failed");
		goto fail;
	}

	if (!freerdp_assistance_parse_connection_string2(file))
		goto fail;

	rc = TRUE;
fail:
	winpr_Cipher_Free(aesDec);
	free(PasswordW);
	free(pbOut);
	WLog_DBG(TAG, "freerdp_assistance_parse_connection_string2: %d", status);
	return rc;
}

BYTE* freerdp_assistance_hex_string_to_bin(const void* raw, size_t* size)
{
	BYTE* buffer = NULL;
	if (!raw || !size)
		return NULL;
	*size = 0;
	const size_t length = strlen(raw);
	buffer = calloc(length, sizeof(BYTE));
	if (!buffer)
		return NULL;
	const size_t rc = winpr_HexStringToBinBuffer(raw, length, buffer, length);
	if (rc == 0)
	{
		free(buffer);
		return NULL;
	}
	*size = rc;
	return buffer;
}

char* freerdp_assistance_bin_to_hex_string(const void* raw, size_t size)
{
	return winpr_BinToHexString(raw, size, FALSE);
}

static int freerdp_assistance_parse_uploadinfo(rdpAssistanceFile* file, char* uploadinfo,
                                               size_t uploadinfosize)
{
	const char escalated[9] = "Escalated";
	const size_t esclen = sizeof(escalated);
	const char* typestr = NULL;
	size_t typelen = 0;

	if (!uploadinfo || (uploadinfosize == 0))
		return -1;

	if (strnlen(uploadinfo, uploadinfosize) == uploadinfosize)
	{
		WLog_WARN(TAG, "UPLOADINFOR string is not '\0' terminated");
		return -1;
	}

	if (!freerdp_assistance_parse_attr(&typestr, &typelen, "TYPE", uploadinfo))
		return -1;

	if ((typelen != esclen) || (strncmp(typestr, escalated, esclen) != 0))
	{
		WLog_ERR(TAG,
		         "Failed to parse ASSISTANCE file: Missing or invalid UPLOADINFO TYPE '%s' [%" PRIuz
		         "]",
		         typestr, typelen);
		return -1;
	}

	char* uploaddata = NULL;
	size_t uploaddatasize = 0;
	if (!freerdp_assistance_consume_input_and_get_element(uploadinfo, "UPLOADDATA", &uploaddata,
	                                                      &uploaddatasize))
		return -1;

	/* Parse USERNAME */
	if (!freerdp_assistance_parse_attr_str(&file->Username, "USERNAME", uploaddata))
		return -1;

	/* Parse LHTICKET */
	if (!freerdp_assistance_parse_attr_str(&file->LHTicket, "LHTICKET", uploaddata))
		return -1;

	/* Parse RCTICKET */
	if (!freerdp_assistance_parse_attr_str(&file->RCTicket, "RCTICKET", uploaddata))
		return -1;

	/* Parse RCTICKETENCRYPTED */
	if (!freerdp_assistance_parse_attr_bool(&file->RCTicketEncrypted, "RCTICKETENCRYPTED",
	                                        uploaddata))
		return -1;

	/* Parse PassStub */
	if (!freerdp_assistance_parse_attr_str(&file->PassStub, "PassStub", uploaddata))
		return -1;

	if (file->PassStub)
	{
		const char* amp = "&amp;";
		char* passtub = strstr(file->PassStub, amp);
		while (passtub)
		{
			const char* end = passtub + 5;
			const size_t len = strlen(end);
			memmove(&passtub[1], end, len + 1);
			passtub = strstr(passtub, amp);
		}
	}

	/* Parse DtStart */
	if (!freerdp_assistance_parse_attr_uint32(&file->DtStart, "DtStart", uploaddata))
		return -1;

	/* Parse DtLength */
	if (!freerdp_assistance_parse_attr_uint32(&file->DtLength, "DtLength", uploaddata))
		return -1;

	/* Parse L (LowSpeed) */
	if (!freerdp_assistance_parse_attr_bool(&file->LowSpeed, "L", uploaddata))
		return -1;

	file->Type = (file->LHTicket) ? 2 : 1;
	int status = 0;

	switch (file->Type)
	{
		case 2:
		{
			file->EncryptedLHTicket = freerdp_assistance_hex_string_to_bin(
			    file->LHTicket, &file->EncryptedLHTicketLength);

			if (!freerdp_assistance_decrypt2(file))
				status = -1;
		}
		break;

		case 1:
		{
			if (!freerdp_assistance_parse_connection_string1(file))
				status = -1;
		}
		break;

		default:
			return -1;
	}

	if (status < 0)
	{
		WLog_ERR(TAG, "freerdp_assistance_parse_connection_string1 failure: %d", status);
		return -1;
	}

	file->EncryptedPassStub = freerdp_assistance_encrypt_pass_stub(file->password, file->PassStub,
	                                                               &file->EncryptedPassStubLength);

	if (!file->EncryptedPassStub)
		return -1;

	return 1;
}

static int freerdp_assistance_parse_file_buffer_int(rdpAssistanceFile* file, char* buffer,
                                                    size_t size, const char* password)
{
	WINPR_ASSERT(file);
	WINPR_ASSERT(buffer);
	WINPR_ASSERT(size > 0);

	if (!update_password(file, password))
		return -1;

	char* uploadinfo = NULL;
	size_t uploadinfosize = 0;
	if (freerdp_assistance_consume_input_and_get_element(buffer, "UPLOADINFO", &uploadinfo,
	                                                     &uploadinfosize))
		return freerdp_assistance_parse_uploadinfo(file, uploadinfo, uploadinfosize);

	size_t elen = 0;
	const char* estr = freerdp_assistance_contains_element(buffer, size, "E", &elen, NULL, NULL);
	if (!estr || (elen == 0))
	{
		WLog_ERR(TAG, "Failed to parse ASSISTANCE file: Neither UPLOADINFO nor <E> found");
		return -1;
	}
	if (!update_connectionstring2(file, estr, elen))
		return -1;

	if (!freerdp_assistance_parse_connection_string2(file))
		return -1;

	return 1;
}

int freerdp_assistance_parse_file_buffer(rdpAssistanceFile* file, const char* cbuffer, size_t size,
                                         const char* password)
{
	WINPR_ASSERT(file);
	if (!password)
	{
		WLog_WARN(TAG, "empty password supplied");
	}

	if (!cbuffer || (size == 0))
	{
		WLog_WARN(TAG, "no data supplied [%p, %" PRIuz "]", cbuffer, size);
		return -1;
	}

	char* abuffer = strndup(cbuffer, size);
	const size_t len = strnlen(cbuffer, size);
	if (len == size)
		WLog_WARN(TAG, "Input data not '\0' terminated");

	if (!abuffer)
		return -1;

	const int rc = freerdp_assistance_parse_file_buffer_int(file, abuffer, len + 1, password);
	free(abuffer);
	return rc;
}

int freerdp_assistance_parse_file(rdpAssistanceFile* file, const char* name, const char* password)
{
	int status = 0;
	BYTE* buffer = NULL;
	FILE* fp = NULL;
	size_t readSize = 0;
	union
	{
		INT64 i64;
		size_t s;
	} fileSize;

	if (!update_name(file, name))
		return -1;

	fp = winpr_fopen(name, "r");

	if (!fp)
	{
		WLog_ERR(TAG, "Failed to open ASSISTANCE file %s ", name);
		return -1;
	}

	(void)_fseeki64(fp, 0, SEEK_END);
	fileSize.i64 = _ftelli64(fp);
	(void)_fseeki64(fp, 0, SEEK_SET);

	if (fileSize.i64 < 1)
	{
		WLog_ERR(TAG, "Failed to read ASSISTANCE file %s ", name);
		(void)fclose(fp);
		return -1;
	}

	buffer = (BYTE*)malloc(fileSize.s + 2);

	if (!buffer)
	{
		(void)fclose(fp);
		return -1;
	}

	readSize = fread(buffer, fileSize.s, 1, fp);

	if (!readSize)
	{
		if (!ferror(fp))
			readSize = fileSize.s;
	}

	(void)fclose(fp);

	if (readSize < 1)
	{
		WLog_ERR(TAG, "Failed to read ASSISTANCE file %s ", name);
		free(buffer);
		buffer = NULL;
		return -1;
	}

	buffer[fileSize.s] = '\0';
	buffer[fileSize.s + 1] = '\0';
	status = freerdp_assistance_parse_file_buffer(file, (char*)buffer, fileSize.s, password);
	free(buffer);
	return status;
}

BOOL freerdp_assistance_populate_settings_from_assistance_file(rdpAssistanceFile* file,
                                                               rdpSettings* settings)
{
	if (!freerdp_settings_set_bool(settings, FreeRDP_RemoteAssistanceMode, TRUE))
		return FALSE;

	if (!file->RASessionId || !file->MachineAddresses)
		return FALSE;

	if (!freerdp_settings_set_string(settings, FreeRDP_RemoteAssistanceSessionId,
	                                 file->RASessionId))
		return FALSE;

	if (file->RCTicket)
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_RemoteAssistanceRCTicket,
		                                 file->RCTicket))
			return FALSE;
	}
	else
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_RemoteAssistanceRCTicket,
		                                 file->ConnectionString2))
			return FALSE;
	}

	if (file->PassStub)
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_RemoteAssistancePassStub,
		                                 file->PassStub))
			return FALSE;
	}

	if (ArrayList_Count(file->MachineAddresses) < 1)
		return FALSE;

	const char* addr = ArrayList_GetItem(file->MachineAddresses, 0);
	if (!freerdp_settings_set_string(settings, FreeRDP_ServerHostname, addr))
		return FALSE;

	if (!freerdp_settings_set_string(settings, FreeRDP_AssistanceFile, file->filename))
		return FALSE;

	if (!freerdp_settings_set_string(settings, FreeRDP_RemoteAssistancePassword, file->password))
		return FALSE;

	if (file->Username)
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_Username, file->Username))
			return FALSE;
	}

	if (!freerdp_settings_set_bool(settings, FreeRDP_RemoteAssistanceMode, TRUE))
		return FALSE;

	const size_t ports = ArrayList_Count(file->MachinePorts);
	const size_t addresses = ArrayList_Count(file->MachineAddresses);
	if (ports < 1)
		return FALSE;
	if (ports != addresses)
		return FALSE;

	union
	{
		UINT32 port;
		void* data;
	} cnv;
	cnv.data = ArrayList_GetItem(file->MachinePorts, 0);
	if (!freerdp_settings_set_uint32(settings, FreeRDP_ServerPort, cnv.port))
		return FALSE;

	if (!freerdp_target_net_adresses_reset(settings, ports))
		return FALSE;

	for (size_t x = 0; x < ports; x++)
	{
		cnv.data = ArrayList_GetItem(file->MachinePorts, x);
		if (!freerdp_settings_set_pointer_array(settings, FreeRDP_TargetNetPorts, x, &cnv.port))
			return FALSE;
	}
	for (size_t i = 0; i < addresses; i++)
	{
		const char* maddr = ArrayList_GetItem(file->MachineAddresses, i);
		if (!freerdp_settings_set_pointer_array(settings, FreeRDP_TargetNetAddresses, i, maddr))
			return FALSE;
	}

	return TRUE;
}

static BOOL setup_string(wArrayList* list)
{
	WINPR_ASSERT(list);

	wObject* obj = ArrayList_Object(list);
	if (!obj)
		return FALSE;
	obj->fnObjectFree = free;
	// obj->fnObjectNew = wwinpr_ObjectStringClone;
	return TRUE;
}

rdpAssistanceFile* freerdp_assistance_file_new(void)
{
	winpr_InitializeSSL(WINPR_SSL_INIT_DEFAULT);
	rdpAssistanceFile* file = calloc(1, sizeof(rdpAssistanceFile));
	if (!file)
		return NULL;

	file->MachineAddresses = ArrayList_New(FALSE);
	file->MachinePorts = ArrayList_New(FALSE);
	file->MachineUris = ArrayList_New(FALSE);

	if (!file->MachineAddresses || !file->MachinePorts || !file->MachineUris)
		goto fail;

	if (!setup_string(file->MachineAddresses) || !setup_string(file->MachineUris))
		goto fail;

	return file;

fail:
	WINPR_PRAGMA_DIAG_PUSH
	WINPR_PRAGMA_DIAG_IGNORED_MISMATCHED_DEALLOC
	freerdp_assistance_file_free(file);
	WINPR_PRAGMA_DIAG_POP
	return NULL;
}

void freerdp_assistance_file_free(rdpAssistanceFile* file)
{
	if (!file)
		return;

	update_password(file, NULL);
	update_connectionstring2(file, NULL, 0);
	free(file->filename);
	free(file->Username);
	free(file->LHTicket);
	free(file->RCTicket);
	free(file->PassStub);
	free(file->ConnectionString1);
	free(file->EncryptedLHTicket);
	free(file->RASessionId);
	free(file->RASpecificParams);
	free(file->RASpecificParams2);
	free(file->EncryptedPassStub);

	ArrayList_Free(file->MachineAddresses);
	ArrayList_Free(file->MachinePorts);
	ArrayList_Free(file->MachineUris);
	free(file);
}

void freerdp_assistance_print_file(rdpAssistanceFile* file, wLog* log, DWORD level)
{
	WINPR_ASSERT(file);

	WLog_Print(log, level, "Username: %s", file->Username);
	WLog_Print(log, level, "LHTicket: %s", file->LHTicket);
	WLog_Print(log, level, "RCTicket: %s", file->RCTicket);
	WLog_Print(log, level, "RCTicketEncrypted: %" PRId32, file->RCTicketEncrypted);
	WLog_Print(log, level, "PassStub: %s", file->PassStub);
	WLog_Print(log, level, "DtStart: %" PRIu32, file->DtStart);
	WLog_Print(log, level, "DtLength: %" PRIu32, file->DtLength);
	WLog_Print(log, level, "LowSpeed: %" PRId32, file->LowSpeed);
	WLog_Print(log, level, "RASessionId: %s", file->RASessionId);
	WLog_Print(log, level, "RASpecificParams: %s", file->RASpecificParams);
	WLog_Print(log, level, "RASpecificParams2: %s", file->RASpecificParams2);

	for (size_t x = 0; x < ArrayList_Count(file->MachineAddresses); x++)
	{
		UINT32 port = 0;
		const char* uri = NULL;
		const char* addr = ArrayList_GetItem(file->MachineAddresses, x);
		if (x < ArrayList_Count(file->MachinePorts))
		{
			union
			{
				UINT32 port;
				void* data;
			} cnv;
			cnv.data = ArrayList_GetItem(file->MachinePorts, x);
			port = cnv.port;
		}
		if (x < ArrayList_Count(file->MachineUris))
			uri = ArrayList_GetItem(file->MachineUris, x);

		WLog_Print(log, level, "MachineAddress [%" PRIdz ": %s", x, addr);
		WLog_Print(log, level, "MachinePort    [%" PRIdz ": %" PRIu32, x, port);
		WLog_Print(log, level, "MachineURI     [%" PRIdz ": %s", x, uri);
	}
}

BOOL freerdp_assistance_get_encrypted_pass_stub(rdpAssistanceFile* file, const char** pwd,
                                                size_t* size)
{
	if (!file || !pwd || !size)
		return FALSE;

	*pwd = (const char*)file->EncryptedPassStub;
	*size = file->EncryptedPassStubLength;
	return TRUE;
}

int freerdp_assistance_set_connection_string2(rdpAssistanceFile* file, const char* string,
                                              const char* password)
{
	if (!file || !string || !password)
		return -1;

	char* str = _strdup(string);
	if (!str)
		return -1;

	if (!update_connectionstring2_nocopy(file, str))
		return -1;
	if (!update_password(file, password))
		return -1;
	return freerdp_assistance_parse_connection_string2(file);
}
