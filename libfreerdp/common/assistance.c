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
#include <winpr/print.h>
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

#include <freerdp/assistance.h>

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
	int i;
	BYTE* buffer;
	BYTE pad1[64];
	BYTE pad2[64];
	SHA_CTX hashCtx;

	memset(pad1, 0x36, 64);
	memset(pad2, 0x5C, 64);

	for (i = 0; i < hashLength; i++)
	{
		pad1[i] ^= hash[i];
		pad2[i] ^= hash[i];
	}

	buffer = (BYTE*) calloc(1, hashLength * 2);

	if (!buffer)
		return -1;

	SHA1_Init(&hashCtx);
	SHA1_Update(&hashCtx, pad1, 64);
	SHA1_Final((void*) buffer, &hashCtx);

	SHA1_Init(&hashCtx);
	SHA1_Update(&hashCtx, pad2, 64);
	SHA1_Final((void*) &buffer[hashLength], &hashCtx);

	CopyMemory(key, buffer, keyLength);

	free(buffer);

	return 1;
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

	tokens = (char**) malloc(sizeof(char*) * count);

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
			return -1;

		q[0] = '\0';
		q++;

		file->MachineAddress = _strdup(p);
		file->MachinePort = (UINT32) atoi(q);

		break;
	}

	return 1;
}

