/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Network Level Authentication (NLA)
 *
 * Copyright 2010-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
 * Copyright 2016 Martin Fleisz <martin.fleisz@thincast.com>
 * Copyright 2017 Dorian Ducournau <dorian.ducournau@gmail.com>
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

#include <winpr/crt.h>
#include <winpr/sam.h>
#include <winpr/sspi.h>
#include <winpr/print.h>
#include <winpr/tchar.h>
#include <winpr/dsparse.h>
#include <winpr/library.h>
#include <winpr/registry.h>

#include "nla.h"

#include <pkcs11-helper-1.0/pkcs11.h>

static const char * PREFIX_CONTAINER_NAME = "0x";
static const char * PREFIX_PIN_GLOBAL = "CredProv&PIN Global&";

#define TAG FREERDP_TAG("core.nla")

#define SERVER_KEY "Software\\"FREERDP_VENDOR_STRING"\\" \
	FREERDP_PRODUCT_STRING"\\Server"

/**
 * TSRequest ::= SEQUENCE {
 * 	version    [0] INTEGER,
 * 	negoTokens [1] NegoData OPTIONAL,
 * 	authInfo   [2] OCTET STRING OPTIONAL,
 * 	pubKeyAuth [3] OCTET STRING OPTIONAL,
 * 	errorCode  [4] INTEGER OPTIONAL
 * }
 *
 * NegoData ::= SEQUENCE OF NegoDataItem
 *
 * NegoDataItem ::= SEQUENCE {
 * 	negoToken [0] OCTET STRING
 * }
 *
 * TSCredentials ::= SEQUENCE {
 * 	credType    [0] INTEGER,
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
 */

#define NLA_PKG_NAME	NEGOSSP_NAME

#define TERMSRV_SPN_PREFIX	"TERMSRV/"

static BOOL nla_send(rdpNla* nla);
static int nla_recv(rdpNla* nla);
static void nla_buffer_print(rdpNla* nla);
static void nla_buffer_free(rdpNla* nla);
static SECURITY_STATUS nla_encrypt_public_key_echo(rdpNla* nla);
static SECURITY_STATUS nla_decrypt_public_key_echo(rdpNla* nla);
static SECURITY_STATUS nla_encrypt_ts_credentials(rdpNla* nla);
static SECURITY_STATUS nla_decrypt_ts_credentials(rdpNla* nla);
static BOOL nla_read_ts_password_creds(rdpNla* nla, wStream* s);
static BOOL nla_read_ts_smartcard_creds(rdpNla* nla, wStream* s);
static void nla_identity_free(SEC_WINNT_AUTH_IDENTITY* identity);

#define ber_sizeof_sequence_octet_string(length) ber_sizeof_contextual_tag(ber_sizeof_octet_string(length)) + ber_sizeof_octet_string(length)
#define ber_write_sequence_octet_string(stream, context, value, length) ber_write_contextual_tag(stream, context, ber_sizeof_octet_string(length), TRUE) + ber_write_octet_string(stream, value, length)

void nla_identity_free(SEC_WINNT_AUTH_IDENTITY* identity)
{
	if (identity)
	{
		/* Password authentication */
		if (identity->User)
		{
			memset(identity->User, 0, identity->UserLength * 2);
			free(identity->User);
		}
		if (identity->Password)
		{
			memset(identity->Password, 0, identity->PasswordLength * 2);
			free(identity->Password);
		}
		if (identity->Domain)
		{
			memset(identity->Domain, 0, identity->DomainLength * 2);
			free(identity->Domain);
		}

		/* Smartcard authentication */
		if(identity->CspData)
		{
			if (identity->Pin)
			{
				rsize_t size_max_pin_formatted = ( strlen(PREFIX_PIN_GLOBAL) + PIN_LENGTH ) * 2;
				/* safely erase PIN buffer  */
				if( memset_s(identity->Pin, size_max_pin_formatted, 0, identity->PinLength * 2) == EINVAL )
					memset(identity->Pin, 0, identity->PinLength * 2);
				free(identity->Pin);
			}
			if (identity->CspData->CardName)
			{
				memset(identity->CspData->CardName, 0, identity->CspData->CardNameLength * 2);
				free(identity->CspData->CardName);
			}
			if (identity->CspData->ReaderName)
			{
				memset(identity->CspData->ReaderName, 0, identity->CspData->ReaderNameLength * 2);
				free(identity->CspData->ReaderName);
			}
			if (identity->CspData->ContainerName)
			{
				memset(identity->CspData->ContainerName, 0, identity->CspData->ContainerNameLength * 2);
				free(identity->CspData->ContainerName);
			}
			if (identity->CspData->CspName)
			{
				memset(identity->CspData->CspName, 0, identity->CspData->CspNameLength * 2);
				free(identity->CspData->CspName);
			}
			if (identity->CspData)
			{
				memset(identity->CspData, 0, sizeof( SEC_WINNT_AUTH_IDENTITY_CSPDATADETAIL ) );
				free(identity->CspData);
			}
			if (identity->UserHint)
			{
				memset(identity->UserHint, 0, identity->UserHintLength * 2);
				free(identity->UserHint);
			}
			if (identity->DomainHint)
			{
				memset(identity->DomainHint, 0, identity->DomainHintLength * 2);
				free(identity->DomainHint);
			}
		}
	}

	free(identity);
}

/**
 * Initialize NTLM/Kerberos SSP authentication module (client).
 * @param credssp
 */

int nla_client_init(rdpNla* nla)
{
	char* spn;
	int length;
	rdpTls* tls = NULL;
	BOOL PromptPassword = FALSE;
	BOOL PromptPin = FALSE;
	freerdp* instance = nla->instance;
	rdpSettings* settings = nla->settings;
	WINPR_SAM* sam;
	WINPR_SAM_ENTRY* entry;

	nla->state = NLA_STATE_INITIAL;
	nla->credType = SEC_DEFAULT_DELEGATION_CRED_TYPE;

	if (settings->RestrictedAdminModeRequired)
		settings->DisableCredentialsDelegation = TRUE;

	if ( (!settings->Password) || (!settings->Username)
			|| (!strlen(settings->Password)) || (!strlen(settings->Username)) )
	{
		PromptPassword = TRUE;
	}

	if( settings->SmartcardLogon )
		PromptPin = TRUE;

	if (PromptPassword && settings->Username && strlen(settings->Username))
	{
		sam = SamOpen(NULL, TRUE);

		if (sam)
		{
			entry = SamLookupUserA(sam, settings->Username, strlen(settings->Username), NULL, 0);

			if (entry)
			{
				/**
				 * The user could be found in SAM database.
				 * Use entry in SAM database later instead of prompt
				 */
				PromptPassword = FALSE;
				SamFreeEntry(sam, entry);
			}

			SamClose(sam);
		}
	}

#ifndef _WIN32

	if (PromptPassword)
	{
		if (settings->RestrictedAdminModeRequired)
		{
			if ((settings->PasswordHash) && (strlen(settings->PasswordHash) > 0))
				PromptPassword = FALSE;
		}
	}

#endif

	if (PromptPassword || PromptPin)
	{
		if (instance->Authenticate)
		{
			BOOL proceed = instance->Authenticate(instance,
			                                      &settings->Username, &settings->Password, &settings->Domain);

			if (!proceed)
			{
				freerdp_set_last_error(instance->context, FREERDP_ERROR_CONNECT_CANCELLED);
				return 0;
			}
		}
	}

	if (!settings->SmartcardLogon && !settings->Username)
	{
		nla_identity_free(nla->identity);
		nla->identity = NULL;
	}
	else
	{
		if( settings->Username && !settings->SmartcardLogon ){
			if( sspi_SetAuthIdentity(nla->identity, settings->Username, settings->Domain,
					settings->Password) < 0 )
				return -1;
		}
		else if( settings->SmartcardLogon && settings->CredentialsType == SEC_SMARTCARD_DELEGATION_CRED_TYPE ){
			nla->credType = settings->CredentialsType;

#if defined(WITH_PKCS11H) && defined(WITH_GSSAPI)
			if( get_info_smartcard(nla) != CKR_OK ){
				WLog_ERR(TAG, "Failed to retrieve UPN !");
				return -1;
			}

			if( get_TGT_kerberos(settings) == FALSE ){
				WLog_ERR(TAG, "Failed to get TGT from KDC !");
				return -1;
			}
#else
			WLog_ERR(TAG, "Enable PKCS11H and GSSAPI features to authenticate via smartcard");
			return -1;
#endif

			settings->Password = calloc( strlen(PREFIX_PIN_GLOBAL) + PIN_LENGTH + 1, sizeof(char) );

			if( settings->Password == NULL )
				return -1;

			if(settings->PinPadIsPresent){
				strncat(settings->Password, PREFIX_PIN_GLOBAL, strlen(PREFIX_PIN_GLOBAL) );
				/* The middleware talking to the card performs PIN caching and will provide
				 * to its CSP (Cryptographic Service Provider) the PIN code
				 * when asked. If PIN caching fails, or is not handled by the middleware,
				 * the PIN code will be asked one more time before opening the session.
				 * Thus, entering PIN code on pinpad does not give the PIN code explicitly to the CSP.
				 * That's why we set it here to "0000".
				 * The PIN code is not communicated to any software module, nor central processing unit.
				 * Contrary to /pin option in command line or with getpass() which are less secure,
				 * because the PIN code is communicated (at the present) in clear and transit via the code.
				 */
				strncat(settings->Password, "0000", PIN_LENGTH);
				settings->Password[ strlen(PREFIX_PIN_GLOBAL) + PIN_LENGTH] = '\0';
			}
			else if (settings->Pin){
				strncat(settings->Password, PREFIX_PIN_GLOBAL, strlen(PREFIX_PIN_GLOBAL) );
				strncat(settings->Password, settings->Pin, PIN_LENGTH);
				settings->Password[ strlen(PREFIX_PIN_GLOBAL) + PIN_LENGTH] = '\0';
			}

			settings->Username = NULL;

			if( settings->UserPrincipalName != NULL ){
				settings->Username = _strdup( settings->UserPrincipalName);
				if( settings->Username == NULL){
					WLog_ERR(TAG, "Error strdup UPN-Username");
					return -1;
				}
			}

			if( settings->Domain != NULL )
				settings->DomainHint = settings->Domain;
			else
				return -1;

			if( settings->DomainHint != NULL ){
				if( settings->CanonicalizedUserHint != NULL ){
					settings->UserHint = settings->CanonicalizedUserHint;
				}
				else{
					WLog_ERR(TAG, "User Hint NOT canonicalized");
					return -1;
				}
			}

			settings->ContainerName = calloc( strlen(PREFIX_CONTAINER_NAME) + strlen( (const char *) settings->IdCertificate) + 1, sizeof(char) );
			if(settings->ContainerName == NULL)
				return -1;

			strncpy(settings->ContainerName, PREFIX_CONTAINER_NAME, strlen(PREFIX_CONTAINER_NAME));
			strncat(settings->ContainerName, (const char *) settings->IdCertificate, strlen( (const char *) settings->IdCertificate));
			settings->ContainerName[strlen(PREFIX_CONTAINER_NAME) + strlen( (const char *) settings->IdCertificate)] = '\0';

			if( (settings->CspName == NULL) || (settings->CspName != NULL && strlen(settings->CspName)==0) ){
				WLog_ERR(TAG, "/csp argument is mandatory for smartcard-logon ");
				return -1;
			}

			if( !settings->RedirectSmartCards && !settings->DeviceRedirection ){
				WLog_ERR(TAG, "/smartcard argument is mandatory for smartcard-logon ");
				return -1;
			}

			int ret = sspi_SetAuthIdentity_Smartcard(nla->identity, settings->Password, AT_KEYEXCHANGE,
					settings->CardName,
					settings->SmartcardReaderName,
					settings->ContainerName,
					settings->CspName,
					settings->UserHint,
					settings->DomainHint);

			if(ret < 0){
				WLog_ERR(TAG, "%s %d : Failed to set smartcard authentication parameters !", __FUNCTION__, __LINE__ );
				return -1;
			}
		}
	}

#if !defined(_WIN32) && !defined(WITH_PKCS11H)
	{
		SEC_WINNT_AUTH_IDENTITY* identity = nla->identity;

		if (!identity)
		{
			WLog_ERR(TAG, "NLA identity=%p", (void*) identity);
			return -1;
		}

		if (settings->RestrictedAdminModeRequired)
		{
			if (settings->PasswordHash)
			{
				if (strlen(settings->PasswordHash) == 32)
				{
					free(identity->Password);
					identity->PasswordLength = ConvertToUnicode(CP_UTF8, 0,
					                           settings->PasswordHash, -1, &identity->Password, 0) - 1;
					/**
					 * Multiply password hash length by 64 to obtain a length exceeding
					 * the maximum (256) and use it this for hash identification in WinPR.
					 */
					identity->PasswordLength = 32 * 64; /* 2048 */
				}
			}
		}
	}
#endif
	tls = nla->transport->tls;

	if (!tls)
	{
		WLog_ERR(TAG, "Unknown NLA transport layer");
		return -1;
	}

	if (!sspi_SecBufferAlloc(&nla->PublicKey, tls->PublicKeyLength))
	{
		WLog_ERR(TAG, "Failed to allocate sspi secBuffer");
		return -1;
	}

	CopyMemory(nla->PublicKey.pvBuffer, tls->PublicKey, tls->PublicKeyLength);
	length = sizeof(TERMSRV_SPN_PREFIX) + strlen(settings->ServerHostname);
	spn = (SEC_CHAR*) malloc(length + 1);

	if (!spn)
		return -1;

	sprintf(spn, "%s%s", TERMSRV_SPN_PREFIX, settings->ServerHostname);
#ifdef UNICODE
	nla->ServicePrincipalName = NULL;
	ConvertToUnicode(CP_UTF8, 0, spn, -1, &nla->ServicePrincipalName, 0);
	free(spn);
#else
	nla->ServicePrincipalName = spn;
#endif

	nla->table = InitSecurityInterfaceEx(0);

#ifdef WITH_GSSAPI /* KERBEROS SSP */

	nla->status = nla->table->QuerySecurityPackageInfo( KERBEROS_SSP_NAME, &nla->pPackageInfo);

	if (nla->status != SEC_E_OK)
	{
		WLog_ERR(TAG, "QuerySecurityPackageInfo status %s [0x%08"PRIX32"]",
				GetSecurityStatusString(nla->status), nla->status);
		return -1;
	}
#else /* NTML SSP */
	nla->status = nla->table->QuerySecurityPackageInfo( NLA_PKG_NAME, &nla->pPackageInfo);

	if (nla->status != SEC_E_OK)
	{
		WLog_ERR(TAG, "QuerySecurityPackageInfo status %s [0x%08"PRIX32"]",
			GetSecurityStatusString(nla->status), nla->status);
		return -1;
	}
#endif

	nla->cbMaxToken = nla->pPackageInfo->cbMaxToken;
	nla->packageName = nla->pPackageInfo->Name;

	WLog_DBG(TAG, "%s %"PRIu32" : packageName=%s ; cbMaxToken=%d", __FUNCTION__, __LINE__, nla->packageName, nla->cbMaxToken);

	nla->status = nla->table->AcquireCredentialsHandle(NULL, NLA_PKG_NAME,
			SECPKG_CRED_OUTBOUND, NULL, nla->identity, NULL, NULL, &nla->credentials,
			&nla->expiration);

	if (nla->status != SEC_E_OK)
	{
		WLog_ERR(TAG, "AcquireCredentialsHandle status %s [0x%08"PRIX32"]",
			 	GetSecurityStatusString(nla->status), nla->status);
		return -1;
	}

	nla->haveContext = FALSE;
	nla->haveInputBuffer = FALSE;
	nla->havePubKeyAuth = FALSE;
	ZeroMemory(&nla->inputBuffer, sizeof(SecBuffer));
	ZeroMemory(&nla->outputBuffer, sizeof(SecBuffer));
	ZeroMemory(&nla->ContextSizes, sizeof(SecPkgContext_Sizes));
	/*
	 * from tspkg.dll: 0x00000132
	 * ISC_REQ_MUTUAL_AUTH
	 * ISC_REQ_CONFIDENTIALITY
	 * ISC_REQ_USE_SESSION_KEY
	 * ISC_REQ_ALLOCATE_MEMORY
	 */
	nla->fContextReq = ISC_REQ_MUTUAL_AUTH | ISC_REQ_CONFIDENTIALITY | ISC_REQ_USE_SESSION_KEY;
	return 1;
}

