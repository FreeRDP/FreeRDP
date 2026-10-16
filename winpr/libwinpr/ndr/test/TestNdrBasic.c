#include <stddef.h>
#include <winpr/ndr.h>

static int TestNdrBasic_1(void)
{
	int retCode = -2;
	WinPrNdrDecoder* decoder = winpr_ndr_decoder_new(FALSE, 1);
	if (!decoder)
		return -1;

	BYTE payload[] = {
		// == conformant array ==
		0x02, 0x00, 0x00, 0x00, // (nitems)
		0x30, 0x00,             // content
		0x00, 0x00              // (padding)
	};
	wStream staticS;
	wStream* s = Stream_StaticInit(&staticS, payload, sizeof(payload));

	BYTE* target = nullptr;
	WinPrNdrArrayHints hints = { 2 };
	WinPrNdrDeferredEntry e = { 0x020028,       "arrayContent", &hints,
		                        (void*)&target, TRUE,           winpr_ndr_uint8Array_descr() };

	if (!winpr_ndr_decoder_push_deferreds(decoder, &e, 1))
		goto out;

	if (!winpr_ndr_decoder_treat_deferreds(decoder, s))
		goto out;

	/* the decoded pointers are released, the type destroys the referent with the last reference */
	winpr_ndr_pointer_release(target);
	retCode = 0;
out:
	winpr_ndr_decoder_destroy(&decoder);
	return retCode;
}

