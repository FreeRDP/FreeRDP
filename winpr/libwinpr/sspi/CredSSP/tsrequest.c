/**
 * WinPR: Windows Portable Runtime
 * CredSSP TSRequest codec ([MS-CSSP] 2.2.1)
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

#include <winpr/config.h>

#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/asn1.h>
#include <winpr/crypto.h>
#include <winpr/credssp.h>

#include "../../log.h"

#define TAG WINPR_TAG("sspi.CredSSP.tsrequest")

/*
 * TSRequest ::= SEQUENCE {
 * 	version    [0] INTEGER,
 * 	negoTokens [1] NegoData OPTIONAL,
 * 	authInfo   [2] OCTET STRING OPTIONAL,
 * 	pubKeyAuth [3] OCTET STRING OPTIONAL,
 * 	errorCode  [4] INTEGER OPTIONAL,
 * 	clientNonce[5] OCTET STRING OPTIONAL
 * }
 */

static BOOL ts_buffer_set(SecBuffer* buffer, const BYTE* data, size_t size)
{
	sspi_SecBufferFree(buffer);
	if (size == 0)
		return TRUE;
	if (!sspi_SecBufferAlloc(buffer, (ULONG)size))
		return FALSE;
	if (data)
		memcpy(buffer->pvBuffer, data, size);
	return TRUE;
}

static BOOL ts_write_octet_string(WinPrAsn1Encoder* enc, const SecBuffer* buffer,
                                  WinPrAsn1_tagId tagId)
{
	if (buffer->cbBuffer == 0)
		return TRUE;

	WinPrAsn1_OctetString os = { buffer->cbBuffer, buffer->pvBuffer };
	return WinPrAsn1EncContextualOctetString(enc, tagId, &os) != 0;
}

void winpr_tsrequest_free_contents(WINPR_TSREQUEST* request)
{
	if (!request)
		return;
	sspi_SecBufferFree(&request->negoToken);
	sspi_SecBufferFree(&request->authInfo);
	sspi_SecBufferFree(&request->pubKeyAuth);
	sspi_SecBufferFree(&request->clientNonce);
	request->errorCode = 0;
	request->version = 0;
}

BOOL winpr_tsrequest_encode(const WINPR_TSREQUEST* request, UINT32 peerVersion, BOOL server,
                            SecBuffer* output)
{
	WINPR_ASSERT(request);
	WINPR_ASSERT(output);

	BOOL rc = FALSE;
	WinPrAsn1Encoder* enc = WinPrAsn1Encoder_New(WINPR_ASN1_DER);
	if (!enc)
		return FALSE;

	/* TSRequest */
	if (!WinPrAsn1EncSeqContainer(enc))
		goto fail;

	/* version [0] INTEGER */
	if (!WinPrAsn1EncContextualInteger(
	        enc, 0, WINPR_ASSERTING_INT_CAST(WinPrAsn1_INTEGER, request->version)))
		goto fail;

	/* negoTokens [1] SEQUENCE OF SEQUENCE */
	if (request->negoToken.cbBuffer > 0)
	{
		if (!WinPrAsn1EncContextualSeqContainer(enc, 1) || !WinPrAsn1EncSeqContainer(enc))
			goto fail;
		/* negoToken [0] OCTET STRING */
		if (!ts_write_octet_string(enc, &request->negoToken, 0))
			goto fail;
		if (!WinPrAsn1EncEndContainer(enc) || !WinPrAsn1EncEndContainer(enc))
			goto fail;
	}

	/* authInfo [2] OCTET STRING */
	if (!ts_write_octet_string(enc, &request->authInfo, 2))
		goto fail;

	/* pubKeyAuth [3] OCTET STRING */
	if (!ts_write_octet_string(enc, &request->pubKeyAuth, 3))
		goto fail;

	/* errorCode [4] INTEGER */
	if (request->errorCode && (peerVersion >= 3) && (peerVersion != 5))
	{
		if (!WinPrAsn1EncContextualInteger(
		        enc, 4, WINPR_ASSERTING_INT_CAST(WinPrAsn1_INTEGER, request->errorCode)))
			goto fail;
	}

	/* clientNonce [5] OCTET STRING */
	if (!server && (request->clientNonce.cbBuffer > 0))
	{
		if (!ts_write_octet_string(enc, &request->clientNonce, 5))
			goto fail;
	}

	/* End TSRequest */
	if (!WinPrAsn1EncEndContainer(enc))
		goto fail;

	size_t length = 0;
	if (!WinPrAsn1EncStreamSize(enc, &length))
		goto fail;

	if (!sspi_SecBufferAlloc(output, (ULONG)length))
		goto fail;

	wStream staticStream = { 0 };
	wStream* s = Stream_StaticInit(&staticStream, output->pvBuffer, length);
	if (!WinPrAsn1EncToStream(enc, s))
	{
		sspi_SecBufferFree(output);
		goto fail;
	}
	output->cbBuffer = (ULONG)length;
	rc = TRUE;

fail:
	WinPrAsn1Encoder_Free(&enc);
	return rc;
}