int nla_client_begin(rdpNla* nla)
{
	if (nla_client_init(nla) < 1)
		return -1;

	if (nla->state != NLA_STATE_INITIAL)
		return -1;

	nla->outputBufferDesc.ulVersion = SECBUFFER_VERSION;
	nla->outputBufferDesc.cBuffers = 1;
	nla->outputBufferDesc.pBuffers = &nla->outputBuffer;
	nla->outputBuffer.BufferType = SECBUFFER_TOKEN;
	nla->outputBuffer.cbBuffer = nla->cbMaxToken;
	nla->outputBuffer.pvBuffer = malloc(nla->outputBuffer.cbBuffer);

	if (!nla->outputBuffer.pvBuffer)
		return -1;

	nla->status = nla->table->InitializeSecurityContext(&nla->credentials,
	              NULL, nla->ServicePrincipalName, nla->fContextReq, 0,
	              SECURITY_NATIVE_DREP, NULL, 0, &nla->context,
	              &nla->outputBufferDesc, &nla->pfContextAttr, &nla->expiration);
	WLog_VRB(TAG, " InitializeSecurityContext status %s [0x%08"PRIX32"]",
		   GetSecurityStatusString(nla->status), nla->status);

	/* Handle kerberos context initialization failure.
	 * After kerberos failed initialize NTLM context */
	if( nla->status == SEC_E_NO_CREDENTIALS ){
		nla->status = nla->table->InitializeSecurityContext(&nla->credentials,
				NULL, nla->ServicePrincipalName, nla->fContextReq, 0,
				SECURITY_NATIVE_DREP, NULL, 0, &nla->context,
				&nla->outputBufferDesc, &nla->pfContextAttr, &nla->expiration);

		WLog_VRB(TAG, " InitializeSecurityContext status %s [0x%08"PRIX32"]",
			   GetSecurityStatusString(nla->status), nla->status);

		if(nla->status){

			SECURITY_STATUS status = nla->table->QuerySecurityPackageInfo( NTLMSSP_NAME, &nla->pPackageInfo);

			if (status != SEC_E_OK){
				WLog_ERR(TAG, "QuerySecurityPackageInfo status %s [0x%08"PRIX32"]",
					GetSecurityStatusString(nla->status), status);
				return -1;
			}

			nla->cbMaxToken = nla->pPackageInfo->cbMaxToken;
			nla->packageName = nla->pPackageInfo->Name;
		}
	}

	if ((nla->status == SEC_I_COMPLETE_AND_CONTINUE) || (nla->status == SEC_I_COMPLETE_NEEDED))
	{
		if (nla->table->CompleteAuthToken)
		{
			SECURITY_STATUS status;
			status = nla->table->CompleteAuthToken(&nla->context, &nla->outputBufferDesc);

			if (status != SEC_E_OK)
			{
				WLog_WARN(TAG, "CompleteAuthToken status %s [0x%08"PRIX32"]",
				          GetSecurityStatusString(status), status);
				return -1;
			}
		}

		if (nla->status == SEC_I_COMPLETE_NEEDED)
			nla->status = SEC_E_OK;
		else if (nla->status == SEC_I_COMPLETE_AND_CONTINUE)
			nla->status = SEC_I_CONTINUE_NEEDED;
	}

	if (nla->status != SEC_I_CONTINUE_NEEDED)
		return -1;

	if (nla->outputBuffer.cbBuffer < 1)
		return -1;

	nla->negoToken.pvBuffer = nla->outputBuffer.pvBuffer;
	nla->negoToken.cbBuffer = nla->outputBuffer.cbBuffer;
	WLog_DBG(TAG, "Sending Authentication Token");
	winpr_HexDump(TAG, WLOG_DEBUG, nla->negoToken.pvBuffer, nla->negoToken.cbBuffer);

	if (!nla_send(nla))
	{
		nla_buffer_free(nla);
		return -1;
	}

	nla_buffer_free(nla);
	nla->state = NLA_STATE_NEGO_TOKEN;
	return 1;
}