#define NDR_TYPEDEF(V)                                                                            \
	static BOOL ndr_descr_read_##V(WinPrNdrDecoder* context, wStream* s, const void* hints,       \
	                               void* obj)                                                     \
	{                                                                                             \
		WINPR_UNUSED(hints);                                                                      \
		return winpr_ndr_decoder_read_fromDescr(context, s, &V##_struct, obj);                    \
	}                                                                                             \
	static BOOL ndr_descr_write_##V(WinPrNdrEncoder* context, wStream* s, const void* hints,      \
	                                const void* obj)                                              \
	{                                                                                             \
		WINPR_UNUSED(hints);                                                                      \
		return winpr_ndr_encoder_write_fromDescr(context, s, &V##_struct, obj);                   \
	}                                                                                             \
	static void ndr_descr_destroy_##V(const void* hints, void* obj)                               \
	{                                                                                             \
		WINPR_UNUSED(hints);                                                                      \
		winpr_ndr_struct_destroy(&V##_struct, obj);                                               \
	}                                                                                             \
	static void ndr_descr_dump_##V(wLog* logger, UINT32 lvl, size_t indentLevel, const void* obj) \
	{                                                                                             \
		winpr_ndr_struct_dump_fromDescr(logger, lvl, indentLevel, &V##_struct, obj);              \
	}                                                                                             \
                                                                                                  \
	static WinPrNdrMessageDescr ndr_##V##_descr_s = {                                             \
		WINPR_NDR_ARITY_SIMPLE, sizeof(V),          ndr_descr_read_##V, ndr_descr_write_##V,      \
		ndr_descr_destroy_##V,  ndr_descr_dump_##V,                                               \
	};                                                                                            \
                                                                                                  \
	WinPrNdrMessageType ndr_##V##_descr(void)                                                     \
	{                                                                                             \
		return &ndr_##V##_descr_s;                                                                \
	}

typedef struct
{
	/* [range] */ DWORD cbContext;
	/* [size_is][unique] */ BYTE pbContext[16];
} REDIR_SCARDCONTEXT;

static const WinPrNdrFieldStruct REDIR_SCARDCONTEXT_fields[] = {
	{ "cbContext", offsetof(REDIR_SCARDCONTEXT, cbContext), WINPR_NDR_NOT_POINTER, -1,
	  &winpr_ndr_uint32_descr_s },
	{ "pbContext", offsetof(REDIR_SCARDCONTEXT, pbContext), WINPR_NDR_FIXED_ARRAY, 0,
	  &winpr_ndr_uint8Array_descr_s, sizeof(((REDIR_SCARDCONTEXT*)nullptr)->pbContext) },
};

static const WinPrNdrStructDescr REDIR_SCARDCONTEXT_struct = { "REDIR_SCARDCONTEXT",
	                                                           ARRAYSIZE(REDIR_SCARDCONTEXT_fields),
	                                                           REDIR_SCARDCONTEXT_fields };

NDR_TYPEDEF(REDIR_SCARDCONTEXT)

/* ========================================================================== */
typedef struct
{
	REDIR_SCARDCONTEXT hContext;
	LONG ReturnCode;
} EstablishContext_Return;

static const WinPrNdrFieldStruct EstablishContext_Return_fields[] = {
	{ "ReturnCode", offsetof(EstablishContext_Return, ReturnCode), WINPR_NDR_NOT_POINTER, -1,
	  &winpr_ndr_uint32_descr_s },
	{ "hContext", offsetof(EstablishContext_Return, hContext), WINPR_NDR_NOT_POINTER, -1,
	  &ndr_REDIR_SCARDCONTEXT_descr_s },
};

static const WinPrNdrStructDescr EstablishContext_Return_struct = {
	"EstablishContext_Return", ARRAYSIZE(EstablishContext_Return_fields),
	EstablishContext_Return_fields
};

NDR_TYPEDEF(EstablishContext_Return)

/* ========================================================================== */

static int TestNdrStaticArrayRead(void)
{
	int retCode = -2;

	unsigned char payload[] = { 0x01, 0x10, 0x08, 0x00, 0xcc, 0xcc, 0xcc, 0xcc,
		                        0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

		                        0x0,  0x0,  0x0,  0x0,  0x8,  0x0,  0x0,  0x0,
		                        0x0,  0x0,  0x2,  0x0,  0x8,  0x0,  0x0,  0x0,
		                        0x49, 0x91, 0xe4, 0x7c, 0x0,  0x0,  0x0,  0x0 };
	wStream staticS;
	wStream* s = Stream_StaticConstInit(&staticS, payload, sizeof(payload));

	WinPrNdrDecoder* decoder = winpr_ndr_decoder_new_fromStream(s);
	if (!decoder)
		return -1;

	EstablishContext_Return context = WINPR_C_ARRAY_INIT;
	wStream cpayload;
	if (!winpr_ndr_decoder_read_object(decoder, s, &cpayload) ||
	    !ndr_EstablishContext_Return_descr_s.readFn(decoder, &cpayload, nullptr, &context) ||
	    !winpr_ndr_decoder_treat_deferreds(decoder, &cpayload))
		goto out;
	retCode = -3;

	unsigned char expected[] = { 0x49, 0x91, 0xe4, 0x7c, 0x0, 0x0, 0x0, 0x0 };
	if (context.hContext.cbContext != 8 || memcmp(context.hContext.pbContext, expected, 8) != 0)
		goto out;

	retCode = 0;
out:
	winpr_ndr_decoder_destroy(&decoder);
	return retCode;
}

static int TestNdrStaticArrayWrite(void)
{
	int retCode = -2;
	WinPrNdrEncoder* encoder = winpr_ndr_encoder_new(FALSE, 1);
	if (!encoder)
		return -1;

	BYTE buffer[1000];
	wStream staticS;
	wStream* s = Stream_StaticInit(&staticS, buffer, sizeof(buffer));

	REDIR_SCARDCONTEXT context = WINPR_C_ARRAY_INIT;
	context.cbContext = 10;
	for (int i = 0; i < 10; i++)
		context.pbContext[i] = i + 1;
	if (!winpr_ndr_encoder_write_fromDescr(encoder, s, &REDIR_SCARDCONTEXT_struct, &context))
		goto out;

	retCode = winpr_ndr_encoder_treat_deferreds(encoder, s) ? 0 : -3;
	Stream_SealLength(s);

	unsigned char expected[] = {
		0xa, 0x0, 0x0, 0x0, 0x0, 0x0, 0x2, 0x0, 0xa, 0x0, 0x0,
		0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa,
	};

	if (Stream_Length(s) != sizeof(expected))
		goto out;

	retCode = memcmp(Stream_Buffer(s), expected, sizeof(expected));
out:
	winpr_ndr_encoder_destroy(&encoder);
	return retCode;
}

static int TestNdrStaticArrayOverflow(void)
{
	/* cbContext announces 32 bytes, more than the 16 bytes of pbContext */
	unsigned char payload[] = { 0x01, 0x10, 0x08, 0x00, 0xcc, 0xcc, 0xcc, 0xcc, 0x30, 0x00,
		                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

		                        0x0,  0x0,  0x0,  0x0,  0x20, 0x0,  0x0,  0x0,  0x0,  0x0,
		                        0x2,  0x0,  0x20, 0x0,  0x0,  0x0,  0x41, 0x41, 0x41, 0x41,
		                        0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
		                        0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
		                        0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41 };
	wStream staticS;
	wStream* s = Stream_StaticConstInit(&staticS, payload, sizeof(payload));

	WinPrNdrDecoder* decoder = winpr_ndr_decoder_new_fromStream(s);
	if (!decoder)
		return -1;

	int retCode = -4;
	EstablishContext_Return context = WINPR_C_ARRAY_INIT;
	wStream cpayload;
	if (winpr_ndr_decoder_read_object(decoder, s, &cpayload) &&
	    ndr_EstablishContext_Return_descr_s.readFn(decoder, &cpayload, nullptr, &context) &&
	    winpr_ndr_decoder_treat_deferreds(decoder, &cpayload))
		goto out;

	retCode = 0;
out:
	winpr_ndr_decoder_destroy(&decoder);
	return retCode;
}

/* ========================================================================== */
typedef struct
{
	UINT32 count;
	BYTE* data;
} TEST_BLOB;

static const WinPrNdrFieldStruct TEST_BLOB_fields[] = {
	{ "count", offsetof(TEST_BLOB, count), WINPR_NDR_NOT_POINTER, -1, &winpr_ndr_uint32_descr_s },
	{ "data", offsetof(TEST_BLOB, data), WINPR_NDR_POINTER_NON_NULL, 0,
	  &winpr_ndr_uint8Array_descr_s },
};

static const WinPrNdrStructDescr TEST_BLOB_struct = { "TEST_BLOB", ARRAYSIZE(TEST_BLOB_fields),
	                                                  TEST_BLOB_fields };

NDR_TYPEDEF(TEST_BLOB)

static BOOL ndr_read_TEST_BLOB_array(WinPrNdrDecoder* context, wStream* s, const void* hints,
                                     void* v)
{
	return winpr_ndr_decoder_read_uconformant_array(context, s, hints, &ndr_TEST_BLOB_descr_s, v);
}

static BOOL ndr_write_TEST_BLOB_array(WinPrNdrEncoder* context, wStream* s, const void* hints,
                                      const void* v)
{
	const WinPrNdrArrayHints* ahints = hints;
	return winpr_ndr_encoder_write_uconformant_array(context, s, ahints->count,
	                                                 &ndr_TEST_BLOB_descr_s, v);
}

static void ndr_destroy_TEST_BLOB_array(const void* hints, void* obj)
{
	const WinPrNdrArrayHints* ahints = hints;
	TEST_BLOB* blobs = obj;
	for (UINT32 i = 0; i < ahints->count; i++)
		winpr_ndr_struct_destroy(&TEST_BLOB_struct, &blobs[i]);
}

static const WinPrNdrMessageDescr ndr_TEST_BLOB_array_descr_s = {
	WINPR_NDR_ARITY_ARRAYOF,   sizeof(TEST_BLOB),           ndr_read_TEST_BLOB_array,
	ndr_write_TEST_BLOB_array, ndr_destroy_TEST_BLOB_array, nullptr
};

typedef struct
{
	WinPrNdrArrayHints hints;
	TEST_BLOB* blobs;
} TEST_BLOBS;

static const WinPrNdrFieldStruct TEST_BLOBS_fields[] = {
	{ "count", offsetof(TEST_BLOBS, hints.count), WINPR_NDR_NOT_POINTER, -1,
	  &winpr_ndr_uint32_descr_s },
	{ "blobs", offsetof(TEST_BLOBS, blobs), WINPR_NDR_POINTER, 0, &ndr_TEST_BLOB_array_descr_s },
};

static const WinPrNdrStructDescr TEST_BLOBS_struct = { "TEST_BLOBS", ARRAYSIZE(TEST_BLOBS_fields),
	                                                   TEST_BLOBS_fields };

/* the referents of the pointers embedded in array items come after the array, in item order */
static const BYTE blobsPayload[] = {
	0x02, 0x00, 0x00, 0x00, /* count */
	0x00, 0x00, 0x02, 0x00, /* blobs */
	0x02, 0x00, 0x00, 0x00, /* blobs: conformance */
	0x01, 0x00, 0x00, 0x00, /* blobs[0].count */
	0x04, 0x00, 0x02, 0x00, /* blobs[0].data */
	0x02, 0x00, 0x00, 0x00, /* blobs[1].count */
	0x08, 0x00, 0x02, 0x00, /* blobs[1].data */
	0x01, 0x00, 0x00, 0x00, /* blobs[0].data: conformance */
	'A',  0x00, 0x00, 0x00, /* blobs[0].data, padding */
	0x02, 0x00, 0x00, 0x00, /* blobs[1].data: conformance */
	'B',  'C'               /* blobs[1].data */
};

static int TestNdrDeferredOrderWrite(void)
{
	int retCode = -2;
	BYTE data0[] = { 'A' };
	BYTE data1[] = { 'B', 'C' };
	TEST_BLOB items[] = { { 1, data0 }, { 2, data1 } };
	TEST_BLOBS blobs = { { 2 }, items };

	WinPrNdrEncoder* encoder = winpr_ndr_encoder_new(FALSE, 1);
	wStream* s = Stream_New(nullptr, 100);
	if (!encoder || !s)
		goto out;

	retCode = -3;
	if (!winpr_ndr_encoder_write_fromDescr(encoder, s, &TEST_BLOBS_struct, &blobs) ||
	    !winpr_ndr_encoder_treat_deferreds(encoder, s))
		goto out;

	retCode = -4;
	if ((Stream_GetPosition(s) != sizeof(blobsPayload)) ||
	    (memcmp(Stream_Buffer(s), blobsPayload, sizeof(blobsPayload)) != 0))
		goto out;

	retCode = 0;
out:
	Stream_Free(s, TRUE);
	winpr_ndr_encoder_destroy(&encoder);
	return retCode;
}

static int TestNdrDeferredOrderRead(void)
{
	int retCode = -2;
	TEST_BLOBS blobs = WINPR_C_ARRAY_INIT;
	wStream staticS;
	wStream* s = Stream_StaticConstInit(&staticS, blobsPayload, sizeof(blobsPayload));

	WinPrNdrDecoder* decoder = winpr_ndr_decoder_new(FALSE, 1);
	if (!decoder)
		return -1;

	if (!winpr_ndr_decoder_read_fromDescr(decoder, s, &TEST_BLOBS_struct, &blobs) ||
	    !winpr_ndr_decoder_treat_deferreds(decoder, s))
		goto out;

	retCode = -3;
	if ((Stream_GetRemainingLength(s) != 0) || (blobs.hints.count != 2) || !blobs.blobs)
		goto out;

	retCode = -4;
	if ((blobs.blobs[0].count != 1) || (blobs.blobs[0].data[0] != 'A') ||
	    (blobs.blobs[1].count != 2) || (memcmp(blobs.blobs[1].data, "BC", 2) != 0))
		goto out;

	retCode = 0;
out:
	winpr_ndr_struct_destroy(&TEST_BLOBS_struct, &blobs);
	winpr_ndr_decoder_destroy(&decoder);
	return retCode;
}

static int TestNdrObject(void)
{
	int retCode = -2;
	WinPrNdrEncoder* encoder = winpr_ndr_encoder_new(FALSE, 1);
	wStream* s = Stream_New(nullptr, 100);
	if (!encoder || !s)
		goto out;

	retCode = -3;
	if (!winpr_ndr_encoder_write_header(encoder, s) ||
	    !winpr_ndr_encoder_start_object(encoder, s) ||
	    !winpr_ndr_encoder_write_toplevel_refid(encoder, s) ||
	    !winpr_ndr_encoder_write_uint16(encoder, s, 0x1234) ||
	    !winpr_ndr_encoder_end_object(encoder, s))
		goto out;

	/* the object data is padded to a multiple of 8 */
	const BYTE expected[] = { 0x01, 0x10, 0x08, 0x00, 0xcc, 0xcc, 0xcc, 0xcc,
		                      0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		                      0x00, 0x00, 0x02, 0x00, 0x34, 0x12, 0x00, 0x00 };
	retCode = -4;
	if ((Stream_GetPosition(s) != sizeof(expected)) ||
	    (memcmp(Stream_Buffer(s), expected, sizeof(expected)) != 0))
		goto out;

	retCode = -5;
	Stream_SealLength(s);
	Stream_SetPosition(s, 0);

	WinPrNdrDecoder* decoder = winpr_ndr_decoder_new_fromStream(s);
	wStream object = WINPR_C_ARRAY_INIT;
	winpr_ndr_refid refId = 0;
	UINT16 v = 0;
	const BOOL ok = decoder && winpr_ndr_decoder_read_object(decoder, s, &object) &&
	                (Stream_GetRemainingLength(s) == 0) &&
	                winpr_ndr_decoder_read_refpointer(decoder, &object, &refId) &&
	                (refId == 0x20000) && winpr_ndr_decoder_read_uint16(decoder, &object, &v) &&
	                (v == 0x1234);
	winpr_ndr_decoder_destroy(&decoder);
	if (ok)
		retCode = 0;
out:
	Stream_Free(s, TRUE);
	winpr_ndr_encoder_destroy(&encoder);
	return retCode;
}

/* the signed types in both byte orders: aligned on their size, sign kept */
static int TestNdrSignedTypes(void)
{
	for (int bigEndian = 0; bigEndian < 2; bigEndian++)
	{
		int retCode = -2;
		WinPrNdrEncoder* encoder = winpr_ndr_encoder_new(bigEndian != 0, 1);
		WinPrNdrDecoder* decoder = nullptr;
		wStream* s = Stream_New(nullptr, 64);
		if (!encoder || !s)
			goto out;

		retCode = -3;
		const INT8 i8 = -2;
		const INT16 i16 = -0x1234;
		const INT64 i64 = -0x123456789abcdefLL;
		if (!winpr_ndr_encoder_write_int8(encoder, s, i8) ||
		    !winpr_ndr_encoder_write_int16(encoder, s, i16) ||
		    !winpr_ndr_encoder_write_int8_(encoder, s, nullptr, &i8) ||
		    !winpr_ndr_encoder_write_int64(encoder, s, i64))
			goto out;

		const BYTE expectedLE[] = { 0xfe, 0x00, 0xcc, 0xed, 0xfe, 0x00, 0x00, 0x00,
			                        0x11, 0x32, 0x54, 0x76, 0x98, 0xba, 0xdc, 0xfe };
		const BYTE expectedBE[] = { 0xfe, 0x00, 0xed, 0xcc, 0xfe, 0x00, 0x00, 0x00,
			                        0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x11 };
		const BYTE* expected = bigEndian ? expectedBE : expectedLE;
		retCode = -4;
		if ((Stream_GetPosition(s) != sizeof(expectedLE)) ||
		    (memcmp(Stream_Buffer(s), expected, sizeof(expectedLE)) != 0))
			goto out;

		retCode = -5;
		Stream_SealLength(s);
		Stream_SetPosition(s, 0);
		decoder = winpr_ndr_decoder_new(bigEndian != 0, 1);
		INT8 r8 = 0;
		INT8 r8bis = 0;
		INT16 r16 = 0;
		INT64 r64 = 0;
		if (!decoder || !winpr_ndr_decoder_read_int8(decoder, s, &r8) ||
		    !winpr_ndr_int16_descr()->readFn(decoder, s, nullptr, &r16) ||
		    !winpr_ndr_decoder_read_int8_(decoder, s, nullptr, &r8bis) ||
		    !winpr_ndr_decoder_read_int64(decoder, s, &r64))
			goto out;

		retCode = -6;
		if ((r8 != i8) || (r16 != i16) || (r8bis != i8) || (r64 != i64) ||
		    (Stream_GetRemainingLength(s) != 0) || (winpr_ndr_int8_descr()->itemSize != 1) ||
		    (winpr_ndr_int16_descr()->itemSize != 2) || (winpr_ndr_int64_descr()->itemSize != 8))
			goto out;

		retCode = 0;
	out:
		winpr_ndr_decoder_destroy(&decoder);
		winpr_ndr_encoder_destroy(&encoder);
		Stream_Free(s, TRUE);
		if (retCode)
			return retCode - (bigEndian * 10);
	}
	return 0;
}

/* ================================== full pointers ======================================== */

static BOOL decode_payload(const WinPrNdrStructDescr* descr, const BYTE* payload, size_t len,
                           void* obj)
{
	wStream staticS;
	wStream* s = Stream_StaticConstInit(&staticS, payload, len);

	WinPrNdrDecoder* decoder = winpr_ndr_decoder_new(FALSE, 1);
	const BOOL ok = decoder && winpr_ndr_decoder_read_fromDescr(decoder, s, descr, obj) &&
	                winpr_ndr_decoder_treat_deferreds(decoder, s) &&
	                (Stream_GetRemainingLength(s) == 0);
	winpr_ndr_decoder_destroy(&decoder);
	return ok;
}

typedef struct
{
	UINT32* a;
	UINT32* b;
} FULL_PAIR;

static const WinPrNdrFieldStruct FULL_PAIR_fields[] = {
	{ "a", offsetof(FULL_PAIR, a), WINPR_NDR_POINTER_FULL, -1, &winpr_ndr_uint32_descr_s },
	{ "b", offsetof(FULL_PAIR, b), WINPR_NDR_POINTER_FULL, -1, &winpr_ndr_uint32_descr_s },
};

static const WinPrNdrStructDescr FULL_PAIR_struct = { "FULL_PAIR", ARRAYSIZE(FULL_PAIR_fields),
	                                                  FULL_PAIR_fields };

/* two full pointers on the same referent: the referent is sent once */
static int TestNdrFullPointerAlias(void)
{
	const BYTE payload[] = {
		0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x00, 0x78, 0x56, 0x34, 0x12
	};
	FULL_PAIR pair = WINPR_C_ARRAY_INIT;
	int retCode = -2;

	if (!decode_payload(&FULL_PAIR_struct, payload, sizeof(payload), &pair))
		goto out;

	retCode = -3;
	if (!pair.a || (pair.a != pair.b) || (*pair.a != 0x12345678))
		goto out;

	/* a reference keeps the referent after the structure is destroyed */
	UINT32* kept = winpr_ndr_pointer_ref(pair.a);
	winpr_ndr_struct_destroy(&FULL_PAIR_struct, &pair);

	retCode = -4;
	if (pair.a || pair.b || (*kept != 0x12345678))
	{
		winpr_ndr_pointer_release(kept);
		goto out;
	}
	winpr_ndr_pointer_release(kept);

	/* distinct referents */
	const BYTE payload2[] = { 0x00, 0x00, 0x02, 0x00, 0x04, 0x00, 0x02, 0x00,
		                      0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00 };
	retCode = -5;
	if (!decode_payload(&FULL_PAIR_struct, payload2, sizeof(payload2), &pair) ||
	    (pair.a == pair.b) || (*pair.a != 1) || (*pair.b != 2))
		goto out;

	retCode = 0;
out:
	winpr_ndr_struct_destroy(&FULL_PAIR_struct, &pair);
	return retCode;
}

typedef struct
{
	UINT32* a;
	UINT16* b;
} FULL_MIXED;

static const WinPrNdrFieldStruct FULL_MIXED_fields[] = {
	{ "a", offsetof(FULL_MIXED, a), WINPR_NDR_POINTER_FULL, -1, &winpr_ndr_uint32_descr_s },
	{ "b", offsetof(FULL_MIXED, b), WINPR_NDR_POINTER_FULL, -1, &winpr_ndr_uint16_descr_s },
};

static const WinPrNdrStructDescr FULL_MIXED_struct = { "FULL_MIXED", ARRAYSIZE(FULL_MIXED_fields),
	                                                   FULL_MIXED_fields };

typedef struct
{
	UINT32* u;
	UINT32* f;
} UNIQUE_FULL;

static const WinPrNdrFieldStruct UNIQUE_FULL_fields[] = {
	{ "u", offsetof(UNIQUE_FULL, u), WINPR_NDR_POINTER, -1, &winpr_ndr_uint32_descr_s },
	{ "f", offsetof(UNIQUE_FULL, f), WINPR_NDR_POINTER_FULL, -1, &winpr_ndr_uint32_descr_s },
};

static const WinPrNdrStructDescr UNIQUE_FULL_struct = { "UNIQUE_FULL",
	                                                    ARRAYSIZE(UNIQUE_FULL_fields),
	                                                    UNIQUE_FULL_fields };

typedef struct
{
	WinPrNdrArrayHints n1;
	BYTE* a;
	WinPrNdrArrayHints n2;
	BYTE* b;
} FULL_ARRAYS;

static const WinPrNdrFieldStruct FULL_ARRAYS_fields[] = {
	{ "n1", offsetof(FULL_ARRAYS, n1), WINPR_NDR_NOT_POINTER, -1, &winpr_ndr_uint32_descr_s },
	{ "a", offsetof(FULL_ARRAYS, a), WINPR_NDR_POINTER_FULL, 0, &winpr_ndr_uint8Array_descr_s },
	{ "n2", offsetof(FULL_ARRAYS, n2), WINPR_NDR_NOT_POINTER, -1, &winpr_ndr_uint32_descr_s },
	{ "b", offsetof(FULL_ARRAYS, b), WINPR_NDR_POINTER_FULL, 2, &winpr_ndr_uint8Array_descr_s },
};

static const WinPrNdrStructDescr FULL_ARRAYS_struct = { "FULL_ARRAYS",
	                                                    ARRAYSIZE(FULL_ARRAYS_fields),
	                                                    FULL_ARRAYS_fields };

/* an aliased referent must keep its type and size */
static int TestNdrFullPointerConfusion(void)
{
	int retCode = -2;

	/* a UINT32 referent read as a UINT16 */
	const BYTE mixed[] = { 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x00, 0x78, 0x56, 0x34, 0x12 };
	FULL_MIXED fmixed = WINPR_C_ARRAY_INIT;
	const BOOL mixedOk = decode_payload(&FULL_MIXED_struct, mixed, sizeof(mixed), &fmixed);
	winpr_ndr_struct_destroy(&FULL_MIXED_struct, &fmixed);
	if (mixedOk)
		return retCode;

	/* the referent of a unique pointer can't be aliased */
	retCode = -3;
	UNIQUE_FULL ufull = WINPR_C_ARRAY_INIT;
	const BOOL ufullOk = decode_payload(&UNIQUE_FULL_struct, mixed, sizeof(mixed), &ufull);
	winpr_ndr_struct_destroy(&UNIQUE_FULL_struct, &ufull);
	if (ufullOk)
		return retCode;

	/* arrays of the same type and size can be aliased */
	retCode = -4;
	const BYTE arrays[] = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x02, 0x00, 0x00,
		                    0x00, 0x00, 0x00, 0x02, 0x00, 0x02, 0x00, 0x00, 0x00, 'A',  'B' };
	FULL_ARRAYS farrays = WINPR_C_ARRAY_INIT;
	const BOOL arraysOk = decode_payload(&FULL_ARRAYS_struct, arrays, sizeof(arrays), &farrays) &&
	                      farrays.a && (farrays.a == farrays.b) &&
	                      (memcmp(farrays.a, "AB", 2) == 0);
	winpr_ndr_struct_destroy(&FULL_ARRAYS_struct, &farrays);
	if (!arraysOk)
		return retCode;

	/* but not arrays of another size: the second one would be read past the referent */
	retCode = -5;
	const BYTE sizes[] = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x40, 0x00, 0x00,
		                   0x00, 0x00, 0x00, 0x02, 0x00, 0x02, 0x00, 0x00, 0x00, 'A',  'B' };
	const BOOL sizesOk = decode_payload(&FULL_ARRAYS_struct, sizes, sizeof(sizes), &farrays);
	winpr_ndr_struct_destroy(&FULL_ARRAYS_struct, &farrays);
	if (sizesOk)
		return retCode;

	return 0;
}

