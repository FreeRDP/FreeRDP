#include <winpr/print.h>

#include <rdpear-common/ndr.h>
#include <rdpear-common/rdpear_common.h>

#ifndef MAX
#define MAX(a, b) ((a) > (b)) ? (a) : (b)
#endif
#ifndef MIN
#define MIN(a, b) ((a) < (b)) ? (a) : (b)
#endif

static BYTE nextValue(BYTE old, INT32 offset, char symbol, char startSymbol)
{
	const INT32 uold = 16 * old;
	const INT32 diff = symbol - startSymbol;
	const INT32 res = uold + diff + offset;
	return (BYTE)MIN(MAX(0, res), UINT8_MAX);
}

static BYTE* parseHexBlock(const char* str, size_t* plen)
{
	WINPR_ASSERT(str);
	WINPR_ASSERT(plen);

	BYTE* ret = malloc(strlen(str) / 2);
	BYTE* dest = ret;
	const char* ptr = str;
	BYTE tmp = 0;
	size_t nchars = 0;
	size_t len = 0;

	for (; *ptr; ptr++)
	{
		switch (*ptr)
		{
			case ' ':
			case '\n':
			case '\t':
				if (nchars)
				{
					WLog_ERR("", "error parsing hex block, unpaired char");
					free(ret);
					return NULL;
				}
				break;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				tmp = nextValue(tmp, 0, *ptr, '0');
				nchars++;
				if (nchars == 2)
				{
					*dest = tmp;
					dest++;
					len++;
					tmp = 0;
					nchars = 0;
				}
				break;

			case 'a':
			case 'b':
			case 'c':
			case 'd':
			case 'e':
			case 'f':
				tmp = nextValue(tmp, 10, *ptr, 'a');
				nchars++;
				if (nchars == 2)
				{
					*dest = tmp;
					dest++;
					len++;
					tmp = 0;
					nchars = 0;
				}
				break;

			case 'A':
			case 'B':
			case 'C':
			case 'D':
			case 'E':
			case 'F':
				tmp = nextValue(tmp, 10, *ptr, 'A');
				nchars++;
				if (nchars == 2)
				{
					*dest = tmp;
					dest++;
					len++;
					tmp = 0;
					nchars = 0;
				}
				break;
			default:
				WLog_ERR("", "invalid char in hex block");
				free(ret);
				return NULL;
		}
	}

	*plen = len;
	return ret;
}

