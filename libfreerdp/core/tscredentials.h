/* ============================================================================================================
 * this file has been generated using
 * ./tools/asn_parser_generator.py --input=libfreerdp/core/credssp.asn1 --output-kind=headers
 * --output=libfreerdp/core/tscredentials.h
 *
 * /!\ If you want to modify this file you'd probably better change asn_parser_generator.py or the
 * corresponding ASN1 definition file
 *
 * ============================================================================================================
 */
#ifndef LIBFREERDP_CORE_CREDSSP_ASN1_H
#define LIBFREERDP_CORE_CREDSSP_ASN1_H

#include <winpr/stream.h>

typedef struct
{
	UINT32 credType;
	size_t credentialsLen;
	BYTE* credentials;
} TSCredentials_t;

typedef struct
{
	size_t domainNameLen;
	BYTE* domainName;
	size_t userNameLen;
	BYTE* userName;
	size_t passwordLen;
	BYTE* password;
} TSPasswordCreds_t;

typedef struct
{
	UINT32 keySpec;
	char* cardName;
	char* readerName;
	char* containerName;
	char* cspName;
} TSCspDataDetail_t;

typedef struct
{
	char* pin;
	TSCspDataDetail_t* cspData;
	char* userHint;
	char* domainHint;
} TSSmartCardCreds_t;

typedef struct
{
	size_t packageNameLen;
	BYTE* packageName;
	size_t credBufferLen;
	BYTE* credBuffer;
} TSRemoteGuardPackageCred_t;

typedef struct
{
	TSRemoteGuardPackageCred_t* logonCred;
	size_t supplementalCredsItems;
	TSRemoteGuardPackageCred_t* supplementalCreds;
} TSRemoteGuardCreds_t;

size_t ber_sizeof_nla_TSCredentials_content(const TSCredentials_t* item);
size_t ber_sizeof_nla_TSCredentials(const TSCredentials_t* item);
size_t ber_sizeof_contextual_nla_TSCredentials(const TSCredentials_t* item);
void nla_TSCredentials_free(TSCredentials_t** pitem);
size_t ber_write_nla_TSCredentials(wStream* s, const TSCredentials_t* item);
size_t ber_write_contextual_nla_TSCredentials(wStream* s, BYTE tag, const TSCredentials_t* item);
BOOL ber_read_nla_TSCredentials(wStream* s, TSCredentials_t** pret);

size_t ber_sizeof_nla_TSPasswordCreds_content(const TSPasswordCreds_t* item);
size_t ber_sizeof_nla_TSPasswordCreds(const TSPasswordCreds_t* item);
size_t ber_sizeof_contextual_nla_TSPasswordCreds(const TSPasswordCreds_t* item);
void nla_TSPasswordCreds_free(TSPasswordCreds_t** pitem);
size_t ber_write_nla_TSPasswordCreds(wStream* s, const TSPasswordCreds_t* item);
size_t ber_write_contextual_nla_TSPasswordCreds(wStream* s, BYTE tag,
                                                const TSPasswordCreds_t* item);
BOOL ber_read_nla_TSPasswordCreds(wStream* s, TSPasswordCreds_t** pret);

size_t ber_sizeof_nla_TSCspDataDetail_content(const TSCspDataDetail_t* item);
size_t ber_sizeof_nla_TSCspDataDetail(const TSCspDataDetail_t* item);
size_t ber_sizeof_contextual_nla_TSCspDataDetail(const TSCspDataDetail_t* item);
void nla_TSCspDataDetail_free(TSCspDataDetail_t** pitem);
size_t ber_write_nla_TSCspDataDetail(wStream* s, const TSCspDataDetail_t* item);
size_t ber_write_contextual_nla_TSCspDataDetail(wStream* s, BYTE tag,
                                                const TSCspDataDetail_t* item);
BOOL ber_read_nla_TSCspDataDetail(wStream* s, TSCspDataDetail_t** pret);

size_t ber_sizeof_nla_TSSmartCardCreds_content(const TSSmartCardCreds_t* item);
size_t ber_sizeof_nla_TSSmartCardCreds(const TSSmartCardCreds_t* item);
size_t ber_sizeof_contextual_nla_TSSmartCardCreds(const TSSmartCardCreds_t* item);
void nla_TSSmartCardCreds_free(TSSmartCardCreds_t** pitem);
size_t ber_write_nla_TSSmartCardCreds(wStream* s, const TSSmartCardCreds_t* item);
size_t ber_write_contextual_nla_TSSmartCardCreds(wStream* s, BYTE tag,
                                                 const TSSmartCardCreds_t* item);
BOOL ber_read_nla_TSSmartCardCreds(wStream* s, TSSmartCardCreds_t** pret);

size_t ber_sizeof_nla_TSRemoteGuardPackageCred_content(const TSRemoteGuardPackageCred_t* item);
size_t ber_sizeof_nla_TSRemoteGuardPackageCred(const TSRemoteGuardPackageCred_t* item);
size_t ber_sizeof_contextual_nla_TSRemoteGuardPackageCred(const TSRemoteGuardPackageCred_t* item);
void nla_TSRemoteGuardPackageCred_free(TSRemoteGuardPackageCred_t** pitem);
size_t ber_write_nla_TSRemoteGuardPackageCred(wStream* s, const TSRemoteGuardPackageCred_t* item);
size_t ber_write_contextual_nla_TSRemoteGuardPackageCred(wStream* s, BYTE tag,
                                                         const TSRemoteGuardPackageCred_t* item);
BOOL ber_read_nla_TSRemoteGuardPackageCred(wStream* s, TSRemoteGuardPackageCred_t** pret);
size_t ber_sizeof_nla_TSRemoteGuardPackageCred_array_content(const TSRemoteGuardPackageCred_t* item,
                                                             size_t nitems);
size_t ber_sizeof_nla_TSRemoteGuardPackageCred_array(const TSRemoteGuardPackageCred_t* item,
                                                     size_t nitems);
size_t
ber_sizeof_contextual_nla_TSRemoteGuardPackageCred_array(const TSRemoteGuardPackageCred_t* item,
                                                         size_t nitems);
size_t ber_write_nla_TSRemoteGuardPackageCred_array(wStream* s,
                                                    const TSRemoteGuardPackageCred_t* item,
                                                    size_t nitems);
size_t ber_write_contextual_nla_TSRemoteGuardPackageCred_array(
    wStream* s, BYTE tag, const TSRemoteGuardPackageCred_t* item, size_t nitems);
BOOL ber_read_nla_TSRemoteGuardPackageCred_array(wStream* s, TSRemoteGuardPackageCred_t** item,
                                                 size_t* nitems);

size_t ber_sizeof_nla_TSRemoteGuardCreds_content(const TSRemoteGuardCreds_t* item);
size_t ber_sizeof_nla_TSRemoteGuardCreds(const TSRemoteGuardCreds_t* item);
size_t ber_sizeof_contextual_nla_TSRemoteGuardCreds(const TSRemoteGuardCreds_t* item);
void nla_TSRemoteGuardCreds_free(TSRemoteGuardCreds_t** pitem);
size_t ber_write_nla_TSRemoteGuardCreds(wStream* s, const TSRemoteGuardCreds_t* item);
size_t ber_write_contextual_nla_TSRemoteGuardCreds(wStream* s, BYTE tag,
                                                   const TSRemoteGuardCreds_t* item);
BOOL ber_read_nla_TSRemoteGuardCreds(wStream* s, TSRemoteGuardCreds_t** pret);

#endif /* LIBFREERDP_CORE_CREDSSP_ASN1_H */