/* ================================== unions ======================================== */

/* struct { short CallId; [switch_type(short), switch_is(CallId)] union {
 *     [case(0x10)] long Value; [case(0x20)] long* Pointer; [default]; } u; } */
typedef union
{
	UINT32 Value;
	UINT32* Pointer;
} SHORT_UNION;

static const WinPrNdrUnionArm SHORT_UNION_arms[] = {
	{ 0x10, { "Value", 0, WINPR_NDR_NOT_POINTER, -1, &winpr_ndr_uint32_descr_s, 0, 0 } },
	{ 0x20, { "Pointer", 0, WINPR_NDR_POINTER, -1, &winpr_ndr_uint32_descr_s, 0, 0 } },
};
static const WinPrNdrUnionArm SHORT_UNION_default = {
	0, { "default", 0, WINPR_NDR_NOT_POINTER, -1, nullptr, 0, 0 }
};
static const WinPrNdrUnionDescr SHORT_UNION_union = { "SHORT_UNION", ARRAYSIZE(SHORT_UNION_arms),
	                                                  SHORT_UNION_arms, 2, &SHORT_UNION_default };

WINPR_NDR_UNION_MESSAGE_DEFINE(SHORT_UNION, SHORT_UNION);

typedef struct
{
	UINT16 CallId;
	SHORT_UNION u;
} SHORT_SWITCH;

