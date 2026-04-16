/**
 * WinPR: Windows Portable Runtime
 * libFuzzer harness for the WinPrAsn1 decoder
 */

#include <stddef.h>
#include <stdint.h>

#include <winpr/asn1.h>
#include <winpr/crt.h>
#include <winpr/wlog.h>

static void fuzz_walk_sequence(WinPrAsn1Decoder* outer, int depth);

static void fuzz_consume_one(WinPrAsn1Decoder* decoder, int depth)
{
	WinPrAsn1Decoder inner = WinPrAsn1Decoder_init();
	WinPrAsn1_tag tag = 0;
	size_t len = 0;

	if (depth > 6)
		return;

	if (WinPrAsn1DecReadTagLenValue(decoder, &tag, &len, &inner) == 0)
		return;

	switch (tag & ER_TAG_MASK)
	{
		case ER_TAG_BOOLEAN:
		{
			WinPrAsn1_BOOL value = 0;
			(void)WinPrAsn1DecReadBoolean(&inner, &value);
			break;
		}
		case ER_TAG_INTEGER:
		{
			WinPrAsn1_INTEGER value = 0;
			(void)WinPrAsn1DecReadInteger(&inner, &value);
			break;
		}
		case ER_TAG_OCTET_STRING:
		{
			WinPrAsn1_OctetString value = { 0 };
			if (WinPrAsn1DecReadOctetString(&inner, &value, TRUE))
				WinPrAsn1FreeOctetString(&value);
			break;
		}
		case ER_TAG_OBJECT_IDENTIFIER:
		{
			WinPrAsn1_OID value = { 0 };
			if (WinPrAsn1DecReadOID(&inner, &value, TRUE))
				WinPrAsn1FreeOID(&value);
			break;
		}
		case ER_TAG_ENUMERATED:
		{
			WinPrAsn1_ENUMERATED value = 0;
			(void)WinPrAsn1DecReadEnumerated(&inner, &value);
			break;
		}
		case ER_TAG_UTCTIME:
		{
			WinPrAsn1_UTCTIME value = { 0 };
			(void)WinPrAsn1DecReadUtcTime(&inner, &value);
			break;
		}
		case ER_TAG_IA5STRING:
		{
			WinPrAsn1_IA5STRING value = NULL;
			(void)WinPrAsn1DecReadIA5String(&inner, &value);
			free(value);
			break;
		}
		case ER_TAG_NULL:
			(void)WinPrAsn1DecReadNull(&inner);
			break;
		default:
			break;
	}

	if ((tag == ER_TAG_SEQUENCE) || (tag == ER_TAG_SET))
		fuzz_walk_sequence(&inner, depth + 1);
	else if ((tag & 0xC0) == 0x80)
		fuzz_walk_sequence(&inner, depth + 1);
}

static void fuzz_walk_sequence(WinPrAsn1Decoder* outer, int depth)
{
	if (depth > 6)
		return;

	for (size_t index = 0; index < 64; index++)
	{
		WinPrAsn1_tag tag = 0;

		if (!WinPrAsn1DecPeekTag(outer, &tag))
			return;

		fuzz_consume_one(outer, depth);
	}
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	static BOOL loggingInitialized = FALSE;

	if (!loggingInitialized)
	{
		(void)WLog_SetLogLevel(WLog_GetRoot(), WLOG_OFF);
		loggingInitialized = TRUE;
	}

	if ((size == 0) || (size > (1u << 20)))
		return 0;

	{
		WinPrAsn1Decoder decoder = WinPrAsn1Decoder_init();
		WinPrAsn1Decoder_InitMem(&decoder, WINPR_ASN1_DER, data, size);
		fuzz_walk_sequence(&decoder, 0);
	}

	{
		WinPrAsn1Decoder decoder = WinPrAsn1Decoder_init();
		WinPrAsn1Decoder_InitMem(&decoder, WINPR_ASN1_BER, data, size);
		fuzz_walk_sequence(&decoder, 0);
	}

	{
		WinPrAsn1Decoder decoder = WinPrAsn1Decoder_init();
		WinPrAsn1Decoder sequence = WinPrAsn1Decoder_init();

		WinPrAsn1Decoder_InitMem(&decoder, WINPR_ASN1_DER, data, size);
		if (WinPrAsn1DecReadSequence(&decoder, &sequence))
			fuzz_walk_sequence(&sequence, 1);
	}

	return 0;
}
