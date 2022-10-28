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

	UINT32 MachineCount;
	char** MachineAddresses;
	UINT32* MachinePorts;

	char* RASessionId;
	char* RASpecificParams;

	char* filename;
	char* password;
};

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

static BOOL freerdp_assistance_crypt_derive_key_sha1(BYTE* hash, size_t hashLength, BYTE* key,
                                                     size_t keyLength)
{
	BOOL rc = FALSE;
	size_t i;
	BYTE* buffer;
	BYTE pad1[64];
	BYTE pad2[64];
	memset(pad1, 0x36, 64);
	memset(pad2, 0x5C, 64);

	for (i = 0; i < hashLength; i++)
	{
		pad1[i] ^= hash[i];
		pad2[i] ^= hash[i];
	}

	buffer = (BYTE*)calloc(hashLength, 2);

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

static BOOL reallocate(rdpAssistanceFile* file, const char* host, UINT32 port)
{
	void *tmp1, *tmp2;
	file->MachineCount++;
	tmp1 = realloc(file->MachinePorts, sizeof(UINT32) * file->MachineCount);
	tmp2 = realloc(file->MachineAddresses, sizeof(char*) * file->MachineCount);

	if (!tmp1 || !tmp2)
	{
		free(tmp1);
		free(tmp2);
		return FALSE;
	}

	file->MachinePorts = tmp1;
	file->MachineAddresses = tmp2;
	file->MachinePorts[file->MachineCount - 1] = port;
	file->MachineAddresses[file->MachineCount - 1] = _strdup(host);
	return TRUE;
}

static BOOL append_address(rdpAssistanceFile* file, const char* host, const char* port)
{
	unsigned long p;
	errno = 0;
	p = strtoul(port, NULL, 0);

	if ((errno != 0) || (p == 0) || (p > UINT16_MAX))
	{
		WLog_ERR(TAG, "Failed to parse ASSISTANCE file: ConnectionString2 invalid port value %s",
		         port);
		return FALSE;
	}

	return reallocate(file, host, (UINT16)p);
}

static BOOL freerdp_assistance_parse_address_list(rdpAssistanceFile* file, char* list)
{
	WLog_DBG(TAG, "freerdp_assistance_parse_address_list list=%s", list);

	BOOL rc = FALSE;

	if (!file || !list)
		return FALSE;

	char* strp = list;
	char* s = ";";
	char* token;

	// get the first token
	token = strtok(strp, s);

	// walk through other tokens
	while (token != NULL)
	{
		char* port = strchr(token, ':');
		*port = '\0';
		port++;

		if (!append_address(file, token, port))
			goto out;

		token = strtok(NULL, s);
	}
	rc = TRUE;
out:
	return rc;
}

static BOOL freerdp_assistance_parse_connection_string1(rdpAssistanceFile* file)
{
	size_t i;
	char* str;
	int count;
	size_t length;
	char* tokens[8];
	BOOL rc = FALSE;

	if (!file || !file->RCTicket)
		return FALSE;

	/**
	 * <ProtocolVersion>,<protocolType>,<machineAddressList>,<assistantAccountPwd>,
	 * <RASessionID>,<RASessionName>,<RASessionPwd>,<protocolSpecificParms>
	 */
	count = 1;
	str = _strdup(file->RCTicket);

	if (!str)
		goto error;

	length = strlen(str);

	for (i = 0; i < length; i++)
	{
		if (str[i] == ',')
			count++;
	}

	if (count != 8)
		goto error;

	count = 0;
	tokens[count++] = str;

	for (i = 0; i < length; i++)
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

static BOOL freerdp_assistance_parse_connection_string2(rdpAssistanceFile* file)
{
	char* str;
	char* tag;
	char* end;
	char* p;
	BOOL rc = FALSE;

	if (!file || !file->ConnectionString2)
		return FALSE;

	str = file->ConnectionString2;

	if (!strstr(str, "<E>"))
	{
		WLog_ERR(TAG, "Failed to parse ASSISTANCE file: ConnectionString2 missing field <E>");
		return FALSE;
	}

	if (!strstr(str, "<C>"))
	{
		WLog_ERR(TAG, "Failed to parse ASSISTANCE file: ConnectionString2 missing field <C>");
		return FALSE;
	}

	str = _strdup(file->ConnectionString2);

	if (!str)
		goto out_fail;

	if (!(tag = strstr(str, "<A")))
	{
		WLog_ERR(TAG, "Failed to parse ASSISTANCE file: ConnectionString2 missing field <A");
		goto out_fail;
	}

	/* Parse Auth String Node (<A>) */
	end = strstr(tag, "/>");

	if (!end)
		goto out_fail;

	*end = '\0';
	p = strstr(tag, "KH=\"");

	if (p)
	{
		char* q;
		size_t length;
		p += sizeof("KH=\"") - 1;
		q = strchr(p, '"');

		if (!q)
		{
			WLog_ERR(TAG, "Failed to parse ASSISTANCE file: ConnectionString2 invalid field KH=%s",
			         q);
			goto out_fail;
		}

		if (p > q)
		{
			WLog_ERR(
			    TAG,
			    "Failed to parse ASSISTANCE file: ConnectionString2 invalid field order for KH");
			goto out_fail;
		}

		length = q - p;
		free(file->RASpecificParams);
		file->RASpecificParams = (char*)malloc(length + 1);

		if (!file->RASpecificParams)
			goto out_fail;

		CopyMemory(file->RASpecificParams, p, length);
		file->RASpecificParams[length] = '\0';
	}

	p = strstr(tag, "ID=\"");

	if (p)
	{
		char* q;
		size_t length;
		p += sizeof("ID=\"") - 1;
		q = strchr(p, '"');

		if (!q)
		{
			WLog_ERR(TAG, "Failed to parse ASSISTANCE file: ConnectionString2 invalid field ID=%s",
			         q);
			goto out_fail;
		}

		if (p > q)
		{
			WLog_ERR(TAG, "Failed to parse ASSISTANCE file: ConnectionString2 invalid field "
			              "order for ID");
			goto out_fail;
		}
		length = q - p;
		free(file->RASessionId);
		file->RASessionId = (char*)malloc(length + 1);

		if (!file->RASessionId)
			goto out_fail;

		CopyMemory(file->RASessionId, p, length);
		file->RASessionId[length] = '\0';
	}

	*end = '/';
	/* Parse <L  last address is used */
	p = strstr(str, "<L P=\"");

	while (p)
	{
		char* port;
		char* q;
		size_t length;
		p += sizeof("<L P=\"") - 1;
		q = strchr(p, '"');

		if (!q)
		{
			WLog_ERR(TAG,
			         "Failed to parse ASSISTANCE file: ConnectionString2 invalid field <L P=%s", q);
			goto out_fail;
		}

		q[0] = '\0';
		q++;
		port = p;
		p = strstr(q, " N=\"");

		if (!p)
		{
			WLog_ERR(TAG, "Failed to parse ASSISTANCE file: ConnectionString2 invalid field N=%s",
			         p);
			goto out_fail;
		}

		p += sizeof(" N=\"") - 1;
		q = strchr(p, '"');

		if (!q)
		{
			WLog_ERR(TAG, "Failed to parse ASSISTANCE file: ConnectionString2 invalid field N=%s",
			         q);
			goto out_fail;
		}

		q[0] = '\0';
		q++;
		length = strlen(p);

		if (length > 6)
		{
			if (!append_address(file, p, port))
				goto out_fail;
		}

		p = strstr(q, "<L P=\"");
	}

	rc = TRUE;
out_fail:
	free(str);
	return rc;
}

char* freerdp_assistance_construct_expert_blob(const char* name, const char* pass)
{
	size_t size;
	size_t nameLength;
	size_t passLength;
	char* ExpertBlob = NULL;

	if (!name || !pass)
		return NULL;

	nameLength = strlen(name) + strlen("NAME=");
	passLength = strlen(pass) + strlen("PASS=");
	size = nameLength + passLength + 64;
	ExpertBlob = (char*)calloc(1, size);

	if (!ExpertBlob)
		return NULL;

	sprintf_s(ExpertBlob, size, "%" PRIdz ";NAME=%s%" PRIdz ";PASS=%s", nameLength, name,
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
	winpr_RAND((BYTE*)nums, sizeof(nums));
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
	BOOL rc;
	size_t cbPasswordW;
	size_t cbPassStubW;
	size_t EncryptedSize;
	BYTE PasswordHash[WINPR_MD5_DIGEST_LENGTH];
	WINPR_CIPHER_CTX* rc4Ctx = NULL;
	BYTE* pbIn = NULL;
	BYTE* pbOut = NULL;
	size_t cbOut, cbIn, cbFinal;
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

static BOOL freerdp_assistance_decrypt2(rdpAssistanceFile* file, const char* password)
{
	BOOL rc = FALSE;
	int status = 0;
	size_t cbPasswordW;
	size_t cchOutW = 0;
	WINPR_CIPHER_CTX* aesDec = NULL;
	WCHAR* PasswordW = NULL;
	BYTE* pbIn = NULL;
	BYTE* pbOut = NULL;
	size_t cbOut, cbIn, cbFinal;
	BYTE DerivedKey[WINPR_AES_BLOCK_SIZE] = { 0 };
	BYTE InitializationVector[WINPR_AES_BLOCK_SIZE] = { 0 };
	BYTE PasswordHash[WINPR_SHA1_DIGEST_LENGTH] = { 0 };

	if (!file || !password)
		return FALSE;

	PasswordW = ConvertUtf8ToWCharAlloc(password, &cbPasswordW);
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
	cbIn = (size_t)file->EncryptedLHTicketLength;
	pbIn = (BYTE*)file->EncryptedLHTicket;
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
	file->ConnectionString2 = ConvertWCharNToUtf8Alloc(cnv.wc, cchOutW, NULL);
	if (!file->ConnectionString2)
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
	size_t length, rc;
	if (!raw || !size)
		return NULL;
	*size = 0;
	length = strlen(raw);
	buffer = calloc(length, sizeof(BYTE));
	if (!buffer)
		return NULL;
	rc = winpr_HexStringToBinBuffer(raw, length, buffer, length);
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

int freerdp_assistance_parse_file_buffer(rdpAssistanceFile* file, const char* buffer, size_t size,
                                         const char* password)
{
	char* p;
	char* q;
	char* r;
	char* amp;
	int status;
	size_t length;

	free(file->password);
	file->password = _strdup(password);

	p = strstr(buffer, "UPLOADINFO");

	if (p)
	{
		p = strstr(p + sizeof("UPLOADINFO") - 1, "TYPE=\"");

		if (!p)
		{
			WLog_ERR(TAG, "Failed to parse ASSISTANCE file: Missing UPLOADINFO TYPE");
			return -1;
		}

		p = strstr(buffer, "UPLOADDATA");

		if (!p)
		{
			WLog_ERR(TAG, "Failed to parse ASSISTANCE file: Missing UPLOADDATA");
			return -1;
		}

		/* Parse USERNAME */
		p = strstr(buffer, "USERNAME=\"");

		if (p)
		{
			p += sizeof("USERNAME=\"") - 1;
			q = strchr(p, '"');

			if (!q)
			{
				WLog_ERR(TAG, "Failed to parse ASSISTANCE file: Invalid USERNAME=%s", p);
				return -1;
			}

			if (p > q)
			{
				WLog_ERR(TAG, "Failed to parse ASSISTANCE file: invalid field "
				              "order for USERNAME");
				return -1;
			}

			length = q - p;
			file->Username = (char*)malloc(length + 1);

			if (!file->Username)
				return -1;

			CopyMemory(file->Username, p, length);
			file->Username[length] = '\0';
		}

		/* Parse LHTICKET */
		p = strstr(buffer, "LHTICKET=\"");

		if (p)
		{
			p += sizeof("LHTICKET=\"") - 1;
			q = strchr(p, '"');

			if (!q)
			{
				WLog_ERR(TAG, "Failed to parse ASSISTANCE file: Invalid LHTICKET=%s", p);
				return -1;
			}

			if (p > q)
			{
				WLog_ERR(TAG, "Failed to parse ASSISTANCE file: invalid field "
				              "order for LHTICKET");
				return -1;
			}

			length = q - p;
			file->LHTicket = (char*)malloc(length + 1);

			if (!file->LHTicket)
				return -1;

			CopyMemory(file->LHTicket, p, length);
			file->LHTicket[length] = '\0';
		}

		/* Parse RCTICKET */
		p = strstr(buffer, "RCTICKET=\"");

		if (p)
		{
			p += sizeof("RCTICKET=\"") - 1;
			q = strchr(p, '"');

			if (!q)
			{
				WLog_ERR(TAG, "Failed to parse ASSISTANCE file: Invalid RCTICKET=%s", p);
				return -1;
			}

			if (p > q)
			{
				WLog_ERR(TAG, "Failed to parse ASSISTANCE file: invalid field "
				              "order for RCTICKET");
				return -1;
			}

			length = q - p;
			file->RCTicket = (char*)malloc(length + 1);

			if (!file->RCTicket)
				return -1;

			CopyMemory(file->RCTicket, p, length);
			file->RCTicket[length] = '\0';
		}

		/* Parse RCTICKETENCRYPTED */
		p = strstr(buffer, "RCTICKETENCRYPTED=\"");

		if (p)
		{
			p += sizeof("RCTICKETENCRYPTED=\"") - 1;
			q = strchr(p, '"');

			if (!q)
			{
				WLog_ERR(TAG, "Failed to parse ASSISTANCE file: Invalid RCTICKETENCRYPTED=%s", p);
				return -1;
			}

			if (p > q)
			{
				WLog_ERR(TAG, "Failed to parse ASSISTANCE file: invalid field "
				              "order for RCTICKETENCRYPTED");
				return -1;
			}

			length = q - p;

			if ((length == 1) && (p[0] == '1'))
				file->RCTicketEncrypted = TRUE;
		}

		/* Parse PassStub */
		p = strstr(buffer, "PassStub=\"");

		if (p)
		{
			p += sizeof("PassStub=\"") - 1;

			// needs to be unescaped (&amp; => &)
			amp = strstr(p, "&amp;");

			q = strchr(p, '"');

			if (!q)
			{
				WLog_ERR(TAG, "Failed to parse ASSISTANCE file: Invalid PassStub=%s", p);
				return -1;
			}

			if (p > q)
			{
				WLog_ERR(TAG, "Failed to parse ASSISTANCE file: invalid field "
				              "order for PassStub");
				return -1;
			}

			if (amp)
			{
				length = q - p - 4;
			}
			else
			{
				length = q - p;
			}

			file->PassStub = (char*)malloc(length + 1);

			if (!file->PassStub)
				return -1;

			if (amp)
			{
				// just skip over "amp;" leaving "&"
				CopyMemory(file->PassStub, p, amp - p + 1);
				CopyMemory(file->PassStub + (amp - p + 1), amp + 5, q - amp + 5);
			}
			else
			{
				CopyMemory(file->PassStub, p, length);
			}

			file->PassStub[length] = '\0';
		}

		/* Parse DtStart */
		p = strstr(buffer, "DtStart=\"");

		if (p)
		{
			p += sizeof("DtStart=\"") - 1;
			q = strchr(p, '"');

			if (!q)
			{
				WLog_ERR(TAG, "Failed to parse ASSISTANCE file: Invalid DtStart=%s", p);
				return -1;
			}

			if (p > q)
			{
				WLog_ERR(TAG, "Failed to parse ASSISTANCE file: invalid field "
				              "order for DtStart");
				return -1;
			}

			length = q - p;
			r = (char*)malloc(length + 1);

			if (!r)
				return -1;

			CopyMemory(r, p, length);
			r[length] = '\0';
			errno = 0;
			{
				unsigned long val = strtoul(r, NULL, 0);

				if ((errno != 0) || (val > UINT32_MAX))
				{
					WLog_ERR(TAG, "Failed to parse ASSISTANCE file: Invalid DtStart value %s", r);
					free(r);
					return -1;
				}

				free(r);
				file->DtStart = val;
			}
		}

		/* Parse DtLength */
		p = strstr(buffer, "DtLength=\"");

		if (p)
		{
			p += sizeof("DtLength=\"") - 1;
			q = strchr(p, '"');

			if (!q)
			{
				WLog_ERR(TAG, "Failed to parse ASSISTANCE file: Invalid DtLength=%s", p);
				return -1;
			}

			if (p > q)
			{
				WLog_ERR(TAG, "Failed to parse ASSISTANCE file: invalid field "
				              "order for DtLength");
				return -1;
			}

			length = q - p;
			r = (char*)malloc(length + 1);

			if (!r)
				return -1;

			CopyMemory(r, p, length);
			r[length] = '\0';
			errno = 0;
			{
				unsigned long val = strtoul(r, NULL, 0);

				if ((errno != 0) || (val > UINT32_MAX))
				{
					WLog_ERR(TAG, "Failed to parse ASSISTANCE file: Invalid DtLength value %s", r);
					free(r);
					return -1;
				}

				free(r);
				file->DtLength = val;
			}
		}

		/* Parse L (LowSpeed) */
		p = strstr(buffer, " L=\"");

		if (p)
		{
			p += sizeof(" L=\"") - 1;
			q = strchr(p, '"');

			if (!q)
			{
				WLog_ERR(TAG, "Failed to parse ASSISTANCE file: Invalid L=%s", p);
				return -1;
			}

			if (p > q)
			{
				WLog_ERR(TAG, "Failed to parse ASSISTANCE file: invalid field "
				              "order for L");
				return -1;
			}

			length = q - p;

			if ((length == 1) && (p[0] == '1'))
				file->LowSpeed = TRUE;
		}

		file->Type = (file->LHTicket) ? 2 : 1;
		status = 0;

		switch (file->Type)
		{
			case 2:
			{
				file->EncryptedLHTicket = freerdp_assistance_hex_string_to_bin(
				    file->LHTicket, &file->EncryptedLHTicketLength);

				if (!freerdp_assistance_decrypt2(file, password))
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

		file->EncryptedPassStub = freerdp_assistance_encrypt_pass_stub(
		    password, file->PassStub, &file->EncryptedPassStubLength);

		if (!file->EncryptedPassStub)
			return -1;

		return 1;
	}

	p = strstr(buffer, "<E>");

	if (p)
	{
		q = strstr(buffer, "</E>");

		if (!q)
		{
			WLog_ERR(TAG, "Failed to parse ASSISTANCE file: Missing </E> tag");
			return -1;
		}

		if (p > q)
		{
			WLog_ERR(TAG, "Failed to parse ASSISTANCE file: invalid field order for <E>");
			return -1;
		}

		q += sizeof("</E>") - 1;
		length = q - p;
		file->ConnectionString2 = (char*)malloc(length + 1);

		if (!file->ConnectionString2)
			return -1;

		CopyMemory(file->ConnectionString2, p, length);
		file->ConnectionString2[length] = '\0';

		if (!freerdp_assistance_parse_connection_string2(file))
			return -1;

		return 1;
	}

	WLog_ERR(TAG, "Failed to parse ASSISTANCE file: Neither UPLOADINFO nor <E> found");
	return -1;
}

int freerdp_assistance_parse_file(rdpAssistanceFile* file, const char* name, const char* password)
{
	int status;
	BYTE* buffer;
	FILE* fp = NULL;
	size_t readSize;
	union
	{
		INT64 i64;
		size_t s;
	} fileSize;

	if (!name)
	{
		WLog_ERR(TAG, "ASSISTANCE file %s invalid name", name);
		return -1;
	}

	free(file->filename);
	file->filename = _strdup(name);
	fp = winpr_fopen(name, "r");

	if (!fp)
	{
		WLog_ERR(TAG, "Failed to open ASSISTANCE file %s ", name);
		return -1;
	}

	_fseeki64(fp, 0, SEEK_END);
	fileSize.i64 = _ftelli64(fp);
	_fseeki64(fp, 0, SEEK_SET);

	if (fileSize.i64 < 1)
	{
		WLog_ERR(TAG, "Failed to read ASSISTANCE file %s ", name);
		fclose(fp);
		return -1;
	}

	buffer = (BYTE*)malloc(fileSize.s + 2);

	if (!buffer)
	{
		fclose(fp);
		return -1;
	}

	readSize = fread(buffer, fileSize.s, 1, fp);

	if (!readSize)
	{
		if (!ferror(fp))
			readSize = fileSize.s;
	}

	fclose(fp);

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
	UINT32 i;

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

	if (!freerdp_settings_set_string(settings, FreeRDP_ServerHostname, file->MachineAddresses[0]))
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

	if (!freerdp_settings_set_uint32(settings, FreeRDP_ServerPort, file->MachinePorts[0]))
		return FALSE;

	if (!freerdp_settings_set_pointer_len(settings, FreeRDP_TargetNetAddresses, NULL,
	                                      file->MachineCount))
		return FALSE;
	if (!freerdp_settings_set_pointer_len(settings, FreeRDP_TargetNetPorts, file->MachinePorts,
	                                      file->MachineCount))
		return FALSE;

	for (i = 0; i < file->MachineCount; i++)
	{
		if (!freerdp_settings_set_pointer_array(settings, FreeRDP_TargetNetAddresses, i,
		                                        file->MachineAddresses[i]))
			return FALSE;
	}

	return TRUE;
}

rdpAssistanceFile* freerdp_assistance_file_new(void)
{
	winpr_InitializeSSL(WINPR_SSL_INIT_DEFAULT);
	return (rdpAssistanceFile*)calloc(1, sizeof(rdpAssistanceFile));
}

void freerdp_assistance_file_free(rdpAssistanceFile* file)
{
	UINT32 i;

	if (!file)
		return;

	free(file->filename);
	free(file->password);
	free(file->Username);
	free(file->LHTicket);
	free(file->RCTicket);
	free(file->PassStub);
	free(file->ConnectionString1);
	free(file->ConnectionString2);
	free(file->EncryptedLHTicket);
	free(file->RASessionId);
	free(file->RASpecificParams);
	free(file->EncryptedPassStub);

	for (i = 0; i < file->MachineCount; i++)
	{
		free(file->MachineAddresses[i]);
	}

	free(file->MachineAddresses);
	free(file->MachinePorts);
	free(file);
}

void freerdp_assistance_print_file(rdpAssistanceFile* file, wLog* log, DWORD level)
{
	size_t x;
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

	for (x = 0; x < file->MachineCount; x++)
	{
		WLog_Print(log, level, "MachineAddress [%" PRIdz ": %s", x, file->MachineAddresses[x]);
		WLog_Print(log, level, "MachinePort    [%" PRIdz ": %" PRIu32, x, file->MachinePorts[x]);
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

	free(file->ConnectionString2);
	free(file->password);
	file->ConnectionString2 = _strdup(string);
	file->password = _strdup(password);
	return freerdp_assistance_parse_connection_string2(file);
}