static const WinPrNdrFieldStruct SHORT_SWITCH_fields[] = {
	{ "CallId", offsetof(SHORT_SWITCH, CallId), WINPR_NDR_NOT_POINTER, -1,
	  &winpr_ndr_uint16_descr_s, 0, 0 },
	{ "u", offsetof(SHORT_SWITCH, u), WINPR_NDR_NOT_POINTER, 0, &SHORT_UNION_descr_s, 0, 0 },
};

static const WinPrNdrStructDescr SHORT_SWITCH_struct = { "SHORT_SWITCH",
	                                                     ARRAYSIZE(SHORT_SWITCH_fields),
	                                                     SHORT_SWITCH_fields, 4 };

static const BYTE shortSwitchPointer[] = {
	0x20, 0x00, 0x20, 0x00, /* CallId, discriminant */
	0x00, 0x00, 0x02, 0x00, /* Pointer */
	0x44, 0x33, 0x22, 0x11, /* *Pointer */
};

static const BYTE shortSwitchValue[] = {
	0x10, 0x00, 0x10, 0x00, /* CallId, discriminant */
	0x88, 0x77, 0x66, 0x55, /* Value */
};

/* CallId, discriminant */
static const BYTE shortSwitchDefault[] = { 0x99, 0x00, 0x99, 0x00 };

