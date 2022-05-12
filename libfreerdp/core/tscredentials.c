/* ============================================================================================================
 * this file has been generated using
 * ./tools/asn_parser_generator.py --input=libfreerdp/core/credssp.asn1 --output-kind=impls
 * --output=libfreerdp/core/tscredentials.c
 *
 * /!\ If you want to modify this file you'd probably better change asn_parser_generator.py or the
 * corresponding ASN1 definition file
 *
 * ============================================================================================================
 */

#include <winpr/string.h>
#include <freerdp/crypto/ber.h>

#include "tscredentials.h"

#include <freerdp/log.h>

#define TAG FREERDP_TAG("core.tscredentials")

size_t ber_sizeof_nla_TSCredentials_content(const TSCredentials_t* item)
{
	size_t ret = 0;

	/* [0] credType (INTEGER)*/
	ret += ber_sizeof_contextual_integer(item->credType);

	/* [1] credentials (OCTET STRING)*/
	ret += ber_sizeof_contextual_octet_string(item->credentialsLen);

	return ret;
}

size_t ber_sizeof_nla_TSCredentials(const TSCredentials_t* item)
{
	size_t ret = ber_sizeof_nla_TSCredentials_content(item);
	return ber_sizeof_sequence(ret);
}
size_t ber_sizeof_contextual_nla_TSCredentials(const TSCredentials_t* item)
{
	size_t innerSz = ber_sizeof_nla_TSCredentials(item);
	return ber_sizeof_contextual_tag(innerSz) + innerSz;
}

void nla_TSCredentials_free(TSCredentials_t** pitem)
{
	TSCredentials_t* item;

	WINPR_ASSERT(pitem);
	item = *pitem;
	if (!item)
		return;

	free(item->credentials);
	free(item);
	*pitem = NULL;
}

size_t ber_write_nla_TSCredentials(wStream* s, const TSCredentials_t* item)
{
	size_t content_size = ber_sizeof_nla_TSCredentials_content(item);
	size_t ret = 0;

	ret = ber_write_sequence_tag(s, content_size);
	/* [0] credType (INTEGER) */
	if (!ber_write_contextual_integer(s, 0, item->credType))
		return 0;

	/* [1] credentials (OCTET STRING) */
	if (!ber_write_contextual_octet_string(s, 1, item->credentials, item->credentialsLen))
		return 0;

	return ret + content_size;
}

size_t ber_write_contextual_nla_TSCredentials(wStream* s, BYTE tag, const TSCredentials_t* item)
{
	size_t ret;
	size_t inner = ber_sizeof_nla_TSCredentials(item);

	ret = ber_write_contextual_tag(s, tag, inner, TRUE);
	ber_write_nla_TSCredentials(s, item);
	return ret + inner;
}

BOOL ber_read_nla_TSCredentials(wStream* s, TSCredentials_t** pret)
{
	wStream seqstream;
	size_t seqLength;
	size_t inner_size;
	wStream fieldStream;
	TSCredentials_t* item;
	BOOL ret;

	if (!ber_read_sequence_tag(s, &seqLength) ||
	    !Stream_CheckAndLogRequiredLength(TAG, s, seqLength))
		return FALSE;
	Stream_StaticInit(&seqstream, Stream_Pointer(s), seqLength);

	item = calloc(1, sizeof(*item));
	if (!item)
		return FALSE;

	/* [0] credType (INTEGER) */
	ret = ber_read_contextual_tag(&seqstream, 0, &inner_size, TRUE);
	if (!ret)
		goto out_fail_credType;
	Stream_StaticInit(&fieldStream, Stream_Pointer(&seqstream), inner_size);
	Stream_Seek(&seqstream, inner_size);

	ret = ber_read_integer(&fieldStream, &item->credType);
	if (!ret)
		goto out_fail_credType;

	/* [1] credentials (OCTET STRING) */
	ret = ber_read_contextual_tag(&seqstream, 1, &inner_size, TRUE);
	if (!ret)
		goto out_fail_credentials;
	Stream_StaticInit(&fieldStream, Stream_Pointer(&seqstream), inner_size);
	Stream_Seek(&seqstream, inner_size);

	ret = ber_read_octet_string(&fieldStream, &item->credentials, &item->credentialsLen);
	if (!ret)
		goto out_fail_credentials;

	*pret = item;
	return TRUE;

out_fail_credentials:

out_fail_credType:
	free(item);
	return FALSE;
}

