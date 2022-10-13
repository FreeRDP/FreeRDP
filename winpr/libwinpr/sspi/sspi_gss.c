/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Generic Security Service Application Program Interface (GSSAPI)
 *
 * Copyright 2015 ANSSI, Author Thomas Calderon
 * Copyright 2015 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/endian.h>
#include <winpr/asn1.h>
#include <winpr/stream.h>

#include "sspi_gss.h"

BOOL sspi_gss_wrap_token(SecBuffer* buf, const WinPrAsn1_OID* oid, uint16_t tok_id,
                         const sspi_gss_data* token)
{
	WinPrAsn1Encoder* enc;
	BYTE tok_id_buf[2];
	WinPrAsn1_MemoryChunk mc = { 2, tok_id_buf };
	wStream s;
	size_t len;
	BOOL ret = FALSE;

	WINPR_ASSERT(buf);
	WINPR_ASSERT(oid);
	WINPR_ASSERT(token);

	Data_Write_UINT16_BE(tok_id_buf, tok_id);

	enc = WinPrAsn1Encoder_New(WINPR_ASN1_DER);
	if (!enc)
		return FALSE;

	/* initialContextToken [APPLICATION 0] */
	if (!WinPrAsn1EncAppContainer(enc, 0))
		goto cleanup;

	/* thisMech OID */
	if (!WinPrAsn1EncOID(enc, oid))
		goto cleanup;

	/* TOK_ID */
	if (!WinPrAsn1EncRawContent(enc, &mc))
		goto cleanup;

	/* innerToken */
	mc.data = (BYTE*)token->data;
	mc.len = token->length;
	if (!WinPrAsn1EncRawContent(enc, &mc))
		goto cleanup;

	if (!WinPrAsn1EncEndContainer(enc))
		goto cleanup;

	if (!WinPrAsn1EncStreamSize(enc, &len) || len > buf->cbBuffer)
		goto cleanup;

	Stream_StaticInit(&s, buf->pvBuffer, len);
	if (WinPrAsn1EncToStream(enc, &s))
	{
		buf->cbBuffer = len;
		ret = TRUE;
	}

cleanup:
	WinPrAsn1Encoder_Free(&enc);
	return ret;
}

BOOL sspi_gss_unwrap_token(const SecBuffer* buf, WinPrAsn1_OID* oid, uint16_t* tok_id,
                           sspi_gss_data* token)
{
	WinPrAsn1Decoder dec, dec2;
	WinPrAsn1_tagId tag;
	wStream sbuffer = { 0 };
	wStream* s;

	WINPR_ASSERT(buf);
	WINPR_ASSERT(oid);
	WINPR_ASSERT(token);

	WinPrAsn1Decoder_InitMem(&dec, WINPR_ASN1_DER, buf->pvBuffer, buf->cbBuffer);

	if (!WinPrAsn1DecReadApp(&dec, &tag, &dec2) || tag != 0)
		return FALSE;

	if (!WinPrAsn1DecReadOID(&dec2, oid, FALSE))
		return FALSE;

	sbuffer = WinPrAsn1DecGetStream(&dec2);
	s = &sbuffer;

	if (Stream_Length(s) < 2)
		return FALSE;

	if (tok_id)
		Stream_Read_INT16_BE(s, *tok_id);

	token->data = (char*)Stream_Pointer(s);
	token->length = (UINT)Stream_GetRemainingLength(s);

	return TRUE;
}
