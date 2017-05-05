/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Security Support Provider Interface (SSPI)
 *
 * Copyright 2012-2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2017 Dorian Ducournau <dorian.ducournau@gmail.com>
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

#include <winpr/windows.h>

#include <winpr/crt.h>
#include <winpr/sspi.h>
#include <winpr/ssl.h>
#include <winpr/print.h>

#include "sspi.h"

#include "sspi_winpr.h"

#include "../log.h"
#define TAG WINPR_TAG("sspi")

static errno_t memset_s(void *v, rsize_t smax, int c, rsize_t n) {
	if (v == NULL) return -1;
	if (smax > RSIZE_MAX) return -1;
	if (n > smax) return -1;

	volatile unsigned char *p = v;
	while (smax-- && n--) {
		*p++ = c;
	}
	return 0;
}

/* Authentication Functions: http://msdn.microsoft.com/en-us/library/windows/desktop/aa374731/ */

extern const SecPkgInfoA NTLM_SecPkgInfoA;
extern const SecPkgInfoW NTLM_SecPkgInfoW;
extern const SecurityFunctionTableA NTLM_SecurityFunctionTableA;
extern const SecurityFunctionTableW NTLM_SecurityFunctionTableW;

extern const SecPkgInfoA KERBEROS_SecPkgInfoA;
extern const SecPkgInfoW KERBEROS_SecPkgInfoW;
extern const SecurityFunctionTableA KERBEROS_SecurityFunctionTableA;
extern const SecurityFunctionTableW KERBEROS_SecurityFunctionTableW;

extern const SecPkgInfoA NEGOTIATE_SecPkgInfoA;
extern const SecPkgInfoW NEGOTIATE_SecPkgInfoW;
extern const SecurityFunctionTableA NEGOTIATE_SecurityFunctionTableA;
extern const SecurityFunctionTableW NEGOTIATE_SecurityFunctionTableW;

extern const SecPkgInfoA CREDSSP_SecPkgInfoA;
extern const SecPkgInfoW CREDSSP_SecPkgInfoW;
extern const SecurityFunctionTableA CREDSSP_SecurityFunctionTableA;
extern const SecurityFunctionTableW CREDSSP_SecurityFunctionTableW;

extern const SecPkgInfoA SCHANNEL_SecPkgInfoA;
extern const SecPkgInfoW SCHANNEL_SecPkgInfoW;
extern const SecurityFunctionTableA SCHANNEL_SecurityFunctionTableA;
extern const SecurityFunctionTableW SCHANNEL_SecurityFunctionTableW;

const SecPkgInfoA* SecPkgInfoA_LIST[] =
{
	&NTLM_SecPkgInfoA,
	&KERBEROS_SecPkgInfoA,
	&NEGOTIATE_SecPkgInfoA,
	&CREDSSP_SecPkgInfoA,
	&SCHANNEL_SecPkgInfoA
};

const SecPkgInfoW* SecPkgInfoW_LIST[] =
{
	&NTLM_SecPkgInfoW,
	&KERBEROS_SecPkgInfoW,
	&NEGOTIATE_SecPkgInfoW,
	&CREDSSP_SecPkgInfoW,
	&SCHANNEL_SecPkgInfoW
};

SecurityFunctionTableA winpr_SecurityFunctionTableA;
SecurityFunctionTableW winpr_SecurityFunctionTableW;

struct _SecurityFunctionTableA_NAME
{
	SEC_CHAR* Name;
	const SecurityFunctionTableA* SecurityFunctionTable;
};
typedef struct _SecurityFunctionTableA_NAME SecurityFunctionTableA_NAME;

struct _SecurityFunctionTableW_NAME
{
	SEC_WCHAR* Name;
	const SecurityFunctionTableW* SecurityFunctionTable;
};
typedef struct _SecurityFunctionTableW_NAME SecurityFunctionTableW_NAME;

const SecurityFunctionTableA_NAME SecurityFunctionTableA_NAME_LIST[] =
{
	{ "NTLM", &NTLM_SecurityFunctionTableA },
	{ "Kerberos", &KERBEROS_SecurityFunctionTableA },
	{ "Negotiate", &NEGOTIATE_SecurityFunctionTableA },
	{ "CREDSSP", &CREDSSP_SecurityFunctionTableA },
	{ "Schannel", &SCHANNEL_SecurityFunctionTableA }
};

WCHAR NTLM_NAME_W[] = { 'N','T','L','M','\0' };
WCHAR KERBEROS_NAME_W[] = { 'K','e','r','b','e','r','o','s','\0' };
WCHAR NEGOTIATE_NAME_W[] = { 'N','e','g','o','t','i','a','t','e','\0' };
WCHAR CREDSSP_NAME_W[] = { 'C','r','e','d','S','S','P','\0' };
WCHAR SCHANNEL_NAME_W[] = { 'S','c','h','a','n','n','e','l','\0' };

const SecurityFunctionTableW_NAME SecurityFunctionTableW_NAME_LIST[] =
{
	{ NTLM_NAME_W, &NTLM_SecurityFunctionTableW },
	{ KERBEROS_NAME_W, &KERBEROS_SecurityFunctionTableW },
	{ NEGOTIATE_NAME_W, &NEGOTIATE_SecurityFunctionTableW },
	{ CREDSSP_NAME_W, &CREDSSP_SecurityFunctionTableW },
	{ SCHANNEL_NAME_W, &SCHANNEL_SecurityFunctionTableW }
};

#define SecHandle_LOWER_MAX	0xFFFFFFFF
#define SecHandle_UPPER_MAX	0xFFFFFFFE

struct _CONTEXT_BUFFER_ALLOC_ENTRY
{
	void* contextBuffer;
	UINT32 allocatorIndex;
};
typedef struct _CONTEXT_BUFFER_ALLOC_ENTRY CONTEXT_BUFFER_ALLOC_ENTRY;

struct _CONTEXT_BUFFER_ALLOC_TABLE
{
	UINT32 cEntries;
	UINT32 cMaxEntries;
	CONTEXT_BUFFER_ALLOC_ENTRY* entries;
};
typedef struct _CONTEXT_BUFFER_ALLOC_TABLE CONTEXT_BUFFER_ALLOC_TABLE;

CONTEXT_BUFFER_ALLOC_TABLE ContextBufferAllocTable;

int sspi_ContextBufferAllocTableNew()
{
	size_t size;

	ContextBufferAllocTable.entries = NULL;
	ContextBufferAllocTable.cEntries = 0;
	ContextBufferAllocTable.cMaxEntries = 4;

	size = sizeof(CONTEXT_BUFFER_ALLOC_ENTRY) * ContextBufferAllocTable.cMaxEntries;

	ContextBufferAllocTable.entries = (CONTEXT_BUFFER_ALLOC_ENTRY*) calloc(1, size);

	if (!ContextBufferAllocTable.entries)
		return -1;

	return 1;
}

int sspi_ContextBufferAllocTableGrow()
{
	size_t size;
	CONTEXT_BUFFER_ALLOC_ENTRY* entries;
	ContextBufferAllocTable.cEntries = 0;
	ContextBufferAllocTable.cMaxEntries *= 2;

	size = sizeof(CONTEXT_BUFFER_ALLOC_ENTRY) * ContextBufferAllocTable.cMaxEntries;

	if (!size)
		return -1;

	entries = (CONTEXT_BUFFER_ALLOC_ENTRY*) realloc(ContextBufferAllocTable.entries, size);

	if (!entries)
	{
		free(ContextBufferAllocTable.entries);
		return -1;
	}

	ContextBufferAllocTable.entries = entries;

	ZeroMemory((void*) &ContextBufferAllocTable.entries[ContextBufferAllocTable.cMaxEntries / 2], size / 2);

	return 1;
}

void sspi_ContextBufferAllocTableFree()
{
	ContextBufferAllocTable.cEntries = ContextBufferAllocTable.cMaxEntries = 0;
	free(ContextBufferAllocTable.entries);
}