size_t ber_sizeof_nla_TSPasswordCreds_content(const TSPasswordCreds_t* item)
{
	size_t ret = 0;

	/* [0] domainName (OCTET STRING)*/
	ret += ber_sizeof_contextual_octet_string(item->domainNameLen);

	/* [1] userName (OCTET STRING)*/
	ret += ber_sizeof_contextual_octet_string(item->userNameLen);

	/* [2] password (OCTET STRING)*/
	ret += ber_sizeof_contextual_octet_string(item->passwordLen);

	return ret;
}

size_t ber_sizeof_nla_TSPasswordCreds(const TSPasswordCreds_t* item)
{
	size_t ret = ber_sizeof_nla_TSPasswordCreds_content(item);
	return ber_sizeof_sequence(ret);
}
size_t ber_sizeof_contextual_nla_TSPasswordCreds(const TSPasswordCreds_t* item)
{
	size_t innerSz = ber_sizeof_nla_TSPasswordCreds(item);
	return ber_sizeof_contextual_tag(innerSz) + innerSz;
}

void nla_TSPasswordCreds_free(TSPasswordCreds_t** pitem)
{
	TSPasswordCreds_t* item;

	WINPR_ASSERT(pitem);
	item = *pitem;
	if (!item)
		return;

	free(item->domainName);
	free(item->userName);
	free(item->password);
	free(item);
	*pitem = NULL;
}

size_t ber_write_nla_TSPasswordCreds(wStream* s, const TSPasswordCreds_t* item)
{
	size_t content_size = ber_sizeof_nla_TSPasswordCreds_content(item);
	size_t ret = 0;

	ret = ber_write_sequence_tag(s, content_size);
	/* [0] domainName (OCTET STRING) */
	if (!ber_write_contextual_octet_string(s, 0, item->domainName, item->domainNameLen))
		return 0;

	/* [1] userName (OCTET STRING) */
	if (!ber_write_contextual_octet_string(s, 1, item->userName, item->userNameLen))
		return 0;

	/* [2] password (OCTET STRING) */
	if (!ber_write_contextual_octet_string(s, 2, item->password, item->passwordLen))
		return 0;

	return ret + content_size;
}

size_t ber_write_contextual_nla_TSPasswordCreds(wStream* s, BYTE tag, const TSPasswordCreds_t* item)
{
	size_t ret;
	size_t inner = ber_sizeof_nla_TSPasswordCreds(item);

	ret = ber_write_contextual_tag(s, tag, inner, TRUE);
	ber_write_nla_TSPasswordCreds(s, item);
	return ret + inner;
}

BOOL ber_read_nla_TSPasswordCreds(wStream* s, TSPasswordCreds_t** pret)
{
	wStream seqstream;
	size_t seqLength;
	size_t inner_size;
	wStream fieldStream;
	TSPasswordCreds_t* item;
	BOOL ret;

	if (!ber_read_sequence_tag(s, &seqLength) ||
	    !Stream_CheckAndLogRequiredLength(TAG, s, seqLength))
		return FALSE;
	Stream_StaticInit(&seqstream, Stream_Pointer(s), seqLength);

	item = calloc(1, sizeof(*item));
	if (!item)
		return FALSE;

	/* [0] domainName (OCTET STRING) */
	ret = ber_read_contextual_tag(&seqstream, 0, &inner_size, TRUE);
	if (!ret)
		goto out_fail_domainName;
	Stream_StaticInit(&fieldStream, Stream_Pointer(&seqstream), inner_size);
	Stream_Seek(&seqstream, inner_size);

	ret = ber_read_octet_string(&fieldStream, &item->domainName, &item->domainNameLen);
	if (!ret)
		goto out_fail_domainName;

	/* [1] userName (OCTET STRING) */
	ret = ber_read_contextual_tag(&seqstream, 1, &inner_size, TRUE);
	if (!ret)
		goto out_fail_userName;
	Stream_StaticInit(&fieldStream, Stream_Pointer(&seqstream), inner_size);
	Stream_Seek(&seqstream, inner_size);

	ret = ber_read_octet_string(&fieldStream, &item->userName, &item->userNameLen);
	if (!ret)
		goto out_fail_userName;

	/* [2] password (OCTET STRING) */
	ret = ber_read_contextual_tag(&seqstream, 2, &inner_size, TRUE);
	if (!ret)
		goto out_fail_password;
	Stream_StaticInit(&fieldStream, Stream_Pointer(&seqstream), inner_size);
	Stream_Seek(&seqstream, inner_size);

	ret = ber_read_octet_string(&fieldStream, &item->password, &item->passwordLen);
	if (!ret)
		goto out_fail_password;

	*pret = item;
	return TRUE;

out_fail_password:
	free(item->userName);
out_fail_userName:
	free(item->domainName);
out_fail_domainName:
	free(item);
	return FALSE;
}