BOOL winpr_tsrequest_decode(wStream* s, WINPR_TSREQUEST* request)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(request);

	WinPrAsn1Decoder dec = WinPrAsn1Decoder_init();
	WinPrAsn1Decoder dec2 = WinPrAsn1Decoder_init();
	BOOL error = FALSE;
	WinPrAsn1_tagId tag = 0;
	WinPrAsn1_INTEGER val = 0;

	WinPrAsn1Decoder_Init(&dec, WINPR_ASN1_DER, s);

	/* TSRequest */
	const size_t offset = WinPrAsn1DecReadSequence(&dec, &dec2);
	if (offset == 0)
		return FALSE;
	dec = dec2;

	/* version [0] INTEGER */
	if (WinPrAsn1DecReadContextualInteger(&dec, 0, &error, &val) == 0)
		return FALSE;
	if (!Stream_SafeSeek(s, offset))
		return FALSE;
	request->version = (UINT32)val;

	size_t tagcount = 0;
	while (WinPrAsn1DecReadContextualTag(&dec, &tag, &dec2) != 0)
	{
		WinPrAsn1Decoder dec3 = WinPrAsn1Decoder_init();
		WinPrAsn1_OctetString os = { 0 };

		tagcount++;
		switch (tag)
		{
			case 1:
				/* negoTokens [1] SEQUENCE OF SEQUENCE */
				if ((WinPrAsn1DecReadSequence(&dec2, &dec3) == 0) ||
				    (WinPrAsn1DecReadSequence(&dec3, &dec2) == 0))
					return FALSE;
				/* negoToken [0] OCTET STRING */
				if ((WinPrAsn1DecReadContextualOctetString(&dec2, 0, &error, &os, FALSE) == 0) &&
				    error)
					return FALSE;
				if (!ts_buffer_set(&request->negoToken, os.data, os.len))
					return FALSE;
				break;
			case 2:
				/* authInfo [2] OCTET STRING */
				if (WinPrAsn1DecReadOctetString(&dec2, &os, FALSE) == 0)
					return FALSE;
				if (!ts_buffer_set(&request->authInfo, os.data, os.len))
					return FALSE;
				break;
			case 3:
				/* pubKeyAuth [3] OCTET STRING */
				if (WinPrAsn1DecReadOctetString(&dec2, &os, FALSE) == 0)
					return FALSE;
				if (!ts_buffer_set(&request->pubKeyAuth, os.data, os.len))
					return FALSE;
				break;
			case 4:
				/* errorCode [4] INTEGER */
				if (WinPrAsn1DecReadInteger(&dec2, &val) == 0)
					return FALSE;
				request->errorCode = val;
				break;
			case 5:
				/* clientNonce [5] OCTET STRING */
				if (WinPrAsn1DecReadOctetString(&dec2, &os, FALSE) == 0)
					return FALSE;
				if (!ts_buffer_set(&request->clientNonce, os.data, os.len))
					return FALSE;
				break;
			default:
				return FALSE;
		}
	}

	return tagcount > 0;
}

