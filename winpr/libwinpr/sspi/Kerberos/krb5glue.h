/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Kerberos Auth Protocol
 *
 * Copyright 2022 Isaac Klein <fifthdegree@protonmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef WINPR_SSPI_KERBEROS_GLUE_PRIVATE_H
#define WINPR_SSPI_KERBEROS_GLUE_PRIVATE_H

#ifdef WITH_KRB5

#include <winpr/winpr.h>
#include <winpr/sspi.h>

#ifdef WITH_KRB5_MIT
#include <krb5/krb5.h>
typedef krb5_key krb5glue_key;
typedef krb5_authenticator* krb5glue_authenticator;

#define krb5glue_crypto_length(ctx, key, type, size) \
	krb5_c_crypto_length(ctx, krb5_k_key_enctype(ctx, key), type, size)
#define krb5glue_crypto_length_iov(ctx, key, iov, size) \
	krb5_c_crypto_length_iov(ctx, krb5_k_key_enctype(ctx, key), iov, size)
#define krb5glue_encrypt_iov(ctx, key, usage, iov, size) \
	krb5_k_encrypt_iov(ctx, key, usage, NULL, iov, size)
#define krb5glue_decrypt_iov(ctx, key, usage, iov, size) \
	krb5_k_decrypt_iov(ctx, key, usage, NULL, iov, size)
#define krb5glue_make_checksum_iov(ctx, key, usage, iov, size) \
	krb5_k_make_checksum_iov(ctx, 0, key, usage, iov, size)
#define krb5glue_verify_checksum_iov(ctx, key, usage, iov, size, is_valid) \
	krb5_k_verify_checksum_iov(ctx, 0, key, usage, iov, size, is_valid)
#define krb5glue_auth_con_set_cksumtype(ctx, auth_ctx, cksumtype) \
	krb5_auth_con_set_req_cksumtype(ctx, auth_ctx, cksumtype)
#define krb5glue_set_principal_realm(ctx, principal, realm) \
	krb5_set_principal_realm(ctx, principal, realm)
#define krb5glue_free_keytab_entry_contents(ctx, entry) krb5_free_keytab_entry_contents(ctx, entry)
#define krb5glue_auth_con_setuseruserkey(ctx, auth_ctx, keytab) \
	krb5_auth_con_setuseruserkey(ctx, auth_ctx, keytab)
#define krb5glue_free_data_contents(ctx, data) krb5_free_data_contents(ctx, data)
krb5_prompt_type krb5glue_get_prompt_type(krb5_context ctx, krb5_prompt prompts[], int index);

#define krb5glue_creds_getkey(creds) creds.keyblock

#else
#include <krb5.h>
typedef krb5_crypto krb5glue_key;
typedef krb5_authenticator krb5glue_authenticator;

krb5_error_code krb5glue_crypto_length(krb5_context ctx, krb5glue_key key, int type,
                                       unsigned int* size);
#define krb5glue_crypto_length_iov(ctx, key, iov, size) krb5_crypto_length_iov(ctx, key, iov, size)
#define krb5glue_encrypt_iov(ctx, key, usage, iov, size) \
	krb5_encrypt_iov_ivec(ctx, key, usage, iov, size, NULL)
#define krb5glue_decrypt_iov(ctx, key, usage, iov, size) \
	krb5_decrypt_iov_ivec(ctx, key, usage, iov, size, NULL)
#define krb5glue_make_checksum_iov(ctx, key, usage, iov, size) \
	krb5_create_checksum_iov(ctx, key, usage, iov, size, NULL)
krb5_error_code krb5glue_verify_checksum_iov(krb5_context ctx, krb5glue_key key, unsigned usage,
                                             krb5_crypto_iov* iov, unsigned int iov_size,
                                             krb5_boolean* is_valid);
#define krb5glue_auth_con_set_cksumtype(ctx, auth_ctx, cksumtype) \
	krb5_auth_con_setcksumtype(ctx, auth_ctx, cksumtype)
#define krb5glue_set_principal_realm(ctx, principal, realm) \
	krb5_principal_set_realm(ctx, principal, realm)
#define krb5glue_free_keytab_entry_contents(ctx, entry) krb5_kt_free_entry(ctx, entry)
#define krb5glue_auth_con_setuseruserkey(ctx, auth_ctx, keytab) \
	krb5_auth_con_setuserkey(ctx, auth_ctx, keytab)
#define krb5glue_free_data_contents(ctx, data) krb5_data_free(data)
#define krb5glue_get_prompt_type(ctx, prompts, index) prompts[index].type

#define krb5glue_creds_getkey(creds) creds.session

#endif

struct krb5glue_keyset
{
	krb5glue_key session_key;
	krb5glue_key initiator_key;
	krb5glue_key acceptor_key;
};

void krb5glue_keys_free(krb5_context ctx, struct krb5glue_keyset* keyset);
krb5_error_code krb5glue_update_keyset(krb5_context ctx, krb5_auth_context auth_ctx, BOOL acceptor,
                                       struct krb5glue_keyset* keyset);
krb5_error_code krb5glue_log_error(krb5_context ctx, krb5_data* msg, const char* tag);
BOOL krb5glue_authenticator_validate_chksum(krb5glue_authenticator authenticator, int cksumtype,
                                            uint32_t* flags);
krb5_error_code krb5glue_get_init_creds(krb5_context ctx, krb5_principal princ, krb5_ccache ccache,
                                        krb5_prompter_fct prompter, char* password,
                                        SEC_WINPR_KERBEROS_SETTINGS* krb_settings);

#endif /* WITH_KRB5 */

#endif /* WINPR_SSPI_KERBEROS_GLUE_PRIVATE_H */