size_t ber_sizeof_nla_TSCspDataDetail_content(const TSCspDataDetail_t* item)
{
	size_t ret = 0;

	/* [0] keySpec (INTEGER)*/
	ret += ber_sizeof_contextual_integer(item->keySpec);

	/* [1] cardName (OCTET STRING) OPTIONAL*/
	if (item->cardName)
	{
		ret += ber_sizeof_contextual_octet_string(strlen(item->cardName) * 2);
	}

	/* [2] readerName (OCTET STRING) OPTIONAL*/
	if (item->readerName)
	{
		ret += ber_sizeof_contextual_octet_string(strlen(item->readerName) * 2);
	}

	/* [3] containerName (OCTET STRING) OPTIONAL*/
	if (item->containerName)
	{
		ret += ber_sizeof_contextual_octet_string(strlen(item->containerName) * 2);
	}

	/* [4] cspName (OCTET STRING) OPTIONAL*/
	if (item->cspName)
	{
		ret += ber_sizeof_contextual_octet_string(strlen(item->cspName) * 2);
	}

	return ret;
}

size_t ber_sizeof_nla_TSCspDataDetail(const TSCspDataDetail_t* item)
{
	size_t ret = ber_sizeof_nla_TSCspDataDetail_content(item);
	return ber_sizeof_sequence(ret);
}
size_t ber_sizeof_contextual_nla_TSCspDataDetail(const TSCspDataDetail_t* item)
{
	size_t innerSz = ber_sizeof_nla_TSCspDataDetail(item);
	return ber_sizeof_contextual_tag(innerSz) + innerSz;
}

void nla_TSCspDataDetail_free(TSCspDataDetail_t** pitem)
{
	TSCspDataDetail_t* item;

	WINPR_ASSERT(pitem);
	item = *pitem;
	if (!item)
		return;

	free(item->cardName);
	free(item->readerName);
	free(item->containerName);
	free(item->cspName);
	free(item);
	*pitem = NULL;
}

size_t ber_write_nla_TSCspDataDetail(wStream* s, const TSCspDataDetail_t* item)
{
	size_t content_size = ber_sizeof_nla_TSCspDataDetail_content(item);
	size_t ret = 0;

	ret = ber_write_sequence_tag(s, content_size);
	/* [0] keySpec (INTEGER) */
	if (!ber_write_contextual_integer(s, 0, item->keySpec))
		return 0;

	/* [1] cardName (OCTET STRING) OPTIONAL */
	if (item->cardName)
	{
		if (!ber_write_contextual_char_to_unicode_octet_string(s, 1, item->cardName))
			return 0;
	}

	/* [2] readerName (OCTET STRING) OPTIONAL */
	if (item->readerName)
	{
		if (!ber_write_contextual_char_to_unicode_octet_string(s, 2, item->readerName))
			return 0;
	}

	/* [3] containerName (OCTET STRING) OPTIONAL */
	if (item->containerName)
	{
		if (!ber_write_contextual_char_to_unicode_octet_string(s, 3, item->containerName))
			return 0;
	}

	/* [4] cspName (OCTET STRING) OPTIONAL */
	if (item->cspName)
	{
		if (!ber_write_contextual_char_to_unicode_octet_string(s, 4, item->cspName))
			return 0;
	}

	return ret + content_size;
}

size_t ber_write_contextual_nla_TSCspDataDetail(wStream* s, BYTE tag, const TSCspDataDetail_t* item)
{
	size_t ret;
	size_t inner = ber_sizeof_nla_TSCspDataDetail(item);

	ret = ber_write_contextual_tag(s, tag, inner, TRUE);
	ber_write_nla_TSCspDataDetail(s, item);
	return ret + inner;
}