/* ---- TSCredentials ([MS-CSSP] 2.2.1.2) ---- */

/* "Kerberos" as a little-endian UTF-16 string (no terminator), the package name
 * TSRemoteGuardCreds uses for the logon credential. */
static const BYTE KerberosPackageNameW[] = { 'K', 0, 'e', 0, 'r', 0, 'b', 0,
	                                         'e', 0, 'r', 0, 'o', 0, 's', 0 };

BOOL winpr_kerb_ticket_logon_encode(const KERB_TICKET_LOGON* ticket, SecBuffer* output)
{
	WINPR_ASSERT(ticket);
	WINPR_ASSERT(output);

	const size_t total = 32ull + ticket->ServiceTicketLength + ticket->TicketGrantingTicketLength;
	if (!sspi_SecBufferAlloc(output, (ULONG)total))
		return FALSE;

	wStream ss = { 0 };
	wStream* s = Stream_StaticInit(&ss, output->pvBuffer, total);
	Stream_Write_UINT32(s, KerbTicketLogon);
	Stream_Write_UINT32(s, ticket->Flags);
	Stream_Write_UINT32(s, ticket->ServiceTicketLength);
	Stream_Write_UINT32(s, ticket->TicketGrantingTicketLength);
	Stream_Write_UINT64(s, 0x20);                                  /* offset of the TGS ticket */
	Stream_Write_UINT64(s, 0x20ull + ticket->ServiceTicketLength); /* offset of the TGT */
	if (ticket->ServiceTicketLength)
		Stream_Write(s, ticket->ServiceTicket, ticket->ServiceTicketLength);
	if (ticket->TicketGrantingTicketLength)
		Stream_Write(s, ticket->TicketGrantingTicket, ticket->TicketGrantingTicketLength);

	output->cbBuffer = (ULONG)total;
	return TRUE;
}

BOOL winpr_kerb_ticket_logon_decode(const BYTE* data, size_t length, KERB_TICKET_LOGON* ticket)
{
	WINPR_ASSERT(data);
	WINPR_ASSERT(ticket);
	memset(ticket, 0, sizeof(*ticket));

	wStream ss = { 0 };
	wStream* s = Stream_StaticConstInit(&ss, data, length);

	/* 4 x UINT32 header, then 2 x UINT64 offsets */
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 32))
		return FALSE;

	UINT32 type = 0;
	Stream_Read_UINT32(s, type);
	if (type != KerbTicketLogon)
		return FALSE;
	ticket->MessageType = (KERB_LOGON_SUBMIT_TYPE)type;
	Stream_Read_UINT32(s, ticket->Flags);
	Stream_Read_UINT32(s, ticket->ServiceTicketLength);
	Stream_Read_UINT32(s, ticket->TicketGrantingTicketLength);
	Stream_Seek(s, 16); /* the two offsets */

	if (!Stream_CheckAndLogRequiredLength(
	        TAG, s, 1ull * ticket->ServiceTicketLength + ticket->TicketGrantingTicketLength))
		return FALSE;

	if (ticket->ServiceTicketLength)
	{
		ticket->ServiceTicket = (PUCHAR)malloc(ticket->ServiceTicketLength);
		if (!ticket->ServiceTicket)
			return FALSE;
		Stream_Read(s, ticket->ServiceTicket, ticket->ServiceTicketLength);
	}
	if (ticket->TicketGrantingTicketLength)
	{
		ticket->TicketGrantingTicket = (PUCHAR)malloc(ticket->TicketGrantingTicketLength);
		if (!ticket->TicketGrantingTicket)
		{
			free(ticket->ServiceTicket);
			ticket->ServiceTicket = nullptr;
			return FALSE;
		}
		Stream_Read(s, ticket->TicketGrantingTicket, ticket->TicketGrantingTicketLength);
	}
	return TRUE;
}

