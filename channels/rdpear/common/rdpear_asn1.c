/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * ASN1 routines for RDPEAR
 *
 * Copyright 2024 David Fort <contact@hardening-consulting.com>
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

#include <rdpear-common/rdpear_asn1.h>
#include <winpr/asn1.h>

wStream* rdpear_enc_Checksum(UINT32 cksumtype, krb5_checksum* csum)
{
	/**
	 * Checksum        ::= SEQUENCE {
	 *   cksumtype       [0] Int32,
	 *   checksum        [1] OCTET STRING
	 * }
	 */
	wStream* ret = NULL;
	WinPrAsn1Encoder* enc = WinPrAsn1Encoder_New(WINPR_ASN1_DER);
	if (!enc)
		return NULL;

	if (!WinPrAsn1EncSeqContainer(enc))
		goto out;

	if (!WinPrAsn1EncContextualInteger(enc, 0, (WinPrAsn1_INTEGER)cksumtype))
		goto out;

	WinPrAsn1_OctetString octets;
	octets.data = (BYTE*)csum->contents;
	octets.len = csum->length;
	if (!WinPrAsn1EncContextualOctetString(enc, 1, &octets) || !WinPrAsn1EncEndContainer(enc))
		goto out;

	ret = Stream_New(NULL, 1024);
	if (!ret)
		goto out;

	if (!WinPrAsn1EncToStream(enc, ret))
	{
		Stream_Free(ret, TRUE);
		ret = NULL;
		goto out;
	}

out:
	WinPrAsn1Encoder_Free(&enc);
	return ret;
}

wStream* rdpear_enc_EncryptedData(UINT32 encType, krb5_data* payload)
{
	/**
	 * EncryptedData   ::= SEQUENCE {
	 *   etype   [0] Int32 -- EncryptionType --,
	 *   kvno    [1] UInt32 OPTIONAL,
	 *   cipher  [2] OCTET STRING -- ciphertext
	 *	}
	 */
	wStream* ret = NULL;
	WinPrAsn1Encoder* enc = WinPrAsn1Encoder_New(WINPR_ASN1_DER);
	if (!enc)
		return NULL;

	if (!WinPrAsn1EncSeqContainer(enc))
		goto out;

	if (!WinPrAsn1EncContextualInteger(enc, 0, (WinPrAsn1_INTEGER)encType))
		goto out;

	WinPrAsn1_OctetString octets;
	octets.data = (BYTE*)payload->data;
	octets.len = payload->length;
	if (!WinPrAsn1EncContextualOctetString(enc, 2, &octets) || !WinPrAsn1EncEndContainer(enc))
		goto out;

	ret = Stream_New(NULL, 1024);
	if (!ret)
		goto out;

	if (!WinPrAsn1EncToStream(enc, ret))
	{
		Stream_Free(ret, TRUE);
		ret = NULL;
		goto out;
	}

out:
	WinPrAsn1Encoder_Free(&enc);
	return ret;
}
