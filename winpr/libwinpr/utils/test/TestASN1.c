#include <winpr/asn1.h>
#include <winpr/print.h>

static const BYTE boolContent[] = { 0x01, 0x01, 0xFF };
static const BYTE badBoolContent[] = { 0x01, 0x04, 0xFF };

static const BYTE integerContent[] = { 0x02, 0x01, 0x02 };
static const BYTE badIntegerContent[] = { 0x02, 0x04, 0x02 };

static const BYTE seqContent[] = { 0x30, 0x22, 0x06, 0x03, 0x55, 0x04, 0x0A, 0x13, 0x1B, 0x44,
	                               0x69, 0x67, 0x69, 0x74, 0x61, 0x6C, 0x20, 0x53, 0x69, 0x67,
	                               0x6E, 0x61, 0x74, 0x75, 0x72, 0x65, 0x20, 0x54, 0x72, 0x75,
	                               0x73, 0x74, 0x20, 0x43, 0x6F, 0x2E, 0x31 };

static const BYTE contextualInteger[] = { 0xA0, 0x03, 0x02, 0x01, 0x02 };

static const BYTE oidContent[] = { 0x06, 0x03, 0x55, 0x04, 0x0A };
static const BYTE badOidContent[] = { 0x06, 0x89, 0x55, 0x04, 0x0A };
static const BYTE oidValue[] = { 0x55, 0x04, 0x0A };

static const BYTE ia5stringContent[] = { 0x16, 0x22, 0x68, 0x74, 0x74, 0x70, 0x3A, 0x2F, 0x2F,
	                                     0x63, 0x70, 0x73, 0x2E, 0x72, 0x6F, 0x6F, 0x74, 0x2D,
	                                     0x78, 0x31, 0x2E, 0x6C, 0x65, 0x74, 0x73, 0x65, 0x6E,
	                                     0x63, 0x72, 0x79, 0x70, 0x74, 0x2E, 0x6F, 0x72, 0x67 };

static const BYTE utctimeContent[] = { 0x17, 0x0D, 0x32, 0x31, 0x30, 0x33, 0x31, 0x37,
	                                   0x31, 0x36, 0x34, 0x30, 0x34, 0x36, 0x5A };