static int TestNdrEarWrite(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	int rc = -1;
	BYTE buffer[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
	KERB_ASN1_DATA asn1 = { 7, { 16 }, buffer };

	wStream* s = Stream_New(NULL, 100);
	if (!s)
		return -1;

	NdrContext* context = ndr_context_new(FALSE, 1);
	if (!context)
		goto fail;

	if (!ndr_write_KERB_ASN1_DATA(context, s, NULL, &asn1))
		goto fail;
	if (!ndr_treat_deferred_write(context, s))
		goto fail;

	// winpr_HexDump("", WLOG_DEBUG, Stream_Buffer(s), Stream_GetPosition(s));

	rc = 0;
fail:
	ndr_context_destroy(&context);
	Stream_Free(s, TRUE);
	return rc;
}

static int TestNdrEarRead(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	int retCode = -2;

	/* ====================================================================== */
	NdrContext* context = ndr_context_new(FALSE, 1);
	if (!context)
		return -1;

	wStream staticS = { 0 };
	wStream* s = NULL;

#if 0
	BYTE payload[] = {
		0x00, 0x00, 0x00, 0x00, // (PduType)
		0x02, 0x00, 0x00, 0x00, // (Length)
		0x28, 0x00, 0x02, 0x00, // (Asn1Buffer)

		// == conformant array ==
		0x02, 0x00, 0x00, 0x00, // (nitems)
		0x30, 0x00,             // content
		0x00, 0x00              // (padding)
	};
	s = Stream_StaticInit(&staticS, payload, sizeof(payload));

	KERB_ASN1_DATA asn1 = { 0 };
	if (!ndr_read_KERB_ASN1_DATA(context, s, NULL, &asn1) || !ndr_treat_deferred_read(context, s) ||
			asn1.Asn1BufferHints.count != 2 || *asn1.Asn1Buffer != 0x30)
		goto out;
	KERB_ASN1_DATA_destroy(context, &asn1);
	ndr_context_reset(context);

	/* ====================================================================== */
	BYTE payload2[] = {
		// ------------ a RPC_UNICODE_STRING: Administrateur -------------------------
		0x1c, 0x00,             // (Length)
		0x1e, 0x00,             // (MaximumLength)
		0x1c, 0x00, 0x02, 0x00, // (Buffer ptr)
		                        // == conformant array ==
		0x0f, 0x00, 0x00, 0x00, // (maximum count)
		0x00, 0x00, 0x00, 0x00, // (offset)
		0x0e, 0x00, 0x00, 0x00, // (length)

		0x48, 0x00, 0x41, 0x00, 0x52, 0x00, 0x44, 0x00, 0x45, 0x00, 0x4e, 0x00, 0x49, 0x00, 0x4e,
		0x00, 0x47, 0x00, 0x33, 0x00, 0x2e, 0x00, 0x43, 0x00, 0x4f, 0x00, 0x4d, 0x00, 0x00, 0x00,

		0x00, 0x00
	};
	retCode = -3;

	s = Stream_StaticInit(&staticS, payload2, sizeof(payload2));
	RPC_UNICODE_STRING unicode = { 0 };
	if (!ndr_read_RPC_UNICODE_STRING(context, s, NULL, &unicode) || !ndr_treat_deferred_read(context, s))
		goto out;
	RPC_UNICODE_STRING_destroy(context, &unicode);
	ndr_context_reset(context);

	/* ====================================================================== */
	BYTE payload3[] = {
		// ------------ an KERB_RPC_INTERNAL_NAME: HARDENING3.COM -------------------------
		0x01, 0x00,             // (NameType)
		0x01, 0x00,             // (NameCount)
		0x10, 0x00, 0x02, 0x00, // (Names)
		                        // == conformant array ==
		0x01, 0x00, 0x00, 0x00, // (nitems)

		// = RPC_UNICODE_STRING =
		0x1c, 0x00,             // (Length)
		0x1e, 0x00,             // (MaximumLength)
		0x14, 0x00, 0x02, 0x00, /// (Buffer ptr)
		                        // == Uni-dimensional Conformant-varying Array ==
		0x0f, 0x00, 0x00, 0x00, // (maximum count)
		0x00, 0x00, 0x00, 0x00, // (offset)
		0x0e, 0x00, 0x00, 0x00, // (length)
		0x41, 0x00, 0x64, 0x00, 0x6d, 0x00, 0x69, 0x00, 0x6e, 0x00, 0x69, 0x00, 0x73, 0x00, 0x74,
		0x00, 0x72, 0x00, 0x61, 0x00, 0x74, 0x00, 0x65, 0x00, 0x75, 0x00, 0x72, 0x00, 0x00, 0x00,

		0x00, 0x00
	};
	KERB_RPC_INTERNAL_NAME intName = { 0 };
	retCode = -4;
	s = Stream_StaticInit(&staticS, payload3, sizeof(payload3));
	if (!ndr_read_KERB_RPC_INTERNAL_NAME(context, s, NULL, &intName)  || !ndr_treat_deferred_read(context, s))
		goto out;
	KERB_RPC_INTERNAL_NAME_destroy(context, &intName);
	ndr_context_reset(context);
#endif

	/* ====================================================================== */
#if 0
	BYTE payload4[] = {
			0x03, 0x01, 0x03, 0x01, // unionId / unionId
			0x04, 0x00, 0x02, 0x00, // (EncryptionKey ptr)
			0xf8, 0xca, 0x95, 0x11, // (SequenceNumber)
			0x0c, 0x00, 0x02, 0x00, // (ClientName ptr)
			0x18, 0x00, 0x02, 0x00, // (ClientRealm ptr)
			0x20, 0x00, 0x02, 0x00, // (SkewTime ptr)
			0x00, 0x00, 0x00, 0x00, // (SubKey ptr)
			0x24, 0x00, 0x02, 0x00, // (AuthData ptr)
			0x2c, 0x00, 0x02, 0x00, // (GssChecksum ptr)
			0x07, 0x00, 0x00, 0x00, // (KeyUsage)

			// === EncryptionKey ===
			0x40, 0xe9, 0x12, 0xdf, // reserved1
			0x12, 0x00, 0x00, 0x00, // reserved2
			    // KERB_RPC_OCTET_STRING
				0x4c, 0x00, 0x00, 0x00, // (length)
				0x08, 0x00, 0x02, 0x00, // (value ptr)
			    // == conformant array ==
				0x4c, 0x00, 0x00, 0x00, // (length)
				0xc4, 0x41, 0xee, 0x34,
				0x82, 0x2b, 0x29, 0x61, 0xe2, 0x96, 0xb5, 0x75, 0x61, 0x2d, 0xbf, 0x86, 0x01, 0x00, 0x00, 0x00,
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x89, 0x08, 0x60, 0x2e,
				0x30, 0x3e, 0xfe, 0x56, 0x11, 0xf0, 0x31, 0xf2, 0xd6, 0x2e, 0x3d, 0x33, 0xfe, 0xce, 0x56, 0x12,
				0xbf, 0xb2, 0xe5, 0x86, 0x29, 0x8d, 0x29, 0x74, 0x1f, 0x8a, 0xf9, 0xb9, 0x8c, 0xd4, 0x86, 0x3a,
				0x21, 0x92, 0xb2, 0x07, 0x95, 0x4b, 0xea, 0xee,

			//=== ClientName - KERB_RPC_INTERNAL_NAME ===
			0x01, 0x00, // (NameType)
			0x01, 0x00, // (NameCount)
			0x10, 0x00, 0x02, 0x00, // (Names)

				0x01, 0x00, 0x00, 0x00, // (nitems)

			    // = RPC_UNICODE_STRING =
				0x1c, 0x00, // (Length)
				0x1e, 0x00, // (MaximumLength)
				0x14, 0x00, 0x02, 0x00, //(Buffer ptr)
			    // == Uni-dimensional Conformant-varying Array ==
			    0x0f, 0x00, 0x00, 0x00, // (maximum count)
				0x00, 0x00, 0x00, 0x00, // (offset)
				0x0e, 0x00, 0x00, 0x00, // (length)
				0x41, 0x00, 0x64, 0x00, 0x6d, 0x00, 0x69, 0x00, 0x6e, 0x00, 0x69, 0x00, 0x73, 0x00, 0x74, 0x00,
				0x72, 0x00, 0x61, 0x00, 0x74, 0x00, 0x65, 0x00, 0x75, 0x00, 0x72, 0x00,

			// === ClientRealm - RPC_UNICODE_STRING ===
			0x1c, 0x00, // (Length)
			0x1e, 0x00, // (MaximumLength)
			0x1c, 0x00, 0x02, 0x00, // (Buffer ptr)
			    // == Uni-dimensional conformant varying array ==
				0x0f, 0x00, 0x00, 0x00, // (maximum count)
				0x00, 0x00, 0x00, 0x00, // (offset)
				0x0e, 0x00, 0x00, 0x00, // (length)
				0x48, 0x00, 0x41, 0x00, 0x52, 0x00, 0x44, 0x00, 0x45, 0x00, 0x4e, 0x00, 0x49, 0x00, 0x4e, 0x00,
				0x47, 0x00, 0x33, 0x00, 0x2e, 0x00, 0x43, 0x00,	0x4f, 0x00, 0x4d, 0x00,

			0x00, 0x00, 0x00, 0x00, // padding

			// == SkewTime ==
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00,

			// === AuthData - KERB_ASN1_DATA ==
			0x00, 0x00, 0x00, 0x00, // (PduType)
			0x02, 0x00, 0x00, 0x00, // (Length)
			0x28, 0x00, 0x02, 0x00, // (Asn1Buffer)
			    // == conformant array ==
				0x02, 0x00, 0x00, 0x00, // (nitems)
				0x30, 0x00,
				0x00, 0x00, // (padding)

			// === GssChecksum - KERB_ASN1_DATA ===
			0x08, 0x00, 0x00, 0x00, // (PduType)
			0x1b, 0x00, 0x00, 0x00, // (Length)
			0x30, 0x00, 0x02, 0x00, // (Asn1Buffer)
			    // == conformant array ==
				0x1b, 0x00, 0x00, 0x00, // (length)
				0x30, 0x19,
					0xa0, 0x03,
						0x02, 0x01, 0x07,
					0xa1, 0x12,
						0x04, 0x10, 0xb9, 0x4f, 0xcd, 0xae, 0xd9, 0xa8, 0xff, 0x49, 0x69, 0x5a, 0xd1,
						0x1d, 0x38, 0x49, 0xb6, 0x92, 0x00
	};
	size_t sizeofPayload4 = sizeof(payload4);
#endif
#if 1
	size_t sizeofPayload4 = 0;
	BYTE* payload4 = parseHexBlock("03 01 03 01 \
		04 00 02 00 38 9e ef 6b 0c 00 02 00 18 00 02 00 \
		20 00 02 00 00 00 00 00 24 00 02 00 2c 00 02 00 \
		07 00 00 00 13 8a a5 a8 12 00 00 00 20 00 00 00 \
		08 00 02 00 20 00 00 00 c9 03 42 a8 17 8f d9 c4 \
		9b d2 c4 6e 73 64 98 7b 90 f5 9a 28 77 8e ca de \
		29 2e a3 8d 8a 56 36 d5 01 00 01 00 10 00 02 00 \
		01 00 00 00 1c 00 1e 00 14 00 02 00 0f 00 00 00 \
		00 00 00 00 0e 00 00 00 41 00 64 00 6d 00 69 00 \
		6e 00 69 00 73 00 74 00 72 00 61 00 74 00 65 00 \
		75 00 72 00 1c 00 1e 00 1c 00 02 00 0f 00 00 00 \
		00 00 00 00 0e 00 00 00 48 00 41 00 52 00 44 00 \
		45 00 4e 00 49 00 4e 00 47 00 33 00 2e 00 43 00 \
		4f 00 4d 00 00 00 00 00 00 00 00 00 00 00 00 00 \
		02 00 00 00 28 00 02 00 02 00 00 00 30 00 00 00 \
		08 00 00 00 1b 00 00 00 30 00 02 00 1b 00 00 00 \
		30 19 a0 03 02 01 07 a1 12 04 10 e4 aa ff 2b 93 \
		97 4c f2 5c 0b 49 85 72 92 94 54 00",
	                               &sizeofPayload4);

	if (!payload4)
		goto out;
#endif

	CreateApReqAuthenticatorReq createApReqAuthenticatorReq = { 0 };
	s = Stream_StaticInit(&staticS, payload4, sizeofPayload4);
	if (!ndr_skip_bytes(context, s, 4) || /* skip union id */
	    !ndr_read_CreateApReqAuthenticatorReq(context, s, NULL, &createApReqAuthenticatorReq) ||
	    !ndr_treat_deferred_read(context, s) || createApReqAuthenticatorReq.KeyUsage != 7 ||
	    !createApReqAuthenticatorReq.EncryptionKey || createApReqAuthenticatorReq.SubKey ||
	    !createApReqAuthenticatorReq.ClientName ||
	    createApReqAuthenticatorReq.ClientName->nameHints.count != 1 ||
	    !createApReqAuthenticatorReq.ClientRealm || !createApReqAuthenticatorReq.AuthData ||
	    createApReqAuthenticatorReq.AuthData->Asn1BufferHints.count != 2 ||
	    !createApReqAuthenticatorReq.SkewTime ||
	    createApReqAuthenticatorReq.SkewTime->QuadPart != 0)
		goto out;
	ndr_destroy_CreateApReqAuthenticatorReq(context, NULL, &createApReqAuthenticatorReq);
	ndr_context_reset(context);

	/* ============ successful end of test =============== */
	retCode = 0;
out:
	free(payload4);
	ndr_context_destroy(&context);
	return retCode;
}

int TestNdrEar(int argc, char* argv[])
{
	const int rc = TestNdrEarWrite(argc, argv);
	if (rc)
		return rc;
	return TestNdrEarRead(argc, argv);
}