void* sspi_ContextBufferAlloc(UINT32 allocatorIndex, size_t size)
{
	int index;
	void* contextBuffer;

	for (index = 0; index < (int) ContextBufferAllocTable.cMaxEntries; index++)
	{
		if (!ContextBufferAllocTable.entries[index].contextBuffer)
		{
			contextBuffer = calloc(1, size);

			if (!contextBuffer)
				return NULL;

			ContextBufferAllocTable.cEntries++;

			ContextBufferAllocTable.entries[index].contextBuffer = contextBuffer;
			ContextBufferAllocTable.entries[index].allocatorIndex = allocatorIndex;

			return ContextBufferAllocTable.entries[index].contextBuffer;
		}
	}

	/* no available entry was found, the table needs to be grown */

	if (sspi_ContextBufferAllocTableGrow() < 0)
		return NULL;

	/* the next call to sspi_ContextBufferAlloc() should now succeed */

	return sspi_ContextBufferAlloc(allocatorIndex, size);
}

SSPI_CREDENTIALS* sspi_CredentialsNew()
{
	SSPI_CREDENTIALS* credentials;

	credentials = (SSPI_CREDENTIALS*) calloc(1, sizeof(SSPI_CREDENTIALS));

	return credentials;
}

void sspi_CredentialsFree(SSPI_CREDENTIALS* credentials)
{
	size_t userLength=0;
	size_t domainLength=0;
	size_t passwordLength=0;
	size_t pinLength=0;
	size_t userHintLength=0;
	size_t domainHintLength=0;
	size_t cardNameLength=0;
	size_t readerNameLength=0;
	size_t containerNameLength=0;
	size_t cspNameLength=0;

	if (!credentials)
		return;

	userLength = credentials->identity.UserLength;
	domainLength = credentials->identity.DomainLength;
	passwordLength = credentials->identity.PasswordLength;

	pinLength = credentials->identity.PinLength;
	userHintLength = credentials->identity.UserHintLength;
	domainHintLength = credentials->identity.DomainHintLength;
	if( credentials->identity.CspData ){
		cardNameLength = credentials->identity.CspData->CardNameLength;
		readerNameLength = credentials->identity.CspData->ReaderNameLength;
		containerNameLength = credentials->identity.CspData->ContainerNameLength;
		cspNameLength = credentials->identity.CspData->CspNameLength;
	}

	if (credentials->identity.Flags & SEC_WINNT_AUTH_IDENTITY_UNICODE)
	{
		userLength *= 2;
		domainLength *= 2;
		passwordLength *= 2;

		pinLength *= 2;
		userHintLength *= 2;
		domainHintLength *= 2;
		if( credentials->identity.CspData ){
			cardNameLength *= 2;
			readerNameLength *= 2;
			containerNameLength *= 2;	
			cspNameLength *= 2;	
		}
	}

	memset(credentials->identity.User, 0, userLength);
	memset(credentials->identity.Domain, 0, domainLength);
	memset(credentials->identity.Password, 0, passwordLength);

	/* safely erase PIN buffer */
	if( memset_s(credentials->identity.Pin, 48, 0, pinLength) ) /* 48 = 2 * ( CredSSP formatted pin length ) */
		memset(credentials->identity.Pin, 0, pinLength);
	memset(credentials->identity.UserHint, 0, userHintLength);
	memset(credentials->identity.DomainHint, 0, domainHintLength);
    if( credentials->identity.CspData ){
		memset(credentials->identity.CspData->CardName, 0, cardNameLength);
		memset(credentials->identity.CspData->ReaderName, 0, readerNameLength);
		memset(credentials->identity.CspData->ContainerName, 0, containerNameLength);
		memset(credentials->identity.CspData->CspName, 0, cspNameLength);
	}

	if(credentials->identity.PasswordLength)
	{
		free(credentials->identity.User);
		free(credentials->identity.Domain);
		free(credentials->identity.Password);
	}
	else
	{
		free(credentials->identity.Pin);
		free(credentials->identity.CspData);
		free(credentials->identity.UserHint);
		free(credentials->identity.DomainHint);
	}

	free(credentials);
}

void* sspi_SecBufferAlloc(PSecBuffer SecBuffer, ULONG size)
{
	if (!SecBuffer)
		return NULL;

	SecBuffer->pvBuffer = calloc(1, size);
	if (!SecBuffer->pvBuffer)
		return NULL;

	SecBuffer->cbBuffer = size;
	return SecBuffer->pvBuffer;
}

void sspi_SecBufferFree(PSecBuffer SecBuffer)
{
	if (!SecBuffer)
		return;

	memset(SecBuffer->pvBuffer, 0, SecBuffer->cbBuffer);
	free(SecBuffer->pvBuffer);
	SecBuffer->pvBuffer = NULL;
	SecBuffer->cbBuffer = 0;
}

SecHandle* sspi_SecureHandleAlloc()
{
	SecHandle* handle = (SecHandle*) calloc(1, sizeof(SecHandle));

	if (!handle)
		return NULL;

	SecInvalidateHandle(handle);

	return handle;
}

void* sspi_SecureHandleGetLowerPointer(SecHandle* handle)
{
	void* pointer;

	if (!handle || !SecIsValidHandle(handle) || !handle->dwLower)
		return NULL;

	pointer = (void*) ~((size_t) handle->dwLower);

	return pointer;
}

void sspi_SecureHandleSetLowerPointer(SecHandle* handle, void* pointer)
{
	if (!handle)
		return;

	handle->dwLower = (ULONG_PTR) (~((size_t) pointer));
}

void* sspi_SecureHandleGetUpperPointer(SecHandle* handle)
{
	void* pointer;

	if (!handle || !SecIsValidHandle(handle) || !handle->dwUpper)
		return NULL;

	pointer = (void*) ~((size_t) handle->dwUpper);

	return pointer;
}

void sspi_SecureHandleSetUpperPointer(SecHandle* handle, void* pointer)
{
	if (!handle)
		return;

	handle->dwUpper = (ULONG_PTR) (~((size_t) pointer));
}

void sspi_SecureHandleFree(SecHandle* handle)
{
	free(handle);
}

int sspi_SetAuthIdentity(SEC_WINNT_AUTH_IDENTITY* identity, const char* user, const char* domain, const char* password)
{
	int status;

	identity->Flags = SEC_WINNT_AUTH_IDENTITY_UNICODE;

	free(identity->User);

	identity->User = (UINT16*) NULL;
	identity->UserLength = 0;

	if (user)
	{
		status = ConvertToUnicode(CP_UTF8, 0, user, -1, (LPWSTR*) &(identity->User), 0);

		if (status <= 0)
			return -1;

		identity->UserLength = (ULONG) (status - 1);
	}

	free(identity->Domain);

	identity->Domain = (UINT16*) NULL;
	identity->DomainLength = 0;

	if (domain)
	{
		status = ConvertToUnicode(CP_UTF8, 0, domain, -1, (LPWSTR*) &(identity->Domain), 0);

		if (status <= 0)
			return -1;

		identity->DomainLength = (ULONG) (status - 1);
	}

	free(identity->Password);

	identity->Password = NULL;
	identity->PasswordLength = 0;

	if (password)
	{
		status = ConvertToUnicode(CP_UTF8, 0, password, -1, (LPWSTR*) &(identity->Password), 0);

		if (status <= 0)
			return -1;

		identity->PasswordLength = (ULONG) (status - 1);
	}

	return 1;
}