int nla_client_recv(rdpNla* nla)
{
	int status = -1;

	if (nla->state == NLA_STATE_NEGO_TOKEN)
	{
		nla->inputBufferDesc.ulVersion = SECBUFFER_VERSION;
		nla->inputBufferDesc.cBuffers = 1;
		nla->inputBufferDesc.pBuffers = &nla->inputBuffer;
		nla->inputBuffer.BufferType = SECBUFFER_TOKEN;
		nla->inputBuffer.pvBuffer = nla->negoToken.pvBuffer;
		nla->inputBuffer.cbBuffer = nla->negoToken.cbBuffer;
		nla->outputBufferDesc.ulVersion = SECBUFFER_VERSION;
		nla->outputBufferDesc.cBuffers = 1;
		nla->outputBufferDesc.pBuffers = &nla->outputBuffer;
		nla->outputBuffer.BufferType = SECBUFFER_TOKEN;
		nla->outputBuffer.cbBuffer = nla->cbMaxToken;
		nla->outputBuffer.pvBuffer = malloc(nla->outputBuffer.cbBuffer);

		if (!nla->outputBuffer.pvBuffer)
			return -1;

		nla->status = nla->table->InitializeSecurityContext(&nla->credentials,
		              &nla->context, nla->ServicePrincipalName, nla->fContextReq, 0,
		              SECURITY_NATIVE_DREP, &nla->inputBufferDesc,
		              0, &nla->context, &nla->outputBufferDesc, &nla->pfContextAttr, &nla->expiration);
		WLog_VRB(TAG, "InitializeSecurityContext  %s [0x%08"PRIX32"]",
		         GetSecurityStatusString(nla->status), nla->status);
		free(nla->inputBuffer.pvBuffer);
		nla->inputBuffer.pvBuffer = NULL;

		if ((nla->status == SEC_I_COMPLETE_AND_CONTINUE) || (nla->status == SEC_I_COMPLETE_NEEDED))
		{
			if (nla->table->CompleteAuthToken)
			{
				SECURITY_STATUS status;
				status = nla->table->CompleteAuthToken(&nla->context, &nla->outputBufferDesc);

				if (status != SEC_E_OK)
				{
					WLog_WARN(TAG, "CompleteAuthToken status %s [0x%08"PRIX32"]",
					          GetSecurityStatusString(status), status);
					return -1;
				}
			}

			if (nla->status == SEC_I_COMPLETE_NEEDED)
				nla->status = SEC_E_OK;
			else if (nla->status == SEC_I_COMPLETE_AND_CONTINUE)
				nla->status = SEC_I_CONTINUE_NEEDED;
		}

		if (nla->status == SEC_E_OK)
		{
			nla->havePubKeyAuth = TRUE;
			nla->status = nla->table->QueryContextAttributes(&nla->context, SECPKG_ATTR_SIZES,
			              &nla->ContextSizes);

			if (nla->status != SEC_E_OK)
			{
				WLog_ERR(TAG, "QueryContextAttributes SECPKG_ATTR_SIZES failure %s [0x%08"PRIX32"]",
				         GetSecurityStatusString(nla->status), nla->status);
				return -1;
			}

			nla->status = nla_encrypt_public_key_echo(nla);

			if (nla->status != SEC_E_OK)
				return -1;
		}

		nla->negoToken.pvBuffer = nla->outputBuffer.pvBuffer;
		nla->negoToken.cbBuffer = nla->outputBuffer.cbBuffer;
		WLog_DBG(TAG, "Sending Authentication Token");
		winpr_HexDump(TAG, WLOG_DEBUG, nla->negoToken.pvBuffer, nla->negoToken.cbBuffer);

		if (!nla_send(nla))
		{
			nla_buffer_free(nla);
			return -1;
		}

		nla_buffer_free(nla);

		if (nla->status == SEC_E_OK)
			nla->state = NLA_STATE_PUB_KEY_AUTH;

		status = 1;
	}
	else if (nla->state == NLA_STATE_PUB_KEY_AUTH)
	{
		/* Verify Server Public Key Echo */
		nla->status = nla_decrypt_public_key_echo(nla);
		nla_buffer_free(nla);

		if (nla->status != SEC_E_OK)
		{
			WLog_ERR(TAG, "Could not verify public key echo %s [0x%08"PRIX32"]",
			         GetSecurityStatusString(nla->status), nla->status);
			return -1;
		}

		/* Send encrypted credentials */
		nla->status = nla_encrypt_ts_credentials(nla);

		if (nla->status != SEC_E_OK)
		{
			WLog_ERR(TAG, "nla_encrypt_ts_credentials status %s [0x%08"PRIX32"]",
			         GetSecurityStatusString(nla->status), nla->status);
			return -1;
		}

		if (!nla_send(nla))
		{
			nla_buffer_free(nla);
			return -1;
		}

		nla_buffer_free(nla);
		nla->table->FreeCredentialsHandle(&nla->credentials);

		if (nla->status != SEC_E_OK)
		{
			WLog_ERR(TAG, "FreeCredentialsHandle status %s [0x%08"PRIX32"]",
			         GetSecurityStatusString(nla->status), nla->status);
		}

		nla->status = nla->table->FreeContextBuffer(nla->pPackageInfo);

		if (nla->status != SEC_E_OK)
		{
			WLog_ERR(TAG, "FreeContextBuffer status %s [0x%08"PRIX32"]",
			         GetSecurityStatusString(nla->status), nla->status);
		}

		if (nla->status != SEC_E_OK)
			return -1;

		nla->state = NLA_STATE_AUTH_INFO;
		status = 1;
	}

	return status;
}

int nla_client_authenticate(rdpNla* nla)
{
	wStream* s;
	int status;
	s = Stream_New(NULL, 4096);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return -1;
	}

	if (nla_client_begin(nla) < 1)
	{
		Stream_Free(s, TRUE);
		return -1;
	}

	while (nla->state < NLA_STATE_AUTH_INFO)
	{
		Stream_SetPosition(s, 0);
		status = transport_read_pdu(nla->transport, s);

		if (status < 0)
		{
			WLog_ERR(TAG, "nla_client_authenticate failure");
			Stream_Free(s, TRUE);
			return -1;
		}

		status = nla_recv_pdu(nla, s);

		if (status < 0)
		{
			Stream_Free(s, TRUE);
			return -1;
		}
	}

	Stream_Free(s, TRUE);
	return 1;
}

/**
 * Initialize NTLMSSP authentication module (server).
 * @param credssp
 */

int nla_server_init(rdpNla* nla)
{
	rdpTls* tls = nla->transport->tls;

	if (!sspi_SecBufferAlloc(&nla->PublicKey, tls->PublicKeyLength))
	{
		WLog_ERR(TAG, "Failed to allocate SecBuffer for public key");
		return -1;
	}

	CopyMemory(nla->PublicKey.pvBuffer, tls->PublicKey, tls->PublicKeyLength);

	if (nla->SspiModule)
	{
		HMODULE hSSPI;
		INIT_SECURITY_INTERFACE pInitSecurityInterface;
		hSSPI = LoadLibrary(nla->SspiModule);

		if (!hSSPI)
		{
			WLog_ERR(TAG, "Failed to load SSPI module: %s", nla->SspiModule);
			return -1;
		}

#ifdef UNICODE
		pInitSecurityInterface = (INIT_SECURITY_INTERFACE) GetProcAddress(hSSPI, "InitSecurityInterfaceW");
#else
		pInitSecurityInterface = (INIT_SECURITY_INTERFACE) GetProcAddress(hSSPI, "InitSecurityInterfaceA");
#endif
		nla->table = pInitSecurityInterface();
	}
	else
	{
		nla->table = InitSecurityInterfaceEx(0);
	}

	nla->status = nla->table->QuerySecurityPackageInfo(NLA_PKG_NAME, &nla->pPackageInfo);

	if (nla->status != SEC_E_OK)
	{
		WLog_ERR(TAG, "QuerySecurityPackageInfo status %s [0x%08"PRIX32"]",
		         GetSecurityStatusString(nla->status), nla->status);
		return -1;
	}

	nla->cbMaxToken = nla->pPackageInfo->cbMaxToken;
	nla->status = nla->table->AcquireCredentialsHandle(NULL, NLA_PKG_NAME,
	              SECPKG_CRED_INBOUND, NULL, NULL, NULL, NULL, &nla->credentials, &nla->expiration);

	if (nla->status != SEC_E_OK)
	{
		WLog_ERR(TAG, "AcquireCredentialsHandle status %s [0x%08"PRIX32"]",
		         GetSecurityStatusString(nla->status), nla->status);
		return -1;
	}

	nla->haveContext = FALSE;
	nla->haveInputBuffer = FALSE;
	nla->havePubKeyAuth = FALSE;
	ZeroMemory(&nla->inputBuffer, sizeof(SecBuffer));
	ZeroMemory(&nla->outputBuffer, sizeof(SecBuffer));
	ZeroMemory(&nla->inputBufferDesc, sizeof(SecBufferDesc));
	ZeroMemory(&nla->outputBufferDesc, sizeof(SecBufferDesc));
	ZeroMemory(&nla->ContextSizes, sizeof(SecPkgContext_Sizes));
	/*
	 * from tspkg.dll: 0x00000112
	 * ASC_REQ_MUTUAL_AUTH
	 * ASC_REQ_CONFIDENTIALITY
	 * ASC_REQ_ALLOCATE_MEMORY
	 */
	nla->fContextReq = 0;
	nla->fContextReq |= ASC_REQ_MUTUAL_AUTH;
	nla->fContextReq |= ASC_REQ_CONFIDENTIALITY;
	nla->fContextReq |= ASC_REQ_CONNECTION;
	nla->fContextReq |= ASC_REQ_USE_SESSION_KEY;
	nla->fContextReq |= ASC_REQ_REPLAY_DETECT;
	nla->fContextReq |= ASC_REQ_SEQUENCE_DETECT;
	nla->fContextReq |= ASC_REQ_EXTENDED_ERROR;
	return 1;
}

/**
 * Authenticate with client using CredSSP (server).
 * @param credssp
 * @return 1 if authentication is successful
 */

