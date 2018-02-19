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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>

#include <winpr/wtypes.h>
#include <winpr/crt.h>
#include <winpr/crypto.h>
#include <winpr/print.h>
#include <winpr/windows.h>

#include <freerdp/log.h>
#include <freerdp/client/file.h>
#include <freerdp/client/cmdline.h>

#include <freerdp/assistance.h>

#define TAG FREERDP_TAG("common")

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

int freerdp_assistance_crypt_derive_key_sha1(BYTE* hash, int hashLength, BYTE* key, int keyLength)
{
	int rc = -1;
	int i;
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

	buffer = (BYTE*) calloc(hashLength, 2);

	if (!buffer)
		goto fail;

	if (!winpr_Digest(WINPR_MD_SHA1, pad1, 64, buffer, hashLength))
		goto fail;

	if (!winpr_Digest(WINPR_MD_SHA1, pad2, 64, &buffer[hashLength], hashLength))
		goto fail;

	CopyMemory(key, buffer, keyLength);
	rc = 1;
fail:
	free(buffer);
	return rc;
}

int freerdp_assistance_parse_address_list(rdpAssistanceFile* file, char* list)
{
	int i;
	char* p;
	char* q;
	char* str;
	int count;
	int length;
	char** tokens;
	count = 1;
	str = _strdup(list);

	if (!str)
		return -1;

	length = strlen(str);

	for (i = 0; i < length; i++)
	{
		if (str[i] == ';')
			count++;
	}

	tokens = (char**) calloc(count, sizeof(char*));

	if (!tokens)
	{
		free(str);
		return -1;
	}

	count = 0;
	tokens[count++] = str;

	for (i = 0; i < length; i++)
	{
		if (str[i] == ';')
		{
			str[i] = '\0';
			tokens[count++] = &str[i + 1];
		}
	}

	file->MachineCount = count;
	file->MachineAddresses = (char**) calloc(count, sizeof(char*));
	file->MachinePorts = (UINT32*) calloc(count, sizeof(UINT32));

	if (!file->MachineAddresses || !file->MachinePorts)
		goto out;

	for (i = 0; i < count; i++)
	{
		p = tokens[i];
		q = strchr(p, ':');

		if (!q)
			goto out;

		q[0] = '\0';
		q++;
		file->MachineAddresses[i] = _strdup(p);
		errno = 0;
		{
			unsigned long val = strtoul(q, NULL, 0);

			if ((errno != 0) || (val > UINT32_MAX))
				goto out;

			file->MachinePorts[i] = val;
		}

		if (!file->MachineAddresses[i])
			goto out;

		q[-1] = ':';
	}

	for (i = 0; i < count; i++)
	{
		length = strlen(tokens[i]);

		if (length > 8)
		{
			if (strncmp(tokens[i], "169.254.", 8) == 0)
				continue;
		}

		p = tokens[i];
		q = strchr(p, ':');

		if (!q)
			goto out;

		q[0] = '\0';
		q++;

		if (file->MachineAddress)
			free(file->MachineAddress);

		file->MachineAddress = _strdup(p);

		if (!file->MachineAddress)
			goto out;

		errno = 0;
		{
			unsigned long val = strtoul(q, NULL, 0);

			if ((errno != 0) || (val > UINT32_MAX))
				goto out;

			file->MachinePort = val;
		}

		if (!file->MachineAddress)
			goto out;

		break;
	}

	free(tokens);
	free(str);
	return 1;
out:

	if (file->MachineAddresses)
	{
		for (i = 0; i < count; i++)
			free(file->MachineAddresses[i]);
	}

	free(file->MachineAddresses);
	free(file->MachinePorts);
	file->MachineCount = 0;
	file->MachinePorts = NULL;
	file->MachineAddresses = NULL;
	free(tokens);
	free(str);
	return -1;
}