int sspi_SetAuthIdentity_Smartcard(SEC_WINNT_AUTH_IDENTITY* identity, const char* pin, const UINT32 keySpec, const char * cardName,
		const char * readerName, const char * containerName, const char * cspName, const char * userHint, const char * domainHint)
{
	int status;

	identity->Flags = SEC_WINNT_AUTH_IDENTITY_UNICODE;

	if (identity->Pin)
		free(identity->Pin);

	identity->Pin = (UINT16*) NULL;
	identity->PinLength = 0;

	if (pin)
	{
		status = ConvertToUnicode(CP_UTF8, 0, pin, -1, (LPWSTR*) &(identity->Pin), 0);

		if (status <= 0)
			return -1;

		identity->PinLength = (ULONG) (status - 1);
	}

	if (identity->CspData)
		free(identity->CspData);

	identity->CspData = (SEC_WINNT_AUTH_IDENTITY_CSPDATADETAIL*) NULL;

	if(keySpec || cardName || readerName || containerName || cspName)
	{
		status = setCSPData(status, &identity->CspData, keySpec, cardName, readerName, containerName, cspName);

		if(status <= 0)
			return -1;
	}

	if (identity->UserHint)
			free(identity->UserHint);

	identity->UserHint = (UINT16*) NULL;
	identity->UserHintLength = 0;

	if (userHint)
	{
		status = ConvertToUnicode(CP_UTF8, 0, userHint, -1, (LPWSTR*) &(identity->UserHint), 0);

		if (status <= 0)
			return -1;

		identity->UserHintLength = (ULONG) (status - 1);
	}

	if (identity->DomainHint)
		free(identity->DomainHint);

	identity->DomainHint = (UINT16*) NULL;
	identity->DomainHintLength = 0;

	if (domainHint)
	{
		status = ConvertToUnicode(CP_UTF8, 0, domainHint, -1, (LPWSTR*) &(identity->DomainHint), 0);

		if (status <= 0)
			return -1;

		identity->DomainHintLength = (ULONG) (status - 1);
	}

	return 1;
}

int CopyCSPData(SEC_WINNT_AUTH_IDENTITY * identity, SEC_WINNT_AUTH_IDENTITY * srcIdentity, int identityCspDataLength)
{
	if( identity->CspData == NULL && srcIdentity->CspData != NULL && identityCspDataLength)
	{
		identity->CspData = (SEC_WINNT_AUTH_IDENTITY_CSPDATADETAIL*) calloc(1, sizeof(SEC_WINNT_AUTH_IDENTITY_CSPDATADETAIL) );
		if(!identity->CspData)
		{
			WLog_ERR( TAG, "Error allocation CspData identity ");
			return -1;
		}

		/* [0] keySpec */
		identity->CspData->KeySpec = srcIdentity->CspData->KeySpec;

		/* [1] cardName */
		identity->CspData->CardNameLength = srcIdentity->CspData->CardNameLength;

		if(identity->CspData->CardNameLength > 0)
			identity->CspData->CardName = srcIdentity->CspData->CardName;

		/* [2] readerName */
		identity->CspData->ReaderNameLength = srcIdentity->CspData->ReaderNameLength;

		if(identity->CspData->ReaderNameLength > 0)
			identity->CspData->ReaderName = srcIdentity->CspData->ReaderName;

		/* [3] containerName */
		identity->CspData->ContainerNameLength = srcIdentity->CspData->ContainerNameLength;

		if(identity->CspData->ContainerNameLength > 0)
			identity->CspData->ContainerName = srcIdentity->CspData->ContainerName;

		/* [4] cspName */
		identity->CspData->CspNameLength = srcIdentity->CspData->CspNameLength;

		if(identity->CspData->CspNameLength > 0)
			identity->CspData->CspName = srcIdentity->CspData->CspName;
	}
	else if ( srcIdentity->CspData == NULL ){
		WLog_ERR(TAG, "Error src CspData NULL");
		return -1;
	}

	return 1;
}

int setCSPData(int status, SEC_WINNT_AUTH_IDENTITY_CSPDATADETAIL ** pIdentityCspData, const UINT32 keySpec, const char * cardName, const char * readerName, const char * containerName, const char * cspName)
{
	*pIdentityCspData = (SEC_WINNT_AUTH_IDENTITY_CSPDATADETAIL*) calloc( 1, sizeof(SEC_WINNT_AUTH_IDENTITY_CSPDATADETAIL) );

	if(*pIdentityCspData == NULL)
	{
		WLog_ERR( TAG, "Error allocation identity CspData");
		return -1;
	}

	(*pIdentityCspData)->KeySpec = keySpec;

	if ((*pIdentityCspData)->CardName)
		free((*pIdentityCspData)->CardName);

	(*pIdentityCspData)->CardName = (UINT16*) NULL;
	(*pIdentityCspData)->CardNameLength = 0;

	if (cardName)
	{
		status = ConvertToUnicode(CP_UTF8, 0, cardName, -1, (LPWSTR*) &((*pIdentityCspData)->CardName), 0);

		if (status <= 0)
			return -1;

		(*pIdentityCspData)->CardNameLength = (ULONG) (status - 1);
	}

	if ((*pIdentityCspData)->ReaderName)
		free((*pIdentityCspData)->ReaderName);

	(*pIdentityCspData)->ReaderName = (UINT16*) NULL;
	(*pIdentityCspData)->ReaderNameLength = 0;

	if (readerName)
	{
		status = ConvertToUnicode(CP_UTF8, 0, readerName, -1, (LPWSTR*) &((*pIdentityCspData)->ReaderName), 0);

		if (status <= 0)
			return -1;

		(*pIdentityCspData)->ReaderNameLength = (ULONG) (status - 1);
	}

	if ((*pIdentityCspData)->ContainerName)
		free((*pIdentityCspData)->ContainerName);

	(*pIdentityCspData)->ContainerName = (UINT16*) NULL;
	(*pIdentityCspData)->ContainerNameLength = 0;

	if (containerName)
	{
		status = ConvertToUnicode(CP_UTF8, 0, containerName, -1, (LPWSTR*) &((*pIdentityCspData)->ContainerName), 0);

		if (status <= 0)
			return -1;

		(*pIdentityCspData)->ContainerNameLength = (ULONG) (status - 1);
	}

	if ((*pIdentityCspData)->CspName)
		free((*pIdentityCspData)->CspName);

	(*pIdentityCspData)->CspName = (UINT16*) NULL;
	(*pIdentityCspData)->CspNameLength = 0;

	if (cspName)
	{
		status = ConvertToUnicode(CP_UTF8, 0, cspName, -1, (LPWSTR*) &((*pIdentityCspData)->CspName), 0);

		if (status <= 0)
			return -1;

		(*pIdentityCspData)->CspNameLength = (ULONG) (status - 1);
	}

	return ( (*pIdentityCspData)->CardNameLength + (*pIdentityCspData)->ReaderNameLength + (*pIdentityCspData)->ContainerNameLength + (*pIdentityCspData)->CspNameLength );
}