int nla_server_authenticate(rdpNla* nla)
{
	if (nla_server_init(nla) < 1)
		return -1;

	while (TRUE)
	{
		/* receive authentication token */
		nla->inputBufferDesc.ulVersion = SECBUFFER_VERSION;
		nla->inputBufferDesc.cBuffers = 1;
		nla->inputBufferDesc.pBuffers = &nla->inputBuffer;
		nla->inputBuffer.BufferType = SECBUFFER_TOKEN;

		if (nla_recv(nla) < 0)
			return -1;

		WLog_DBG(TAG, "Receiving Authentication Token");
		nla_buffer_print(nla);
		nla->inputBuffer.pvBuffer = nla->negoToken.pvBuffer;
		nla->inputBuffer.cbBuffer = nla->negoToken.cbBuffer;

		if (nla->negoToken.cbBuffer < 1)
		{
			WLog_ERR(TAG, "CredSSP: invalid negoToken!");
			return -1;
		}

		nla->outputBufferDesc.ulVersion = SECBUFFER_VERSION;
		nla->outputBufferDesc.cBuffers = 1;
		nla->outputBufferDesc.pBuffers = &nla->outputBuffer;
		nla->outputBuffer.BufferType = SECBUFFER_TOKEN;
		nla->outputBuffer.cbBuffer = nla->cbMaxToken;
		nla->outputBuffer.pvBuffer = malloc(nla->outputBuffer.cbBuffer);

		if (!nla->outputBuffer.pvBuffer)
			return -1;

		nla->status = nla->table->AcceptSecurityContext(&nla->credentials,
		              nla->haveContext ? &nla->context : NULL,
		              &nla->inputBufferDesc, nla->fContextReq, SECURITY_NATIVE_DREP, &nla->context,
		              &nla->outputBufferDesc, &nla->pfContextAttr, &nla->expiration);
		WLog_VRB(TAG, "AcceptSecurityContext status %s [0x%08"PRIX32"]",
		         GetSecurityStatusString(nla->status), nla->status);
		nla->negoToken.pvBuffer = nla->outputBuffer.pvBuffer;
		nla->negoToken.cbBuffer = nla->outputBuffer.cbBuffer;

		if ((nla->status == SEC_I_COMPLETE_AND_CONTINUE) || (nla->status == SEC_I_COMPLETE_NEEDED))
		{
			if (nla->SamFile)
			{
				nla->table->SetContextAttributes(&nla->context,
				                                 SECPKG_ATTR_AUTH_NTLM_SAM_FILE, nla->SamFile, strlen(nla->SamFile) + 1);
			}

			if (nla->table->CompleteAuthToken)
			{
				SECURITY_STATUS status;
				status = nla->table->CompleteAuthToken(&nla->context, &nla->outputBufferDesc);

				if (status != SEC_E_OK)
				{
					WLog_WARN(TAG, "CompleteAuthToken status %s [0x%08"PRIX32"]",
					          GetSecurityStatusString(status), status);
					return -1;
				}
			}

			if (nla->status == SEC_I_COMPLETE_NEEDED)
				nla->status = SEC_E_OK;
			else if (nla->status == SEC_I_COMPLETE_AND_CONTINUE)
				nla->status = SEC_I_CONTINUE_NEEDED;
		}

		if (nla->status == SEC_E_OK)
		{
			if (nla->outputBuffer.cbBuffer != 0)
			{
				if (!nla_send(nla))
				{
					nla_buffer_free(nla);
					return -1;
				}

				if (nla_recv(nla) < 0)
					return -1;

				WLog_DBG(TAG, "Receiving pubkey Token");
				nla_buffer_print(nla);
			}

			nla->havePubKeyAuth = TRUE;
			nla->status  = nla->table->QueryContextAttributes(&nla->context, SECPKG_ATTR_SIZES,
			               &nla->ContextSizes);

			if (nla->status != SEC_E_OK)
			{
				WLog_ERR(TAG, "QueryContextAttributes SECPKG_ATTR_SIZES failure %s [0x%08"PRIX32"]",
				         GetSecurityStatusString(nla->status), nla->status);
				return -1;
			}

			nla->status = nla_decrypt_public_key_echo(nla);

			if (nla->status != SEC_E_OK)
			{
				WLog_ERR(TAG, "Error: could not verify client's public key echo %s [0x%08"PRIX32"]",
				         GetSecurityStatusString(nla->status), nla->status);
				return -1;
			}

			sspi_SecBufferFree(&nla->negoToken);
			nla->negoToken.pvBuffer = NULL;
			nla->negoToken.cbBuffer = 0;
			nla->status = nla_encrypt_public_key_echo(nla);

			if (nla->status != SEC_E_OK)
				return -1;
		}

		if ((nla->status != SEC_E_OK) && (nla->status != SEC_I_CONTINUE_NEEDED))
		{
			/* Special handling of these specific error codes as NTSTATUS_FROM_WIN32
			   unfortunately does not map directly to the corresponding NTSTATUS values
			 */
			switch (GetLastError())
			{
				case ERROR_PASSWORD_MUST_CHANGE:
					nla->errorCode = STATUS_PASSWORD_MUST_CHANGE;
					break;

				case ERROR_PASSWORD_EXPIRED:
					nla->errorCode = STATUS_PASSWORD_EXPIRED;
					break;

				case ERROR_ACCOUNT_DISABLED:
					nla->errorCode = STATUS_ACCOUNT_DISABLED;
					break;

				default:
					nla->errorCode = NTSTATUS_FROM_WIN32(GetLastError());
					break;
			}

			WLog_ERR(TAG, "AcceptSecurityContext status %s [0x%08"PRIX32"]",
			         GetSecurityStatusString(nla->status), nla->status);
			nla_send(nla);
			return -1; /* Access Denied */
		}

		/* send authentication token */
		WLog_DBG(TAG, "Sending Authentication Token");
		nla_buffer_print(nla);

		if (!nla_send(nla))
		{
			nla_buffer_free(nla);
			return -1;
		}

		nla_buffer_free(nla);

		if (nla->status != SEC_I_CONTINUE_NEEDED)
			break;

		nla->haveContext = TRUE;
	}

	/* Receive encrypted credentials */

	if (nla_recv(nla) < 0)
		return -1;

	nla->status = nla_decrypt_ts_credentials(nla);

	if (nla->status != SEC_E_OK)
	{
		WLog_ERR(TAG, "Could not decrypt TSCredentials status %s [0x%08"PRIX32"]",
		         GetSecurityStatusString(nla->status), nla->status);
		return -1;
	}

	nla->status = nla->table->ImpersonateSecurityContext(&nla->context);

	if (nla->status != SEC_E_OK)
	{
		WLog_ERR(TAG, "ImpersonateSecurityContext status %s [0x%08"PRIX32"]",
		         GetSecurityStatusString(nla->status), nla->status);
		return -1;
	}
	else
	{
		nla->status = nla->table->RevertSecurityContext(&nla->context);

		if (nla->status != SEC_E_OK)
		{
			WLog_ERR(TAG, "RevertSecurityContext status %s [0x%08"PRIX32"]",
			         GetSecurityStatusString(nla->status), nla->status);
			return -1;
		}
	}

	nla->status = nla->table->FreeContextBuffer(nla->pPackageInfo);

	if (nla->status != SEC_E_OK)
	{
		WLog_ERR(TAG, "DeleteSecurityContext status %s [0x%08"PRIX32"]",
		         GetSecurityStatusString(nla->status), nla->status);
		return -1;
	}

	return 1;
}

/**
 * Authenticate using CredSSP.
 * @param credssp
 * @return 1 if authentication is successful
 */

int nla_authenticate(rdpNla* nla)
{
	if (nla->server)
		return nla_server_authenticate(nla);
	else
		return nla_client_authenticate(nla);
}

void ap_integer_increment_le(BYTE* number, int size)
{
	int index;

	for (index = 0; index < size; index++)
	{
		if (number[index] < 0xFF)
		{
			number[index]++;
			break;
		}
		else
		{
			number[index] = 0;
			continue;
		}
	}
}

void ap_integer_decrement_le(BYTE* number, int size)
{
	int index;

	for (index = 0; index < size; index++)
	{
		if (number[index] > 0)
		{
			number[index]--;
			break;
		}
		else
		{
			number[index] = 0xFF;
			continue;
		}
	}
}

SECURITY_STATUS nla_encrypt_public_key_echo(rdpNla* nla)
{
	SecBuffer Buffers[2];
	SecBufferDesc Message;
	SECURITY_STATUS status;
	int public_key_length;
	public_key_length = nla->PublicKey.cbBuffer;

	if (!sspi_SecBufferAlloc(&nla->pubKeyAuth, public_key_length + nla->ContextSizes.cbSecurityTrailer))
		return SEC_E_INSUFFICIENT_MEMORY;
	
	if( strcmp(nla->packageName, KERBEROS_SSP_NAME) == 0 ) {
		Buffers[0].BufferType = SECBUFFER_DATA; /* TLS Public Key */
		Buffers[0].cbBuffer = public_key_length;
		Buffers[0].pvBuffer = nla->pubKeyAuth.pvBuffer;
		CopyMemory(Buffers[0].pvBuffer, nla->PublicKey.pvBuffer, Buffers[0].cbBuffer);
	}
	else if ( (strcmp(nla->packageName, NEGOSSP_NAME) != 0) || ( strcmp(nla->packageName, NTLMSSP_NAME) != 0 ) ){
		Buffers[0].BufferType = SECBUFFER_TOKEN; /* Signature */
		Buffers[0].cbBuffer = nla->ContextSizes.cbSecurityTrailer;
		Buffers[0].pvBuffer = nla->pubKeyAuth.pvBuffer;

		Buffers[1].BufferType = SECBUFFER_DATA; /* TLS Public Key */
		Buffers[1].cbBuffer = public_key_length;
		Buffers[1].pvBuffer = ((BYTE*) nla->pubKeyAuth.pvBuffer) + nla->ContextSizes.cbSecurityTrailer;
		CopyMemory(Buffers[1].pvBuffer, nla->PublicKey.pvBuffer, Buffers[1].cbBuffer);
	}

	if ( (strcmp(nla->packageName, KERBEROS_SSP_NAME) != 0) && nla->server)
	{
		/* server echos the public key +1 */
		ap_integer_increment_le((BYTE*) Buffers[1].pvBuffer, Buffers[1].cbBuffer);
	}

	Message.cBuffers = 2;
	Message.ulVersion = SECBUFFER_VERSION;
	Message.pBuffers = (PSecBuffer) &Buffers;
	status = nla->table->EncryptMessage(&nla->context, 0, &Message, nla->sendSeqNum++);

	if (status != SEC_E_OK)
	{
		WLog_ERR(TAG, "EncryptMessage status %s [0x%08"PRIX32"]",
		         GetSecurityStatusString(status), status);
		return status;
	}

	return status;
}

SECURITY_STATUS nla_decrypt_public_key_echo(rdpNla* nla)
{
	int length;
	BYTE* buffer;
	ULONG pfQOP = 0;
	BYTE* public_key1;
	BYTE* public_key2;
	int public_key_length;
	int signature_length;
	SecBuffer Buffers[2];
	SecBufferDesc Message;
	SECURITY_STATUS status;
	signature_length = nla->pubKeyAuth.cbBuffer - nla->PublicKey.cbBuffer;
	
	if (signature_length < 0 || signature_length > nla->ContextSizes.cbSecurityTrailer)
	{
		WLog_ERR(TAG, "unexpected pubKeyAuth buffer size: %"PRIu32"", nla->pubKeyAuth.cbBuffer);
		return SEC_E_INVALID_TOKEN;
	}

	if ((nla->PublicKey.cbBuffer + nla->ContextSizes.cbSecurityTrailer ) != nla->pubKeyAuth.cbBuffer)
	{
		WLog_ERR(TAG, "unexpected pubKeyAuth buffer size: %"PRIu32"", (int) nla->pubKeyAuth.cbBuffer);
		return SEC_E_INVALID_TOKEN;
	}

	length = nla->pubKeyAuth.cbBuffer;
	buffer = (BYTE*) malloc(length);

	if (!buffer)
		return SEC_E_INSUFFICIENT_MEMORY;

	if( strcmp(nla->packageName, KERBEROS_SSP_NAME) == 0 ){
		CopyMemory(buffer, nla->pubKeyAuth.pvBuffer, length);
		Buffers[0].BufferType = SECBUFFER_DATA; /* Wrapped and encrypted TLS Public Key */
		Buffers[0].cbBuffer = length;
		Buffers[0].pvBuffer = buffer;
		Message.cBuffers = 1;
		Message.ulVersion = SECBUFFER_VERSION;
		Message.pBuffers = (PSecBuffer) &Buffers;
	}
	else if( ( strcmp(nla->packageName, NEGOSSP_NAME) == 0 ) || ( strcmp(nla->packageName,  NTLMSSP_NAME) == 0 ) ){
		CopyMemory(buffer, nla->pubKeyAuth.pvBuffer, length);
		public_key_length = nla->PublicKey.cbBuffer;
		Buffers[0].BufferType = SECBUFFER_TOKEN; /* Signature */
		Buffers[0].cbBuffer = signature_length;
		Buffers[0].pvBuffer = buffer;
		Buffers[1].BufferType = SECBUFFER_DATA; /* Encrypted TLS Public Key */
		Buffers[1].cbBuffer = length - signature_length;
		Buffers[1].pvBuffer = buffer + signature_length;
		Message.cBuffers = 2;
		Message.ulVersion = SECBUFFER_VERSION;
		Message.pBuffers = (PSecBuffer) &Buffers;
	}

	status = nla->table->DecryptMessage(&nla->context, &Message, nla->recvSeqNum++, &pfQOP);

	if (status != SEC_E_OK)
	{
		WLog_ERR(TAG, "DecryptMessage failure %s [%08"PRIX32"]",
		         GetSecurityStatusString(status), status);
		return status;
	}

	if( strcmp(nla->packageName, KERBEROS_SSP_NAME) == 0 ){
		public_key1 = public_key2 = (BYTE*) nla->pubKeyAuth.pvBuffer ;
		public_key_length = length;
	}
	else if( ( strcmp(nla->packageName, NEGOSSP_NAME) == 0 ) || ( strcmp(nla->packageName, NTLMSSP_NAME) == 0 ) ){
		public_key1 = (BYTE*) nla->PublicKey.pvBuffer;
		public_key2 = (BYTE*) Buffers[1].pvBuffer;
	}

	if (!nla->server)
	{
		/* server echos the public key +1 */
		ap_integer_decrement_le(public_key2, public_key_length);
	}

	if (memcmp(public_key1, public_key2, public_key_length) != 0)
	{
		WLog_ERR(TAG, "Could not verify server's public key echo");
		WLog_ERR(TAG, "Expected (length = %d):", public_key_length);
		winpr_HexDump(TAG, WLOG_ERROR, public_key1, public_key_length);
		WLog_ERR(TAG, "Actual (length = %d):", public_key_length);
		winpr_HexDump(TAG, WLOG_ERROR, public_key2, public_key_length);
		return SEC_E_MESSAGE_ALTERED; /* DO NOT SEND CREDENTIALS! */
	}

	free(buffer);
	return SEC_E_OK;
}