int freerdp_assistance_parse_connection_string1(rdpAssistanceFile* file)
{
	int i;
	char* str;
	int count;
	int length;
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

	freerdp_assistance_parse_address_list(file, tokens[2]);

	free(str);

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
	char* p;
	char* q;
	char* str;
	size_t length;

	str = _strdup(file->ConnectionString2);

	if (!str)
		return -1;

	p = strstr(str, "<E>");

	if (!p)
		return -1;

	p = strstr(str, "<C>");

	if (!p)
		return -1;

	/* Auth String Node (<A>) */

	p = strstr(str, "<A");

	if (!p)
		return -1;

	p = strstr(p, "KH=\"");

	if (p)
	{
		p += sizeof("KH=\"") - 1;
		q = strchr(p, '"');

		if (!q)
			return -1;

		length = q - p;
		free(file->RASpecificParams);
		file->RASpecificParams = (char*) malloc(length + 1);

		if (!file->RASpecificParams)
			return -1;

		CopyMemory(file->RASpecificParams, p, length);
		file->RASpecificParams[length] = '\0';

		p += length;
	}

	p = strstr(p, "ID=\"");

	if (p)
	{
		p += sizeof("ID=\"") - 1;
		q = strchr(p, '"');

		if (!q)
			return -1;

		length = q - p;
		free(file->RASessionId);
		file->RASessionId = (char*) malloc(length + 1);

		if (!file->RASessionId)
			return -1;

		CopyMemory(file->RASessionId, p, length);
		file->RASessionId[length] = '\0';

		p += length;
	}

	free(str);

	return 1;
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

BYTE* freerdp_assistance_encrypt_pass_stub(const char* password, const char* passStub, int* pEncryptedSize)
{
	int status;
	MD5_CTX md5Ctx;
	int cbPasswordW;
	int cbPassStubW;
	int EncryptedSize;
	BYTE PasswordHash[16];
	EVP_CIPHER_CTX rc4Ctx;
	BYTE *pbIn, *pbOut;
	int cbOut, cbIn, cbFinal;
	WCHAR* PasswordW = NULL;
	WCHAR* PassStubW = NULL;

	status = ConvertToUnicode(CP_UTF8, 0, password, -1, &PasswordW, 0);

	if (status <= 0)
		return NULL;

	cbPasswordW = (status - 1) * 2;

	MD5_Init(&md5Ctx);
	MD5_Update(&md5Ctx, PasswordW, cbPasswordW);
	MD5_Final((void*) PasswordHash, &md5Ctx);

	status = ConvertToUnicode(CP_UTF8, 0, passStub, -1, &PassStubW, 0);

	if (status <= 0)
		return NULL;

	cbPassStubW = (status - 1) * 2;

	EncryptedSize = cbPassStubW + 4;

	pbIn = (BYTE*) calloc(1, EncryptedSize);
	pbOut = (BYTE*) calloc(1, EncryptedSize);

	if (!pbIn)
		return NULL;

	if (!EncryptedSize)
		return NULL;

	*((UINT32*) pbIn) = cbPassStubW;
	CopyMemory(&pbIn[4], PassStubW, cbPassStubW);

	EVP_CIPHER_CTX_init(&rc4Ctx);

	status = EVP_EncryptInit_ex(&rc4Ctx, EVP_rc4(), NULL, NULL, NULL);

	if (!status)
	{
		fprintf(stderr, "EVP_CipherInit_ex failure\n");
		return NULL;
	}

	status = EVP_EncryptInit_ex(&rc4Ctx, NULL, NULL, PasswordHash, NULL);

	if (!status)
	{
		fprintf(stderr, "EVP_CipherInit_ex failure\n");
		return NULL;
	}

	cbOut = cbFinal = 0;
	cbIn = EncryptedSize;

	status = EVP_EncryptUpdate(&rc4Ctx, pbOut, &cbOut, pbIn, cbIn);

	if (!status)
	{
		fprintf(stderr, "EVP_CipherUpdate failure\n");
		return NULL;
	}

	status = EVP_EncryptFinal_ex(&rc4Ctx, pbOut + cbOut, &cbFinal);

	if (!status)
	{
		fprintf(stderr, "EVP_CipherFinal_ex failure\n");
		return NULL;
	}

	EVP_CIPHER_CTX_cleanup(&rc4Ctx);

	free(pbIn);
	free(PasswordW);
	free(PassStubW);

	*pEncryptedSize = EncryptedSize;

	return pbOut;
}

int freerdp_assistance_decrypt2(rdpAssistanceFile* file, const char* password)
{
	int status;
	SHA_CTX shaCtx;
	int cbPasswordW;
	int cchOutW = 0;
	WCHAR* pbOutW = NULL;
	EVP_CIPHER_CTX aesDec;
	WCHAR* PasswordW = NULL;
	BYTE *pbIn, *pbOut;
	int cbOut, cbIn, cbFinal;
	BYTE DerivedKey[AES_BLOCK_SIZE];
	BYTE InitializationVector[AES_BLOCK_SIZE];
	BYTE PasswordHash[SHA_DIGEST_LENGTH];

	status = ConvertToUnicode(CP_UTF8, 0, password, -1, &PasswordW, 0);

	if (status <= 0)
		return -1;

	cbPasswordW = (status - 1) * 2;

	SHA1_Init(&shaCtx);
	SHA1_Update(&shaCtx, PasswordW, cbPasswordW);
	SHA1_Final((void*) PasswordHash, &shaCtx);

	status = freerdp_assistance_crypt_derive_key_sha1(PasswordHash, sizeof(PasswordHash),
			DerivedKey, sizeof(DerivedKey));

	if (status < 0)
		return -1;

	ZeroMemory(InitializationVector, sizeof(InitializationVector));

	EVP_CIPHER_CTX_init(&aesDec);

	status = EVP_DecryptInit_ex(&aesDec, EVP_aes_128_cbc(), NULL, NULL, NULL);

	if (status != 1)
		return -1;

	EVP_CIPHER_CTX_set_key_length(&aesDec, (128 / 8));
	EVP_CIPHER_CTX_set_padding(&aesDec, 0);

	status = EVP_DecryptInit_ex(&aesDec, EVP_aes_128_cbc(), NULL, DerivedKey, InitializationVector);

	if (status != 1)
		return -1;

	cbOut = cbFinal = 0;
	cbIn = file->EncryptedLHTicketLength;
	pbIn = (BYTE*) file->EncryptedLHTicket;
	pbOut = (BYTE*) calloc(1, cbIn + AES_BLOCK_SIZE + 2);

	if (!pbOut)
		return -1;

	status = EVP_DecryptUpdate(&aesDec, pbOut, &cbOut, pbIn, cbIn);

	if (status != 1)
		return -1;

	status = EVP_DecryptFinal_ex(&aesDec, pbOut + cbOut, &cbFinal);

	if (status != 1)
	{
		fprintf(stderr, "EVP_DecryptFinal_ex failure\n");
		return -1;
	}

	EVP_CIPHER_CTX_cleanup(&aesDec);

	cbOut += cbFinal;
	cbFinal = 0;

	pbOutW = (WCHAR*) pbOut;
	cchOutW = cbOut / 2;

	file->ConnectionString2 = NULL;
	status = ConvertFromUnicode(CP_UTF8, 0, pbOutW, cchOutW, &file->ConnectionString2, 0, NULL, NULL);

	if (status <= 0)
		return -1;

	free(PasswordW);
	free(pbOut);

	status = freerdp_assistance_parse_connection_string2(file);

	printf("freerdp_assistance_parse_connection_string2: %d\n", status);

	return 1;
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

	p = (char*) malloc((size + 1) * 2);

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

	file->Type = (file->LHTicket) ? 2 : 1;

	if (file->LHTicket)
	{
		file->EncryptedLHTicket = freerdp_assistance_hex_string_to_bin(file->LHTicket,
				&file->EncryptedLHTicketLength);
	}

	status = freerdp_assistance_parse_connection_string1(file);

	if (status < 0)
	{
		fprintf(stderr, "freerdp_assistance_parse_connection_string1 failure: %d\n", status);
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

	status = freerdp_assistance_parse_file_buffer(file, (char*) buffer, fileSize);

	free(buffer);

	return status;
}

int freerdp_client_populate_settings_from_assistance_file(rdpAssistanceFile* file, rdpSettings* settings)
{
	freerdp_set_param_bool(settings, FreeRDP_RemoteAssistanceMode, TRUE);

	if (!file->RASessionId)
		return -1;

	freerdp_set_param_string(settings, FreeRDP_RemoteAssistanceSessionId, file->RASessionId);

	if (file->RCTicket)
		freerdp_set_param_string(settings, FreeRDP_RemoteAssistanceRCTicket, file->RCTicket);

	if (file->PassStub)
		freerdp_set_param_string(settings, FreeRDP_RemoteAssistancePassStub, file->PassStub);

	if (!file->MachineAddress)
		return -1;

	freerdp_set_param_string(settings, FreeRDP_ServerHostname, file->MachineAddress);
	freerdp_set_param_uint32(settings, FreeRDP_ServerPort, file->MachinePort);

	return 1;
}

rdpAssistanceFile* freerdp_assistance_file_new()
{
	rdpAssistanceFile* file;

	file = (rdpAssistanceFile*) calloc(1, sizeof(rdpAssistanceFile));

	if (file)
	{

	}

	return file;
}

void freerdp_assistance_file_free(rdpAssistanceFile* file)
{
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

	free(file);
}