int freerdp_assistance_parse_connection_string1(rdpAssistanceFile* file)
{
	int i;
	char* str;
	int count;
	int length;
	char* tokens[8];
	int ret = -1;
	/**
	 * <ProtocolVersion>,<protocolType>,<machineAddressList>,<assistantAccountPwd>,
	 * <RASessionID>,<RASessionName>,<RASessionPwd>,<protocolSpecificParms>
	 */
	count = 1;
	str = _strdup(file->RCTicket);

	if (!str)
		return -1;

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

	ret = freerdp_assistance_parse_address_list(file, tokens[2]);
error:
	free(str);

	if (ret != 1)
		return -1;

	return 1;
}

/**
 * Decrypted Connection String 2:
 *
 * <E>
 * <A KH="BNRjdu97DyczQSRuMRrDWoue+HA=" ID="+ULZ6ifjoCa6cGPMLQiGHRPwkg6VyJqGwxMnO6GcelwUh9a6/FBq3It5ADSndmLL"/>
 * <C>
 * <T ID="1" SID="0">
 * 	<L P="49228" N="fe80::1032:53d9:5a01:909b%3"/>
 * 	<L P="49229" N="fe80::3d8f:9b2d:6b4e:6aa%6"/>
 * 	<L P="49230" N="192.168.1.200"/>
 * 	<L P="49231" N="169.254.6.170"/>
 * </T>
 * </C>
 * </E>
 */

int freerdp_assistance_parse_connection_string2(rdpAssistanceFile* file)
{
	char* str;
	char* tag;
	char* end;
	char* p;
	int ret = -1;
	str = file->ConnectionString2;

	if (!strstr(str, "<E>"))
		return -1;

	if (!strstr(str, "<C>"))
		return -1;

	str = _strdup(file->ConnectionString2);

	if (!str)
		return -1;

	if (!(tag = strstr(str, "<A")))
		goto out_fail;

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
			goto out_fail;

		length = q - p;
		free(file->RASpecificParams);
		file->RASpecificParams = (char*) malloc(length + 1);

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
			goto out_fail;

		length = q - p;
		free(file->RASessionId);
		file->RASessionId = (char*) malloc(length + 1);

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
		char* q;
		int port;
		size_t length;
		p += sizeof("<L P=\"") - 1;
		q = strchr(p, '"');

		if (!q)
			goto out_fail;

		q[0] = '\0';
		q++;
		errno = 0;
		{
			unsigned long val = strtoul(p, NULL, 0);

			if ((errno != 0) || (val == 0) || (val > UINT16_MAX))
				goto out_fail;

			port = val;
		}
		p = strstr(q, " N=\"");

		if (!p)
			goto out_fail;

		p += sizeof(" N=\"") - 1;
		q = strchr(p, '"');

		if (!q)
			goto out_fail;

		q[0] = '\0';
		q++;
		length = strlen(p);

		if (length > 8)
		{
			if (strncmp(p, "169.254.", 8) != 0)
			{
				if (file->MachineAddress)
					free(file->MachineAddress);

				file->MachineAddress = _strdup(p);

				if (!file->MachineAddress)
					goto out_fail;

				file->MachinePort = (UINT32) port;
				break;
			}
		}

		p = strstr(q, "<L P=\"");
	}

	ret = 1;
out_fail:
	free(str);
	return ret;
}

char* freerdp_assistance_construct_expert_blob(const char* name, const char* pass)
{
	int size;
	int nameLength;
	int passLength;
	char* ExpertBlob = NULL;

	if (!name || !pass)
		return NULL;

	nameLength = strlen(name) + strlen("NAME=");
	passLength = strlen(pass) + strlen("PASS=");
	size = nameLength + passLength + 64;
	ExpertBlob = (char*) calloc(1, size);

	if (!ExpertBlob)
		return NULL;

	sprintf_s(ExpertBlob, size, "%d;NAME=%s%d;PASS=%s",
	          nameLength, name, passLength, pass);
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
	passStub = (char*) malloc(15);

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
	winpr_RAND((BYTE*) nums, sizeof(nums));
	passStub[0] = set1[nums[0] % sizeof(set1)]; /* character 0 */
	passStub[1] = set2[nums[1] % sizeof(set2)]; /* character 1 */
	passStub[2] = set3[nums[2] % sizeof(set3)]; /* character 2 */
	passStub[3] = set4[nums[3] % sizeof(set4)]; /* character 3 */
	passStub[4] = set5[nums[4] % sizeof(set5)]; /* character 4 */
	passStub[5] = set1[nums[5] % sizeof(set1)]; /* character 5 */
	passStub[6] = set1[nums[6] % sizeof(set1)]; /* character 6 */
	passStub[7] = set1[nums[7] % sizeof(set1)]; /* character 7 */
	passStub[8] = set1[nums[8] % sizeof(set1)]; /* character 8 */
	passStub[9] = set1[nums[9] % sizeof(set1)]; /* character 9 */
	passStub[10] = set1[nums[10] % sizeof(set1)]; /* character 10 */
	passStub[11] = set1[nums[11] % sizeof(set1)]; /* character 11 */
	passStub[12] = set1[nums[12] % sizeof(set1)]; /* character 12 */
	passStub[13] = set1[nums[13] % sizeof(set1)]; /* character 13 */
	passStub[14] = '\0';
	return passStub;
}

