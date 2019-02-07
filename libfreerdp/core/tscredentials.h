#ifndef FREERDP_LIB_CORE_TSREQUEST_H
#define FREERDP_LIB_CORE_TSREQUEST_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>
#include <winpr/sspi.h>
#include <winpr/print.h>
#include <winpr/tchar.h>
#include <winpr/strlst.h>



typedef struct
{
	UINT32 KeySpec;
	char* CardName;
	char* ReaderName;
	char* ContainerName;
	char* CspName;
} csp_data_detail;

typedef struct
{
	char* Pin;
	char* UserHint;   /* OPTIONAL */
	char* DomainHint; /* OPTIONAL */
	csp_data_detail* csp_data;
} smartcard_creds;

typedef struct
{
	char*    package_name;
	unsigned credential_size;
	BYTE*    credential;
} remote_guard_package_cred;

typedef struct
{
	remote_guard_package_cred*  login_cred;
	unsigned                    supplemental_creds_count;
	remote_guard_package_cred** supplemental_creds;
} remote_guard_creds;

typedef enum
{
	credential_type_password = 1,
	credential_type_smartcard = 2,
	credential_type_remote_guard = 6,
	credential_type_default = credential_type_password,
} credential_type;

typedef struct
{
	credential_type          cred_type;
	union
	{
		SEC_WINNT_AUTH_IDENTITY* password_creds;
		smartcard_creds*         smartcard_creds;
		remote_guard_creds*      remote_guard_creds;
	} creds;
} auth_identity;


SEC_WINNT_AUTH_IDENTITY* SEC_WINNT_AUTH_IDENTITY_new(char* user,
        char* password,
        char* domain);
SEC_WINNT_AUTH_IDENTITY* SEC_WINNT_AUTH_IDENTITY_deepcopy(SEC_WINNT_AUTH_IDENTITY* original);
void SEC_WINNT_AUTH_IDENTITY_free(SEC_WINNT_AUTH_IDENTITY* password_creds);

csp_data_detail* csp_data_detail_new_nocopy(UINT32 KeySpec,
        char* CardName,
        char* ReaderName,
        char* ContainerName,
        char* CspName);
csp_data_detail* csp_data_detail_new(UINT32 KeySpec,
                                     char* cardname,
                                     char* readername,
                                     char* containername,
                                     char* cspname);
csp_data_detail* csp_data_detail_deepcopy(csp_data_detail* original);
void csp_data_detail_free(csp_data_detail* csp);

smartcard_creds* smartcard_creds_new_nocopy(char* Pin, char* UserHint, char* DomainHint,
        csp_data_detail* csp_data);
smartcard_creds* smartcard_creds_new(char* pin, char* userhint, char* domainhint,
                                     csp_data_detail* cspdata);
smartcard_creds* smartcard_creds_deepcopy(smartcard_creds* original);
void smartcard_creds_free(smartcard_creds* that);

remote_guard_package_cred* remote_guard_package_cred_new_nocopy(char* package_name,
        unsigned credential_size,
        BYTE*    credential);
remote_guard_package_cred* remote_guard_package_cred_deepcopy(remote_guard_package_cred* that);
void remote_guard_package_cred_deepfree(remote_guard_package_cred* that);

remote_guard_creds* remote_guard_creds_new_logon_cred(remote_guard_package_cred* logon_cred);
remote_guard_creds* remote_guard_creds_new_nocopy(char* package_name,
        unsigned credenial_size,
        BYTE*    credential);
remote_guard_creds* remote_guard_creds_deepcopy(remote_guard_creds* that);
void remote_guard_creds_add_supplemental_cred(remote_guard_creds* that,
        remote_guard_package_cred* supplemental_cred);
void remote_guard_creds_free(remote_guard_creds* that);

auth_identity* auth_identity_new_password(SEC_WINNT_AUTH_IDENTITY* password_creds);
auth_identity* auth_identity_new_smartcard(smartcard_creds* smartcard_creds);
auth_identity* auth_identity_new_remote_guard(remote_guard_creds* remote_guard_creds);
auth_identity* auth_identity_deepcopy(auth_identity* that);
void auth_identity_free(auth_identity* that);
const char* auth_identity_credential_type_label(auth_identity* that);

size_t nla_sizeof_ts_creds(auth_identity* identity);
size_t nla_write_ts_creds(auth_identity* identity, wStream* s);
size_t nla_sizeof_ts_credentials_inner(auth_identity* identity);
size_t nla_sizeof_ts_credentials(auth_identity* identity);
size_t nla_write_ts_credentials(auth_identity* identity, wStream* s);
auth_identity* nla_read_ts_credentials(PSecBuffer ts_credentials);

#define ber_sizeof_sequence_octet_string(length) \
	(ber_sizeof_contextual_tag(ber_sizeof_octet_string(length)) \
	 + ber_sizeof_octet_string(length))

#define ber_write_sequence_octet_string(stream, context, value, length) \
	(ber_write_contextual_tag(stream, context, ber_sizeof_octet_string(length), TRUE) \
	 + ber_write_octet_string(stream, value, length))

/* exported for tests */
size_t nla_sizeof_ts_password_creds_inner(SEC_WINNT_AUTH_IDENTITY* password_creds);
size_t nla_sizeof_ts_password_creds(SEC_WINNT_AUTH_IDENTITY* password_creds);
size_t nla_sizeof_ts_cspdatadetail_inner(csp_data_detail* csp_data);
size_t nla_sizeof_ts_cspdatadetail(csp_data_detail*  csp_data);
size_t nla_sizeof_ts_smartcard_creds_inner(smartcard_creds* smartcard_creds);
size_t nla_sizeof_ts_smartcard_creds(smartcard_creds* smartcard_creds);
size_t nla_sizeof_ts_remote_guard_package_cred_inner(remote_guard_package_cred* package_cred);
size_t nla_sizeof_ts_remote_guard_package_cred(remote_guard_package_cred* package_cred);
size_t nla_sizeof_ts_remote_guard_creds_inner(remote_guard_creds* remote_guard_creds);
size_t nla_sizeof_ts_remote_guard_creds(remote_guard_creds* remote_guard_creds);

size_t nla_write_ts_password_creds(SEC_WINNT_AUTH_IDENTITY* password_creds, wStream* s);
size_t nla_write_ts_csp_data_detail(csp_data_detail* csp_data, int contextual_tag,  wStream* s);
size_t nla_write_ts_smartcard_creds(smartcard_creds* smartcard_creds, wStream* s);
size_t nla_write_ts_remote_guard_package_cred(remote_guard_package_cred* package_cred, wStream* s);
size_t nla_write_ts_remote_guard_creds(remote_guard_creds*  remote_guard_creds, wStream* s);

#endif