BOOL ber_read_nla_TSCspDataDetail(wStream* s, TSCspDataDetail_t** pret)
{
	wStream seqstream;
	size_t seqLength;
	size_t inner_size;
	wStream fieldStream;
	TSCspDataDetail_t* item;
	BOOL ret;

	if (!ber_read_sequence_tag(s, &seqLength) ||
	    !Stream_CheckAndLogRequiredLength(TAG, s, seqLength))
		return FALSE;
	Stream_StaticInit(&seqstream, Stream_Pointer(s), seqLength);

	item = calloc(1, sizeof(*item));
	if (!item)
		return FALSE;

	/* [0] keySpec (INTEGER) */
	ret = ber_read_contextual_tag(&seqstream, 0, &inner_size, TRUE);
	if (!ret)
		goto out_fail_keySpec;
	Stream_StaticInit(&fieldStream, Stream_Pointer(&seqstream), inner_size);
	Stream_Seek(&seqstream, inner_size);

	ret = ber_read_integer(&fieldStream, &item->keySpec);
	if (!ret)
		goto out_fail_keySpec;

	/* [1] cardName (OCTET STRING) OPTIONAL */
	ret = ber_read_contextual_tag(&seqstream, 1, &inner_size, TRUE);
	if (ret)
	{
		Stream_StaticInit(&fieldStream, Stream_Pointer(&seqstream), inner_size);
		Stream_Seek(&seqstream, inner_size);

		ret = ber_read_char_from_unicode_octet_string(&fieldStream, &item->cardName);
		if (!ret)
			goto out_fail_cardName;
	}
	/* [2] readerName (OCTET STRING) OPTIONAL */
	ret = ber_read_contextual_tag(&seqstream, 2, &inner_size, TRUE);
	if (ret)
	{
		Stream_StaticInit(&fieldStream, Stream_Pointer(&seqstream), inner_size);
		Stream_Seek(&seqstream, inner_size);

		ret = ber_read_char_from_unicode_octet_string(&fieldStream, &item->readerName);
		if (!ret)
			goto out_fail_readerName;
	}
	/* [3] containerName (OCTET STRING) OPTIONAL */
	ret = ber_read_contextual_tag(&seqstream, 3, &inner_size, TRUE);
	if (ret)
	{
		Stream_StaticInit(&fieldStream, Stream_Pointer(&seqstream), inner_size);
		Stream_Seek(&seqstream, inner_size);

		ret = ber_read_char_from_unicode_octet_string(&fieldStream, &item->containerName);
		if (!ret)
			goto out_fail_containerName;
	}
	/* [4] cspName (OCTET STRING) OPTIONAL */
	ret = ber_read_contextual_tag(&seqstream, 4, &inner_size, TRUE);
	if (ret)
	{
		Stream_StaticInit(&fieldStream, Stream_Pointer(&seqstream), inner_size);
		Stream_Seek(&seqstream, inner_size);

		ret = ber_read_char_from_unicode_octet_string(&fieldStream, &item->cspName);
		if (!ret)
			goto out_fail_cspName;
	}
	*pret = item;
	return TRUE;

out_fail_cspName:
	free(item->containerName);
out_fail_containerName:
	free(item->readerName);
out_fail_readerName:
	free(item->cardName);
out_fail_cardName:

out_fail_keySpec:
	free(item);
	return FALSE;
}

size_t ber_sizeof_nla_TSSmartCardCreds_content(const TSSmartCardCreds_t* item)
{
	size_t ret = 0;

	/* [0] pin (OCTET STRING)*/
	ret += ber_sizeof_contextual_octet_string(strlen(item->pin) * 2);

	/* [1] cspData (TSCspDataDetail)*/
	ret += ber_sizeof_contextual_nla_TSCspDataDetail(item->cspData);

	/* [2] userHint (OCTET STRING) OPTIONAL*/
	if (item->userHint)
	{
		ret += ber_sizeof_contextual_octet_string(strlen(item->userHint) * 2);
	}

	/* [3] domainHint (OCTET STRING) OPTIONAL*/
	if (item->domainHint)
	{
		ret += ber_sizeof_contextual_octet_string(strlen(item->domainHint) * 2);
	}

	return ret;
}

size_t ber_sizeof_nla_TSSmartCardCreds(const TSSmartCardCreds_t* item)
{
	size_t ret = ber_sizeof_nla_TSSmartCardCreds_content(item);
	return ber_sizeof_sequence(ret);
}
size_t ber_sizeof_contextual_nla_TSSmartCardCreds(const TSSmartCardCreds_t* item)
{
	size_t innerSz = ber_sizeof_nla_TSSmartCardCreds(item);
	return ber_sizeof_contextual_tag(innerSz) + innerSz;
}