int nla_sizeof_ts_password_creds(rdpNla* nla)
{
	int length = 0;

	if (nla->identity)
	{
		length += ber_sizeof_sequence_octet_string(nla->identity->DomainLength * 2);
		length += ber_sizeof_sequence_octet_string(nla->identity->UserLength * 2);
		length += ber_sizeof_sequence_octet_string(nla->identity->PasswordLength * 2);
	}
	return length;
}

int nla_sizeof_ts_cspdatadetail(rdpNla* nla)
{
	int length = 0;
	if (nla->identity->CspData)
	{
		length += ber_sizeof_contextual_tag(ber_sizeof_integer(nla->identity->CspData->KeySpec));
		length += ber_sizeof_integer(nla->identity->CspData->KeySpec);
		length += ber_sizeof_sequence_octet_string(nla->identity->CspData->CardNameLength * 2);
		length += ber_sizeof_sequence_octet_string(nla->identity->CspData->ReaderNameLength * 2);
		length += ber_sizeof_sequence_octet_string(nla->identity->CspData->ContainerNameLength * 2);
		length += ber_sizeof_sequence_octet_string(nla->identity->CspData->CspNameLength * 2);
	}
	return length;
}

int nla_sizeof_sequence_ts_cspdatadetail(rdpNla* nla)
{
	int length = 0;
	length += nla_sizeof_ts_cspdatadetail(nla);
	length += ber_sizeof_sequence_tag(length);
	length += ber_sizeof_contextual_tag(length);
	return length;
}

int nla_sizeof_ts_smartcard_creds(rdpNla* nla)
{
	int length = 0;
	if (nla->identity)
	{
		length += ber_sizeof_sequence_octet_string(nla->identity->PinLength * 2);
		length += nla_sizeof_sequence_ts_cspdatadetail(nla);
		length += ber_sizeof_sequence_octet_string(nla->identity->UserHintLength * 2);
		length += ber_sizeof_sequence_octet_string(nla->identity->DomainHintLength * 2);
	}
	return length;
}

int NLA_SIZEOF_TS_PWD_OR_SC_CREDS(rdpNla* nla, SEC_DELEGATION_CREDENTIALS_TYPE credType)
{
	if(credType == SEC_PASSWORD_DELEGATION_CRED_TYPE) return nla_sizeof_ts_password_creds(nla);
	else if (credType == SEC_SMARTCARD_DELEGATION_CRED_TYPE) {
		return nla_sizeof_ts_smartcard_creds(nla);
	}
	else {
        return 0;
	}
}

int nla_sizeof_ts_credentials(rdpNla* nla)
{
	UINT32 credType = (UINT32) nla->credType;

	int size = 0;
	size += ber_sizeof_integer(credType);
	size += ber_sizeof_contextual_tag(ber_sizeof_integer(credType));
	size += ber_sizeof_sequence_octet_string(ber_sizeof_sequence( (NLA_SIZEOF_TS_PWD_OR_SC_CREDS(nla, credType)) ));
	return size;
}

BOOL nla_read_ts_password_creds(rdpNla* nla, wStream* s)
{
	int length;

	if (!nla->identity)
	{
		WLog_ERR(TAG, "nla->identity is NULL!");
		return FALSE;
	}

	/* TSPasswordCreds (SEQUENCE)
	 * Initialise to default values. */
	nla->identity->Flags = SEC_WINNT_AUTH_IDENTITY_UNICODE;
	nla->identity->UserLength = (UINT32) 0;
	nla->identity->User = NULL;
	nla->identity->DomainLength = (UINT32) 0;
	nla->identity->Domain = NULL;
	nla->identity->Password = NULL;
	nla->identity->PasswordLength = (UINT32) 0;

	if (!ber_read_sequence_tag(s, &length))
		return FALSE;

	/* The sequence is empty, return early,
	 * TSPasswordCreds (SEQUENCE) is optional. */
	if (length == 0)
		return TRUE;

	/* [0] domainName (OCTET STRING) */
	if (!ber_read_contextual_tag(s, 0, &length, TRUE) ||
	    !ber_read_octet_string_tag(s, &length))
	{
		return FALSE;
	}

	nla->identity->DomainLength = (UINT32) length;

	if (nla->identity->DomainLength > 0)
	{
		nla->identity->Domain = (UINT16*) malloc(length);

		if (!nla->identity->Domain)
			return FALSE;

		CopyMemory(nla->identity->Domain, Stream_Pointer(s), nla->identity->DomainLength);
		Stream_Seek(s, nla->identity->DomainLength);
		nla->identity->DomainLength /= 2;
	}

	/* [1] userName (OCTET STRING) */
	if (!ber_read_contextual_tag(s, 1, &length, TRUE) ||
	    !ber_read_octet_string_tag(s, &length))
	{
		return FALSE;
	}

	nla->identity->UserLength = (UINT32) length;

	if (nla->identity->UserLength > 0)
	{
		nla->identity->User = (UINT16*) malloc(length);

		if (!nla->identity->User)
			return FALSE;

		CopyMemory(nla->identity->User, Stream_Pointer(s), nla->identity->UserLength);
		Stream_Seek(s, nla->identity->UserLength);
		nla->identity->UserLength /= 2;
	}

	/* [2] password (OCTET STRING) */
	if (!ber_read_contextual_tag(s, 2, &length, TRUE) ||
	    !ber_read_octet_string_tag(s, &length))
	{
		return FALSE;
	}

	nla->identity->PasswordLength = (UINT32) length;

	if (nla->identity->PasswordLength > 0)
	{
		nla->identity->Password = (UINT16*) malloc(length);

		if (!nla->identity->Password)
			return FALSE;

		CopyMemory(nla->identity->Password, Stream_Pointer(s), nla->identity->PasswordLength);
		Stream_Seek(s, nla->identity->PasswordLength);
		nla->identity->PasswordLength /= 2;
	}

	return TRUE;
}

BOOL nla_read_ts_cspdatadetail(rdpNla * nla, wStream* s, int * length)
{
	if (!nla->identity->CspData)
		return FALSE;

	/* TSCspDataDetail (SEQUENCE)
	 * Initialise to default values. */
	nla->identity->Flags = SEC_WINNT_AUTH_IDENTITY_UNICODE;

	nla->identity->CspData->KeySpec = (UINT32) 0;

	nla->identity->CspData->CardNameLength = (UINT32) 0;
	nla->identity->CspData->CardName = NULL;

	nla->identity->CspData->ReaderNameLength = (UINT32) 0;
	nla->identity->CspData->ReaderName = NULL;

	nla->identity->CspData->ContainerNameLength = (UINT32) 0;
	nla->identity->CspData->ContainerName = NULL;

	nla->identity->CspData->CspNameLength = (UINT32) 0;
	nla->identity->CspData->CspName = NULL;

	if (!ber_read_sequence_tag(s, length)){
		return FALSE;
	}

	/* The sequence is empty, return early,
	 * TSCspDataDetail (SEQUENCE) is optional. */
	if (*length == 0)
		return TRUE;

	/* [0] keySpec (INTEGER) */
	if ( !ber_read_contextual_tag(s, 0, length, TRUE) &&
		!ber_read_integer(s, &nla->identity->CspData->KeySpec))
		return FALSE;

	/* [1] cardName (OCTET STRING) */
	if (!ber_read_contextual_tag(s, 1, length, TRUE) ||
		!ber_read_octet_string_tag(s, length))
		return FALSE;

	nla->identity->CspData->CardNameLength = (UINT32) (*length);
	if (nla->identity->CspData->CardNameLength > 0)
	{
		nla->identity->CspData->CardName = (UINT16 *) calloc(1, *length);
		if (!nla->identity->CspData->CardName)
			return FALSE;
		CopyMemory(nla->identity->CspData->CardName, Stream_Pointer(s), nla->identity->CspData->CardNameLength);
		Stream_Seek(s, nla->identity->CspData->CardNameLength);
		nla->identity->CspData->CardNameLength /= 2;
	}

	/* [2] readerName (OCTET STRING) */
	if (!ber_read_contextual_tag(s, 2, length, TRUE) ||
		!ber_read_octet_string_tag(s, length))
		return FALSE;

	nla->identity->CspData->ReaderNameLength = (UINT32) (*length);
	if (nla->identity->CspData->ReaderNameLength > 0)
	{
		nla->identity->CspData->ReaderName = (UINT16 *) calloc(1, *length);
		if (!nla->identity->CspData->ReaderName)
			return FALSE;
		CopyMemory(nla->identity->CspData->ReaderName, Stream_Pointer(s), nla->identity->CspData->ReaderNameLength);
		Stream_Seek(s, nla->identity->CspData->ReaderNameLength);
		nla->identity->CspData->ReaderNameLength /= 2;
	}

	/* [3] containerName (OCTET STRING) */
	if (!ber_read_contextual_tag(s, 3, length, TRUE) ||
		!ber_read_octet_string_tag(s, length))
		return FALSE;

	nla->identity->CspData->ContainerNameLength = (UINT32) (*length);
	if (nla->identity->CspData->ContainerNameLength > 0)
	{
		nla->identity->CspData->ContainerName = (UINT16 *) calloc(1, *length);
		if (!nla->identity->CspData->ContainerName)
			return FALSE;
		CopyMemory(nla->identity->CspData->ContainerName, Stream_Pointer(s), nla->identity->CspData->ContainerNameLength);
		Stream_Seek(s, nla->identity->CspData->ContainerNameLength);
		nla->identity->CspData->ContainerNameLength /= 2;
	}

	/* [4] cspName (OCTET STRING) */
	if (!ber_read_contextual_tag(s, 4, length, TRUE) ||
		!ber_read_octet_string_tag(s, length))
		return FALSE;

	nla->identity->CspData->CspNameLength = (UINT32) (*length);
	if (nla->identity->CspData->CspNameLength > 0)
	{
		nla->identity->CspData->CspName = (UINT16 *) calloc(1, *length);
		if (!nla->identity->CspData->CspName)
			return FALSE;
		CopyMemory(nla->identity->CspData->CspName, Stream_Pointer(s), nla->identity->CspData->CspNameLength);
		Stream_Seek(s, nla->identity->CspData->CspNameLength);
		nla->identity->CspData->CspNameLength /= 2;
	}

	return TRUE;
}


