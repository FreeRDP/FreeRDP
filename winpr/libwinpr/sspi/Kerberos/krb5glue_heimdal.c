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

#ifdef WITH_KRB5_HEIMDAL

#include <winpr/endian.h>
#include <winpr/wlog.h>
#include <winpr/assert.h>
#include "krb5glue.h"

void krb5glue_keys_free(krb5_context ctx, struct krb5glue_keyset* keyset)
{
	if (!ctx || !keyset)
		return;
	if (keyset->session_key)
		krb5_crypto_destroy(ctx, keyset->session_key);
	if (keyset->initiator_key)
		krb5_crypto_destroy(ctx, keyset->initiator_key);
	if (keyset->acceptor_key)
		krb5_crypto_destroy(ctx, keyset->acceptor_key);
}

krb5_error_code krb5glue_update_keyset(krb5_context ctx, krb5_auth_context auth_ctx, BOOL acceptor,
                                       struct krb5glue_keyset* keyset)
{
	krb5_keyblock* keyblock = NULL;
	krb5_error_code rv = 0;

	WINPR_ASSERT(ctx);
	WINPR_ASSERT(auth_ctx);
	WINPR_ASSERT(keyset);

	krb5glue_keys_free(ctx, keyset);

	if (!(rv = krb5_auth_con_getkey(ctx, auth_ctx, &keyblock)))
	{
		krb5_crypto_init(ctx, keyblock, ENCTYPE_NULL, &keyset->session_key);
		krb5_free_keyblock(ctx, keyblock);
		keyblock = NULL;
	}

	if (acceptor)
		rv = krb5_auth_con_getremotesubkey(ctx, auth_ctx, &keyblock);
	else
		rv = krb5_auth_con_getlocalsubkey(ctx, auth_ctx, &keyblock);

	if (!rv && keyblock)
	{
		krb5_crypto_init(ctx, keyblock, ENCTYPE_NULL, &keyset->initiator_key);
		krb5_free_keyblock(ctx, keyblock);
		keyblock = NULL;
	}

	if (acceptor)
		rv = krb5_auth_con_getlocalsubkey(ctx, auth_ctx, &keyblock);
	else
		rv = krb5_auth_con_getremotesubkey(ctx, auth_ctx, &keyblock);

	if (!rv && keyblock)
	{
		krb5_crypto_init(ctx, keyblock, ENCTYPE_NULL, &keyset->acceptor_key);
		krb5_free_keyblock(ctx, keyblock);
	}

	return rv;
}

krb5_error_code krb5glue_verify_checksum_iov(krb5_context ctx, krb5glue_key key, unsigned usage,
                                             krb5_crypto_iov* iov, unsigned int iov_size,
                                             krb5_boolean* is_valid)
{
	krb5_error_code rv = 0;

	WINPR_ASSERT(ctx);
	WINPR_ASSERT(key);
	WINPR_ASSERT(is_valid);

	rv = krb5_verify_checksum_iov(ctx, key, usage, iov, iov_size, NULL);
	*is_valid = (rv == 0);
	return rv;
}

krb5_error_code krb5glue_crypto_length(krb5_context ctx, krb5glue_key key, int type,
                                       unsigned int* size)
{
	krb5_error_code rv = 0;
	size_t s = 0;

	WINPR_ASSERT(ctx);
	WINPR_ASSERT(key);
	WINPR_ASSERT(size);

	rv = krb5_crypto_length(ctx, key, type, &s);
	*size = (UINT)s;
	return rv;
}

krb5_error_code krb5glue_log_error(krb5_context ctx, krb5_data* msg, const char* tag)
{
	krb5_error error = { 0 };
	krb5_error_code rv = 0;

	WINPR_ASSERT(ctx);
	WINPR_ASSERT(msg);
	WINPR_ASSERT(tag);

	if (!(rv = krb5_rd_error(ctx, msg, &error)))
	{
		WLog_ERR(tag, "KRB_ERROR: %" PRIx32, error.error_code);
		krb5_free_error_contents(ctx, &error);
	}
	return rv;
}

BOOL krb5glue_authenticator_validate_chksum(krb5glue_authenticator authenticator, int cksumtype,
                                            uint32_t* flags)
{
	WINPR_ASSERT(flags);

	if (!authenticator || !authenticator->cksum || authenticator->cksum->cksumtype != cksumtype ||
	    authenticator->cksum->checksum.length < 24)
		return FALSE;
	Data_Read_UINT32((authenticator->cksum->checksum.data + 20), (*flags));
	return TRUE;
}

krb5_error_code krb5glue_get_init_creds(krb5_context ctx, krb5_principal princ, krb5_ccache ccache,
                                        krb5_prompter_fct prompter, char* password,
                                        SEC_WINPR_KERBEROS_SETTINGS* krb_settings)
{
	krb5_error_code rv = 0;
	krb5_deltat start_time = 0;
	krb5_get_init_creds_opt* gic_opt = NULL;
	krb5_init_creds_context creds_ctx = NULL;
	krb5_creds creds = { 0 };

	WINPR_ASSERT(ctx);

	do
	{
		if ((rv = krb5_get_init_creds_opt_alloc(ctx, &gic_opt)) != 0)
			break;

		krb5_get_init_creds_opt_set_forwardable(gic_opt, 0);
		krb5_get_init_creds_opt_set_proxiable(gic_opt, 0);

		if (krb_settings)
		{
			if (krb_settings->startTime)
				start_time = krb_settings->startTime;
			if (krb_settings->lifeTime)
				krb5_get_init_creds_opt_set_tkt_life(gic_opt, krb_settings->lifeTime);
			if (krb_settings->renewLifeTime)
				krb5_get_init_creds_opt_set_renew_life(gic_opt, krb_settings->renewLifeTime);
			if (krb_settings->withPac)
				krb5_get_init_creds_opt_set_pac_request(ctx, gic_opt, TRUE);
			if (krb_settings->pkinitX509Anchors || krb_settings->pkinitX509Identity)
			{
				if ((rv = krb5_get_init_creds_opt_set_pkinit(
				         ctx, gic_opt, princ, krb_settings->pkinitX509Identity,
				         krb_settings->pkinitX509Anchors, NULL, NULL, 0, prompter, password,
				         password)) != 0)
					break;
			}
		}

		if ((rv = krb5_init_creds_init(ctx, princ, prompter, password, start_time, gic_opt,
		                               &creds_ctx)) != 0)
			break;
		if ((rv = krb5_init_creds_set_password(ctx, creds_ctx, password)) != 0)
			break;
		if (krb_settings->armorCache)
		{
			krb5_ccache armor_cc = NULL;
			if ((rv = krb5_cc_resolve(ctx, krb_settings->armorCache, &armor_cc)) != 0)
				break;
			if ((rv = krb5_init_creds_set_fast_ccache(ctx, creds_ctx, armor_cc)) != 0)
				break;
			krb5_cc_close(ctx, armor_cc);
		}
		if ((rv = krb5_init_creds_get(ctx, creds_ctx)) != 0)
			break;
		if ((rv = krb5_init_creds_get_creds(ctx, creds_ctx, &creds)) != 0)
			break;
		if ((rv = krb5_cc_store_cred(ctx, ccache, &creds)) != 0)
			break;
	} while (0);

	krb5_free_cred_contents(ctx, &creds);
	krb5_init_creds_free(ctx, creds_ctx);
	krb5_get_init_creds_opt_free(ctx, gic_opt);

	return rv;
}

#endif /* WITH_KRB5_HEIMDAL */