void nla_TSSmartCardCreds_free(TSSmartCardCreds_t** pitem)
{
	TSSmartCardCreds_t* item;

	WINPR_ASSERT(pitem);
	item = *pitem;
	if (!item)
		return;

	free(item->pin);
	nla_TSCspDataDetail_free(&item->cspData);
	free(item->userHint);
	free(item->domainHint);
	free(item);
	*pitem = NULL;
}

size_t ber_write_nla_TSSmartCardCreds(wStream* s, const TSSmartCardCreds_t* item)
{
	size_t content_size = ber_sizeof_nla_TSSmartCardCreds_content(item);
	size_t ret = 0;

	ret = ber_write_sequence_tag(s, content_size);
	/* [0] pin (OCTET STRING) */
	if (!ber_write_contextual_char_to_unicode_octet_string(s, 0, item->pin))
		return 0;

	/* [1] cspData (TSCspDataDetail) */
	if (!ber_write_contextual_nla_TSCspDataDetail(s, 1, item->cspData))
		return 0;

	/* [2] userHint (OCTET STRING) OPTIONAL */
	if (item->userHint)
	{
		if (!ber_write_contextual_char_to_unicode_octet_string(s, 2, item->userHint))
			return 0;
	}

	/* [3] domainHint (OCTET STRING) OPTIONAL */
	if (item->domainHint)
	{
		if (!ber_write_contextual_char_to_unicode_octet_string(s, 3, item->domainHint))
			return 0;
	}

	return ret + content_size;
}

size_t ber_write_contextual_nla_TSSmartCardCreds(wStream* s, BYTE tag,
                                                 const TSSmartCardCreds_t* item)
{
	size_t ret;
	size_t inner = ber_sizeof_nla_TSSmartCardCreds(item);

	ret = ber_write_contextual_tag(s, tag, inner, TRUE);
	ber_write_nla_TSSmartCardCreds(s, item);
	return ret + inner;
}

BOOL ber_read_nla_TSSmartCardCreds(wStream* s, TSSmartCardCreds_t** pret)
{
	wStream seqstream;
	size_t seqLength;
	size_t inner_size;
	wStream fieldStream;
	TSSmartCardCreds_t* item;
	BOOL ret;

	if (!ber_read_sequence_tag(s, &seqLength) ||
	    !Stream_CheckAndLogRequiredLength(TAG, s, seqLength))
		return FALSE;
	Stream_StaticInit(&seqstream, Stream_Pointer(s), seqLength);

	item = calloc(1, sizeof(*item));
	if (!item)
		return FALSE;

	/* [0] pin (OCTET STRING) */
	ret = ber_read_contextual_tag(&seqstream, 0, &inner_size, TRUE);
	if (!ret)
		goto out_fail_pin;
	Stream_StaticInit(&fieldStream, Stream_Pointer(&seqstream), inner_size);
	Stream_Seek(&seqstream, inner_size);

	ret = ber_read_char_from_unicode_octet_string(&fieldStream, &item->pin);
	if (!ret)
		goto out_fail_pin;

	/* [1] cspData (TSCspDataDetail) */
	ret = ber_read_contextual_tag(&seqstream, 1, &inner_size, TRUE);
	if (!ret)
		goto out_fail_cspData;
	Stream_StaticInit(&fieldStream, Stream_Pointer(&seqstream), inner_size);
	Stream_Seek(&seqstream, inner_size);

	ret = ber_read_nla_TSCspDataDetail(&fieldStream, &item->cspData);
	if (!ret)
		goto out_fail_cspData;

	/* [2] userHint (OCTET STRING) OPTIONAL */
	ret = ber_read_contextual_tag(&seqstream, 2, &inner_size, TRUE);
	if (ret)
	{
		Stream_StaticInit(&fieldStream, Stream_Pointer(&seqstream), inner_size);
		Stream_Seek(&seqstream, inner_size);

		ret = ber_read_char_from_unicode_octet_string(&fieldStream, &item->userHint);
		if (!ret)
			goto out_fail_userHint;
	}
	/* [3] domainHint (OCTET STRING) OPTIONAL */
	ret = ber_read_contextual_tag(&seqstream, 3, &inner_size, TRUE);
	if (ret)
	{
		Stream_StaticInit(&fieldStream, Stream_Pointer(&seqstream), inner_size);
		Stream_Seek(&seqstream, inner_size);

		ret = ber_read_char_from_unicode_octet_string(&fieldStream, &item->domainHint);
		if (!ret)
			goto out_fail_domainHint;
	}
	*pret = item;
	return TRUE;

out_fail_domainHint:
	free(item->userHint);
out_fail_userHint:
	nla_TSCspDataDetail_free(&item->cspData);
out_fail_cspData:
	free(item->pin);
out_fail_pin:
	free(item);
	return FALSE;
}

