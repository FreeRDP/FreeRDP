/**
* FreeRDP: A Remote Desktop Protocol Implementation
* TSCredentials reading and writing.
*
* Copyright 2010-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
* Copyright 2015 Thincast Technologies GmbH
* Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
* Copyright 2016 Martin Fleisz <martin.fleisz@thincast.com>
* Copyright 2017 Dorian Ducournau <dorian.ducournau@gmail.com>
* Copyright 2017 Pascal J. Bourguignon <pjb@informatimago.com>
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*		 http://www.apache.org/licenses/LICENSE-2.0
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

#include <time.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#include <freerdp/log.h>
#include <freerdp/crypto/tls.h>
#include <freerdp/build-config.h>
#include <freerdp/peer.h>

#include <winpr/crt.h>
#include <winpr/sam.h>
#include <winpr/sspi.h>
#include <winpr/print.h>
#include <winpr/tchar.h>
#include <winpr/dsparse.h>
#include <winpr/strlst.h>
#include <winpr/library.h>
#include <winpr/registry.h>

#include "tscredentials.h"

#define TAG FREERDP_TAG("core.tscredentials")

static void memory_clear_and_free(void* memory, size_t size)
{
	if (memory)
	{
		memset(memory, 0, size);
		free(memory);
	}
}

static void string_clear_and_free(char* string)
{
	memory_clear_and_free(string, strlen(string));
}

static void* memdup(void* source, size_t size)
{
	void* destination = malloc(size);

	if (destination != NULL)
	{
		memcpy(destination, source, size);
	}

	return destination;
}

/**
* TSCredentials ::= SEQUENCE {
* 	cred_type    [0] INTEGER,
* 	credentials [1] OCTET STRING
* }
*
* TSPasswordCreds ::= SEQUENCE {
* 	domainName  [0] OCTET STRING,
* 	userName    [1] OCTET STRING,
* 	password    [2] OCTET STRING
* }
*
* TSSmartCardCreds ::= SEQUENCE {
* 	pin        [0] OCTET STRING,
* 	cspData    [1] TSCspDataDetail,
* 	userHint   [2] OCTET STRING OPTIONAL,
* 	domainHint [3] OCTET STRING OPTIONAL
* }
*
* TSCspDataDetail ::= SEQUENCE {
* 	keySpec       [0] INTEGER,
* 	cardName      [1] OCTET STRING OPTIONAL,
* 	readerName    [2] OCTET STRING OPTIONAL,
* 	containerName [3] OCTET STRING OPTIONAL,
* 	cspName       [4] OCTET STRING OPTIONAL
* }
*
*
* TSRemoteGuardCreds ::= SEQUENCE {
*     logonCred         [0] TSRemoteGuardPackageCred,
*     supplementalCreds [1] SEQUENCE OF TSRemoteGuardPackageCred OPTIONAL,
* }
*
* TSRemoteGuardPackageCred ::=  SEQUENCE {
*     packageName [0] OCTET STRING,
*     credBuffer  [1] OCTET STRING,
* }
*
*/

#define CHECK_MEMORY(pointer, error_action)                             \
	do								\
	{								\
		if (!(pointer))						\
		{							\
			WLog_ERR(TAG, "%s:%d: out of memory",		\
			         __FUNCTION__, __LINE__);		\
			error_action;					\
		}							\
	}while (0)


/* ============================================================ */

/* SEC_WINNT_AUTH_IDENTITY contains only UTF-16 strings,  with length fields. */
#define WSTRING_LENGTH_CLEAR_AND_FREE(structure, field)				\
	memory_clear_and_free(structure->field, structure->field##Length * 2)
#define WSTRING_LENGTH_SET_CSTRING(structure, field, cstring)				\
	(structure->field##Length = (cstring						\
	                             ?ConvertToUnicode(CP_UTF8, 0, cstring, -1, &(structure->field), 0)	\
	                             :(structure->field = NULL, 0)))
