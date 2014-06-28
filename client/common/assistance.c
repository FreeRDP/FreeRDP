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

#include <winpr/crt.h>
#include <winpr/windows.h>

#include <openssl/ssl.h>
#include <openssl/md5.h>
#include <openssl/rc4.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/rand.h>
#include <openssl/engine.h>

#include <freerdp/client/file.h>
#include <freerdp/client/cmdline.h>

#include <freerdp/client/assistance.h>

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

int freerdp_client_assistance_crypt_derive_key(BYTE* hash, int hashLength, BYTE* key, int keyLength)
{
	int i;
	BYTE* bufferHash;
	BYTE buffer36[64];
	BYTE buffer5c[64];

	memset(buffer36, 0x36, sizeof(buffer36));
	memset(buffer5c, 0x5C, sizeof(buffer5c));

	for (i = 0; i < hashLength; i++)
	{
		buffer36[i] ^= hash[i];
		buffer5c[i] ^= hash[i];
	}

	bufferHash = (BYTE*) calloc(1, hashLength * 2);

	if (!bufferHash)
		return -1;

	SHA1(buffer36, 64, bufferHash);
	SHA1(buffer5c, 64, &bufferHash[hashLength]);

	CopyMemory(key, bufferHash, keyLength);

	free(bufferHash);

	return 1;
}

int freerdp_client_assistance_decrypt(rdpAssistanceFile* file, const char* password)
{
	int status;
	SHA_CTX shaCtx;
	int cbOut, cbFinal;
	EVP_CIPHER_CTX aesDec;
	WCHAR* PasswordW = NULL;
	BYTE EncryptionKey[AES_BLOCK_SIZE];
	BYTE PasswordHash[SHA_DIGEST_LENGTH];

	status = ConvertToUnicode(CP_UTF8, 0, password, -1, &PasswordW, 0);

	if (status <= 0)
		return -1;

	SHA_Init(&shaCtx);
	SHA_Update(&shaCtx, PasswordW, status * 2);
	SHA_Final((void*) PasswordHash, &shaCtx);

	status = freerdp_client_assistance_crypt_derive_key(PasswordHash, sizeof(PasswordHash),
			EncryptionKey, sizeof(EncryptionKey));

	if (status < 0)
		return -1;

	EVP_CIPHER_CTX_init(&aesDec);

	status = EVP_DecryptInit(&aesDec, EVP_aes_128_cbc(), EncryptionKey, NULL);

	if (status != 1)
		return -1;

	cbOut = file->EncryptedConnectionStringLength;
	file->ConnectionString = (char*) calloc(1, cbOut);

	if (!file->ConnectionString)
		return -1;

	status = EVP_DecryptUpdate(&aesDec, (BYTE*) file->ConnectionString, &cbOut,
			(BYTE*) file->EncryptedConnectionString, file->EncryptedConnectionStringLength);
	if (status != 1)
		return -1;

	status = EVP_DecryptFinal(&aesDec, (BYTE*) &file->ConnectionString[cbOut], &cbFinal);

	/* FIXME: still fails */

	if (status != 1)
		return -1;

	return 1;
}

BYTE* freerdp_client_assistance_parse_hex_string(const char* hexStr, int* size)
{
	char c;
	int length;
	BYTE* buffer;
	int i, ln, hn;

	length = strlen(hexStr);

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

		c = hexStr[(i * 2) + 0];

		if ((c >= '0') && (c <= '9'))
			hn = c - '0';
		else if ((c >= 'a') && (c <= 'f'))
			hn = (c - 'a') + 10;
		else if ((c >= 'A') && (c <= 'F'))
			hn = (c - 'A') + 10;

		c = hexStr[(i * 2) + 1];

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

int freerdp_client_assistance_parse_file_buffer(rdpAssistanceFile* file, const char* buffer, size_t size)
{
	char* p;
	char* q;
	char* r;
	int value;
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

		value = atoi(r);
		free(r);

		if (value < 0)
			return -1;

		file->DtStart = (UINT32) value;
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

		value = atoi(r);
		free(r);

		if (value < 0)
			return -1;

		file->DtLength = (UINT32) value;
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

	file->EncryptedConnectionString = freerdp_client_assistance_parse_hex_string(file->LHTicket,
			&file->EncryptedConnectionStringLength);

	return 1;
}

rdpAssistanceFile* freerdp_client_assistance_file_new()
{
	rdpAssistanceFile* file;

	file = (rdpAssistanceFile*) calloc(1, sizeof(rdpAssistanceFile));

	if (file)
	{

	}

	return file;
}

void freerdp_client_assistance_file_free(rdpAssistanceFile* file)
{
	if (!file)
		return;

	free(file->Username);
	free(file->LHTicket);
	free(file->RCTicket);
	free(file->PassStub);
	free(file->ConnectionString);
	free(file->EncryptedConnectionString);

	free(file);
}