size_t ber_sizeof_nla_TSRemoteGuardPackageCred_content(const TSRemoteGuardPackageCred_t* item)
{
	size_t ret = 0;

	/* [0] packageName (OCTET STRING)*/
	ret += ber_sizeof_contextual_octet_string(item->packageNameLen);

	/* [1] credBuffer (OCTET STRING)*/
	ret += ber_sizeof_contextual_octet_string(item->credBufferLen);

	return ret;
}

size_t ber_sizeof_nla_TSRemoteGuardPackageCred(const TSRemoteGuardPackageCred_t* item)
{
	size_t ret = ber_sizeof_nla_TSRemoteGuardPackageCred_content(item);
	return ber_sizeof_sequence(ret);
}
size_t ber_sizeof_contextual_nla_TSRemoteGuardPackageCred(const TSRemoteGuardPackageCred_t* item)
{
	size_t innerSz = ber_sizeof_nla_TSRemoteGuardPackageCred(item);
	return ber_sizeof_contextual_tag(innerSz) + innerSz;
}

void nla_TSRemoteGuardPackageCred_free(TSRemoteGuardPackageCred_t** pitem)
{
	TSRemoteGuardPackageCred_t* item;

	WINPR_ASSERT(pitem);
	item = *pitem;
	if (!item)
		return;

	free(item->packageName);
	free(item->credBuffer);
	free(item);
	*pitem = NULL;
}

size_t ber_write_nla_TSRemoteGuardPackageCred(wStream* s, const TSRemoteGuardPackageCred_t* item)
{
	size_t content_size = ber_sizeof_nla_TSRemoteGuardPackageCred_content(item);
	size_t ret = 0;

	ret = ber_write_sequence_tag(s, content_size);
	/* [0] packageName (OCTET STRING) */
	if (!ber_write_contextual_octet_string(s, 0, item->packageName, item->packageNameLen))
		return 0;

	/* [1] credBuffer (OCTET STRING) */
	if (!ber_write_contextual_octet_string(s, 1, item->credBuffer, item->credBufferLen))
		return 0;

	return ret + content_size;
}

size_t ber_write_contextual_nla_TSRemoteGuardPackageCred(wStream* s, BYTE tag,
                                                         const TSRemoteGuardPackageCred_t* item)
{
	size_t ret;
	size_t inner = ber_sizeof_nla_TSRemoteGuardPackageCred(item);

	ret = ber_write_contextual_tag(s, tag, inner, TRUE);
	ber_write_nla_TSRemoteGuardPackageCred(s, item);
	return ret + inner;
}

BOOL ber_read_nla_TSRemoteGuardPackageCred(wStream* s, TSRemoteGuardPackageCred_t** pret)
{
	wStream seqstream;
	size_t seqLength;
	size_t inner_size;
	wStream fieldStream;
	TSRemoteGuardPackageCred_t* item;
	BOOL ret;

	if (!ber_read_sequence_tag(s, &seqLength) ||
	    !Stream_CheckAndLogRequiredLength(TAG, s, seqLength))
		return FALSE;
	Stream_StaticInit(&seqstream, Stream_Pointer(s), seqLength);

	item = calloc(1, sizeof(*item));
	if (!item)
		return FALSE;

	/* [0] packageName (OCTET STRING) */
	ret = ber_read_contextual_tag(&seqstream, 0, &inner_size, TRUE);
	if (!ret)
		goto out_fail_packageName;
	Stream_StaticInit(&fieldStream, Stream_Pointer(&seqstream), inner_size);
	Stream_Seek(&seqstream, inner_size);

	ret = ber_read_octet_string(&fieldStream, &item->packageName, &item->packageNameLen);
	if (!ret)
		goto out_fail_packageName;

	/* [1] credBuffer (OCTET STRING) */
	ret = ber_read_contextual_tag(&seqstream, 1, &inner_size, TRUE);
	if (!ret)
		goto out_fail_credBuffer;
	Stream_StaticInit(&fieldStream, Stream_Pointer(&seqstream), inner_size);
	Stream_Seek(&seqstream, inner_size);

	ret = ber_read_octet_string(&fieldStream, &item->credBuffer, &item->credBufferLen);
	if (!ret)
		goto out_fail_credBuffer;

	*pret = item;
	return TRUE;

out_fail_credBuffer:
	free(item->packageName);
out_fail_packageName:
	free(item);
	return FALSE;
}