int sspi_CopyAuthIdentity(SEC_WINNT_AUTH_IDENTITY* identity, SEC_WINNT_AUTH_IDENTITY* srcIdentity)
{
	int status;

	if (srcIdentity->Flags == SEC_WINNT_AUTH_IDENTITY_ANSI)
	{
		if(srcIdentity->Password)
		{
			status = sspi_SetAuthIdentity(identity, (char*) srcIdentity->User,
					(char*) srcIdentity->Domain, (char*) srcIdentity->Password);
		}
		else
		{
			status = sspi_SetAuthIdentity_Smartcard(identity, (char*) srcIdentity->Pin, (UINT32) srcIdentity->CspData->KeySpec,
							(char*) srcIdentity->CspData->CardName, (char*) srcIdentity->CspData->ReaderName, (char*) srcIdentity->CspData->ContainerName,
							(char*) srcIdentity->CspData->CspName, (char*) srcIdentity->UserHint, (char*) srcIdentity->DomainHint);
		}

		if (status <= 0)
			return -1;

		identity->Flags = SEC_WINNT_AUTH_IDENTITY_UNICODE;

		return 1;
	}

	identity->Flags = SEC_WINNT_AUTH_IDENTITY_UNICODE;

	if(srcIdentity->Password)
	{
		/* login/password authentication */
		identity->User = identity->Domain = identity->Password = NULL;

		identity->UserLength = srcIdentity->UserLength;

		if (identity->UserLength > 0)
		{
			identity->User = (UINT16*) malloc((identity->UserLength + 1) * sizeof(WCHAR));

			if (!identity->User)
				return -1;

			CopyMemory(identity->User, srcIdentity->User, identity->UserLength * sizeof(WCHAR));
			identity->User[identity->UserLength] = 0;
		}

		identity->DomainLength = srcIdentity->DomainLength;

		if (identity->DomainLength > 0)
		{
			identity->Domain = (UINT16*) malloc((identity->DomainLength + 1) * sizeof(WCHAR));

			if (!identity->Domain)
				return -1;

			CopyMemory(identity->Domain, srcIdentity->Domain, identity->DomainLength * sizeof(WCHAR));
			identity->Domain[identity->DomainLength] = 0;
		}

		identity->PasswordLength = srcIdentity->PasswordLength;

		if (identity->PasswordLength > 256)
			identity->PasswordLength /= SSPI_CREDENTIALS_HASH_LENGTH_FACTOR;

		if (srcIdentity->Password)
		{
			identity->Password = (UINT16*) malloc((identity->PasswordLength + 1) * sizeof(WCHAR));

			if (!identity->Password)
				return -1;

			CopyMemory(identity->Password, srcIdentity->Password, identity->PasswordLength * sizeof(WCHAR));
			identity->Password[identity->PasswordLength] = 0;
		}
		/* End of login/password authentication */
	}
	else if( srcIdentity->CspData )
	{
		/* smartcard authentication */
		/* [0] pin */
		identity->PinLength = srcIdentity->PinLength;

		if (srcIdentity->Pin)
		{
			identity->Pin = (UINT16*) malloc((identity->PinLength + 1) * sizeof(WCHAR));

			if (!identity->Pin)
				return -1;

			CopyMemory(identity->Pin, srcIdentity->Pin, identity->PinLength * sizeof(WCHAR));
			identity->Pin[identity->PinLength] = '\0';
		}

		/* [1] cspData */
		int identityCspDataLength = sizeof(srcIdentity->CspData->KeySpec) + srcIdentity->CspData->CardNameLength + srcIdentity->CspData->ReaderNameLength
									+ srcIdentity->CspData->ContainerNameLength + srcIdentity->CspData->CspNameLength;

		if(identityCspDataLength != 0)
		{
			if( CopyCSPData(identity, srcIdentity, identityCspDataLength) < 0 )
			{
				free(srcIdentity->CspData);
				srcIdentity->CspData = NULL;
				identity->CspData = NULL;
				return -1;
			}
		}

		/* [2] userHint */
		identity->UserHint = identity->DomainHint = NULL;

		identity->UserHintLength = srcIdentity->UserHintLength;

		if (identity->UserHintLength > 0)
		{
			identity->UserHint = (UINT16*) malloc((identity->UserHintLength + 1) * sizeof(WCHAR));

			if (!identity->UserHint)
				return -1;

			CopyMemory(identity->UserHint, srcIdentity->UserHint, identity->UserHintLength * sizeof(WCHAR));
			identity->UserHint[identity->UserHintLength] = '\0';
		}

		/* [3] domainHint */
		identity->DomainHintLength = srcIdentity->DomainHintLength;

		if (identity->DomainHintLength > 0)
		{
			identity->DomainHint = (UINT16*) malloc((identity->DomainHintLength + 1) * sizeof(WCHAR));

			if (!identity->DomainHint)
				return -1;

			CopyMemory(identity->DomainHint, srcIdentity->DomainHint, identity->DomainHintLength * sizeof(WCHAR));
			identity->DomainHint[identity->DomainHintLength] = '\0';
		}
	/* End of smartcard authentication */
	}

	return 1;
}

PSecBuffer sspi_FindSecBuffer(PSecBufferDesc pMessage, ULONG BufferType)
{
	ULONG index;
	PSecBuffer pSecBuffer = NULL;

	for (index = 0; index < pMessage->cBuffers; index++)
	{
		if (pMessage->pBuffers[index].BufferType == BufferType)
		{
			pSecBuffer = &pMessage->pBuffers[index];
			break;
		}
	}

	return pSecBuffer;
}

static BOOL sspi_initialized = FALSE;

void sspi_GlobalInit()
{
	if (!sspi_initialized)
	{
		winpr_InitializeSSL(WINPR_SSL_INIT_DEFAULT);

		sspi_ContextBufferAllocTableNew();
		sspi_initialized = TRUE;
	}
}

void sspi_GlobalFinish()
{
	if (sspi_initialized)
	{
		sspi_ContextBufferAllocTableFree();
	}

	sspi_initialized = FALSE;
}

SecurityFunctionTableA* sspi_GetSecurityFunctionTableAByNameA(const SEC_CHAR* Name)
{
	int index;
	UINT32 cPackages;

	cPackages = sizeof(SecPkgInfoA_LIST) / sizeof(*(SecPkgInfoA_LIST));

	for (index = 0; index < (int) cPackages; index++)
	{
		if (strcmp(Name, SecurityFunctionTableA_NAME_LIST[index].Name) == 0)
		{
			return (SecurityFunctionTableA*) SecurityFunctionTableA_NAME_LIST[index].SecurityFunctionTable;
		}
	}

	return NULL;
}

SecurityFunctionTableA* sspi_GetSecurityFunctionTableAByNameW(const SEC_WCHAR* Name)
{
	return NULL;
}

SecurityFunctionTableW* sspi_GetSecurityFunctionTableWByNameW(const SEC_WCHAR* Name)
{
	int index;
	UINT32 cPackages;

	cPackages = sizeof(SecPkgInfoW_LIST) / sizeof(*(SecPkgInfoW_LIST));

	for (index = 0; index < (int) cPackages; index++)
	{
		if (lstrcmpW(Name, SecurityFunctionTableW_NAME_LIST[index].Name) == 0)
		{
			return (SecurityFunctionTableW*) SecurityFunctionTableW_NAME_LIST[index].SecurityFunctionTable;
		}
	}

	return NULL;
}

SecurityFunctionTableW* sspi_GetSecurityFunctionTableWByNameA(const SEC_CHAR* Name)
{
	int status;
	SEC_WCHAR* NameW = NULL;
	SecurityFunctionTableW* table;

	status = ConvertToUnicode(CP_UTF8, 0, Name, -1, &NameW, 0);

	if (status <= 0)
		return NULL;

	table = sspi_GetSecurityFunctionTableWByNameW(NameW);
	free(NameW);

	return table;
}

void FreeContextBuffer_EnumerateSecurityPackages(void* contextBuffer);
void FreeContextBuffer_QuerySecurityPackageInfo(void* contextBuffer);

void sspi_ContextBufferFree(void* contextBuffer)
{
	int index;
	UINT32 allocatorIndex;

	for (index = 0; index < (int) ContextBufferAllocTable.cMaxEntries; index++)
	{
		if (contextBuffer == ContextBufferAllocTable.entries[index].contextBuffer)
		{
			contextBuffer = ContextBufferAllocTable.entries[index].contextBuffer;
			allocatorIndex = ContextBufferAllocTable.entries[index].allocatorIndex;

			ContextBufferAllocTable.cEntries--;

			ContextBufferAllocTable.entries[index].allocatorIndex = 0;
			ContextBufferAllocTable.entries[index].contextBuffer = NULL;

			switch (allocatorIndex)
			{
			case EnumerateSecurityPackagesIndex:
				FreeContextBuffer_EnumerateSecurityPackages(contextBuffer);
				break;

			case QuerySecurityPackageInfoIndex:
				FreeContextBuffer_QuerySecurityPackageInfo(contextBuffer);
				break;
			}
		}
	}
}

/**
 * Standard SSPI API
 */

/* Package Management */

