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

int freerdp_client_assistance_decrypt1(rdpAssistanceFile* file, const char* password)
{
	int status;
	RC4_KEY rc4;
	MD5_CTX md5Ctx;
	int PassStubLength;
	BYTE* PlainBlob = NULL;
	WCHAR* PasswordW = NULL;
	BYTE EncryptionKey[AES_BLOCK_SIZE];
	BYTE PasswordHash[MD5_DIGEST_LENGTH];

	status = ConvertToUnicode(CP_UTF8, 0, password, -1, &PasswordW, 0);

	if (status <= 0)
		return -1;

	MD5_Init(&md5Ctx);
	MD5_Update(&md5Ctx, PasswordW, status * 2);
	MD5_Final((void*) PasswordHash, &md5Ctx);

	status = freerdp_client_assistance_crypt_derive_key(PasswordHash, sizeof(PasswordHash),
			EncryptionKey, sizeof(EncryptionKey));

	if (status < 0)
		return -1;

	PassStubLength = strlen(file->PassStub);
	file->EncryptedPassStubLength = PassStubLength + 4;

	PlainBlob = (BYTE*) calloc(1, file->EncryptedPassStubLength);
	file->EncryptedPassStub = (BYTE*) calloc(1, file->EncryptedPassStubLength);

	if (!PlainBlob)
		return -1;

	if (!file->EncryptedPassStubLength)
		return -1;

	*((UINT32*) PlainBlob) = PassStubLength;
	CopyMemory(&PlainBlob[4], file->PassStub, PassStubLength);

	RC4_set_key(&rc4, sizeof(EncryptionKey), EncryptionKey);
	RC4(&rc4, file->EncryptedPassStubLength, PlainBlob, file->EncryptedPassStub);

	return 1;
}

int freerdp_client_assistance_decrypt2(rdpAssistanceFile* file, const char* password)
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

	cbOut = file->EncryptedLHTicketLength;
	file->ConnectionString2 = (char*) calloc(1, cbOut);

	if (!file->ConnectionString2)
		return -1;

	status = EVP_DecryptUpdate(&aesDec, (BYTE*) file->ConnectionString2, &cbOut,
			(BYTE*) file->EncryptedLHTicket, file->EncryptedLHTicketLength);
	if (status != 1)
		return -1;

	status = EVP_DecryptFinal(&aesDec, (BYTE*) &file->ConnectionString2[cbOut], &cbFinal);

	/* FIXME: still fails */

	if (status != 1)
		return -1;

	return 1;
}

int freerdp_client_assistance_decrypt(rdpAssistanceFile* file, const char* password)
{
	int status;

	status = freerdp_client_assistance_decrypt1(file, password);

	freerdp_client_assistance_decrypt2(file, password);

	return status;
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

int freerdp_client_assistance_parse_connection_string1(rdpAssistanceFile* file)
{
	int i;
	char* p;
	char* q;
	char* str;
	int count;
	int length;
	char* list;
	char* tokens[8];

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
		return -1;

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
		return -1;

	if (strcmp(tokens[1], "1") != 0)
		return -1;

	if (strcmp(tokens[3], "*") != 0)
		return -1;

	if (strcmp(tokens[5], "*") != 0)
		return -1;

	if (strcmp(tokens[6], "*") != 0)
		return -1;

	file->RASessionId = _strdup(tokens[4]);

	if (!file->RASessionId)
		return -1;

	file->RASpecificParams = _strdup(tokens[7]);

	if (!file->RASpecificParams)
		return -1;

	list = tokens[2];

	q = strchr(list, ';');

	if (q)
		q[0] = '\0';

	p = list;

	q = strchr(p, ':');

	if (!q)
		return -1;

	q[0] = '\0';
	q++;

	file->MachineAddress = _strdup(p);
	file->MachinePort = (UINT32) atoi(q);

	free(str);

	return 1;
}

int freerdp_client_assistance_parse_file_buffer(rdpAssistanceFile* file, const char* buffer, size_t size)
{
	char* p;
	char* q;
	char* r;
	int value;
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

	file->EncryptedLHTicket = freerdp_client_assistance_parse_hex_string(file->LHTicket,
			&file->EncryptedLHTicketLength);

	status = freerdp_client_assistance_parse_connection_string1(file);

	if (status < 0)
	{
		fprintf(stderr, "freerdp_client_assistance_parse_connection_string1 failure: %d\n", status);
		return -1;
	}

	return 1;
}

int freerdp_client_assistance_parse_file(rdpAssistanceFile* file, const char* name)
{
	int status;
	BYTE* buffer;
	FILE* fp = NULL;
	size_t readSize;
	long int fileSize;

	fp = fopen(name, "r");

	if (!fp)
		return -1;

	fseek(fp, 0, SEEK_END);
	fileSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	if (fileSize < 1)
	{
		fclose(fp);
		return -1;
	}

	buffer = (BYTE*) malloc(fileSize + 2);
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

	status = freerdp_client_assistance_parse_file_buffer(file, (char*) buffer, fileSize);

	free(buffer);

	return status;
}

int freerdp_client_populate_settings_from_assistance_file(rdpAssistanceFile* file, rdpSettings* settings)
{
	freerdp_set_param_bool(settings, FreeRDP_RemoteAssistanceMode, TRUE);

	if (!file->RASessionId)
		return -1;

	freerdp_set_param_string(settings, FreeRDP_RemoteAssistanceSessionId, file->RASessionId);

	if (!file->MachineAddress)
		return -1;

	freerdp_set_param_string(settings, FreeRDP_ServerHostname, file->MachineAddress);
	freerdp_set_param_uint32(settings, FreeRDP_ServerPort, file->MachinePort);

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
	free(file->ConnectionString2);
	free(file->EncryptedLHTicket);

	free(file);
}