BOOL nla_read_ts_smartcard_creds(rdpNla* nla, wStream* s)
{
	int length=0 ;

	if (!nla->identity)
		return FALSE;

	/* TSSmartCardCreds (SEQUENCE)
	 * Initialize to default values. */
	nla->identity->Flags = SEC_WINNT_AUTH_IDENTITY_UNICODE;

	nla->identity->Pin = NULL;
	nla->identity->PinLength = (UINT32) 0;

	nla->identity->CspData = NULL;

	nla->identity->UserHint = NULL;
	nla->identity->UserHintLength = (UINT32) 0;

	nla->identity->DomainHint = NULL;
	nla->identity->DomainLength = (UINT32) 0;

	if (!ber_read_sequence_tag(s, &length))
		return FALSE;

	/* The sequence is empty, return early,
	 * TSSmartCardCreds (SEQUENCE) is optional. */
	if (length == 0)
		return TRUE;

	/* [0] Pin (OCTET STRING) */
	if (!ber_read_contextual_tag(s, 0, &length, TRUE) ||
		!ber_read_octet_string_tag(s, &length))
		return FALSE;

	nla->identity->PinLength = (UINT32) length;
	if (nla->identity->PinLength > 0)
	{
		nla->identity->Pin = (UINT16*) malloc(length);
		if (!nla->identity->Pin)
			return FALSE;
		CopyMemory(nla->identity->Pin, Stream_Pointer(s), nla->identity->PinLength);
		Stream_Seek(s, nla->identity->PinLength);
		nla->identity->PinLength /= 2;
	}

	/* [1] CspDataDetail (TSCspDataDetail) */
	nla->identity->CspData = (SEC_WINNT_AUTH_IDENTITY_CSPDATADETAIL *) calloc(1, sizeof(SEC_WINNT_AUTH_IDENTITY_CSPDATADETAIL) );
	if (!nla->identity->CspData)
	{
		return FALSE;
	}
	else
	{
		if( !nla_read_ts_cspdatadetail(nla, s, &length) ) return FALSE;
	}

	/* [2] UserHint (OCTET STRING) */
	if (!ber_read_contextual_tag(s, 2, &length, TRUE) ||
		!ber_read_octet_string_tag(s, &length))
		return FALSE;

	nla->identity->UserHintLength = (UINT32) length;
	if (nla->identity->UserHintLength > 0)
	{
		nla->identity->UserHint = (UINT16 *) malloc(length);
		if (!nla->identity->UserHint)
			return FALSE;
		CopyMemory(nla->identity->UserHint, Stream_Pointer(s), nla->identity->UserHintLength);
		Stream_Seek(s, nla->identity->UserHintLength);
		nla->identity->UserHintLength /= 2;
	}

	/* [3] DomainHint (OCTET STRING) */
	if (!ber_read_contextual_tag(s, 3, &length, TRUE) ||
		!ber_read_octet_string_tag(s, &length))
		return FALSE;

	nla->identity->DomainHintLength = (UINT32) length;
	if (nla->identity->DomainHintLength > 0)
	{
		nla->identity->DomainHint = (UINT16 *) malloc(length);
		if (!nla->identity->DomainHint)
			return FALSE;
		CopyMemory(nla->identity->DomainHint, Stream_Pointer(s), nla->identity->DomainHintLength);
		Stream_Seek(s, nla->identity->DomainHintLength);
		nla->identity->DomainHintLength /= 2;
	}

	return TRUE;
}

int nla_write_ts_password_creds(rdpNla* nla, wStream* s)
{
	int size = 0;
	int innerSize = nla_sizeof_ts_password_creds(nla);
	/* TSPasswordCreds (SEQUENCE) */
	size += ber_write_sequence_tag(s, innerSize);

	if (nla->identity)
	{
		/* [0] domainName (OCTET STRING) */
		size += ber_write_sequence_octet_string(
		            s, 0, (BYTE*) nla->identity->Domain,
		            nla->identity->DomainLength * 2);
		/* [1] userName (OCTET STRING) */
		size += ber_write_sequence_octet_string(
		            s, 1, (BYTE*) nla->identity->User,
		            nla->identity->UserLength * 2);
		/* [2] password (OCTET STRING) */
		size += ber_write_sequence_octet_string(
		            s, 2, (BYTE*) nla->identity->Password,
		            nla->identity->PasswordLength * 2);
	}

	return size;
}

int nla_write_ts_smartcard_creds(rdpNla* nla, wStream* s)
{
	int size = 0;
	int innerSize = nla_sizeof_ts_smartcard_creds(nla);
	/* TSSmartCardCreds (SEQUENCE) */
	size += ber_write_sequence_tag(s, innerSize);
	if (nla->identity)
	{
		/* [0] Pin (OCTET STRING) */
		size += ber_write_sequence_octet_string(
				s, 0, (BYTE*) nla->identity->Pin,
				nla->identity->PinLength * 2);

		/* [1] CspDataDetail (TSCspDataDetail) (SEQUENCE) */
		size += ber_write_contextual_tag(s, 1, ber_sizeof_sequence(nla_sizeof_ts_cspdatadetail(nla)), TRUE);
		size += ber_write_sequence_tag(s, nla_sizeof_ts_cspdatadetail(nla) );

		/* [0] KeySpec (INTEGER) */
		size += ber_write_contextual_tag(s, 0, ber_sizeof_integer(nla->identity->CspData->KeySpec), TRUE);
		size += ber_write_integer(s, nla->identity->CspData->KeySpec);
		/* [1] CardName (OCTER STRING) */
		size += ber_write_sequence_octet_string(s, 1, (BYTE*) nla->identity->CspData->CardName, nla->identity->CspData->CardNameLength * 2);  /* OCTET STRING */
		/* [2] ReaderName (OCTER STRING) */
		size += ber_write_sequence_octet_string(s, 2, (BYTE*) nla->identity->CspData->ReaderName, nla->identity->CspData->ReaderNameLength * 2);  /* OCTET STRING */
		/* [3] ContainerName (OCTER STRING) */
		size += ber_write_sequence_octet_string(s, 3, (BYTE*) nla->identity->CspData->ContainerName, nla->identity->CspData->ContainerNameLength * 2);  /* OCTET STRING */
		/* [4] CspName (OCTER STRING) */
		size += ber_write_sequence_octet_string(s, 4, (BYTE*) nla->identity->CspData->CspName, nla->identity->CspData->CspNameLength * 2);  /* OCTET STRING */

		/* [2] userHint (OCTET STRING) */
		size += ber_write_sequence_octet_string(
				s, 2, (BYTE*) nla->identity->UserHint,
				nla->identity->UserHintLength * 2);

		/* [3] domainHint (OCTET STRING) */
		size += ber_write_sequence_octet_string(
				s, 3, (BYTE*) nla->identity->DomainHint,
				nla->identity->DomainHintLength * 2);
	}
	return size;
}

int nla_write_ts_creds(rdpNla* nla, wStream* s, SEC_DELEGATION_CREDENTIALS_TYPE credType)
{
	if(credType == SEC_PASSWORD_DELEGATION_CRED_TYPE)	return nla_write_ts_password_creds(nla, s);
	else if(credType == SEC_SMARTCARD_DELEGATION_CRED_TYPE) return nla_write_ts_smartcard_creds(nla, s);
	else {
        WLog_ERR(TAG,  "credType unknown: %d\n", credType);
        return 0;
	}
}

int nla_read_ts_creds(rdpNla* nla, wStream* s, SEC_DELEGATION_CREDENTIALS_TYPE credType)
{
	if(credType == SEC_PASSWORD_DELEGATION_CRED_TYPE)	return nla_read_ts_password_creds(nla, s);
	else if(credType == SEC_SMARTCARD_DELEGATION_CRED_TYPE) return nla_read_ts_smartcard_creds(nla, s);
	else {
        WLog_ERR(TAG,  "credType unknown: %d\n", credType);
        return FALSE;
	}
}

BOOL nla_read_ts_credentials(rdpNla* nla, PSecBuffer ts_credentials)
{
	wStream* s;
	int length;
	int ts_creds_length;
	UINT32 * value = NULL;
	UINT32 credType=0;
	BOOL ret;

	s = Stream_New(ts_credentials->pvBuffer, ts_credentials->cbBuffer);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return FALSE;
	}

	/* TSCredentials (SEQUENCE) */
	ret = ber_read_sequence_tag(s, &length) &&
		/* [0] credType (INTEGER) */
		ber_read_contextual_tag(s, 0, &length, TRUE) &&
		ber_read_integer(s, value);

		credType = *value;

		/* [1] credentials (OCTET STRING) */
	ret	= ber_read_contextual_tag(s, 1, &length, TRUE) &&
		ber_read_octet_string_tag(s, &ts_creds_length) &&
		( nla_read_ts_creds(nla, s, credType) ) ;

	Stream_Free(s, FALSE);
	return ret;
}

int nla_write_ts_credentials(rdpNla* nla, wStream* s)
{
	int size = 0;
	int credSize;
	int innerSize = nla_sizeof_ts_credentials(nla);
	UINT32 credType = (UINT32) nla->credType;

	/* TSCredentials (SEQUENCE) */
	size += ber_write_sequence_tag(s, innerSize);

	/* [0] credType (INTEGER) */
	size += ber_write_contextual_tag(s, 0, ber_sizeof_integer(credType), TRUE);
	size += ber_write_integer(s, credType);

	/* [1] credentials (OCTET STRING) */
	credSize = ber_sizeof_sequence( (NLA_SIZEOF_TS_PWD_OR_SC_CREDS(nla, credType)) );
	size += ber_write_contextual_tag(s, 1, ber_sizeof_octet_string(credSize), TRUE);
	size += ber_write_octet_string_tag(s, credSize);
	size += ( nla_write_ts_creds(nla, s, nla->credType) );

	return size;
}

/**
 * Encode TSCredentials structure.
 * @param credssp
 */