SECURITY_STATUS SEC_ENTRY winpr_EnumerateSecurityPackagesW(ULONG* pcPackages, PSecPkgInfoW* ppPackageInfo)
{
	int index;
	size_t size;
	UINT32 cPackages;
	SecPkgInfoW* pPackageInfo;

	cPackages = sizeof(SecPkgInfoW_LIST) / sizeof(*(SecPkgInfoW_LIST));
	size = sizeof(SecPkgInfoW) * cPackages;

	pPackageInfo = (SecPkgInfoW*) sspi_ContextBufferAlloc(EnumerateSecurityPackagesIndex, size);

	if (!pPackageInfo)
		return SEC_E_INSUFFICIENT_MEMORY;

	for (index = 0; index < (int) cPackages; index++)
	{
		pPackageInfo[index].fCapabilities = SecPkgInfoW_LIST[index]->fCapabilities;
		pPackageInfo[index].wVersion = SecPkgInfoW_LIST[index]->wVersion;
		pPackageInfo[index].wRPCID = SecPkgInfoW_LIST[index]->wRPCID;
		pPackageInfo[index].cbMaxToken = SecPkgInfoW_LIST[index]->cbMaxToken;
		pPackageInfo[index].Name = _wcsdup(SecPkgInfoW_LIST[index]->Name);
		pPackageInfo[index].Comment = _wcsdup(SecPkgInfoW_LIST[index]->Comment);
	}

	*(pcPackages) = cPackages;
	*(ppPackageInfo) = pPackageInfo;

	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY winpr_EnumerateSecurityPackagesA(ULONG* pcPackages, PSecPkgInfoA* ppPackageInfo)
{
	int index;
	size_t size;
	UINT32 cPackages;
	SecPkgInfoA* pPackageInfo;

	cPackages = sizeof(SecPkgInfoA_LIST) / sizeof(*(SecPkgInfoA_LIST));
	size = sizeof(SecPkgInfoA) * cPackages;

	pPackageInfo = (SecPkgInfoA*) sspi_ContextBufferAlloc(EnumerateSecurityPackagesIndex, size);

	if (!pPackageInfo)
		return SEC_E_INSUFFICIENT_MEMORY;

	for (index = 0; index < (int) cPackages; index++)
	{
		pPackageInfo[index].fCapabilities = SecPkgInfoA_LIST[index]->fCapabilities;
		pPackageInfo[index].wVersion = SecPkgInfoA_LIST[index]->wVersion;
		pPackageInfo[index].wRPCID = SecPkgInfoA_LIST[index]->wRPCID;
		pPackageInfo[index].cbMaxToken = SecPkgInfoA_LIST[index]->cbMaxToken;
		pPackageInfo[index].Name = _strdup(SecPkgInfoA_LIST[index]->Name);
		pPackageInfo[index].Comment = _strdup(SecPkgInfoA_LIST[index]->Comment);
		if (!pPackageInfo[index].Name || !pPackageInfo[index].Comment)
		{
			sspi_ContextBufferFree(pPackageInfo);
			return SEC_E_INSUFFICIENT_MEMORY;
		}
	}

	*(pcPackages) = cPackages;
	*(ppPackageInfo) = pPackageInfo;

	return SEC_E_OK;
}

void FreeContextBuffer_EnumerateSecurityPackages(void* contextBuffer)
{
	int index;
	UINT32 cPackages;
	SecPkgInfoA* pPackageInfo = (SecPkgInfoA*) contextBuffer;

	cPackages = sizeof(SecPkgInfoA_LIST) / sizeof(*(SecPkgInfoA_LIST));

	for (index = 0; index < (int) cPackages; index++)
	{
		free(pPackageInfo[index].Name);
		free(pPackageInfo[index].Comment);
	}

	free(pPackageInfo);
}

SecurityFunctionTableW* SEC_ENTRY winpr_InitSecurityInterfaceW(void)
{
	return &winpr_SecurityFunctionTableW;
}

SecurityFunctionTableA* SEC_ENTRY winpr_InitSecurityInterfaceA(void)
{
	return &winpr_SecurityFunctionTableA;
}

SECURITY_STATUS SEC_ENTRY winpr_QuerySecurityPackageInfoW(SEC_WCHAR* pszPackageName, PSecPkgInfoW* ppPackageInfo)
{
	int index;
	size_t size;
	UINT32 cPackages;
	SecPkgInfoW* pPackageInfo;

	cPackages = sizeof(SecPkgInfoW_LIST) / sizeof(*(SecPkgInfoW_LIST));

	for (index = 0; index < (int) cPackages; index++)
	{
		if (lstrcmpW(pszPackageName, SecPkgInfoW_LIST[index]->Name) == 0)
		{
			size = sizeof(SecPkgInfoW);
			pPackageInfo = (SecPkgInfoW*) sspi_ContextBufferAlloc(QuerySecurityPackageInfoIndex, size);

			if (!pPackageInfo)
				return SEC_E_INSUFFICIENT_MEMORY;

			pPackageInfo->fCapabilities = SecPkgInfoW_LIST[index]->fCapabilities;
			pPackageInfo->wVersion = SecPkgInfoW_LIST[index]->wVersion;
			pPackageInfo->wRPCID = SecPkgInfoW_LIST[index]->wRPCID;
			pPackageInfo->cbMaxToken = SecPkgInfoW_LIST[index]->cbMaxToken;
			pPackageInfo->Name = _wcsdup(SecPkgInfoW_LIST[index]->Name);
			pPackageInfo->Comment = _wcsdup(SecPkgInfoW_LIST[index]->Comment);

			*(ppPackageInfo) = pPackageInfo;

			return SEC_E_OK;
		}
	}

	*(ppPackageInfo) = NULL;

	return SEC_E_SECPKG_NOT_FOUND;
}

SECURITY_STATUS SEC_ENTRY winpr_QuerySecurityPackageInfoA(SEC_CHAR* pszPackageName, PSecPkgInfoA* ppPackageInfo)
{
	int index;
	size_t size;
	UINT32 cPackages;
	SecPkgInfoA* pPackageInfo;

	cPackages = sizeof(SecPkgInfoA_LIST) / sizeof(*(SecPkgInfoA_LIST));

	for (index = 0; index < (int) cPackages; index++)
	{
		if (strcmp(pszPackageName, SecPkgInfoA_LIST[index]->Name) == 0)
		{
			size = sizeof(SecPkgInfoA);
			pPackageInfo = (SecPkgInfoA*) sspi_ContextBufferAlloc(QuerySecurityPackageInfoIndex, size);

			if (!pPackageInfo)
				return SEC_E_INSUFFICIENT_MEMORY;

			pPackageInfo->fCapabilities = SecPkgInfoA_LIST[index]->fCapabilities;
			pPackageInfo->wVersion = SecPkgInfoA_LIST[index]->wVersion;
			pPackageInfo->wRPCID = SecPkgInfoA_LIST[index]->wRPCID;
			pPackageInfo->cbMaxToken = SecPkgInfoA_LIST[index]->cbMaxToken;
			pPackageInfo->Name = _strdup(SecPkgInfoA_LIST[index]->Name);
			pPackageInfo->Comment = _strdup(SecPkgInfoA_LIST[index]->Comment);
			if (!pPackageInfo->Name || !pPackageInfo->Comment)
			{
				sspi_ContextBufferFree(pPackageInfo);
				return SEC_E_INSUFFICIENT_MEMORY;
			}

			*(ppPackageInfo) = pPackageInfo;

			return SEC_E_OK;
		}
	}

	*(ppPackageInfo) = NULL;

	return SEC_E_SECPKG_NOT_FOUND;
}

void FreeContextBuffer_QuerySecurityPackageInfo(void* contextBuffer)
{
	SecPkgInfo* pPackageInfo = (SecPkgInfo*) contextBuffer;

	if (!pPackageInfo)
		return;

	free(pPackageInfo->Name);
	free(pPackageInfo->Comment);
	free(pPackageInfo);
}

/* Credential Management */

SECURITY_STATUS SEC_ENTRY winpr_AcquireCredentialsHandleW(SEC_WCHAR* pszPrincipal, SEC_WCHAR* pszPackage,
							  ULONG fCredentialUse, void* pvLogonID, void* pAuthData, SEC_GET_KEY_FN pGetKeyFn,
							  void* pvGetKeyArgument, PCredHandle phCredential, PTimeStamp ptsExpiry)
{
	SECURITY_STATUS status;
	SecurityFunctionTableW* table = sspi_GetSecurityFunctionTableWByNameW(pszPackage);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (!table->AcquireCredentialsHandleW)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->AcquireCredentialsHandleW(pszPrincipal, pszPackage, fCredentialUse,
						  pvLogonID, pAuthData, pGetKeyFn, pvGetKeyArgument, phCredential, ptsExpiry);

	if (IsSecurityStatusError(status))
	{
		WLog_WARN(TAG, "AcquireCredentialsHandleW status %s [0x%08"PRIX32"]",
			  GetSecurityStatusString(status), status);
	}

	return status;
}

SECURITY_STATUS SEC_ENTRY winpr_AcquireCredentialsHandleA(SEC_CHAR* pszPrincipal, SEC_CHAR* pszPackage,
							  ULONG fCredentialUse, void* pvLogonID, void* pAuthData, SEC_GET_KEY_FN pGetKeyFn,
							  void* pvGetKeyArgument, PCredHandle phCredential, PTimeStamp ptsExpiry)
{
	SECURITY_STATUS status;
	SecurityFunctionTableA* table = sspi_GetSecurityFunctionTableAByNameA(pszPackage);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (!table->AcquireCredentialsHandleA)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->AcquireCredentialsHandleA(pszPrincipal, pszPackage, fCredentialUse,
						  pvLogonID, pAuthData, pGetKeyFn, pvGetKeyArgument, phCredential, ptsExpiry);

	if (IsSecurityStatusError(status))
	{
		WLog_WARN(TAG, "AcquireCredentialsHandleA status %s [0x%08"PRIX32"]",
			  GetSecurityStatusString(status), status);
	}

	return status;
}

SECURITY_STATUS SEC_ENTRY winpr_ExportSecurityContext(PCtxtHandle phContext, ULONG fFlags, PSecBuffer pPackedContext, HANDLE* pToken)
{
	SEC_CHAR* Name;
	SECURITY_STATUS status;
	SecurityFunctionTableW* table;

	Name = (SEC_CHAR*) sspi_SecureHandleGetUpperPointer(phContext);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableWByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (!table->ExportSecurityContext)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->ExportSecurityContext(phContext, fFlags, pPackedContext, pToken);

	if (IsSecurityStatusError(status))
	{
		WLog_WARN(TAG, "ExportSecurityContext status %s [0x%08"PRIX32"]",
			  GetSecurityStatusString(status), status);
	}

	return status;
}

SECURITY_STATUS SEC_ENTRY winpr_FreeCredentialsHandle(PCredHandle phCredential)
{
	char* Name;
	SECURITY_STATUS status;
	SecurityFunctionTableA* table;

	Name = (char*) sspi_SecureHandleGetUpperPointer(phCredential);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableAByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (!table->FreeCredentialsHandle)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->FreeCredentialsHandle(phCredential);

	if (IsSecurityStatusError(status))
	{
		WLog_WARN(TAG, "FreeCredentialsHandle status %s [0x%08"PRIX32"]",
			  GetSecurityStatusString(status), status);
	}

	return status;
}

SECURITY_STATUS SEC_ENTRY winpr_ImportSecurityContextW(SEC_WCHAR* pszPackage, PSecBuffer pPackedContext, HANDLE pToken, PCtxtHandle phContext)
{
	SEC_CHAR* Name;
	SECURITY_STATUS status;
	SecurityFunctionTableW* table;

	Name = (SEC_CHAR*) sspi_SecureHandleGetUpperPointer(phContext);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableWByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (!table->ImportSecurityContextW)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->ImportSecurityContextW(pszPackage, pPackedContext, pToken, phContext);

	if (IsSecurityStatusError(status))
	{
		WLog_WARN(TAG, "ImportSecurityContextW status %s [0x%08"PRIX32"]",
			  GetSecurityStatusString(status), status);
	}

	return status;
}

SECURITY_STATUS SEC_ENTRY winpr_ImportSecurityContextA(SEC_CHAR* pszPackage, PSecBuffer pPackedContext, HANDLE pToken, PCtxtHandle phContext)
{
	char* Name = NULL;
	SECURITY_STATUS status;
	SecurityFunctionTableA* table;

	Name = (char*) sspi_SecureHandleGetUpperPointer(phContext);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableAByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (!table->ImportSecurityContextA)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->ImportSecurityContextA(pszPackage, pPackedContext, pToken, phContext);

	if (IsSecurityStatusError(status))
	{
		WLog_WARN(TAG, "ImportSecurityContextA status %s [0x%08"PRIX32"]",
			  GetSecurityStatusString(status), status);
	}

	return status;
}

SECURITY_STATUS SEC_ENTRY winpr_QueryCredentialsAttributesW(PCredHandle phCredential, ULONG ulAttribute, void* pBuffer)
{
	SEC_WCHAR* Name;
	SECURITY_STATUS status;
	SecurityFunctionTableW* table;

	Name = (SEC_WCHAR*) sspi_SecureHandleGetUpperPointer(phCredential);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableWByNameW(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (!table->QueryCredentialsAttributesW)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->QueryCredentialsAttributesW(phCredential, ulAttribute, pBuffer);

	if (IsSecurityStatusError(status))
	{
		WLog_WARN(TAG, "QueryCredentialsAttributesW status %s [0x%08"PRIX32"]",
			  GetSecurityStatusString(status), status);
	}

	return status;
}

SECURITY_STATUS SEC_ENTRY winpr_QueryCredentialsAttributesA(PCredHandle phCredential, ULONG ulAttribute, void* pBuffer)
{
	char* Name;
	SECURITY_STATUS status;
	SecurityFunctionTableA* table;

	Name = (char*) sspi_SecureHandleGetUpperPointer(phCredential);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableAByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (!table->QueryCredentialsAttributesA)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->QueryCredentialsAttributesA(phCredential, ulAttribute, pBuffer);

	if (IsSecurityStatusError(status))
	{
		WLog_WARN(TAG, "QueryCredentialsAttributesA status %s [0x%08"PRIX32"]",
			  GetSecurityStatusString(status), status);
	}

	return status;
}

/* Context Management */

SECURITY_STATUS SEC_ENTRY winpr_AcceptSecurityContext(PCredHandle phCredential, PCtxtHandle phContext,
						      PSecBufferDesc pInput, ULONG fContextReq, ULONG TargetDataRep, PCtxtHandle phNewContext,
						      PSecBufferDesc pOutput, PULONG pfContextAttr, PTimeStamp ptsTimeStamp)
{
	char* Name;
	SECURITY_STATUS status;
	SecurityFunctionTableA* table;

	Name = (char*) sspi_SecureHandleGetUpperPointer(phCredential);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableAByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (!table->AcceptSecurityContext)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->AcceptSecurityContext(phCredential, phContext, pInput, fContextReq,
					      TargetDataRep, phNewContext, pOutput, pfContextAttr, ptsTimeStamp);

	if (IsSecurityStatusError(status))
	{
		WLog_WARN(TAG, "AcceptSecurityContext status %s [0x%08"PRIX32"]",
			  GetSecurityStatusString(status), status);
	}

	return status;
}

SECURITY_STATUS SEC_ENTRY winpr_ApplyControlToken(PCtxtHandle phContext, PSecBufferDesc pInput)
{
	char* Name = NULL;
	SECURITY_STATUS status;
	SecurityFunctionTableA* table;

	Name = (char*) sspi_SecureHandleGetUpperPointer(phContext);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableAByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (!table->ApplyControlToken)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->ApplyControlToken(phContext, pInput);

	if (IsSecurityStatusError(status))
	{
		WLog_WARN(TAG, "ApplyControlToken status %s [0x%08"PRIX32"]",
			  GetSecurityStatusString(status), status);
	}

	return status;
}

SECURITY_STATUS SEC_ENTRY winpr_CompleteAuthToken(PCtxtHandle phContext, PSecBufferDesc pToken)
{
	char* Name = NULL;
	SECURITY_STATUS status;
	SecurityFunctionTableA* table;

	Name = (char*) sspi_SecureHandleGetUpperPointer(phContext);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableAByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (!table->CompleteAuthToken)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->CompleteAuthToken(phContext, pToken);

	if (IsSecurityStatusError(status))
	{
		WLog_WARN(TAG, "CompleteAuthToken status %s [0x%08"PRIX32"]",
			  GetSecurityStatusString(status), status);
	}

	return status;
}

SECURITY_STATUS SEC_ENTRY winpr_DeleteSecurityContext(PCtxtHandle phContext)
{
	char* Name = NULL;
	SECURITY_STATUS status;
	SecurityFunctionTableA* table;

	Name = (char*) sspi_SecureHandleGetUpperPointer(phContext);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableAByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (!table->DeleteSecurityContext)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->DeleteSecurityContext(phContext);

	if (IsSecurityStatusError(status))
	{
		WLog_WARN(TAG, "DeleteSecurityContext status %s [0x%08"PRIX32"]",
			  GetSecurityStatusString(status), status);
	}

	return status;
}

SECURITY_STATUS SEC_ENTRY winpr_FreeContextBuffer(void* pvContextBuffer)
{
	if (!pvContextBuffer)
		return SEC_E_INVALID_HANDLE;

	sspi_ContextBufferFree(pvContextBuffer);

	return SEC_E_OK;
}

SECURITY_STATUS SEC_ENTRY winpr_ImpersonateSecurityContext(PCtxtHandle phContext)
{
	SEC_CHAR* Name;
	SECURITY_STATUS status;
	SecurityFunctionTableW* table;

	Name = (SEC_CHAR*) sspi_SecureHandleGetUpperPointer(phContext);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableWByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (!table->ImpersonateSecurityContext)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->ImpersonateSecurityContext(phContext);

	if (IsSecurityStatusError(status))
	{
		WLog_WARN(TAG, "ImpersonateSecurityContext status %s [0x%08"PRIX32"]",
			  GetSecurityStatusString(status), status);
	}

	return status;
}

SECURITY_STATUS SEC_ENTRY winpr_InitializeSecurityContextW(PCredHandle phCredential, PCtxtHandle phContext,
							   SEC_WCHAR* pszTargetName, ULONG fContextReq, ULONG Reserved1, ULONG TargetDataRep,
							   PSecBufferDesc pInput, ULONG Reserved2, PCtxtHandle phNewContext,
							   PSecBufferDesc pOutput, PULONG pfContextAttr, PTimeStamp ptsExpiry)
{
	SEC_CHAR* Name;
	SECURITY_STATUS status;
	SecurityFunctionTableW* table;

	Name = (SEC_CHAR*) sspi_SecureHandleGetUpperPointer(phCredential);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableWByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (!table->InitializeSecurityContextW)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->InitializeSecurityContextW(phCredential, phContext,
						   pszTargetName, fContextReq, Reserved1, TargetDataRep,
						   pInput, Reserved2, phNewContext, pOutput, pfContextAttr, ptsExpiry);

	if (IsSecurityStatusError(status))
	{
		WLog_WARN(TAG, "InitializeSecurityContextW status %s [0x%08"PRIX32"]",
			  GetSecurityStatusString(status), status);
	}

	return status;
}

SECURITY_STATUS SEC_ENTRY winpr_InitializeSecurityContextA(PCredHandle phCredential, PCtxtHandle phContext,
							   SEC_CHAR* pszTargetName, ULONG fContextReq, ULONG Reserved1, ULONG TargetDataRep,
							   PSecBufferDesc pInput, ULONG Reserved2, PCtxtHandle phNewContext,
							   PSecBufferDesc pOutput, PULONG pfContextAttr, PTimeStamp ptsExpiry)
{
	SEC_CHAR* Name;
	SECURITY_STATUS status;
	SecurityFunctionTableA* table;

	Name = (SEC_CHAR*) sspi_SecureHandleGetUpperPointer(phCredential);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableAByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (!table->InitializeSecurityContextA)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->InitializeSecurityContextA(phCredential, phContext,
						   pszTargetName, fContextReq, Reserved1, TargetDataRep,
						   pInput, Reserved2, phNewContext, pOutput, pfContextAttr, ptsExpiry);

	if (IsSecurityStatusError(status))
	{
		WLog_WARN(TAG, "InitializeSecurityContextA status %s [0x%08"PRIX32"]",
			  GetSecurityStatusString(status), status);
	}

	return status;
}

SECURITY_STATUS SEC_ENTRY winpr_QueryContextAttributesW(PCtxtHandle phContext, ULONG ulAttribute, void* pBuffer)
{
	SEC_CHAR* Name;
	SECURITY_STATUS status;
	SecurityFunctionTableW* table;

	Name = (SEC_CHAR*) sspi_SecureHandleGetUpperPointer(phContext);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableWByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (!table->QueryContextAttributesW)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->QueryContextAttributesW(phContext, ulAttribute, pBuffer);

	if (IsSecurityStatusError(status))
	{
		WLog_WARN(TAG, "QueryContextAttributesW status %s [0x%08"PRIX32"]",
			  GetSecurityStatusString(status), status);
	}

	return status;
}

SECURITY_STATUS SEC_ENTRY winpr_QueryContextAttributesA(PCtxtHandle phContext, ULONG ulAttribute, void* pBuffer)
{
	SEC_CHAR* Name;
	SECURITY_STATUS status;
	SecurityFunctionTableA* table;

	Name = (SEC_CHAR*) sspi_SecureHandleGetUpperPointer(phContext);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableAByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (!table->QueryContextAttributesA)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->QueryContextAttributesA(phContext, ulAttribute, pBuffer);

	if (IsSecurityStatusError(status))
	{
		WLog_WARN(TAG, "QueryContextAttributesA status %s [0x%08"PRIX32"]",
			  GetSecurityStatusString(status), status);
	}

	return status;
}

SECURITY_STATUS SEC_ENTRY winpr_QuerySecurityContextToken(PCtxtHandle phContext, HANDLE* phToken)
{
	SEC_CHAR* Name;
	SECURITY_STATUS status;
	SecurityFunctionTableW* table;

	Name = (SEC_CHAR*) sspi_SecureHandleGetUpperPointer(phContext);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableWByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (!table->QuerySecurityContextToken)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->QuerySecurityContextToken(phContext, phToken);

	if (IsSecurityStatusError(status))
	{
		WLog_WARN(TAG, "QuerySecurityContextToken status %s [0x%08"PRIX32"]",
			  GetSecurityStatusString(status), status);
	}

	return status;
}

SECURITY_STATUS SEC_ENTRY winpr_SetContextAttributesW(PCtxtHandle phContext, ULONG ulAttribute, void* pBuffer, ULONG cbBuffer)
{
	SEC_CHAR* Name;
	SECURITY_STATUS status;
	SecurityFunctionTableW* table;

	Name = (SEC_CHAR*) sspi_SecureHandleGetUpperPointer(phContext);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableWByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (!table->SetContextAttributesW)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->SetContextAttributesW(phContext, ulAttribute, pBuffer, cbBuffer);

	if (IsSecurityStatusError(status))
	{
		WLog_WARN(TAG, "SetContextAttributesW status %s [0x%08"PRIX32"]",
			  GetSecurityStatusString(status), status);
	}

	return status;
}

SECURITY_STATUS SEC_ENTRY winpr_SetContextAttributesA(PCtxtHandle phContext, ULONG ulAttribute, void* pBuffer, ULONG cbBuffer)
{
	char* Name;
	SECURITY_STATUS status;
	SecurityFunctionTableA* table;

	Name = (char*) sspi_SecureHandleGetUpperPointer(phContext);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableAByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (!table->SetContextAttributesA)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->SetContextAttributesA(phContext, ulAttribute, pBuffer, cbBuffer);

	if (IsSecurityStatusError(status))
	{
		WLog_WARN(TAG, "SetContextAttributesA status %s [0x%08"PRIX32"]",
			  GetSecurityStatusString(status), status);
	}

	return status;
}

SECURITY_STATUS SEC_ENTRY winpr_RevertSecurityContext(PCtxtHandle phContext)
{
	SEC_CHAR* Name;
	SECURITY_STATUS status;
	SecurityFunctionTableW* table;

	Name = (SEC_CHAR*) sspi_SecureHandleGetUpperPointer(phContext);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableWByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (!table->RevertSecurityContext)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->RevertSecurityContext(phContext);

	if (IsSecurityStatusError(status))
	{
		WLog_WARN(TAG, "RevertSecurityContext status %s [0x%08"PRIX32"]",
			  GetSecurityStatusString(status), status);
	}

	return status;
}

/* Message Support */

SECURITY_STATUS SEC_ENTRY winpr_DecryptMessage(PCtxtHandle phContext, PSecBufferDesc pMessage, ULONG MessageSeqNo, PULONG pfQOP)
{
	char* Name;
	SECURITY_STATUS status;
	SecurityFunctionTableA* table;

	Name = (char*) sspi_SecureHandleGetUpperPointer(phContext);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableAByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (!table->DecryptMessage)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->DecryptMessage(phContext, pMessage, MessageSeqNo, pfQOP);

	if (IsSecurityStatusError(status))
	{
		WLog_WARN(TAG, "DecryptMessage status %s [0x%08"PRIX32"]",
			  GetSecurityStatusString(status), status);
	}

	return status;
}

SECURITY_STATUS SEC_ENTRY winpr_EncryptMessage(PCtxtHandle phContext, ULONG fQOP, PSecBufferDesc pMessage, ULONG MessageSeqNo)
{
	char* Name;
	SECURITY_STATUS status;
	SecurityFunctionTableA* table;

	Name = (char*) sspi_SecureHandleGetUpperPointer(phContext);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableAByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (!table->EncryptMessage)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->EncryptMessage(phContext, fQOP, pMessage, MessageSeqNo);

	if (status != SEC_E_OK)
	{
		WLog_ERR(TAG, "EncryptMessage status %s [0x%08"PRIX32"]",
			 GetSecurityStatusString(status), status);
	}

	return status;
}

SECURITY_STATUS SEC_ENTRY winpr_MakeSignature(PCtxtHandle phContext, ULONG fQOP, PSecBufferDesc pMessage, ULONG MessageSeqNo)
{
	char* Name;
	SECURITY_STATUS status;
	SecurityFunctionTableA* table;

	Name = (char*) sspi_SecureHandleGetUpperPointer(phContext);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableAByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (!table->MakeSignature)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->MakeSignature(phContext, fQOP, pMessage, MessageSeqNo);

	if (IsSecurityStatusError(status))
	{
		WLog_WARN(TAG, "MakeSignature status %s [0x%08"PRIX32"]",
			  GetSecurityStatusString(status), status);
	}

	return status;
}

SECURITY_STATUS SEC_ENTRY winpr_VerifySignature(PCtxtHandle phContext, PSecBufferDesc pMessage, ULONG MessageSeqNo, PULONG pfQOP)
{
	char* Name;
	SECURITY_STATUS status;
	SecurityFunctionTableA* table;

	Name = (char*) sspi_SecureHandleGetUpperPointer(phContext);

	if (!Name)
		return SEC_E_SECPKG_NOT_FOUND;

	table = sspi_GetSecurityFunctionTableAByNameA(Name);

	if (!table)
		return SEC_E_SECPKG_NOT_FOUND;

	if (!table->VerifySignature)
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = table->VerifySignature(phContext, pMessage, MessageSeqNo, pfQOP);

	if (IsSecurityStatusError(status))
	{
		WLog_WARN(TAG, "VerifySignature status %s [0x%08"PRIX32"]",
			  GetSecurityStatusString(status), status);
	}

	return status;
}

SecurityFunctionTableA winpr_SecurityFunctionTableA =
{
	1, /* dwVersion */
	winpr_EnumerateSecurityPackagesA, /* EnumerateSecurityPackages */
	winpr_QueryCredentialsAttributesA, /* QueryCredentialsAttributes */
	winpr_AcquireCredentialsHandleA, /* AcquireCredentialsHandle */
	winpr_FreeCredentialsHandle, /* FreeCredentialsHandle */
	NULL, /* Reserved2 */
	winpr_InitializeSecurityContextA, /* InitializeSecurityContext */
	winpr_AcceptSecurityContext, /* AcceptSecurityContext */
	winpr_CompleteAuthToken, /* CompleteAuthToken */
	winpr_DeleteSecurityContext, /* DeleteSecurityContext */
	winpr_ApplyControlToken, /* ApplyControlToken */
	winpr_QueryContextAttributesA, /* QueryContextAttributes */
	winpr_ImpersonateSecurityContext, /* ImpersonateSecurityContext */
	winpr_RevertSecurityContext, /* RevertSecurityContext */
	winpr_MakeSignature, /* MakeSignature */
	winpr_VerifySignature, /* VerifySignature */
	winpr_FreeContextBuffer, /* FreeContextBuffer */
	winpr_QuerySecurityPackageInfoA, /* QuerySecurityPackageInfo */
	NULL, /* Reserved3 */
	NULL, /* Reserved4 */
	winpr_ExportSecurityContext, /* ExportSecurityContext */
	winpr_ImportSecurityContextA, /* ImportSecurityContext */
	NULL, /* AddCredentials */
	NULL, /* Reserved8 */
	winpr_QuerySecurityContextToken, /* QuerySecurityContextToken */
	winpr_EncryptMessage, /* EncryptMessage */
	winpr_DecryptMessage, /* DecryptMessage */
	winpr_SetContextAttributesA, /* SetContextAttributes */
};

SecurityFunctionTableW winpr_SecurityFunctionTableW =
{
	1, /* dwVersion */
	winpr_EnumerateSecurityPackagesW, /* EnumerateSecurityPackages */
	winpr_QueryCredentialsAttributesW, /* QueryCredentialsAttributes */
	winpr_AcquireCredentialsHandleW, /* AcquireCredentialsHandle */
	winpr_FreeCredentialsHandle, /* FreeCredentialsHandle */
	NULL, /* Reserved2 */
	winpr_InitializeSecurityContextW, /* InitializeSecurityContext */
	winpr_AcceptSecurityContext, /* AcceptSecurityContext */
	winpr_CompleteAuthToken, /* CompleteAuthToken */
	winpr_DeleteSecurityContext, /* DeleteSecurityContext */
	winpr_ApplyControlToken, /* ApplyControlToken */
	winpr_QueryContextAttributesW, /* QueryContextAttributes */
	winpr_ImpersonateSecurityContext, /* ImpersonateSecurityContext */
	winpr_RevertSecurityContext, /* RevertSecurityContext */
	winpr_MakeSignature, /* MakeSignature */
	winpr_VerifySignature, /* VerifySignature */
	winpr_FreeContextBuffer, /* FreeContextBuffer */
	winpr_QuerySecurityPackageInfoW, /* QuerySecurityPackageInfo */
	NULL, /* Reserved3 */
	NULL, /* Reserved4 */
	winpr_ExportSecurityContext, /* ExportSecurityContext */
	winpr_ImportSecurityContextW, /* ImportSecurityContext */
	NULL, /* AddCredentials */
	NULL, /* Reserved8 */
	winpr_QuerySecurityContextToken, /* QuerySecurityContextToken */
	winpr_EncryptMessage, /* EncryptMessage */
	winpr_DecryptMessage, /* DecryptMessage */
	winpr_SetContextAttributesW, /* SetContextAttributes */
};