BYTE* freerdp_assistance_encrypt_pass_stub(const char* password, const char* passStub,
        int* pEncryptedSize)
{
	BOOL rc;
	int status;
	int cbPasswordW;
	int cbPassStubW;
	int EncryptedSize;
	BYTE PasswordHash[WINPR_MD5_DIGEST_LENGTH];
	WINPR_CIPHER_CTX* rc4Ctx;
	BYTE* pbIn, *pbOut;
	size_t cbOut, cbIn, cbFinal;
	WCHAR* PasswordW = NULL;
	WCHAR* PassStubW = NULL;
	status = ConvertToUnicode(CP_UTF8, 0, password, -1, &PasswordW, 0);

	if (status <= 0)
		return NULL;

	cbPasswordW = (status - 1) * 2;

	if (!winpr_Digest(WINPR_MD_MD5, (BYTE*)PasswordW, cbPasswordW, (BYTE*) PasswordHash,
	                  sizeof(PasswordHash)))
	{
		free(PasswordW);
		return NULL;
	}

	status = ConvertToUnicode(CP_UTF8, 0, passStub, -1, &PassStubW, 0);

	if (status <= 0)
	{
		free(PasswordW);
		return NULL;
	}

	cbPassStubW = (status - 1) * 2;
	EncryptedSize = cbPassStubW + 4;
	pbIn = (BYTE*) calloc(1, EncryptedSize);
	pbOut = (BYTE*) calloc(1, EncryptedSize);

	if (!pbIn || !pbOut)
	{
		free(PasswordW);
		free(PassStubW);
		free(pbIn);
		free(pbOut);
		return NULL;
	}

	if (!EncryptedSize)
	{
		free(PasswordW);
		free(PassStubW);
		free(pbIn);
		free(pbOut);
		return NULL;
	}

	*((UINT32*) pbIn) = cbPassStubW;
	CopyMemory(&pbIn[4], PassStubW, cbPassStubW);
	free(PasswordW);
	free(PassStubW);
	rc4Ctx = winpr_Cipher_New(WINPR_CIPHER_ARC4_128, WINPR_ENCRYPT,
	                          PasswordHash, NULL);

	if (!rc4Ctx)
	{
		WLog_ERR(TAG,  "EVP_CipherInit_ex failure");
		free(pbOut);
		free(pbIn);
		return NULL;
	}

	cbOut = cbFinal = 0;
	cbIn = EncryptedSize;
	rc = winpr_Cipher_Update(rc4Ctx, pbIn, cbIn, pbOut, &cbOut);
	free(pbIn);

	if (!rc)
	{
		WLog_ERR(TAG,  "EVP_CipherUpdate failure");
		winpr_Cipher_Free(rc4Ctx);
		free(pbOut);
		return NULL;
	}

	if (!winpr_Cipher_Final(rc4Ctx, pbOut + cbOut, &cbFinal))
	{
		WLog_ERR(TAG,  "EVP_CipherFinal_ex failure");
		winpr_Cipher_Free(rc4Ctx);
		free(pbOut);
		return NULL;
	}

	winpr_Cipher_Free(rc4Ctx);
	*pEncryptedSize = EncryptedSize;
	return pbOut;
}

