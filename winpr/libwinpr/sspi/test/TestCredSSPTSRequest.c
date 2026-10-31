#include <stdio.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/sspi.h>
#include <winpr/stream.h>
#include <winpr/asn1.h>
#include <winpr/secapi.h>
#include <winpr/credssp.h>

static BOOL set_buf(SecBuffer* b, const char* data)
{
	const ULONG len = (ULONG)strlen(data);
	if (!sspi_SecBufferAlloc(b, len))
		return FALSE;
	memcpy(b->pvBuffer, data, len);
	return TRUE;
}

static BOOL buf_equals(const SecBuffer* b, const char* data)
{
	const ULONG len = (ULONG)strlen(data);
	return (b->cbBuffer == len) && (memcmp(b->pvBuffer, data, len) == 0);
}

static int roundtrip(UINT32 peerVersion, BOOL server, BOOL expectError, BOOL expectNonce)
{
	int rc = -1;
	WINPR_TSREQUEST in = { 0 };
	WINPR_TSREQUEST out = { 0 };
	SecBuffer encoded = { 0 };

	in.version = 6;
	in.errorCode = 0x0000000C;
	if (!set_buf(&in.negoToken, "nego-token-data") || !set_buf(&in.authInfo, "auth-info-data") ||
	    !set_buf(&in.pubKeyAuth, "pubkey-auth-data") || !set_buf(&in.clientNonce, "client-nonce!!"))
		goto out;

	if (!winpr_tsrequest_encode(&in, peerVersion, server, &encoded))
	{
		(void)fprintf(stderr, "encode failed\n");
		goto out;
	}

	wStream ss = { 0 };
	wStream* s = Stream_StaticInit(&ss, encoded.pvBuffer, encoded.cbBuffer);
	if (!winpr_tsrequest_decode(s, &out))
	{
		(void)fprintf(stderr, "decode failed\n");
		goto out;
	}

	if ((out.version != 6) || !buf_equals(&out.negoToken, "nego-token-data") ||
	    !buf_equals(&out.authInfo, "auth-info-data") ||
	    !buf_equals(&out.pubKeyAuth, "pubkey-auth-data"))
	{
		(void)fprintf(stderr, "core field mismatch\n");
		goto out;
	}

	if (expectError && (out.errorCode != 0x0000000C))
	{
		(void)fprintf(stderr, "errorCode expected but missing\n");
		goto out;
	}
	if (!expectError && (out.errorCode != 0))
	{
		(void)fprintf(stderr, "errorCode present but should be suppressed\n");
		goto out;
	}

	if (expectNonce && !buf_equals(&out.clientNonce, "client-nonce!!"))
	{
		(void)fprintf(stderr, "clientNonce expected but missing\n");
		goto out;
	}
	if (!expectNonce && (out.clientNonce.cbBuffer != 0))
	{
		(void)fprintf(stderr, "clientNonce present but should be suppressed\n");
		goto out;
	}

	rc = 0;

out:
	winpr_tsrequest_free_contents(&in);
	winpr_tsrequest_free_contents(&out);
	sspi_SecBufferFree(&encoded);
	return rc;
}

static int ts_cred_type(const SecBuffer* creds)
{
	WinPrAsn1Decoder dec = WinPrAsn1Decoder_init();
	WinPrAsn1Decoder seq = WinPrAsn1Decoder_init();
	BOOL error = FALSE;
	WinPrAsn1_INTEGER credType = -1;

	WinPrAsn1Decoder_InitMem(&dec, WINPR_ASN1_DER, creds->pvBuffer, creds->cbBuffer);
	if (WinPrAsn1DecReadSequence(&dec, &seq) == 0)
		return -1;
	if (WinPrAsn1DecReadContextualInteger(&seq, 0, &error, &credType) == 0)
		return -1;
	return (int)credType;
}