BOOL nla_encode_ts_credentials(rdpNla* nla)
{
	wStream* s;
	int length;
	int DomainLength = 0;
	int UserLength = 0;
	int PasswordLength = 0;
	int PinLength = 0;
	int CardNameLength = 0;
	int ReaderNameLength = 0;
	int ContainerNameLength = 0;
	int CspNameLength = 0;
	int UserHintLength = 0;
	int DomainHintLength = 0;

	if (nla->identity)
	{
		if(nla->identity->PasswordLength)
		{
			/* TSPasswordCreds */
			DomainLength = nla->identity->DomainLength;
			UserLength = nla->identity->UserLength;
			PasswordLength = nla->identity->PasswordLength;
		}
		else
		{
			/* TSSmartCardCreds */
			if(nla->identity->CspData != NULL)
			{
				PinLength = nla->identity->PinLength;
				CardNameLength = nla->identity->CspData->CardNameLength;
				ReaderNameLength = nla->identity->CspData->ReaderNameLength;
				ContainerNameLength = nla->identity->CspData->ContainerNameLength;
				CspNameLength = nla->identity->CspData->CspNameLength;
				UserHintLength = nla->identity->UserHintLength;
				DomainHintLength = nla->identity->DomainHintLength;
			}
		}
	}

	if (nla->settings->DisableCredentialsDelegation && nla->identity)
	{
		/* TSPasswordCreds */
		nla->identity->DomainLength = 0;
		nla->identity->UserLength = 0;
		nla->identity->PasswordLength = 0;

		/* TSSmartCardCreds */
		if(nla->identity->CspData != NULL)
		{
			nla->identity->PinLength = 0;
			nla->identity->CspData->CardNameLength = 0;
			nla->identity->CspData->ReaderNameLength = 0;
			nla->identity->CspData->ContainerNameLength = 0;
			nla->identity->CspData->CspNameLength = 0;
 			nla->identity->UserHintLength = 0;
			nla->identity->DomainHintLength = 0;
		}
	}

	length = ber_sizeof_sequence(nla_sizeof_ts_credentials(nla));

	if (!sspi_SecBufferAlloc(&nla->tsCredentials, length))
	{
		WLog_ERR(TAG, "sspi_SecBufferAlloc failed!");
		return FALSE;
	}

	s = Stream_New((BYTE*) nla->tsCredentials.pvBuffer, length);

	if (!s)
	{
		sspi_SecBufferFree(&nla->tsCredentials);
		WLog_ERR(TAG, "Stream_New failed!");
		return FALSE;
	}

	nla_write_ts_credentials(nla, s);

	if (nla->settings->DisableCredentialsDelegation)
	{
		/* TSPasswordCreds */
		nla->identity->DomainLength = DomainLength;
		nla->identity->UserLength = UserLength;
		nla->identity->PasswordLength = PasswordLength;

		/* TSSmartCardCreds */
		nla->identity->PinLength = PinLength;
		nla->identity->CspData->CardNameLength = CardNameLength;
		nla->identity->CspData->ReaderNameLength = ReaderNameLength;
		nla->identity->CspData->ContainerNameLength = ContainerNameLength;
		nla->identity->CspData->CspNameLength = CspNameLength;
		nla->identity->UserHintLength = UserHintLength;
		nla->identity->DomainHintLength = DomainHintLength;
	}

	Stream_Free(s, FALSE);
	return TRUE;
}

SECURITY_STATUS nla_encrypt_ts_credentials(rdpNla* nla)
{
	SecBuffer Buffers[2];
	SecBufferDesc Message;
	SECURITY_STATUS status;

	if (!nla_encode_ts_credentials(nla))
		return SEC_E_INSUFFICIENT_MEMORY;

	if (!sspi_SecBufferAlloc(&nla->authInfo, 
							nla->tsCredentials.cbBuffer + nla->ContextSizes.cbSecurityTrailer))
		return SEC_E_INSUFFICIENT_MEMORY;
	
	if( strcmp(nla->packageName, KERBEROS_SSP_NAME) == 0 ){
		Buffers[0].BufferType = SECBUFFER_DATA; /* TSCredentials */
		Buffers[0].cbBuffer = nla->tsCredentials.cbBuffer;
		Buffers[0].pvBuffer = nla->authInfo.pvBuffer;
		CopyMemory(Buffers[0].pvBuffer, nla->tsCredentials.pvBuffer, Buffers[0].cbBuffer);
		Message.cBuffers = 1;
		Message.ulVersion = SECBUFFER_VERSION;
		Message.pBuffers = (PSecBuffer) &Buffers;
	}
	else if( (strcmp(nla->packageName, NEGOSSP_NAME) == 0) || (strcmp(nla->packageName, NTLMSSP_NAME) == 0) ){
		Buffers[0].BufferType = SECBUFFER_TOKEN; /* Signature */
		Buffers[0].cbBuffer = nla->ContextSizes.cbSecurityTrailer;
		Buffers[0].pvBuffer = nla->authInfo.pvBuffer;
		CopyMemory(Buffers[0].pvBuffer, nla->authInfo.pvBuffer, Buffers[0].cbBuffer);
		Buffers[1].BufferType = SECBUFFER_DATA; /* TSCredentials */
		Buffers[1].cbBuffer = nla->tsCredentials.cbBuffer;
		Buffers[1].pvBuffer = &((BYTE*) nla->authInfo.pvBuffer)[Buffers[0].cbBuffer];
		CopyMemory(Buffers[1].pvBuffer, nla->tsCredentials.pvBuffer, Buffers[1].cbBuffer);
		Message.cBuffers = 2;
		Message.ulVersion = SECBUFFER_VERSION;
		Message.pBuffers = (PSecBuffer) &Buffers;
	}

	status = nla->table->EncryptMessage(&nla->context, 0, &Message, nla->sendSeqNum++);

	if (status != SEC_E_OK)
	{
		WLog_ERR(TAG, "EncryptMessage failure %s [0x%08"PRIX32"]",
		         GetSecurityStatusString(status), status);
		return status;
	}

	return SEC_E_OK;
}

SECURITY_STATUS nla_decrypt_ts_credentials(rdpNla* nla)
{
	int length;
	BYTE* buffer;
	ULONG pfQOP;
	SecBuffer Buffers[2];
	SecBufferDesc Message;
	SECURITY_STATUS status;

	if (nla->authInfo.cbBuffer < 1)
	{
		WLog_ERR(TAG, "nla_decrypt_ts_credentials missing authInfo buffer");
		return SEC_E_INVALID_TOKEN;
	}

	length = nla->authInfo.cbBuffer;
	buffer = (BYTE*) malloc(length);

	if (!buffer)
		return SEC_E_INSUFFICIENT_MEMORY;

	if( strcmp(nla->packageName, KERBEROS_SSP_NAME) == 0 ){
		CopyMemory(buffer, nla->authInfo.pvBuffer, length);
		Buffers[0].BufferType = SECBUFFER_DATA; /* Wrapped and encrypted TSCredentials */
		Buffers[0].cbBuffer = length;
		Buffers[0].pvBuffer = buffer;
		Message.cBuffers = 1;
		Message.ulVersion = SECBUFFER_VERSION;
		Message.pBuffers = (PSecBuffer) &Buffers;
	}
	else if( ( strcmp(nla->packageName,  NEGOSSP_NAME) == 0 ) || ( strcmp(nla->packageName, NTLMSSP_NAME) == 0 ) ){
		CopyMemory(buffer, nla->authInfo.pvBuffer, length);
		Buffers[0].BufferType = SECBUFFER_TOKEN; /* Signature */
		Buffers[0].cbBuffer = nla->ContextSizes.cbSecurityTrailer;
		Buffers[0].pvBuffer = buffer;
		Buffers[1].BufferType = SECBUFFER_DATA; /* TSCredentials */
		Buffers[1].cbBuffer = length - nla->ContextSizes.cbSecurityTrailer;
		Buffers[1].pvBuffer = &buffer[ Buffers[0].cbBuffer ];
		Message.cBuffers = 2;
		Message.ulVersion = SECBUFFER_VERSION;
		Message.pBuffers = (PSecBuffer) &Buffers;
	}

	status = nla->table->DecryptMessage(&nla->context, &Message, nla->recvSeqNum++, &pfQOP);

	if (status != SEC_E_OK)
	{
		WLog_ERR(TAG, "DecryptMessage failure %s [0x%08"PRIX32"]",
		         GetSecurityStatusString(status), status);
		return status;
	}

	if(!nla_read_ts_credentials(nla, &Buffers[1]))
	{
		free(buffer);
		return SEC_E_INSUFFICIENT_MEMORY;
	}

	free(buffer);
	return SEC_E_OK;
}

int nla_sizeof_nego_token(int length)
{
	length = ber_sizeof_octet_string(length);
	length += ber_sizeof_contextual_tag(length);
	return length;
}

int nla_sizeof_nego_tokens(int length)
{
	length = nla_sizeof_nego_token(length);
	length += ber_sizeof_sequence_tag(length);
	length += ber_sizeof_sequence_tag(length);
	length += ber_sizeof_contextual_tag(length);
	return length;
}

int nla_sizeof_pub_key_auth(int length)
{
	length = ber_sizeof_octet_string(length);
	length += ber_sizeof_contextual_tag(length);
	return length;
}

int nla_sizeof_auth_info(int length)
{
	length = ber_sizeof_octet_string(length);
	length += ber_sizeof_contextual_tag(length);
	return length;
}

int nla_sizeof_ts_request(int length)
{
	length += ber_sizeof_integer(2);
	length += ber_sizeof_contextual_tag(3);
	return length;
}

/**
 * Send CredSSP message.
 * @param credssp
 */

BOOL nla_send(rdpNla* nla)
{
	wStream* s;
	int length;
	int ts_request_length;
	int nego_tokens_length = 0;
	int pub_key_auth_length = 0;
	int auth_info_length = 0;
	int error_code_context_length = 0;
	int error_code_length = 0;

	if (nla->version < 3 || nla->errorCode == 0)
	{
		nego_tokens_length = (nla->negoToken.cbBuffer > 0) ? nla_sizeof_nego_tokens(nla->negoToken.cbBuffer) : 0;
		pub_key_auth_length = (nla->pubKeyAuth.cbBuffer > 0) ? nla_sizeof_pub_key_auth(nla->pubKeyAuth.cbBuffer) : 0;
		auth_info_length = (nla->authInfo.cbBuffer > 0) ? nla_sizeof_auth_info(nla->authInfo.cbBuffer) : 0;
	}
	else
	{
		error_code_length = ber_sizeof_integer(nla->errorCode);
		error_code_context_length = ber_sizeof_contextual_tag(error_code_length);
	}

	length = nego_tokens_length + pub_key_auth_length + auth_info_length + error_code_context_length +
	         error_code_length;
	ts_request_length = nla_sizeof_ts_request(length);
	s = Stream_New(NULL, ber_sizeof_sequence(ts_request_length));

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return FALSE;
	}

	/* TSRequest */
	ber_write_sequence_tag(s, ts_request_length); /* SEQUENCE */
	/* [0] version */
	ber_write_contextual_tag(s, 0, 3, TRUE);
	ber_write_integer(s, nla->version); /* INTEGER */

	/* [1] negoTokens (NegoData) */
	if (nego_tokens_length > 0)
	{
		int length = ber_write_contextual_tag(s, 1,
		                                      ber_sizeof_sequence(ber_sizeof_sequence(ber_sizeof_sequence_octet_string(nla->negoToken.cbBuffer))),
		                                      TRUE); /* NegoData */
		length += ber_write_sequence_tag(s,
		                                 ber_sizeof_sequence(ber_sizeof_sequence_octet_string(
		                                         nla->negoToken.cbBuffer))); /* SEQUENCE OF NegoDataItem */
		length += ber_write_sequence_tag(s,
		                                 ber_sizeof_sequence_octet_string(nla->negoToken.cbBuffer)); /* NegoDataItem */
		length += ber_write_sequence_octet_string(s, 0, (BYTE*) nla->negoToken.pvBuffer,
		          nla->negoToken.cbBuffer);  /* OCTET STRING */

		if (length != nego_tokens_length)
			return FALSE;
	}

	/* [2] authInfo (OCTET STRING) */
	if (auth_info_length > 0)
	{
		if (ber_write_sequence_octet_string(s, 2, nla->authInfo.pvBuffer,
		                                    nla->authInfo.cbBuffer) != auth_info_length)
			return FALSE;
	}

	/* [3] pubKeyAuth (OCTET STRING) */
	if (pub_key_auth_length > 0)
	{
		if (ber_write_sequence_octet_string(s, 3, nla->pubKeyAuth.pvBuffer,
		                                    nla->pubKeyAuth.cbBuffer) != pub_key_auth_length)
			return FALSE;
	}

	/* [4] errorCode (INTEGER) */
	if (error_code_length > 0)
	{
		ber_write_contextual_tag(s, 4, error_code_length, TRUE);
		ber_write_integer(s, nla->errorCode);
	}

	Stream_SealLength(s);
	transport_write(nla->transport, s);
	Stream_Free(s, TRUE);
	return TRUE;
}