size_t ber_sizeof_nla_TSRemoteGuardPackageCred_array_content(const TSRemoteGuardPackageCred_t* item,
                                                             size_t nitems)
{
	size_t i, ret = 0;
	for (i = 0; i < nitems; i++, item++)
		ret += ber_sizeof_nla_TSRemoteGuardPackageCred(item);

	return ber_sizeof_sequence(ret);
}

size_t ber_sizeof_nla_TSRemoteGuardPackageCred_array(const TSRemoteGuardPackageCred_t* item,
                                                     size_t nitems)
{
	return ber_sizeof_sequence(ber_sizeof_nla_TSRemoteGuardPackageCred_array_content(item, nitems));
}

size_t
ber_sizeof_contextual_nla_TSRemoteGuardPackageCred_array(const TSRemoteGuardPackageCred_t* item,
                                                         size_t nitems)
{
	size_t inner = ber_sizeof_nla_TSRemoteGuardPackageCred_array(item, nitems);
	return ber_sizeof_contextual_tag(inner) + inner;
}

size_t ber_write_nla_TSRemoteGuardPackageCred_array(wStream* s,
                                                    const TSRemoteGuardPackageCred_t* item,
                                                    size_t nitems)
{
	size_t i, r, ret;
	size_t inner_len = ber_sizeof_nla_TSRemoteGuardPackageCred_array_content(item, nitems);

	ret = ber_write_sequence_tag(s, inner_len);

	for (i = 0; i < nitems; i++, item++)
	{
		r = ber_write_nla_TSRemoteGuardPackageCred(s, item);
		if (!r)
			return 0;
		ret += r;
	}

	return ret;
}

size_t ber_write_contextual_nla_TSRemoteGuardPackageCred_array(
    wStream* s, BYTE tag, const TSRemoteGuardPackageCred_t* item, size_t nitems)
{
	size_t ret;
	size_t inner = ber_sizeof_nla_TSRemoteGuardPackageCred_array(item, nitems);

	ret = ber_write_contextual_tag(s, tag, inner, TRUE);
	ber_write_nla_TSRemoteGuardPackageCred_array(s, item, nitems);
	return ret + inner;
}

BOOL ber_read_nla_TSRemoteGuardPackageCred_array(wStream* s, TSRemoteGuardPackageCred_t** pitems,
                                                 size_t* nitems)
{
	size_t subLen;
	wStream subStream;
	TSRemoteGuardPackageCred_t* retItems = NULL;
	size_t ret = 0;

	if (!ber_read_sequence_tag(s, &subLen) || !Stream_CheckAndLogRequiredLength(TAG, s, subLen))
		return FALSE;

	Stream_StaticInit(&subStream, Stream_Pointer(s), subLen);
	while (Stream_GetRemainingLength(&subStream))
	{
		TSRemoteGuardPackageCred_t* item;
		TSRemoteGuardPackageCred_t* tmpRet;

		if (!ber_read_nla_TSRemoteGuardPackageCred(&subStream, &item))
		{
			free(retItems);
			return FALSE;
		}

		tmpRet = realloc(retItems, (ret + 1) * sizeof(TSRemoteGuardPackageCred_t));
		if (!tmpRet)
		{
			free(retItems);
			return FALSE;
		}
		retItems = tmpRet;

		memcpy(&retItems[ret], item, sizeof(*item));
		free(item);
		ret++;
	}

	*pitems = retItems;
	*nitems = ret;
	return TRUE;
}

size_t ber_sizeof_nla_TSRemoteGuardCreds_content(const TSRemoteGuardCreds_t* item)
{
	size_t ret = 0;

	/* [0] logonCred (TSRemoteGuardPackageCred)*/
	ret += ber_sizeof_contextual_nla_TSRemoteGuardPackageCred(item->logonCred);

	/* [1] supplementalCreds (SEQUENCE OF) OPTIONAL*/
	if (item->supplementalCreds)
	{
		ret += ber_sizeof_contextual_nla_TSRemoteGuardPackageCred_array(
		    item->supplementalCreds, item->supplementalCredsItems);
	}

	return ret;
}