static BOOL ts_encoder_to_buffer(WinPrAsn1Encoder* enc, SecBuffer* output)
{
	size_t length = 0;
	if (!WinPrAsn1EncStreamSize(enc, &length))
		return FALSE;
	if (!sspi_SecBufferAlloc(output, (ULONG)length))
		return FALSE;

	wStream ss = { 0 };
	wStream* s = Stream_StaticInit(&ss, output->pvBuffer, length);
	if (!WinPrAsn1EncToStream(enc, s))
	{
		sspi_SecBufferFree(output);
		return FALSE;
	}
	output->cbBuffer = (ULONG)length;
	return TRUE;
}

BOOL winpr_tscredentials_encode_password(const SEC_WINNT_AUTH_IDENTITY_W* identity,
                                         SecBuffer* output)
{
	WINPR_ASSERT(output);

	BOOL rc = FALSE;
	WinPrAsn1Encoder* enc = WinPrAsn1Encoder_New(WINPR_ASN1_DER);
	if (!enc)
		return FALSE;

	WinPrAsn1_OctetString domain = { 0 };
	WinPrAsn1_OctetString username = { 0 };
	WinPrAsn1_OctetString password = { 0 };
	if (identity)
	{
		domain.len = identity->DomainLength * sizeof(WCHAR);
		domain.data = (BYTE*)identity->Domain;
		username.len = identity->UserLength * sizeof(WCHAR);
		username.data = (BYTE*)identity->User;
		password.len = identity->PasswordLength * sizeof(WCHAR);
		password.data = (BYTE*)identity->Password;
	}

	/* TSCredentials */
	if (!WinPrAsn1EncSeqContainer(enc))
		goto fail;
	if (!WinPrAsn1EncContextualInteger(enc, 0, WINPR_TSCREDS_USER_PASSWD))
		goto fail;
	if (!WinPrAsn1EncContextualOctetStringContainer(enc, 1))
		goto fail;
	/* TSPasswordCreds */
	if (!WinPrAsn1EncSeqContainer(enc))
		goto fail;
	if ((WinPrAsn1EncContextualOctetString(enc, 0, &domain) == 0) ||
	    (WinPrAsn1EncContextualOctetString(enc, 1, &username) == 0) ||
	    (WinPrAsn1EncContextualOctetString(enc, 2, &password) == 0))
		goto fail;
	if (!WinPrAsn1EncEndContainer(enc)) /* TSPasswordCreds */
		goto fail;
	if (!WinPrAsn1EncEndContainer(enc) || !WinPrAsn1EncEndContainer(enc)) /* creds, TSCredentials */
		goto fail;

	rc = ts_encoder_to_buffer(enc, output);

fail:
	WinPrAsn1Encoder_Free(&enc);
	return rc;
}

BOOL winpr_tscredentials_encode_remoteguard_kerb(const KERB_TICKET_LOGON* ticket, SecBuffer* output)
{
	WINPR_ASSERT(ticket);
	WINPR_ASSERT(output);

	BOOL rc = FALSE;
	SecBuffer kerb = { 0 };
	WinPrAsn1Encoder* enc = WinPrAsn1Encoder_New(WINPR_ASN1_DER);
	if (!enc)
		return FALSE;

	if (!winpr_kerb_ticket_logon_encode(ticket, &kerb))
		goto fail;

	WinPrAsn1_OctetString packageName = { sizeof(KerberosPackageNameW),
		                                  (BYTE*)KerberosPackageNameW };
	WinPrAsn1_OctetString credBuffer = { kerb.cbBuffer, kerb.pvBuffer };

	/* TSCredentials */
	if (!WinPrAsn1EncSeqContainer(enc))
		goto fail;
	if (!WinPrAsn1EncContextualInteger(enc, 0, WINPR_TSCREDS_REMOTEGUARD))
		goto fail;
	if (!WinPrAsn1EncContextualOctetStringContainer(enc, 1))
		goto fail;
	/* TSRemoteGuardCreds */
	if (!WinPrAsn1EncSeqContainer(enc))
		goto fail;
	/* logonCred [0] TSRemoteGuardPackageCred */
	if (!WinPrAsn1EncContextualSeqContainer(enc, 0))
		goto fail;
	if ((WinPrAsn1EncContextualOctetString(enc, 0, &packageName) == 0) ||
	    (WinPrAsn1EncContextualOctetString(enc, 1, &credBuffer) == 0))
		goto fail;
	if (!WinPrAsn1EncEndContainer(enc)) /* logonCred */
		goto fail;
	if (!WinPrAsn1EncEndContainer(enc)) /* TSRemoteGuardCreds */
		goto fail;
	if (!WinPrAsn1EncEndContainer(enc) || !WinPrAsn1EncEndContainer(enc)) /* creds, TSCredentials */
		goto fail;

	rc = ts_encoder_to_buffer(enc, output);

fail:
	sspi_SecBufferFree(&kerb);
	WinPrAsn1Encoder_Free(&enc);
	return rc;
}