static int test_kerb_tscreds(void)
{
	BYTE tgs[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 };
	BYTE tgt[] = { 0xAA, 0xBB, 0xCC, 0xDD };
	KERB_TICKET_LOGON kt = { 0 };
	kt.MessageType = KerbTicketLogon;
	kt.Flags = KERB_LOGON_FLAG_REDIRECTED;
	kt.ServiceTicketLength = sizeof(tgs);
	kt.ServiceTicket = tgs;
	kt.TicketGrantingTicketLength = sizeof(tgt);
	kt.TicketGrantingTicket = tgt;

	int rc = -1;
	SecBuffer enc = { 0 };
	SecBuffer creds6 = { 0 };
	SecBuffer creds1 = { 0 };
	KERB_TICKET_LOGON dk = { 0 };

	/* KERB_TICKET_LOGON round-trip */
	if (!winpr_kerb_ticket_logon_encode(&kt, &enc))
		goto out;
	if (!winpr_kerb_ticket_logon_decode(enc.pvBuffer, enc.cbBuffer, &dk))
		goto out;
	if ((dk.Flags != kt.Flags) || (dk.ServiceTicketLength != sizeof(tgs)) ||
	    (dk.TicketGrantingTicketLength != sizeof(tgt)) ||
	    (memcmp(dk.ServiceTicket, tgs, sizeof(tgs)) != 0) ||
	    (memcmp(dk.TicketGrantingTicket, tgt, sizeof(tgt)) != 0))
	{
		(void)fprintf(stderr, "KERB_TICKET_LOGON round-trip mismatch\n");
		goto out;
	}

	/* TSCredentials credType 6 (remote guard) */
	if (!winpr_tscredentials_encode_remoteguard_kerb(&kt, &creds6) ||
	    (ts_cred_type(&creds6) != WINPR_TSCREDS_REMOTEGUARD))
	{
		(void)fprintf(stderr, "TSCredentials credType 6 wrong\n");
		goto out;
	}

	/* TSCredentials credType 1 (password) */
	WCHAR user[] = { 'u', 's', 'e', 'r', 0 };
	WCHAR dom[] = { 'd', 'o', 'm', 0 };
	WCHAR pwd[] = { 'p', 'a', 's', 's', 0 };
	SEC_WINNT_AUTH_IDENTITY_W id = { 0 };
	id.User = (UINT16*)user;
	id.UserLength = 4;
	id.Domain = (UINT16*)dom;
	id.DomainLength = 3;
	id.Password = (UINT16*)pwd;
	id.PasswordLength = 4;
	if (!winpr_tscredentials_encode_password(&id, &creds1) ||
	    (ts_cred_type(&creds1) != WINPR_TSCREDS_USER_PASSWD))
	{
		(void)fprintf(stderr, "TSCredentials credType 1 wrong\n");
		goto out;
	}

	rc = 0;

out:
	free(dk.ServiceTicket);
	free(dk.TicketGrantingTicket);
	sspi_SecBufferFree(&enc);
	sspi_SecBufferFree(&creds6);
	sspi_SecBufferFree(&creds1);
	return rc;
}

static int test_pubkey_hash(void)
{
	BYTE nonceData[32];
	BYTE pkData[270];
	for (size_t i = 0; i < sizeof(nonceData); i++)
		nonceData[i] = (BYTE)(i + 1);
	for (size_t i = 0; i < sizeof(pkData); i++)
		pkData[i] = (BYTE)(i * 7 + 3);
	SecBuffer nonce = { sizeof(nonceData), SECBUFFER_DATA, nonceData };
	SecBuffer pk = { sizeof(pkData), SECBUFFER_DATA, pkData };

	int rc = -1;
	SecBuffer h1 = { 0 };
	SecBuffer h2 = { 0 };
	SecBuffer h3 = { 0 };
	if (!winpr_credssp_public_key_hash(FALSE, &nonce, &pk, &h1) ||
	    !winpr_credssp_public_key_hash(FALSE, &nonce, &pk, &h2) ||
	    !winpr_credssp_public_key_hash(TRUE, &nonce, &pk, &h3))
		goto out;

	/* 32-byte SHA256, deterministic, and the two directions differ (different magic) */
	if ((h1.cbBuffer != 32) || (h2.cbBuffer != 32) || (h3.cbBuffer != 32))
	{
		(void)fprintf(stderr, "pubKeyAuth hash wrong length\n");
		goto out;
	}
	if (memcmp(h1.pvBuffer, h2.pvBuffer, 32) != 0)
	{
		(void)fprintf(stderr, "pubKeyAuth hash not deterministic\n");
		goto out;
	}
	if (memcmp(h1.pvBuffer, h3.pvBuffer, 32) == 0)
	{
		(void)fprintf(stderr, "client and server pubKeyAuth hashes should differ\n");
		goto out;
	}
	rc = 0;

out:
	sspi_SecBufferFree(&h1);
	sspi_SecBufferFree(&h2);
	sspi_SecBufferFree(&h3);
	return rc;
}

int TestCredSSPTSRequest(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	if (test_kerb_tscreds() != 0)
		return -1;
	if (test_pubkey_hash() != 0)
		return -1;

	/* client, peer v6: errorCode and clientNonce both emitted */
	if (roundtrip(6, FALSE, TRUE, TRUE) != 0)
		return -1;

	/* peer v2: errorCode suppressed (needs >= 3) */
	if (roundtrip(2, FALSE, FALSE, TRUE) != 0)
		return -1;

	/* peer v5: errorCode suppressed (the v5 exception) */
	if (roundtrip(5, FALSE, FALSE, TRUE) != 0)
		return -1;

	/* server: clientNonce suppressed */
	if (roundtrip(6, TRUE, TRUE, FALSE) != 0)
		return -1;

	return 0;
}
