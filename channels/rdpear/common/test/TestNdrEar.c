#include <winpr/print.h>
#include <winpr/string.h>
#include <winpr/ndr.h>

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
					return nullptr;
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
				return nullptr;
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

	wStream* s = Stream_New(nullptr, 100);
	if (!s)
		return -1;

	WinPrNdrEncoder* encoder = winpr_ndr_encoder_new(FALSE, 1);
	if (!encoder)
		goto fail;

	if (!ndr_encoder_write_KERB_ASN1_DATA(encoder, s, nullptr, &asn1))
		goto fail;
	if (!winpr_ndr_encoder_treat_deferreds(encoder, s))
		goto fail;

	// winpr_HexDump("", WLOG_DEBUG, Stream_Buffer(s), Stream_GetPosition(s));

	rc = 0;
fail:
	winpr_ndr_encoder_destroy(&encoder);
	Stream_Free(s, TRUE);
	return rc;
}

static int TestNdrEarRead(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	int retCode = -2;

	/* ====================================================================== */
	WinPrNdrDecoder* decoder = winpr_ndr_decoder_new(FALSE, 1);
	if (!decoder)
		return -1;

	wStream staticS = WINPR_C_ARRAY_INIT;
	wStream* s = nullptr;

	/* ====================================================================== */
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

	CreateApReqAuthenticatorReq createApReqAuthenticatorReq = WINPR_C_ARRAY_INIT;
	s = Stream_StaticConstInit(&staticS, payload4, sizeofPayload4);
	if (!winpr_ndr_decoder_skip_bytes(decoder, s, 4) || /* skip union id */
	    !ndr_decoder_read_CreateApReqAuthenticatorReq(decoder, s, nullptr,
	                                                  &createApReqAuthenticatorReq) ||
	    !winpr_ndr_decoder_treat_deferreds(decoder, s) ||
	    createApReqAuthenticatorReq.KeyUsage != 7 || !createApReqAuthenticatorReq.EncryptionKey ||
	    createApReqAuthenticatorReq.SubKey || !createApReqAuthenticatorReq.ClientName ||
	    createApReqAuthenticatorReq.ClientName->nameHints.count != 1 ||
	    !createApReqAuthenticatorReq.ClientRealm || !createApReqAuthenticatorReq.AuthData ||
	    createApReqAuthenticatorReq.AuthData->Asn1BufferHints.count != 2 ||
	    !createApReqAuthenticatorReq.SkewTime ||
	    createApReqAuthenticatorReq.SkewTime->QuadPart != 0)
		goto out;
	ndr_destroy_CreateApReqAuthenticatorReq(nullptr, &createApReqAuthenticatorReq);

	/* ============ successful end of test =============== */
	retCode = 0;
out:
	free(payload4);
	winpr_ndr_decoder_destroy(&decoder);
	return retCode;
}

/* the referents of the Names items come in item order after the Names array */
static int TestNdrEarInternalNames(void)
{
	const BYTE payload[] = {
		0x01, 0x00, 0x02, 0x00, /* NameType, NameCount */
		0x00, 0x00, 0x02, 0x00, /* Names */
		0x02, 0x00, 0x00, 0x00, /* Names: conformance */
		0x06, 0x00, 0x06, 0x00, /* Names[0]: Length, MaximumLength */
		0x04, 0x00, 0x02, 0x00, /* Names[0]: Buffer */
		0x0c, 0x00, 0x0c, 0x00, /* Names[1]: Length, MaximumLength */
		0x08, 0x00, 0x02, 0x00, /* Names[1]: Buffer */
		0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, /* Names[0] */
		'a',  0x00, 'b',  0x00, 'c',  0x00, 0x00, 0x00,                         /* padding */
		0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, /* Names[1] */
		'd',  0x00, 'e',  0x00, 'f',  0x00, 'g',  0x00, 'h',  0x00, 'i',  0x00
	};
	const char* expected[] = { "abc", "defghi" };

	int retCode = -1;
	KERB_RPC_INTERNAL_NAME name = WINPR_C_ARRAY_INIT;
	wStream staticS = WINPR_C_ARRAY_INIT;
	wStream* s = Stream_StaticConstInit(&staticS, payload, sizeof(payload));

	WinPrNdrDecoder* decoder = winpr_ndr_decoder_new(FALSE, 1);
	if (!decoder)
		return -1;

	retCode = -2;
	if (!ndr_decoder_read_KERB_RPC_INTERNAL_NAME(decoder, s, nullptr, &name) ||
	    !winpr_ndr_decoder_treat_deferreds(decoder, s))
		goto out;

	retCode = -3;
	if ((Stream_GetRemainingLength(s) != 0) || (name.nameHints.count != ARRAYSIZE(expected)))
		goto out;

	for (UINT32 i = 0; i < name.nameHints.count; i++)
	{
		char* str =
		    ConvertWCharNToUtf8Alloc(name.Names[i].Buffer, name.Names[i].strLength, nullptr);
		const BOOL match = str && (strcmp(str, expected[i]) == 0);
		free(str);
		if (!match)
			goto out;
	}

	retCode = 0;
out:
	ndr_destroy_KERB_RPC_INTERNAL_NAME(nullptr, &name);
	winpr_ndr_decoder_destroy(&decoder);
	return retCode;
}

int TestNdrEar(int argc, char* argv[])
{
	const int rc = TestNdrEarWrite(argc, argv);
	if (rc)
		return rc;

	const int rc2 = TestNdrEarInternalNames();
	if (rc2)
		return rc2;

	return TestNdrEarRead(argc, argv);
}