int TestASN1Read(int argc, char* argv[])
{
	WinPrAsn1Decoder decoder, seqDecoder;
	wStream staticS;
	WinPrAsn1_BOOL boolV;
	WinPrAsn1_INTEGER integerV;
	WinPrAsn1_OID oidV;
	WinPrAsn1_IA5STRING ia5stringV;
	WinPrAsn1_UTCTIME utctimeV;
	WinPrAsn1_tag tag;
	size_t len;
	BOOL error;

	/* ============== Test INTEGERs ================ */
	Stream_StaticConstInit(&staticS, integerContent, sizeof(integerContent));
	WinPrAsn1Decoder_Init(&decoder, WINPR_ASN1_DER, &staticS);
	if (!WinPrAsn1DecReadInteger(&decoder, &integerV))
		return -1;

	Stream_StaticConstInit(&staticS, badIntegerContent, sizeof(badIntegerContent));
	WinPrAsn1Decoder_Init(&decoder, WINPR_ASN1_DER, &staticS);
	if (WinPrAsn1DecReadInteger(&decoder, &integerV))
		return -1;

	/* ================ Test BOOL ================*/
	Stream_StaticConstInit(&staticS, boolContent, sizeof(boolContent));
	WinPrAsn1Decoder_Init(&decoder, WINPR_ASN1_DER, &staticS);
	if (!WinPrAsn1DecReadBoolean(&decoder, &boolV))
		return -10;

	Stream_StaticConstInit(&staticS, badBoolContent, sizeof(badBoolContent));
	WinPrAsn1Decoder_Init(&decoder, WINPR_ASN1_DER, &staticS);
	if (WinPrAsn1DecReadBoolean(&decoder, &boolV))
		return -11;

	/* ================ Test OID ================*/
	Stream_StaticConstInit(&staticS, oidContent, sizeof(oidContent));
	WinPrAsn1Decoder_Init(&decoder, WINPR_ASN1_DER, &staticS);
	if (!WinPrAsn1DecReadOID(&decoder, &oidV, TRUE) || oidV.len != 3 ||
	    memcmp(oidV.data, oidValue, oidV.len))
		return -15;
	WinPrAsn1FreeOID(&oidV);

	Stream_StaticConstInit(&staticS, badOidContent, sizeof(badOidContent));
	WinPrAsn1Decoder_Init(&decoder, WINPR_ASN1_DER, &staticS);
	if (WinPrAsn1DecReadOID(&decoder, &oidV, TRUE))
		return -15;
	WinPrAsn1FreeOID(&oidV);

	Stream_StaticConstInit(&staticS, ia5stringContent, sizeof(ia5stringContent));
	WinPrAsn1Decoder_Init(&decoder, WINPR_ASN1_DER, &staticS);
	if (!WinPrAsn1DecReadIA5String(&decoder, &ia5stringV) ||
	    strcmp(ia5stringV, "http://cps.root-x1.letsencrypt.org"))
		return -16;
	free(ia5stringV);

	/* ================ Test utc time ================*/
	Stream_StaticConstInit(&staticS, utctimeContent, sizeof(utctimeContent));
	WinPrAsn1Decoder_Init(&decoder, WINPR_ASN1_DER, &staticS);
	if (!WinPrAsn1DecReadUtcTime(&decoder, &utctimeV) || utctimeV.year != 2021 ||
	    utctimeV.month != 3 || utctimeV.day != 17 || utctimeV.minute != 40 || utctimeV.tz != 'Z')
		return -17;

	/* ================ Test sequence ================*/
	Stream_StaticConstInit(&staticS, seqContent, sizeof(seqContent));
	WinPrAsn1Decoder_Init(&decoder, WINPR_ASN1_DER, &staticS);
	if (!WinPrAsn1DecReadSequence(&decoder, &seqDecoder))
		return -20;

	Stream_StaticConstInit(&staticS, seqContent, sizeof(seqContent));
	WinPrAsn1Decoder_Init(&decoder, WINPR_ASN1_DER, &staticS);
	if (!WinPrAsn1DecReadTagLenValue(&decoder, &tag, &len, &seqDecoder))
		return -21;

	if (tag != ER_TAG_SEQUENCE)
		return -22;

	if (!WinPrAsn1DecPeekTag(&seqDecoder, &tag) || tag != ER_TAG_OBJECT_IDENTIFIER)
		return -23;

	/* ================ Test contextual ================*/
	Stream_StaticConstInit(&staticS, contextualInteger, sizeof(contextualInteger));
	WinPrAsn1Decoder_Init(&decoder, WINPR_ASN1_DER, &staticS);
	error = TRUE;
	if (!WinPrAsn1DecReadContextualInteger(&decoder, 0, &error, &integerV) || error)
		return -25;

	/* test reading a contextual integer that is not there (index 1).
	 * that should not touch the decoder read head and we shall be able to extract contextual tag 0
	 * after that
	 */
	WinPrAsn1Decoder_Init(&decoder, WINPR_ASN1_DER, &staticS);
	error = FALSE;
	if (WinPrAsn1DecReadContextualInteger(&decoder, 1, &error, &integerV) || error)
		return -26;

	error = FALSE;
	if (!WinPrAsn1DecReadContextualInteger(&decoder, 0, &error, &integerV) || error)
		return -27;

	return 0;
}

static const BYTE oid1_val[] = { 1 };
static const WinPrAsn1_OID oid1 = { sizeof(oid1_val), (const BYTE*)oid1_val };
static BYTE oid2_val[] = { 2, 2 };
static WinPrAsn1_OID oid2 = { sizeof(oid2_val), oid2_val };
static BYTE oid3_val[] = { 3, 3, 3 };
static WinPrAsn1_OID oid3 = { sizeof(oid3_val), oid3_val };
static BYTE oid4_val[] = { 4, 4, 4, 4 };
static WinPrAsn1_OID oid4 = { sizeof(oid4_val), oid4_val };