/* ---- pubKeyAuth hash ([MS-CSSP] 3.1.5, version 6) ---- */

/* "CredSSP Client-To-Server Binding Hash\0" */
static const BYTE ClientServerHashMagic[] = { 0x43, 0x72, 0x65, 0x64, 0x53, 0x53, 0x50, 0x20,
	                                          0x43, 0x6C, 0x69, 0x65, 0x6E, 0x74, 0x2D, 0x54,
	                                          0x6F, 0x2D, 0x53, 0x65, 0x72, 0x76, 0x65, 0x72,
	                                          0x20, 0x42, 0x69, 0x6E, 0x64, 0x69, 0x6E, 0x67,
	                                          0x20, 0x48, 0x61, 0x73, 0x68, 0x00 };

/* "CredSSP Server-To-Client Binding Hash\0" */
static const BYTE ServerClientHashMagic[] = { 0x43, 0x72, 0x65, 0x64, 0x53, 0x53, 0x50, 0x20,
	                                          0x53, 0x65, 0x72, 0x76, 0x65, 0x72, 0x2D, 0x54,
	                                          0x6F, 0x2D, 0x43, 0x6C, 0x69, 0x65, 0x6E, 0x74,
	                                          0x20, 0x42, 0x69, 0x6E, 0x64, 0x69, 0x6E, 0x67,
	                                          0x20, 0x48, 0x61, 0x73, 0x68, 0x00 };

BOOL winpr_credssp_public_key_hash(BOOL server, const SecBuffer* nonce, const SecBuffer* publicKey,
                                   SecBuffer* hash)
{
	WINPR_ASSERT(nonce);
	WINPR_ASSERT(publicKey);
	WINPR_ASSERT(hash);

	const BYTE* magic = server ? ServerClientHashMagic : ClientServerHashMagic;
	const size_t magicSize = server ? sizeof(ServerClientHashMagic) : sizeof(ClientServerHashMagic);

	BOOL rc = FALSE;
	WINPR_DIGEST_CTX* sha256 = winpr_Digest_New();
	if (!sha256)
		return FALSE;

	if (!sspi_SecBufferAlloc(hash, WINPR_SHA256_DIGEST_LENGTH))
		goto out;

	if (!winpr_Digest_Init(sha256, WINPR_MD_SHA256) ||
	    !winpr_Digest_Update(sha256, magic, magicSize) ||
	    !winpr_Digest_Update(sha256, (const BYTE*)nonce->pvBuffer, nonce->cbBuffer) ||
	    !winpr_Digest_Update(sha256, (const BYTE*)publicKey->pvBuffer, publicKey->cbBuffer) ||
	    !winpr_Digest_Final(sha256, hash->pvBuffer, WINPR_SHA256_DIGEST_LENGTH))
	{
		sspi_SecBufferFree(hash);
		goto out;
	}

	rc = TRUE;

out:
	winpr_Digest_Free(sha256);
	return rc;
}