int freerdp_assistance_decrypt2(rdpAssistanceFile* file, const char* password)
{
	int status;
	int cbPasswordW;
	int cchOutW = 0;
	WCHAR* pbOutW = NULL;
	WINPR_CIPHER_CTX* aesDec;
	WCHAR* PasswordW = NULL;
	BYTE* pbIn, *pbOut;
	size_t cbOut, cbIn, cbFinal;
	BYTE DerivedKey[WINPR_AES_BLOCK_SIZE];
	BYTE InitializationVector[WINPR_AES_BLOCK_SIZE];
	BYTE PasswordHash[WINPR_SHA1_DIGEST_LENGTH];
	status = ConvertToUnicode(CP_UTF8, 0, password, -1, &PasswordW, 0);

	if (status <= 0)
		return -1;

	cbPasswordW = (status - 1) * 2;

	if (!winpr_Digest(WINPR_MD_SHA1, (BYTE*)PasswordW, cbPasswordW, PasswordHash, sizeof(PasswordHash)))
	{
		free(PasswordW);
		return -1;
	}

	status = freerdp_assistance_crypt_derive_key_sha1(PasswordHash, sizeof(PasswordHash),
	         DerivedKey, sizeof(DerivedKey));

	if (status < 0)
	{
		free(PasswordW);
		return -1;
	}

	ZeroMemory(InitializationVector, sizeof(InitializationVector));
	aesDec = winpr_Cipher_New(WINPR_CIPHER_AES_128_CBC, WINPR_DECRYPT,
	                          DerivedKey, InitializationVector);

	if (!aesDec)
	{
		free(PasswordW);
		return -1;
	}

	cbOut = cbFinal = 0;
	cbIn = file->EncryptedLHTicketLength;
	pbIn = (BYTE*) file->EncryptedLHTicket;
	pbOut = (BYTE*) calloc(1, cbIn + WINPR_AES_BLOCK_SIZE + 2);

	if (!pbOut)
	{
		winpr_Cipher_Free(aesDec);
		free(PasswordW);
		return -1;
	}

	if (!winpr_Cipher_Update(aesDec, pbIn, cbIn, pbOut, &cbOut))
	{
		winpr_Cipher_Free(aesDec);
		free(PasswordW);
		free(pbOut);
		return -1;
	}

	if (!winpr_Cipher_Final(aesDec, pbOut + cbOut, &cbFinal))
	{
		WLog_ERR(TAG,  "EVP_DecryptFinal_ex failure");
		winpr_Cipher_Free(aesDec);
		free(PasswordW);
		free(pbOut);
		return -1;
	}

	winpr_Cipher_Free(aesDec);
	cbOut += cbFinal;
	cbFinal = 0;
	pbOutW = (WCHAR*) pbOut;
	cchOutW = cbOut / 2;
	file->ConnectionString2 = NULL;
	status = ConvertFromUnicode(CP_UTF8, 0, pbOutW, cchOutW, &file->ConnectionString2, 0, NULL, NULL);
	free(PasswordW);
	free(pbOut);

	if (status <= 0)
	{
		return -1;
	}

	status = freerdp_assistance_parse_connection_string2(file);
	WLog_DBG(TAG, "freerdp_assistance_parse_connection_string2: %d", status);
	return status;
}

int freerdp_assistance_decrypt(rdpAssistanceFile* file, const char* password)
{
	int status = 1;
	file->EncryptedPassStub = freerdp_assistance_encrypt_pass_stub(password,
	                          file->PassStub, &file->EncryptedPassStubLength);

	if (!file->EncryptedPassStub)
		return -1;

	if (file->Type > 1)
	{
		status = freerdp_assistance_decrypt2(file, password);
	}

	return status;
}