#define WSTRING_LENGTH_COPY(source, destination, field)				\
	((destination->field##Length = source->field##Length),			\
	 (destination->field = ((source->field == NULL)			\
	                        ?NULL							\
	                        :memdup(source->field, source->field##Length))))

SEC_WINNT_AUTH_IDENTITY* SEC_WINNT_AUTH_IDENTITY_new(char* user,  char* password,
        char* domain)
{
	SEC_WINNT_AUTH_IDENTITY* password_creds;
	CHECK_MEMORY(password_creds = malloc(sizeof(*password_creds)), return NULL);
	password_creds->Flags = SEC_WINNT_AUTH_IDENTITY_UNICODE;
	WSTRING_LENGTH_SET_CSTRING(password_creds, User, user);
	WSTRING_LENGTH_SET_CSTRING(password_creds, Domain, domain);
	WSTRING_LENGTH_SET_CSTRING(password_creds, Password, password);
	return password_creds;
}

SEC_WINNT_AUTH_IDENTITY* SEC_WINNT_AUTH_IDENTITY_deepcopy(SEC_WINNT_AUTH_IDENTITY* original)
{
	SEC_WINNT_AUTH_IDENTITY* copy;
	CHECK_MEMORY(copy = calloc(1, sizeof(*copy)), return NULL);
	copy->Flags = SEC_WINNT_AUTH_IDENTITY_UNICODE;
	WSTRING_LENGTH_COPY(original, copy, User);
	WSTRING_LENGTH_COPY(original, copy, Domain);
	WSTRING_LENGTH_COPY(original, copy, Password);

	if (((original->User != NULL) && (copy->User == NULL))
	    || ((original->Password != NULL) && (copy->Password == NULL))
	    || ((original->Domain != NULL) && (copy->Domain == NULL)))
	{
		SEC_WINNT_AUTH_IDENTITY_free(copy);
		WLog_ERR(TAG, "%s:%d: %s() Could not allocate the fields of a SEC_WINNT_AUTH_IDENTITY structure",
		         __FILE__, __LINE__, __FUNCTION__);
		return NULL;
	}

	return copy;
}

void SEC_WINNT_AUTH_IDENTITY_free(SEC_WINNT_AUTH_IDENTITY* password_creds)
{
	if (password_creds)
	{
		WSTRING_LENGTH_CLEAR_AND_FREE(password_creds, User);
		WSTRING_LENGTH_CLEAR_AND_FREE(password_creds, Domain);
		WSTRING_LENGTH_CLEAR_AND_FREE(password_creds, Password);
	}

	free(password_creds);
}


/* ============================================================ */

csp_data_detail* csp_data_detail_new_nocopy(UINT32 KeySpec,
        char* CardName,
        char* ReaderName,
        char* ContainerName,
        char* CspName)
{
	csp_data_detail* csp;
	CHECK_MEMORY(csp = malloc(sizeof(*csp)), return NULL);
	csp->KeySpec = KeySpec;
	csp->CardName = CardName;
	csp->ReaderName = ReaderName;
	csp->ContainerName = ContainerName;
	csp->CspName = CspName;
	return csp;
}

csp_data_detail* csp_data_detail_new(UINT32 KeySpec,
                                     char* cardname,
                                     char* readername,
                                     char* containername,
                                     char* cspname)
{
	char* CardName = strdup(cardname);
	char* ReaderName = strdup(readername);
	char* ContainerName = strdup(containername);
	char* CspName = strdup(cspname);

	if (!CardName || !ReaderName || !ContainerName || !CspName)
	{
		free(CardName);
		free(ReaderName);
		free(ContainerName);
		free(CspName);
		WLog_ERR(TAG, "%s:%d: %s() Could not allocate CardName, ReaderName, ContainerName or CspName",
		         __FILE__, __LINE__, __FUNCTION__);
		return NULL;
	}

	return csp_data_detail_new_nocopy(KeySpec, CardName, ReaderName, ContainerName, CspName);
}

csp_data_detail* csp_data_detail_deepcopy(csp_data_detail* original)
{
	return csp_data_detail_new(original->KeySpec,
	                           original->CardName,
	                           original->ReaderName,
	                           original->ContainerName,
	                           original->CspName);
}

void csp_data_detail_free(csp_data_detail* csp)
{
	if (csp == NULL)
	{
		return;
	}

	string_clear_and_free(csp->CardName);
	string_clear_and_free(csp->ReaderName);
	string_clear_and_free(csp->ContainerName);
	string_clear_and_free(csp->CspName);
	memory_clear_and_free(csp, sizeof(*csp));
}

/* ============================================================ */

smartcard_creds* smartcard_creds_new_nocopy(char* Pin, char* UserHint, char* DomainHint,
        csp_data_detail* csp_data)
{
	smartcard_creds* that;
	CHECK_MEMORY(that = malloc(sizeof(*that)), return NULL);
	that->Pin = Pin;
	that->UserHint = UserHint;
	that->DomainHint = DomainHint;
	that->csp_data = csp_data;
	return that;
}

smartcard_creds* smartcard_creds_new(char* pin, char* userhint, char* domainhint,
                                     csp_data_detail* cspdata)
{
	char* Pin = strdup(pin);
	char* UserHint = strdup(userhint);
	char* DomainHint = strdup(domainhint);
	csp_data_detail* cspData = csp_data_detail_deepcopy(cspdata);

	if ((Pin == NULL) || (UserHint == NULL) || (DomainHint == NULL) || (cspData == NULL))
	{
		free(Pin);
		free(UserHint);
		free(DomainHint);
		csp_data_detail_free(cspData);
		WLog_ERR(TAG,
		         "%s:%d: %s() Could not allocate Pin, UserHint or DomainHint,  or copy CSP Data Details",
		         __FILE__, __LINE__, __FUNCTION__);
		return NULL;
	}

	return smartcard_creds_new_nocopy(Pin, UserHint, DomainHint, cspData);
}

smartcard_creds* smartcard_creds_deepcopy(smartcard_creds* original)
{
	return smartcard_creds_new(original->Pin, original->UserHint, original->DomainHint,
	                           original->csp_data);
}

void smartcard_creds_free(smartcard_creds* that)
{
	if (that == NULL)
	{
		return;
	}

	string_clear_and_free(that->Pin);
	string_clear_and_free(that->UserHint);
	string_clear_and_free(that->DomainHint);
	csp_data_detail_free(that->csp_data);
	memory_clear_and_free(that, sizeof(*that));
}

/* ============================================================ */

remote_guard_package_cred* remote_guard_package_cred_new_nocopy(char* package_name,
        unsigned credential_size,
        BYTE*    credential)
{
	remote_guard_package_cred* that;
	CHECK_MEMORY(that = malloc(sizeof(*that)), return NULL);
	that->package_name = package_name;
	that->credential_size = credential_size;
	that->credential = credential;
	return that;
}

void remote_guard_package_cred_free(remote_guard_package_cred* that)
{
	if (that == NULL)
	{
		return;
	}

	free(that->package_name);
	free(that->credential);
	memset(that, 0, sizeof(*that));
	free(that);
}

remote_guard_package_cred* remote_guard_package_cred_deepcopy(remote_guard_package_cred*
        that)
{
	remote_guard_package_cred* copy;
	CHECK_MEMORY(copy = malloc(sizeof(*copy)), return NULL);
	copy->package_name = strdup(that->package_name);
	copy->credential_size = that->credential_size;
	copy->credential = memdup(that->credential, that->credential_size);

	if ((copy->package_name == NULL) || (copy->credential == NULL))
	{
		remote_guard_package_cred_free(copy);
		CHECK_MEMORY(NULL, return NULL);
	}

	return copy;
}

/* ============================================================ */

remote_guard_creds* remote_guard_creds_new_logon_cred(remote_guard_package_cred* logon_cred)
{
	remote_guard_creds* that;
	CHECK_MEMORY(that = malloc(sizeof(*that)), return NULL);
	that->login_cred = logon_cred;
	that->supplemental_creds_count = 0;
	that->supplemental_creds = NULL;
	return that;
}

remote_guard_creds* remote_guard_creds_new_nocopy(char* package_name,
        unsigned credenial_size,
        BYTE*    credential)
{
	remote_guard_package_cred* logon_cred = remote_guard_package_cred_new_nocopy(package_name,
	                                        credenial_size, credential);
	CHECK_MEMORY(logon_cred, return NULL);
	return remote_guard_creds_new_logon_cred(logon_cred);
}

void remote_guard_creds_add_supplemental_cred(remote_guard_creds* that,
        remote_guard_package_cred* supplemental_cred)
{
	remote_guard_package_cred** new_creds = realloc(that->supplemental_creds,
	                                        (that->supplemental_creds_count + 1) * sizeof(that->supplemental_creds[0]));
	CHECK_MEMORY(new_creds, return);
	new_creds[that->supplemental_creds_count] = supplemental_cred;
	that->supplemental_creds = new_creds;
	that->supplemental_creds_count++;
}

remote_guard_creds* remote_guard_creds_deepcopy(remote_guard_creds* that)
{
	unsigned i;
	remote_guard_creds* copy;
	CHECK_MEMORY(copy = malloc(sizeof(*copy)), return NULL);
	copy->login_cred = remote_guard_package_cred_deepcopy(that->login_cred);
	copy->supplemental_creds_count = that->supplemental_creds_count;
	copy->supplemental_creds = malloc(copy->supplemental_creds_count * sizeof(
	                                      copy->supplemental_creds[0]));

	for (i = 0; i < copy->supplemental_creds_count; i++)
	{
		copy->supplemental_creds[i] = remote_guard_package_cred_deepcopy(that->supplemental_creds[i]);

		if (copy->supplemental_creds[i] == NULL)
		{
			break;
		}
	}

	if ((copy->login_cred == NULL) || (copy->supplemental_creds == NULL)
	    || (i < copy->supplemental_creds_count))
	{
		remote_guard_creds_free(copy);
		CHECK_MEMORY(NULL, return NULL);
	}

	return copy;
}

void remote_guard_creds_free(remote_guard_creds* that)
{
	remote_guard_package_cred_free(that->login_cred);

	if (that->supplemental_creds != NULL)
	{
		unsigned i;

		for (i = 0; i < that->supplemental_creds_count; i++)
		{
			remote_guard_package_cred_free(that->supplemental_creds[i]);
		}

		memory_clear_and_free(that->supplemental_creds,
		                      that->supplemental_creds_count * sizeof(that->supplemental_creds[0]));
	}

	memory_clear_and_free(that, sizeof(* that));
}

/* ============================================================ */

auth_identity* auth_identity_new_password(SEC_WINNT_AUTH_IDENTITY* password_creds)
{
	auth_identity* that;
	CHECK_MEMORY(that = malloc(sizeof(*that)), return NULL);
	that->cred_type = credential_type_password;
	that->creds.password_creds = password_creds;
	return that;
}

auth_identity* auth_identity_new_smartcard(smartcard_creds* smartcard_creds)
{
	auth_identity* that;
	CHECK_MEMORY(that = malloc(sizeof(*that)), return NULL);
	that->cred_type = credential_type_smartcard;
	that->creds.smartcard_creds = smartcard_creds;
	return that;
}

auth_identity* auth_identity_new_remote_guard(remote_guard_creds* remote_guard_creds)
{
	auth_identity* that;
	CHECK_MEMORY(that = malloc(sizeof(*that)), return NULL);
	that->cred_type = credential_type_remote_guard;
	that->creds.remote_guard_creds = remote_guard_creds;
	return that;
}

void auth_identity_free(auth_identity* that)
{
	if (that != NULL)
	{
		switch (that->cred_type)
		{
			case credential_type_password:
				SEC_WINNT_AUTH_IDENTITY_free(that->creds.password_creds);
				break;

			case credential_type_smartcard:
				smartcard_creds_free(that->creds.smartcard_creds);
				break;

			case credential_type_remote_guard:
				remote_guard_creds_free(that->creds.remote_guard_creds);
				break;
		}

		memory_clear_and_free(that, sizeof(*that));
	}
}

const char* auth_identity_credential_type_label(auth_identity* that)
{
	if(that ==  NULL)
	{
		return "NULL";
	}

	switch (that->cred_type)
	{
		case credential_type_password:
			return "password";

		case credential_type_smartcard:
			return "smartcard";

		case credential_type_remote_guard:
			return "remote-guard";
		default:
			return "unknown";
	}
}

auth_identity* auth_identity_deepcopy(auth_identity* that)
{
#define CHECK_COPY(field, copier)					\
	if (that->creds.field != NULL)					\
	{								\
		if ((field = copier(that->creds.field)) == NULL)	\
		{							\
			goto failure;					\
		}							\
	}
	SEC_WINNT_AUTH_IDENTITY* password_creds = NULL;
	smartcard_creds* smartcard_creds = NULL;
	remote_guard_creds* remote_guard_creds = NULL;

	switch (that->cred_type)
	{
		case credential_type_password:
			CHECK_COPY(password_creds, SEC_WINNT_AUTH_IDENTITY_deepcopy);
			return auth_identity_new_password(password_creds);

		case credential_type_smartcard:
			CHECK_COPY(smartcard_creds, smartcard_creds_deepcopy);
			return auth_identity_new_smartcard(smartcard_creds);

		case credential_type_remote_guard:
			CHECK_COPY(remote_guard_creds, remote_guard_creds_deepcopy);
			return auth_identity_new_remote_guard(remote_guard_creds);
	}

failure:
	SEC_WINNT_AUTH_IDENTITY_free(password_creds);
	smartcard_creds_free(smartcard_creds);
	remote_guard_creds_free(remote_guard_creds);
	return NULL;
#undef CHECK_COPY
}

/* ============================================================ */

static BOOL nla_read_octet_string(wStream* s, const char* field_name,
                                  int contextual_tag,
                                  BYTE** field, UINT32* field_length)
{
	size_t length = 0;

	if (!ber_read_contextual_tag(s, contextual_tag, &length, TRUE) ||
	    !ber_read_octet_string_tag(s, &length))
	{
		return FALSE;
	}

	if (length == 0)
	{
		CHECK_MEMORY((*field) = calloc(1, 2), return FALSE);
	}
	else
	{
		CHECK_MEMORY((*field) = malloc(length), return FALSE);
		CopyMemory((*field), Stream_Pointer(s), length);
		Stream_Seek(s, length);
	}

	(*field_length) = (UINT32) length;
	return TRUE;
}

static BOOL nla_read_octet_string_field(wStream* s, const char* field_name,
                                        int contextual_tag,
                                        UINT16** field, UINT32* field_length)
{
	if (nla_read_octet_string(s, field_name, contextual_tag, (BYTE**)field, field_length))
	{
		(*field_length) /= 2;
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

static BOOL nla_read_octet_string_string(wStream* s, const char* field_name,
        int contextual_tag,
        char** string)
{
	int result;
	UINT16* field = NULL;
	UINT32 field_length = 0;
	(*string) = NULL;

	if (!nla_read_octet_string_field(s, field_name, contextual_tag, &field, &field_length))
	{
		return 0;
	}

	result = ConvertFromUnicode(CP_UTF8, 0, field, 2 * field_length, string, 0, 0, FALSE);
	free(field);
	return 0 <= result;
}

static size_t nla_write_sequence_octet_string_string(wStream* s, const char* field_name,
        int contextual_tag,
        char* string)
{
	LPWSTR buffer = NULL;
	size_t length;
	int increment;

	if ((length = ConvertToUnicode(CP_UTF8, 0, string, -1, &buffer, 0)) <=  0)
	{
		WLog_ERR(TAG, "Cannot ConvertToUnicode '%s'", string);
		return 0;
	}

	/* ConvertToUnicode returns the number of codepoints, including the terminating nul! */
	increment = ber_write_sequence_octet_string(s, contextual_tag, (BYTE*)buffer, (length - 1) * 2);
	free(buffer);
	return increment;
}

/*
string_length returns the number of characters in the utf-8 string.
That is, the number of WCHAR in the string converted to "unicode".
*/
int string_length(char* string)
{
	if (string == NULL)
	{
		return 0;
	}
	else
	{
		LPWSTR wstring = NULL;
		int length =  ConvertToUnicode(CP_UTF8, 0, string, -1, & wstring, 0);
		free(wstring);
		/* ConvertToUnicode returns the number of codepoints, including the terminating nul! */
		return (length <= 0) ? 0 : length - 1;
	}
}

/* ============================================================ */

size_t nla_sizeof_ts_password_creds_inner(SEC_WINNT_AUTH_IDENTITY* password_creds)
{
	if (password_creds == NULL)
	{
		return 0;
	}

	return (ber_sizeof_sequence_octet_string(password_creds->DomainLength * 2)
	        + ber_sizeof_sequence_octet_string(password_creds->UserLength * 2)
	        + ber_sizeof_sequence_octet_string(password_creds->PasswordLength * 2));
}

size_t nla_sizeof_ts_password_creds(SEC_WINNT_AUTH_IDENTITY* password_creds)
{
	size_t inner_size = nla_sizeof_ts_password_creds_inner(password_creds);
	return ber_sizeof_sequence(inner_size);
}


size_t nla_sizeof_ts_cspdatadetail_inner(csp_data_detail* csp_data)
{
	if (csp_data == NULL)
	{
		return 0;
	}

	return (ber_sizeof_contextual_tag(ber_sizeof_integer(csp_data->KeySpec))
	        + ber_sizeof_integer(csp_data->KeySpec)
	        + ber_sizeof_sequence_octet_string(string_length(csp_data->CardName) * 2)
	        + ber_sizeof_sequence_octet_string(string_length(csp_data->ReaderName) * 2)
	        + ber_sizeof_sequence_octet_string(string_length(csp_data->ContainerName) * 2)
	        + ber_sizeof_sequence_octet_string(string_length(csp_data->CspName) * 2));
}

size_t nla_sizeof_ts_cspdatadetail(csp_data_detail*  csp_data)
{
	size_t inner_size = nla_sizeof_ts_cspdatadetail_inner(csp_data);
	size_t seq_size = ber_sizeof_sequence(inner_size);
	return (ber_sizeof_contextual_tag(seq_size) + seq_size);
}

size_t nla_sizeof_ts_smartcard_creds_inner(smartcard_creds* smartcard_creds)
{
	if (smartcard_creds == NULL)
	{
		return 0;
	}

	return (ber_sizeof_sequence_octet_string(string_length(smartcard_creds->Pin) * 2)
	        + nla_sizeof_ts_cspdatadetail(smartcard_creds->csp_data)
	        + ber_sizeof_sequence_octet_string(string_length(smartcard_creds->UserHint) * 2)
	        + ber_sizeof_sequence_octet_string(string_length(smartcard_creds->DomainHint) * 2));
}

size_t nla_sizeof_ts_smartcard_creds(smartcard_creds* smartcard_creds)
{
	return ber_sizeof_sequence(nla_sizeof_ts_smartcard_creds_inner(smartcard_creds));
}


size_t nla_sizeof_ts_remote_guard_package_cred_inner(remote_guard_package_cred* package_cred)
{
	return (ber_sizeof_sequence_octet_string(string_length(package_cred->package_name) * 2)
	        + ber_sizeof_sequence_octet_string(package_cred->credential_size));
}

size_t nla_sizeof_ts_remote_guard_package_cred(remote_guard_package_cred* package_cred)
{
	return ber_sizeof_sequence(nla_sizeof_ts_remote_guard_package_cred_inner(package_cred));
}

size_t nla_sizeof_ts_remote_guard_creds_inner(remote_guard_creds* remote_guard_creds)
{
	size_t size = 0;
	unsigned i;

	if (remote_guard_creds == NULL)
	{
		return 0;
	}

	/* logonCred         [0] TSRemoteGuardPackageCred, */
	{
		size_t login_size = nla_sizeof_ts_remote_guard_package_cred(remote_guard_creds->login_cred);
		size += ber_sizeof_contextual_tag(login_size);
		size += login_size;
	}

	/* supplementalCreds [1] SEQUENCE OF TSRemoteGuardPackageCred OPTIONAL, */
	if (0 < remote_guard_creds->supplemental_creds_count)
	{
		size_t seq_size = 0;
		size_t supplemental_size = 0;

		for (i = 0; i < remote_guard_creds->supplemental_creds_count; i++)
		{
			supplemental_size += nla_sizeof_ts_remote_guard_package_cred(
			                         remote_guard_creds->supplemental_creds[i]);
		}

		seq_size = ber_sizeof_sequence(supplemental_size);
		size += ber_sizeof_contextual_tag(seq_size);
		size += seq_size;
	}

	return size;
}

size_t nla_sizeof_ts_remote_guard_creds(remote_guard_creds* remote_guard_creds)
{
	return ber_sizeof_sequence(nla_sizeof_ts_remote_guard_creds_inner(remote_guard_creds));
}


size_t nla_sizeof_ts_creds(auth_identity* identity)
{
	switch (identity->cred_type)
	{
		case credential_type_password:
			return nla_sizeof_ts_password_creds(identity->creds.password_creds);

		case credential_type_smartcard:
			return nla_sizeof_ts_smartcard_creds(identity->creds.smartcard_creds);

		case credential_type_remote_guard:
			return nla_sizeof_ts_remote_guard_creds(identity->creds.remote_guard_creds);

		default:
			return 0;
	}
}

size_t nla_sizeof_ts_credentials_inner(auth_identity* identity)
{
	return (ber_sizeof_contextual_tag(ber_sizeof_integer(identity->cred_type))
	        + ber_sizeof_integer(identity->cred_type)
	        + ber_sizeof_sequence_octet_string(nla_sizeof_ts_creds(identity)));
}


size_t nla_sizeof_ts_credentials(auth_identity* identity)
{
	return ber_sizeof_sequence(nla_sizeof_ts_credentials_inner(identity));
}


static SEC_WINNT_AUTH_IDENTITY* nla_read_ts_password_creds(wStream* s, size_t* length)
{
	SEC_WINNT_AUTH_IDENTITY* password_creds = NULL;

	/* TSPasswordCreds (SEQUENCE) */
	if (!ber_read_sequence_tag(s, length))
	{
		return NULL;
	}

	/* The sequence is empty, return early,
	* TSPasswordCreds (SEQUENCE) is optional. */
	if ((*length) == 0)
	{
		return NULL;
	}

	password_creds = SEC_WINNT_AUTH_IDENTITY_new(NULL, NULL, NULL);

	if (nla_read_octet_string_field(s, "[0] domainName (OCTET STRING)", 0,
	                                &password_creds->Domain,
	                                &password_creds->DomainLength) &&
	    nla_read_octet_string_field(s, "[1] userName (OCTET STRING)", 1,
	                                &password_creds->User,
	                                &password_creds->UserLength) &&
	    nla_read_octet_string_field(s, "[2] password (OCTET STRING)", 2,
	                                &password_creds->Password,
	                                &password_creds->PasswordLength))
	{
		return password_creds;
	}

	SEC_WINNT_AUTH_IDENTITY_free(password_creds);
	return NULL;
}

static csp_data_detail* nla_read_ts_cspdatadetail(wStream* s, size_t* length)
{
	csp_data_detail* csp_data = NULL;
	UINT32 key_spec = 0;

	/* TSCspDataDetail (SEQUENCE)
	* Initialise to default values. */
	if (!ber_read_sequence_tag(s, length))
	{
		return NULL;
	}

	/* The sequence is empty, return early,
	* TSCspDataDetail (SEQUENCE) is optional. */
	if (*length == 0)
	{
		return NULL;
	}

	/* [0] keySpec (INTEGER) */
	if (!ber_read_contextual_tag(s, 0, length, TRUE) ||
	    !ber_read_integer(s, &key_spec))
	{
		return NULL;
	}

	CHECK_MEMORY((csp_data = csp_data_detail_new_nocopy(key_spec, NULL, NULL, NULL, NULL)) == NULL,
	             return NULL);

	if (nla_read_octet_string_string(s, "[1] cardName (OCTET STRING)", 1,
	                                 &csp_data->CardName) &&
	    nla_read_octet_string_string(s, "[2] readerName (OCTET STRING)", 2,
	                                 &csp_data->ReaderName) &&
	    nla_read_octet_string_string(s, "[3] containerName (OCTET STRING)", 3,
	                                 &csp_data->ContainerName) &&
	    nla_read_octet_string_string(s, "[4] cspName (OCTET STRING)", 4,
	                                 &csp_data->CspName))
	{
		return csp_data;
	}

	csp_data_detail_free(csp_data);
	return NULL;
}

static smartcard_creds* nla_read_ts_smartcard_creds(wStream* s, size_t* length)
{
	smartcard_creds* smartcard_creds = NULL;

	/* TSSmartCardCreds (SEQUENCE)
	* Initialize to default values. */
	if (!ber_read_sequence_tag(s, length))
	{
		return NULL;
	}

	/* The sequence is empty, return early,
	* TSSmartCardCreds (SEQUENCE) is optional. */
	if ((*length) == 0)
	{
		return NULL;
	}

	CHECK_MEMORY((smartcard_creds = smartcard_creds_new_nocopy(NULL, NULL, NULL, NULL)), return NULL);

	if (nla_read_octet_string_string(s, "[0] Pin (OCTET STRING)", 0,
	                                 &smartcard_creds->Pin))
	{
		smartcard_creds->csp_data = nla_read_ts_cspdatadetail(s, length);

		if (nla_read_octet_string_string(s, "[2] UserHint (OCTET STRING)", 2,
		                                 &smartcard_creds->UserHint) &&
		    nla_read_octet_string_string(s, "[3] DomainHint (OCTET STRING", 3,
		                                 &smartcard_creds->DomainHint))
		{
			return smartcard_creds;
		}
	}

	smartcard_creds_free(smartcard_creds);
	return NULL;
}

static remote_guard_package_cred* nla_read_ts_remote_guard_package_cred(wStream* s, size_t* length)
{
	remote_guard_package_cred* package_cred = NULL;

	/*
	* TSRemoteGuardPackageCred ::=  SEQUENCE {
	*     packageName [0] OCTET STRING,
	*     credBuffer  [1] OCTET STRING,
	* }
	*/

	if (!ber_read_sequence_tag(s, length))
	{
		return NULL;
	}

	/* The sequence is empty, return early,
	* TSRemoteGuardPackageCreds (SEQUENCE) is optional. */
	if ((*length) == 0)
	{
		return NULL;
	}

	char* package_name;
	BYTE* cred_buffer;
	UINT32 cred_buffer_size;

	if (nla_read_octet_string_string(s, "[0] OCTET STRING,", 0, &package_name) &&
	    nla_read_octet_string(s, "[1] OCTET STRING", 1, &cred_buffer, &cred_buffer_size))
	{
		package_cred = remote_guard_package_cred_new_nocopy(package_name, cred_buffer_size, cred_buffer);
	}

	if (package_cred == NULL)
	{
		free(package_name);
		free(cred_buffer);
		CHECK_MEMORY(NULL, return NULL);
	}

	return package_cred;
}

static remote_guard_creds* nla_read_ts_remote_guard_creds(wStream* s, size_t* length)
{
	/*
	* TSRemoteGuardCreds ::= SEQUENCE {
	*     logonCred         [0] TSRemoteGuardPackageCred,
	*     supplementalCreds [1] SEQUENCE OF TSRemoteGuardPackageCred OPTIONAL,
	* }
	*/
	remote_guard_creds*  remote_guard_creds = NULL;
	remote_guard_package_cred* logon_cred = NULL;
	size_t supplemental_length = 0;

	/* TSRemoteGuardCreds (SEQUENCE)
	* Initialize to default values. */
	if (!ber_read_sequence_tag(s, length))
	{
		return NULL;
	}

	/* The sequence is empty, return early,
	* TSRemoteGuardCreds (SEQUENCE) is optional. */
	if ((*length) == 0)
	{
		return NULL;
	}

	logon_cred = nla_read_ts_remote_guard_package_cred(s, length);
	remote_guard_creds = remote_guard_creds_new_logon_cred(logon_cred);

	if (remote_guard_creds == NULL)
	{
		free(logon_cred);
		return NULL;
	}

	if (ber_read_sequence_tag(s, &supplemental_length)
	    && (supplemental_length > 0))
	{
		remote_guard_package_cred* supplemental_cred = nla_read_ts_remote_guard_package_cred(s, length);

		while (supplemental_cred != NULL)
		{
			remote_guard_creds_add_supplemental_cred(remote_guard_creds, supplemental_cred);
			supplemental_cred = nla_read_ts_remote_guard_package_cred(s, length);
		}
	}

	return remote_guard_creds;
}

static auth_identity* nla_read_ts_creds(wStream* s, credential_type cred_type, size_t* length)
{
	auth_identity* identity = NULL;
	SEC_WINNT_AUTH_IDENTITY* password_creds = NULL;
	smartcard_creds* smartcard_creds = NULL;
	remote_guard_creds* remote_guard_creds = NULL;

	switch (cred_type)
	{
		case credential_type_password:
			if ((password_creds = nla_read_ts_password_creds(s, length)) != NULL)
			{
				identity = auth_identity_new_password(password_creds);
			}

			break;

		case credential_type_smartcard:
			if ((smartcard_creds = nla_read_ts_smartcard_creds(s, length)) != NULL)
			{
				identity = auth_identity_new_smartcard(smartcard_creds);
			}

			break;

		case credential_type_remote_guard:
			if ((remote_guard_creds = nla_read_ts_remote_guard_creds(s, length)) != NULL)
			{
				identity = auth_identity_new_remote_guard(remote_guard_creds);
			}

			break;

		default:
			WLog_ERR(TAG,  "cred_type unknown: %d\n", cred_type);
			return NULL;
	}

	if (identity != NULL)
	{
		return identity;
	}

	SEC_WINNT_AUTH_IDENTITY_free(password_creds);
	smartcard_creds_free(smartcard_creds);
	remote_guard_creds_free(remote_guard_creds);
	return NULL;
}

auth_identity* nla_read_ts_credentials(PSecBuffer ts_credentials)
{
	auth_identity* identity = NULL;
	wStream* s;
	size_t length = 0;
	size_t ts_creds_length = 0;
	UINT32* value = NULL;

	if (!ts_credentials || !ts_credentials->pvBuffer)
		return FALSE;

	s = Stream_New(ts_credentials->pvBuffer, ts_credentials->cbBuffer);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return FALSE;
	}

	/* TSCredentials (SEQUENCE) */
	if (ber_read_sequence_tag(s, &length) &&
	    /* [0] credType (INTEGER) */
	    ber_read_contextual_tag(s, 0, &length, TRUE) &&
	    ber_read_integer(s, value) &&
	    /* [1] credentials (OCTET STRING) */
	    ber_read_contextual_tag(s, 1, &length, TRUE) &&
	    ber_read_octet_string_tag(s, &ts_creds_length))
	{
		identity = nla_read_ts_creds(s, *value, &length);
	}

	Stream_Free(s, FALSE);
	return identity;
}


size_t nla_write_ts_password_creds(SEC_WINNT_AUTH_IDENTITY* password_creds, wStream* s)
{
	size_t size = 0;
	size_t inner_size = nla_sizeof_ts_password_creds_inner(password_creds);
	/* TSPasswordCreds (SEQUENCE) */
	size += ber_write_sequence_tag(s, inner_size);

	if (password_creds != NULL)
	{
		/* [0] domainName (OCTET STRING) */
		size += ber_write_sequence_octet_string(
		            s, 0, (BYTE*) password_creds->Domain,
		            password_creds->DomainLength * 2);
		/* [1] userName (OCTET STRING) */
		size += ber_write_sequence_octet_string(
		            s, 1, (BYTE*) password_creds->User,
		            password_creds->UserLength * 2);
		/* [2] password (OCTET STRING) */
		size += ber_write_sequence_octet_string(
		            s, 2, (BYTE*) password_creds->Password,
		            password_creds->PasswordLength * 2);
	}

	return size;
}

size_t nla_write_ts_csp_data_detail(csp_data_detail* csp_data, int contextual_tag,  wStream* s)
{
	size_t size = 0;

	if (csp_data != NULL)
	{
		size_t inner_size = nla_sizeof_ts_cspdatadetail_inner(csp_data);
		size += ber_write_contextual_tag(s, contextual_tag, ber_sizeof_sequence(inner_size), TRUE);
		size += ber_write_sequence_tag(s, inner_size);
		/* [0] KeySpec (INTEGER) */
		size += ber_write_contextual_tag(s, 0, ber_sizeof_integer(csp_data->KeySpec), TRUE);
		size += ber_write_integer(s, csp_data->KeySpec);
		size += nla_write_sequence_octet_string_string(s, "[1] CardName (OCTET STRING)", 1,
		        csp_data->CardName);
		size += nla_write_sequence_octet_string_string(s, "[2] ReaderName (OCTET STRING)", 2,
		        csp_data->ReaderName);
		size += nla_write_sequence_octet_string_string(s, "[3] ContainerName (OCTET STRING)", 3,
		        csp_data->ContainerName);
		size += nla_write_sequence_octet_string_string(s, "[4] CspName (OCTET STRING)", 4,
		        csp_data->CspName);
	}

	return size;
}

size_t nla_write_ts_smartcard_creds(smartcard_creds* smartcard_creds, wStream* s)
{
	size_t size = 0;
	size_t inner_size = nla_sizeof_ts_smartcard_creds_inner(smartcard_creds);
	/* TSSmartCardCreds (SEQUENCE) */
	size += ber_write_sequence_tag(s, inner_size);

	if (smartcard_creds != NULL)
	{
		size += nla_write_sequence_octet_string_string(s, "[0] Pin (OCTET STRING)", 0,
		        smartcard_creds->Pin);
		/* [1] CspDataDetail (TSCspDataDetail) (SEQUENCE) */
		size += nla_write_ts_csp_data_detail(smartcard_creds->csp_data, 1, s);
		size += nla_write_sequence_octet_string_string(s, "[2] userHint (OCTET STRING)", 2,
		        smartcard_creds->UserHint);
		size += nla_write_sequence_octet_string_string(s, "[3] domainHint (OCTET STRING)", 3,
		        smartcard_creds->DomainHint);
	}

	return size;
}

size_t nla_write_ts_remote_guard_package_cred(remote_guard_package_cred* package_cred,
        wStream* s)
{
	/*
	* TSRemoteGuardPackageCred ::=  SEQUENCE {
	*     packageName [0] OCTET STRING,
	*     credBuffer  [1] OCTET STRING,
	* }
	*/
	size_t size = 0;
	size_t inner_size = nla_sizeof_ts_remote_guard_package_cred_inner(package_cred);
	size += ber_write_sequence_tag(s, inner_size);

	if (package_cred != NULL)
	{
		size += nla_write_sequence_octet_string_string(s, "packageName [0] OCTET STRING", 0,
		        package_cred->package_name);
		/* credBuffer  [1] OCTET STRING, */
		size += ber_write_sequence_octet_string(s, 1, package_cred->credential,
		                                        package_cred->credential_size);
	}

	return size;
}

size_t nla_write_ts_remote_guard_creds(remote_guard_creds*  remote_guard_creds, wStream* s)
{
	unsigned i;
	size_t size = 0;
	size_t inner_size = nla_sizeof_ts_remote_guard_creds_inner(remote_guard_creds);
	/* TSRemoteGuardCreds ::= SEQUENCE { */
	size += ber_write_sequence_tag(s, inner_size);
	/*     logonCred         [0] TSRemoteGuardPackageCred, */
	size += ber_write_contextual_tag(s, 0,
	                                 nla_sizeof_ts_remote_guard_package_cred(remote_guard_creds->login_cred), TRUE);
	size += nla_write_ts_remote_guard_package_cred(remote_guard_creds->login_cred, s);

	/*     supplementalCreds [1] SEQUENCE OF TSRemoteGuardPackageCred OPTIONAL, */
	if (0 < remote_guard_creds->supplemental_creds_count)
	{
		unsigned supplemental_size = 0;

		for (i = 0; i < remote_guard_creds->supplemental_creds_count; i++)
		{
			supplemental_size += nla_sizeof_ts_remote_guard_package_cred(
			                         remote_guard_creds->supplemental_creds[i]);
		}

		size += ber_write_contextual_tag(s, 1, ber_sizeof_sequence(supplemental_size), TRUE);
		size += ber_write_sequence_tag(s, supplemental_size);

		for (i = 0; i < remote_guard_creds->supplemental_creds_count; i++)
		{
			size += nla_write_ts_remote_guard_package_cred(remote_guard_creds->supplemental_creds[i], s);
		}
	}

	return size;
}


size_t nla_write_ts_creds(auth_identity* identity, wStream* s)
{
	switch (identity->cred_type)
	{
		case credential_type_password:
			return nla_write_ts_password_creds(identity->creds.password_creds, s);

		case credential_type_smartcard:
			return nla_write_ts_smartcard_creds(identity->creds.smartcard_creds, s);

		case credential_type_remote_guard:
			return nla_write_ts_remote_guard_creds(identity->creds.remote_guard_creds, s);

		default:
			WLog_ERR(TAG, "cred_type unknown: %d\n", identity->cred_type);
			return 0;
	}
}


size_t nla_write_ts_credentials(auth_identity* identity, wStream* s)
{
	size_t size = 0;
	size_t cred_size = 0;
	size_t inner_size = nla_sizeof_ts_credentials_inner(identity);
	/* TSCredentials (SEQUENCE) */
	size += ber_write_sequence_tag(s, inner_size);
	/* [0] credType (INTEGER) */
	size += ber_write_contextual_tag(s, 0, ber_sizeof_integer(identity->cred_type), TRUE);
	size += ber_write_integer(s, identity->cred_type);
	/* [1] credentials (OCTET STRING) */
	cred_size = nla_sizeof_ts_creds(identity);
	size += ber_write_contextual_tag(s, 1, ber_sizeof_octet_string(cred_size), TRUE);
	size += ber_write_octet_string_tag(s, cred_size);
	size += nla_write_ts_creds(identity, s);
	return size;
}

/**** THE END ****/