static const BYTE shortSwitchMismatch[] = {
	0x10, 0x00, 0x20, 0x00, /* CallId, discriminant */
	0x88, 0x77, 0x66, 0x55,
};

static int TestNdrUnionWrite(void)
{
	int retCode = -2;
	UINT32 value = 0x11223344;
	const SHORT_SWITCH msgs[] = { { 0x20, { .Pointer = &value } },
		                          { 0x10, { .Value = 0x55667788 } },
		                          { 0x99, { .Value = 0 } } };
	const BYTE* expected[] = { shortSwitchPointer, shortSwitchValue, shortSwitchDefault };
	const size_t expectedLen[] = { sizeof(shortSwitchPointer), sizeof(shortSwitchValue),
		                           sizeof(shortSwitchDefault) };

	wStream* s = Stream_New(nullptr, 100);
	if (!s)
		return -1;

	for (size_t i = 0; i < ARRAYSIZE(msgs); i++)
	{
		WinPrNdrEncoder* encoder = winpr_ndr_encoder_new(FALSE, 1);
		Stream_SetPosition(s, 0);

		const BOOL ok =
		    encoder &&
		    winpr_ndr_encoder_write_fromDescr(encoder, s, &SHORT_SWITCH_struct, &msgs[i]) &&
		    winpr_ndr_encoder_treat_deferreds(encoder, s) &&
		    (Stream_GetPosition(s) == expectedLen[i]) &&
		    (memcmp(Stream_Buffer(s), expected[i], expectedLen[i]) == 0);
		winpr_ndr_encoder_destroy(&encoder);
		if (!ok)
			goto out;
	}

	retCode = 0;
out:
	Stream_Free(s, TRUE);
	return retCode;
}