BYTE* freerdp_assistance_hex_string_to_bin(const char* str, int* size)
{
	char c;
	int length;
	BYTE* buffer;
	int i, ln, hn;
	length = strlen(str);

	if ((length % 2) != 0)
		return NULL;

	length /= 2;
	*size = length;
	buffer = (BYTE*) malloc(length);

	if (!buffer)
		return NULL;

	for (i = 0; i < length; i++)
	{
		hn = ln = 0;
		c = str[(i * 2) + 0];

		if ((c >= '0') && (c <= '9'))
			hn = c - '0';
		else if ((c >= 'a') && (c <= 'f'))
			hn = (c - 'a') + 10;
		else if ((c >= 'A') && (c <= 'F'))
			hn = (c - 'A') + 10;

		c = str[(i * 2) + 1];

		if ((c >= '0') && (c <= '9'))
			ln = c - '0';
		else if ((c >= 'a') && (c <= 'f'))
			ln = (c - 'a') + 10;
		else if ((c >= 'A') && (c <= 'F'))
			ln = (c - 'A') + 10;

		buffer[i] = (hn << 4) | ln;
	}

	return buffer;
}

char* freerdp_assistance_bin_to_hex_string(const BYTE* data, int size)
{
	int i;
	char* p;
	int ln, hn;
	char bin2hex[] = "0123456789ABCDEF";
	p = (char*) calloc((size + 1), 2);

	if (!p)
		return NULL;

	for (i = 0; i < size; i++)
	{
		ln = data[i] & 0xF;
		hn = (data[i] >> 4) & 0xF;
		p[i * 2] = bin2hex[hn];
		p[(i * 2) + 1] = bin2hex[ln];
	}

	p[size * 2] = '\0';
	return p;
}