int TestASN1Write(int argc, char* argv[])
{
	size_t i;
	wStream* s = NULL;
	size_t expectedOuputSz;
	int retCode = 100;
	WinPrAsn1_UTCTIME utcTime;
	WinPrAsn1_IA5STRING ia5string;
	WinPrAsn1Encoder* enc = WinPrAsn1Encoder_New(WINPR_ASN1_DER);
	if (!enc)
		goto out;

	/* Let's encode something like:
	 * APP(3)
	 *   SEQ2
	 *         OID1
	 *         OID2
	 *   SEQ3
	 *         OID3
	 *         OID4
	 *
	 *   [5] integer(200)
	 *   [6] SEQ (empty)
	 *   [7] UTC time (2016-03-17 16:40:41 UTC)
	 *   [8] IA5String(test)
	 *   [9] OctetString
	 *       SEQ(empty)
	 *
	 */

	/* APP(3) */
	retCode = 101;
	if (!WinPrAsn1EncAppContainer(enc, 3))
		goto out;

	/* SEQ2 */
	retCode = 102;
	if (!WinPrAsn1EncSeqContainer(enc))
		goto out;

	retCode = 103;
	if (WinPrAsn1EncOID(enc, &oid1) != 3)
		goto out;

	retCode = 104;
	if (WinPrAsn1EncOID(enc, &oid2) != 4)
		goto out;

	retCode = 105;
	if (WinPrAsn1EncEndContainer(enc) != 9)
		goto out;

	/* SEQ3 */
	retCode = 110;
	if (!WinPrAsn1EncSeqContainer(enc))
		goto out;

	retCode = 111;
	if (WinPrAsn1EncOID(enc, &oid3) != 5)
		goto out;

	retCode = 112;
	if (WinPrAsn1EncOID(enc, &oid4) != 6)
		goto out;

	retCode = 113;
	if (WinPrAsn1EncEndContainer(enc) != 13)
		goto out;

	/* [5] integer(200) */
	retCode = 114;
	if (WinPrAsn1EncContextualInteger(enc, 5, 200) != 6)
		goto out;

	/* [6] SEQ (empty) */
	retCode = 115;
	if (!WinPrAsn1EncContextualSeqContainer(enc, 6))
		goto out;

	retCode = 116;
	if (WinPrAsn1EncEndContainer(enc) != 4)
		goto out;

	/* [7] UTC time (2016-03-17 16:40:41 UTC) */
	retCode = 117;
	utcTime.year = 2016;
	utcTime.month = 3;
	utcTime.day = 17;
	utcTime.hour = 16;
	utcTime.minute = 40;
	utcTime.second = 41;
	utcTime.tz = 'Z';
	if (WinPrAsn1EncContextualUtcTime(enc, 7, &utcTime) != 17)
		goto out;

	/* [8] IA5String(test) */
	retCode = 118;
	ia5string = "test";
	if (!WinPrAsn1EncContextualContainer(enc, 8))
		goto out;

	retCode = 119;
	if (WinPrAsn1EncIA5String(enc, ia5string) != 6)
		goto out;

	retCode = 120;
	if (WinPrAsn1EncEndContainer(enc) != 8)
		goto out;

	/*   [9] OctetString
	 *       SEQ(empty)
	 */
	retCode = 121;
	if (!WinPrAsn1EncContextualOctetStringContainer(enc, 9))
		goto out;

	retCode = 122;
	if (!WinPrAsn1EncSeqContainer(enc))
		goto out;

	retCode = 123;
	if (WinPrAsn1EncEndContainer(enc) != 2)
		goto out;

	retCode = 124;
	if (WinPrAsn1EncEndContainer(enc) != 6)
		goto out;

	/* close APP */
	expectedOuputSz = 24 + 6 + 4 + 17 + 8 + 6;
	retCode = 200;
	if (WinPrAsn1EncEndContainer(enc) != expectedOuputSz)
		goto out;

	/* let's output the result */
	retCode = 201;
	s = Stream_New(NULL, 1024);
	if (!s)
		goto out;

	retCode = 202;
	if (!WinPrAsn1EncToStream(enc, s) || Stream_GetPosition(s) != expectedOuputSz)
		goto out;
	/* winpr_HexDump("", WLOG_ERROR, Stream_Buffer(s), Stream_GetPosition(s));*/

	/*
	 * let's perform a mini-performance test, where we encode an ASN1 message with a big depth,
	 * so that we trigger reallocation routines in the encoder. We're gonna encode something like
	 *   SEQ1
	 *      SEQ2
	 *         SEQ3
	 *         ...
	 *            SEQ1000
	 *              INTEGER(2)
	 *
	 *  As static chunks and containers are 50, a depth of 1000 should be enough
	 *
	 */
	WinPrAsn1Encoder_Reset(enc);

	retCode = 203;
	for (i = 0; i < 1000; i++)
	{
		if (!WinPrAsn1EncSeqContainer(enc))
			goto out;
	}

	retCode = 204;
	if (WinPrAsn1EncInteger(enc, 2) != 3)
		goto out;

	retCode = 205;
	for (i = 0; i < 1000; i++)
	{
		if (!WinPrAsn1EncEndContainer(enc))
			goto out;
	}

	retCode = 0;

out:
	if (s)
		Stream_Free(s, TRUE);
	WinPrAsn1Encoder_Free(&enc);
	return retCode;
}

int TestASN1(int argc, char* argv[])
{
	int ret = TestASN1Read(argc, argv);
	if (ret)
		return ret;

	return TestASN1Write(argc, argv);
}