static int TestNdrUnionRead(void)
{
	SHORT_SWITCH msg = WINPR_C_ARRAY_INIT;
	if (!decode_payload(&SHORT_SWITCH_struct, shortSwitchPointer, sizeof(shortSwitchPointer),
	                    &msg) ||
	    (msg.CallId != 0x20) || !msg.u.Pointer || (*msg.u.Pointer != 0x11223344))
		return -2;
	winpr_ndr_struct_destroy(&SHORT_SWITCH_struct, &msg);

	SHORT_SWITCH value = WINPR_C_ARRAY_INIT;
	if (!decode_payload(&SHORT_SWITCH_struct, shortSwitchValue, sizeof(shortSwitchValue), &value) ||
	    (value.CallId != 0x10) || (value.u.Value != 0x55667788))
		return -3;

	SHORT_SWITCH def = WINPR_C_ARRAY_INIT;
	if (!decode_payload(&SHORT_SWITCH_struct, shortSwitchDefault, sizeof(shortSwitchDefault),
	                    &def) ||
	    (def.CallId != 0x99))
		return -4;

	SHORT_SWITCH mismatch = WINPR_C_ARRAY_INIT;
	if (decode_payload(&SHORT_SWITCH_struct, shortSwitchMismatch, sizeof(shortSwitchMismatch),
	                   &mismatch))
		return -5;
	winpr_ndr_struct_destroy(&SHORT_SWITCH_struct, &mismatch);

	return 0;
}

int TestNdrBasic(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	int (*const tests[])(void) = { TestNdrBasic_1,
		                           TestNdrStaticArrayRead,
		                           TestNdrStaticArrayWrite,
		                           TestNdrStaticArrayOverflow,
		                           TestNdrDeferredOrderWrite,
		                           TestNdrDeferredOrderRead,
		                           TestNdrObject,
		                           TestNdrSignedTypes,
		                           TestNdrFullPointerAlias,
		                           TestNdrFullPointerConfusion,
		                           TestNdrUnionWrite,
		                           TestNdrUnionRead };

	for (size_t i = 0; i < ARRAYSIZE(tests); i++)
	{
		const int ret = tests[i]();
		if (ret)
		{
			(void)fprintf(stderr, "test %" PRIuz " failed with %d\n", i, ret);
			return ret;
		}
	}

	return 0;
}