int freerdp_assistance_parse_file_buffer(rdpAssistanceFile* file, const char* buffer, size_t size)
{
	char* p;
	char* q;
	char* r;
	int status;
	size_t length;
	p = strstr(buffer, "UPLOADINFO");

	if (!p)
		return -1;

	p = strstr(p + sizeof("UPLOADINFO") - 1, "TYPE=\"");

	if (!p)
		return -1;

	p = strstr(buffer, "UPLOADDATA");

	if (!p)
		return -1;

	/* Parse USERNAME */
	p = strstr(buffer, "USERNAME=\"");

	if (p)
	{
		p += sizeof("USERNAME=\"") - 1;
		q = strchr(p, '"');

		if (!q)
			return -1;

		length = q - p;
		file->Username = (char*) malloc(length + 1);

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
			return -1;

		length = q - p;
		file->LHTicket = (char*) malloc(length + 1);

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
			return -1;

		length = q - p;
		file->RCTicket = (char*) malloc(length + 1);

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
			return -1;

		length = q - p;

		if ((length == 1) && (p[0] == '1'))
			file->RCTicketEncrypted = TRUE;
	}

	/* Parse PassStub */
	p = strstr(buffer, "PassStub=\"");

	if (p)
	{
		p += sizeof("PassStub=\"") - 1;
		q = strchr(p, '"');

		if (!q)
			return -1;

		length = q - p;
		file->PassStub = (char*) malloc(length + 1);

		if (!file->PassStub)
			return -1;

		CopyMemory(file->PassStub, p, length);
		file->PassStub[length] = '\0';
	}

	/* Parse DtStart */
	p = strstr(buffer, "DtStart=\"");

	if (p)
	{
		p += sizeof("DtStart=\"") - 1;
		q = strchr(p, '"');

		if (!q)
			return -1;

		length = q - p;
		r = (char*) malloc(length + 1);

		if (!r)
			return -1;

		CopyMemory(r, p, length);
		r[length] = '\0';
		errno = 0;
		{
			unsigned long val = strtoul(r, NULL, 0);
			free(r);

			if ((errno != 0) || (val > UINT32_MAX))
				return -1;

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
			return -1;

		length = q - p;
		r = (char*) malloc(length + 1);

		if (!r)
			return -1;

		CopyMemory(r, p, length);
		r[length] = '\0';
		errno = 0;
		{
			unsigned long val = strtoul(r, NULL, 0);
			free(r);

			if ((errno != 0) || (val > UINT32_MAX))
				return -1;

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
			return -1;

		length = q - p;

		if ((length == 1) && (p[0] == '1'))
			file->LowSpeed = TRUE;
	}

	file->Type = (file->LHTicket) ? 2 : 1;

	if (file->LHTicket)
	{
		file->EncryptedLHTicket = freerdp_assistance_hex_string_to_bin(file->LHTicket,
		                          &file->EncryptedLHTicketLength);
	}

	status = freerdp_assistance_parse_connection_string1(file);

	if (status < 0)
	{
		WLog_ERR(TAG,  "freerdp_assistance_parse_connection_string1 failure: %d", status);
		return -1;
	}

	return 1;
}

int freerdp_assistance_parse_file(rdpAssistanceFile* file, const char* name)
{
	int status;
	BYTE* buffer;
	FILE* fp = NULL;
	size_t readSize;
	INT64 fileSize;

	if (!name)
		return -1;

	fp = fopen(name, "r");

	if (!fp)
		return -1;

	_fseeki64(fp, 0, SEEK_END);
	fileSize = _ftelli64(fp);
	_fseeki64(fp, 0, SEEK_SET);

	if (fileSize < 1)
	{
		fclose(fp);
		return -1;
	}

	buffer = (BYTE*) malloc(fileSize + 2);

	if (!buffer)
	{
		fclose(fp);
		return -1;
	}

	readSize = fread(buffer, fileSize, 1, fp);

	if (!readSize)
	{
		if (!ferror(fp))
			readSize = fileSize;
	}

	fclose(fp);

	if (readSize < 1)
	{
		free(buffer);
		buffer = NULL;
		return -1;
	}

	buffer[fileSize] = '\0';
	buffer[fileSize + 1] = '\0';
	status = freerdp_assistance_parse_file_buffer(file, (char*) buffer, fileSize);
	free(buffer);
	return status;
}

int freerdp_client_populate_settings_from_assistance_file(rdpAssistanceFile* file,
        rdpSettings* settings)
{
	UINT32 i;
	freerdp_set_param_bool(settings, FreeRDP_RemoteAssistanceMode, TRUE);

	if (!file->RASessionId || !file->MachineAddress)
		return -1;

	if (freerdp_set_param_string(settings, FreeRDP_RemoteAssistanceSessionId, file->RASessionId) != 0)
		return -1;

	if (file->RCTicket &&
	    (freerdp_set_param_string(settings, FreeRDP_RemoteAssistanceRCTicket, file->RCTicket) != 0))
		return -1;

	if (file->PassStub &&
	    (freerdp_set_param_string(settings, FreeRDP_RemoteAssistancePassStub, file->PassStub) != 0))
		return -1;

	if (freerdp_set_param_string(settings, FreeRDP_ServerHostname, file->MachineAddress) != 0)
		return -1;

	freerdp_set_param_uint32(settings, FreeRDP_ServerPort, file->MachinePort);
	freerdp_target_net_addresses_free(settings);
	settings->TargetNetAddressCount = file->MachineCount;

	if (settings->TargetNetAddressCount)
	{
		settings->TargetNetAddresses = (char**) calloc(file->MachineCount, sizeof(char*));
		settings->TargetNetPorts = (UINT32*) calloc(file->MachineCount, sizeof(UINT32));

		if (!settings->TargetNetAddresses || !settings->TargetNetPorts)
			return -1;

		for (i = 0; i < settings->TargetNetAddressCount; i++)
		{
			settings->TargetNetAddresses[i] = _strdup(file->MachineAddresses[i]);
			settings->TargetNetPorts[i] = file->MachinePorts[i];

			if (!settings->TargetNetAddresses[i])
				return -1;
		}
	}

	return 1;
}

rdpAssistanceFile* freerdp_assistance_file_new(void)
{
	return (rdpAssistanceFile*) calloc(1, sizeof(rdpAssistanceFile));
}

void freerdp_assistance_file_free(rdpAssistanceFile* file)
{
	UINT32 i;

	if (!file)
		return;

	free(file->Username);
	free(file->LHTicket);
	free(file->RCTicket);
	free(file->PassStub);
	free(file->ConnectionString1);
	free(file->ConnectionString2);
	free(file->EncryptedLHTicket);
	free(file->RASessionId);
	free(file->RASpecificParams);
	free(file->MachineAddress);
	free(file->EncryptedPassStub);

	for (i = 0; i < file->MachineCount; i++)
	{
		free(file->MachineAddresses[i]);
	}

	free(file->MachineAddresses);
	free(file->MachinePorts);
	free(file);
}