size_t ber_sizeof_nla_TSRemoteGuardCreds(const TSRemoteGuardCreds_t* item)
{
	size_t ret = ber_sizeof_nla_TSRemoteGuardCreds_content(item);
	return ber_sizeof_sequence(ret);
}
size_t ber_sizeof_contextual_nla_TSRemoteGuardCreds(const TSRemoteGuardCreds_t* item)
{
	size_t innerSz = ber_sizeof_nla_TSRemoteGuardCreds(item);
	return ber_sizeof_contextual_tag(innerSz) + innerSz;
}

void nla_TSRemoteGuardCreds_free(TSRemoteGuardCreds_t** pitem)
{
	TSRemoteGuardCreds_t* item;

	WINPR_ASSERT(pitem);
	item = *pitem;
	if (!item)
		return;

	nla_TSRemoteGuardPackageCred_free(&item->logonCred);
	free(item);
	*pitem = NULL;
}

size_t ber_write_nla_TSRemoteGuardCreds(wStream* s, const TSRemoteGuardCreds_t* item)
{
	size_t content_size = ber_sizeof_nla_TSRemoteGuardCreds_content(item);
	size_t ret = 0;

	ret = ber_write_sequence_tag(s, content_size);
	/* [0] logonCred (TSRemoteGuardPackageCred) */
	if (!ber_write_contextual_nla_TSRemoteGuardPackageCred(s, 0, item->logonCred))
		return 0;

	/* [1] supplementalCreds (SEQUENCE OF) OPTIONAL */
	if (item->supplementalCreds)
	{
		if (!ber_write_contextual_nla_TSRemoteGuardPackageCred_array(s, 1, item->supplementalCreds,
		                                                             item->supplementalCredsItems))
			return 0;
	}

	return ret + content_size;
}

size_t ber_write_contextual_nla_TSRemoteGuardCreds(wStream* s, BYTE tag,
                                                   const TSRemoteGuardCreds_t* item)
{
	size_t ret;
	size_t inner = ber_sizeof_nla_TSRemoteGuardCreds(item);

	ret = ber_write_contextual_tag(s, tag, inner, TRUE);
	ber_write_nla_TSRemoteGuardCreds(s, item);
	return ret + inner;
}

BOOL ber_read_nla_TSRemoteGuardCreds(wStream* s, TSRemoteGuardCreds_t** pret)
{
	wStream seqstream;
	size_t seqLength;
	size_t inner_size;
	wStream fieldStream;
	TSRemoteGuardCreds_t* item;
	BOOL ret;

	if (!ber_read_sequence_tag(s, &seqLength) ||
	    !Stream_CheckAndLogRequiredLength(TAG, s, seqLength))
		return FALSE;
	Stream_StaticInit(&seqstream, Stream_Pointer(s), seqLength);

	item = calloc(1, sizeof(*item));
	if (!item)
		return FALSE;

	/* [0] logonCred (TSRemoteGuardPackageCred) */
	ret = ber_read_contextual_tag(&seqstream, 0, &inner_size, TRUE);
	if (!ret)
		goto out_fail_logonCred;
	Stream_StaticInit(&fieldStream, Stream_Pointer(&seqstream), inner_size);
	Stream_Seek(&seqstream, inner_size);

	ret = ber_read_nla_TSRemoteGuardPackageCred(&fieldStream, &item->logonCred);
	if (!ret)
		goto out_fail_logonCred;

	/* [1] supplementalCreds (SEQUENCE OF) OPTIONAL */
	ret = ber_read_contextual_tag(&seqstream, 1, &inner_size, TRUE);
	if (ret)
	{
		Stream_StaticInit(&fieldStream, Stream_Pointer(&seqstream), inner_size);
		Stream_Seek(&seqstream, inner_size);

		ret = ber_read_nla_TSRemoteGuardPackageCred_array(&fieldStream, &item->supplementalCreds,
		                                                  &item->supplementalCredsItems);
		if (!ret)
			goto out_fail_supplementalCreds;
	}
	*pret = item;
	return TRUE;

out_fail_supplementalCreds:
	nla_TSRemoteGuardPackageCred_free(&item->logonCred);
out_fail_logonCred:
	free(item);
	return FALSE;
}