int nla_decode_ts_request(rdpNla* nla, wStream* s)
{
	int length;

	/* TSRequest */
	if (!ber_read_sequence_tag(s, &length) ||
	    !ber_read_contextual_tag(s, 0, &length, TRUE) ||
	    !ber_read_integer(s, &nla->version))
	{
		return -1;
	}

	/* [1] negoTokens (NegoData) */
	if (ber_read_contextual_tag(s, 1, &length, TRUE) != FALSE)
	{
		if (!ber_read_sequence_tag(s, &length) || /* SEQUENCE OF NegoDataItem */
		    !ber_read_sequence_tag(s, &length) || /* NegoDataItem */
		    !ber_read_contextual_tag(s, 0, &length, TRUE) || /* [0] negoToken */
		    !ber_read_octet_string_tag(s, &length) || /* OCTET STRING */
		    ((int) Stream_GetRemainingLength(s)) < length)
		{
			return -1;
		}

		if (!sspi_SecBufferAlloc(&nla->negoToken, length))
			return -1;

		Stream_Read(s, nla->negoToken.pvBuffer, length);
		nla->negoToken.cbBuffer = length;
	}

	/* [2] authInfo (OCTET STRING) */
	if (ber_read_contextual_tag(s, 2, &length, TRUE) != FALSE)
	{
		if (!ber_read_octet_string_tag(s, &length) || /* OCTET STRING */
		    ((int) Stream_GetRemainingLength(s)) < length)
			return -1;

		if (!sspi_SecBufferAlloc(&nla->authInfo, length))
			return -1;

		Stream_Read(s, nla->authInfo.pvBuffer, length);
		nla->authInfo.cbBuffer = length;
	}

	/* [3] pubKeyAuth (OCTET STRING) */
	if (ber_read_contextual_tag(s, 3, &length, TRUE) != FALSE)
	{
		if (!ber_read_octet_string_tag(s, &length) || /* OCTET STRING */
		    ((int) Stream_GetRemainingLength(s)) < length)
			return -1;

		if (!sspi_SecBufferAlloc(&nla->pubKeyAuth, length))
			return -1;

		Stream_Read(s, nla->pubKeyAuth.pvBuffer, length);
		nla->pubKeyAuth.cbBuffer = length;
	}

	/* [4] errorCode (INTEGER) */
	if (nla->version >= 3)
	{
		if (ber_read_contextual_tag(s, 4, &length, TRUE) != FALSE)
		{
			if (!ber_read_integer(s, &nla->errorCode))
				return -1;
		}
	}

	return 1;
}

int nla_recv_pdu(rdpNla* nla, wStream* s)
{
	if (nla_decode_ts_request(nla, s) < 1)
		return -1;

	if (nla->errorCode)
	{
		WLog_ERR(TAG, "SPNEGO failed with NTSTATUS: 0x%08"PRIX32"", nla->errorCode);
		freerdp_set_last_error(nla->instance->context, nla->errorCode);
		return -1;
	}

	if (nla_client_recv(nla) < 1)
		return -1;

	return 1;
}

int nla_recv(rdpNla* nla)
{
	wStream* s;
	int status;
	s = Stream_New(NULL, 4096);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return -1;
	}

	status = transport_read_pdu(nla->transport, s);

	if (status < 0)
	{
		WLog_ERR(TAG, "nla_recv() error: %d", status);
		Stream_Free(s, TRUE);
		return -1;
	}

	if (nla_decode_ts_request(nla, s) < 1)
	{
		Stream_Free(s, TRUE);
		return -1;
	}

	Stream_Free(s, TRUE);
	return 1;
}

void nla_buffer_print(rdpNla* nla)
{
	if (nla->negoToken.cbBuffer > 0)
	{
		WLog_DBG(TAG, "NLA.negoToken (length = %"PRIu32"):", nla->negoToken.cbBuffer);
		winpr_HexDump(TAG, WLOG_DEBUG, nla->negoToken.pvBuffer, nla->negoToken.cbBuffer);
	}

	if (nla->pubKeyAuth.cbBuffer > 0)
	{
		WLog_DBG(TAG, "NLA.pubKeyAuth (length = %"PRIu32"):", nla->pubKeyAuth.cbBuffer);
		winpr_HexDump(TAG, WLOG_DEBUG, nla->pubKeyAuth.pvBuffer, nla->pubKeyAuth.cbBuffer);
	}

	if (nla->authInfo.cbBuffer > 0)
	{
		WLog_DBG(TAG, "NLA.authInfo (length = %"PRIu32"):", nla->authInfo.cbBuffer);
		winpr_HexDump(TAG, WLOG_DEBUG, nla->authInfo.pvBuffer, nla->authInfo.cbBuffer);
	}
}

void nla_buffer_free(rdpNla* nla)
{
	sspi_SecBufferFree(&nla->negoToken);
	sspi_SecBufferFree(&nla->pubKeyAuth);
	sspi_SecBufferFree(&nla->authInfo);
}

LPTSTR nla_make_spn(const char* ServiceClass, const char* hostname)
{
	DWORD status;
	DWORD SpnLength;
	LPTSTR hostnameX = NULL;
	LPTSTR ServiceClassX = NULL;
	LPTSTR ServicePrincipalName = NULL;
#ifdef UNICODE
	ConvertToUnicode(CP_UTF8, 0, hostname, -1, &hostnameX, 0);
	ConvertToUnicode(CP_UTF8, 0, ServiceClass, -1, &ServiceClassX, 0);
#else
	hostnameX = _strdup(hostname);
	ServiceClassX = _strdup(ServiceClass);
#endif

	if (!hostnameX || !ServiceClassX)
	{
		free(hostnameX);
		free(ServiceClassX);
		return NULL;
	}

	if (!ServiceClass)
	{
		ServicePrincipalName = (LPTSTR) _tcsdup(hostnameX);
		free(ServiceClassX);
		free(hostnameX);
		return ServicePrincipalName;
	}

	SpnLength = 0;
	status = DsMakeSpn(ServiceClassX, hostnameX, NULL, 0, NULL, &SpnLength, NULL);

	if (status != ERROR_BUFFER_OVERFLOW)
	{
		free(ServiceClassX);
		free(hostnameX);
		return NULL;
	}

	ServicePrincipalName = (LPTSTR) malloc(SpnLength * sizeof(TCHAR));

	if (!ServicePrincipalName)
	{
		free(ServiceClassX);
		free(hostnameX);
		return NULL;
	}

	status = DsMakeSpn(ServiceClassX, hostnameX, NULL, 0, NULL, &SpnLength, ServicePrincipalName);

	if (status != ERROR_SUCCESS)
	{
		free(ServicePrincipalName);
		free(ServiceClassX);
		free(hostnameX);
		return NULL;
	}

	free(ServiceClassX);
	free(hostnameX);
	return ServicePrincipalName;
}

/**
 * Create new CredSSP state machine.
 * @param transport
 * @return new CredSSP state machine.
 */

rdpNla* nla_new(freerdp* instance, rdpTransport* transport, rdpSettings* settings)
{
	rdpNla* nla = (rdpNla*) calloc(1, sizeof(rdpNla));

	if (!nla)
		return NULL;

	nla->identity = calloc(1, sizeof(SEC_WINNT_AUTH_IDENTITY));

	if (!nla->identity)
	{
		free(nla);
		return NULL;
	}

	nla->instance = instance;
	nla->settings = settings;
	nla->server = settings->ServerMode;
	nla->transport = transport;
	nla->sendSeqNum = 0;
	nla->recvSeqNum = 0;
	nla->version = 3;

	if (settings->NtlmSamFile)
	{
		nla->SamFile = _strdup(settings->NtlmSamFile);

		if (!nla->SamFile)
		{
			free(nla->identity);
			free(nla);
			return NULL;
		}
	}

	ZeroMemory(&nla->negoToken, sizeof(SecBuffer));
	ZeroMemory(&nla->pubKeyAuth, sizeof(SecBuffer));
	ZeroMemory(&nla->authInfo, sizeof(SecBuffer));
	SecInvalidateHandle(&nla->context);

	if (nla->server)
	{
		LONG status;
		HKEY hKey;
		DWORD dwType;
		DWORD dwSize;
		status = RegOpenKeyExA(HKEY_LOCAL_MACHINE, SERVER_KEY,
		                       0, KEY_READ | KEY_WOW64_64KEY, &hKey);

		if (status != ERROR_SUCCESS)
			return nla;

		status = RegQueryValueEx(hKey, _T("SspiModule"), NULL, &dwType, NULL, &dwSize);

		if (status != ERROR_SUCCESS)
		{
			RegCloseKey(hKey);
			return nla;
		}

		nla->SspiModule = (LPTSTR) malloc(dwSize + sizeof(TCHAR));

		if (!nla->SspiModule)
		{
			RegCloseKey(hKey);
			free(nla);
			return NULL;
		}

		status = RegQueryValueEx(hKey, _T("SspiModule"), NULL, &dwType,
		                         (BYTE*) nla->SspiModule, &dwSize);

		if (status == ERROR_SUCCESS)
			WLog_INFO(TAG, "Using SSPI Module: %s", nla->SspiModule);

		RegCloseKey(hKey);
	}

	return nla;
}

/**
 * Free CredSSP state machine.
 * @param credssp
 */

void nla_free(rdpNla* nla)
{
	if (!nla)
		return;

	if (nla->table)
	{
		SECURITY_STATUS status;
		status = nla->table->DeleteSecurityContext(&nla->context);

		if (status != SEC_E_OK)
		{
			WLog_WARN(TAG, "DeleteSecurityContext status %s [0x%08"PRIX32"]",
			          GetSecurityStatusString(status), status);
		}
	}

	free(nla->SamFile);
	nla->SamFile = NULL;
	sspi_SecBufferFree(&nla->PublicKey);
	sspi_SecBufferFree(&nla->tsCredentials);
	free(nla->ServicePrincipalName);
	nla_identity_free(nla->identity);
	free(nla);
}
