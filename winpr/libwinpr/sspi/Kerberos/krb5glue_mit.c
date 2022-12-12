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

#ifdef WITH_KRB5_MIT

#include <winpr/path.h>
#include <winpr/wlog.h>
#include <winpr/endian.h>
#include <winpr/crypto.h>
#include <winpr/print.h>
#include <winpr/assert.h>
#include <errno.h>
#include "krb5glue.h"
#include <profile.h>

static char* create_temporary_file(void)
{
	BYTE buffer[32];
	char* hex;
	char* path;

	winpr_RAND(buffer, sizeof(buffer));
	hex = winpr_BinToHexString(buffer, sizeof(buffer), FALSE);
	path = GetKnownSubPath(KNOWN_PATH_TEMP, hex);
	free(hex);
	return path;
}

void krb5glue_keys_free(krb5_context ctx, struct krb5glue_keyset* keyset)
{
	WINPR_ASSERT(ctx);
	WINPR_ASSERT(keyset);

	krb5_k_free_key(ctx, keyset->session_key);
	krb5_k_free_key(ctx, keyset->initiator_key);
	krb5_k_free_key(ctx, keyset->acceptor_key);
}

krb5_error_code krb5glue_update_keyset(krb5_context ctx, krb5_auth_context auth_ctx, BOOL acceptor,
                                       struct krb5glue_keyset* keyset)
{
	WINPR_ASSERT(ctx);
	WINPR_ASSERT(auth_ctx);
	WINPR_ASSERT(keyset);

	krb5glue_keys_free(ctx, keyset);
	krb5_auth_con_getkey_k(ctx, auth_ctx, &keyset->session_key);
	if (acceptor)
	{
		krb5_auth_con_getsendsubkey_k(ctx, auth_ctx, &keyset->acceptor_key);
		krb5_auth_con_getrecvsubkey_k(ctx, auth_ctx, &keyset->initiator_key);
	}
	else
	{
		krb5_auth_con_getsendsubkey_k(ctx, auth_ctx, &keyset->initiator_key);
		krb5_auth_con_getrecvsubkey_k(ctx, auth_ctx, &keyset->acceptor_key);
	}
	return 0;
}

krb5_prompt_type krb5glue_get_prompt_type(krb5_context ctx, krb5_prompt prompts[], int index)
{
	WINPR_ASSERT(ctx);
	WINPR_ASSERT(prompts);

	krb5_prompt_type* types = krb5_get_prompt_types(ctx);
	return types ? types[index] : 0;
}

krb5_error_code krb5glue_log_error(krb5_context ctx, krb5_data* msg, const char* tag)
{
	krb5_error* error = NULL;
	krb5_error_code rv = 0;

	WINPR_ASSERT(ctx);
	WINPR_ASSERT(msg);
	WINPR_ASSERT(tag);

	if (!(rv = krb5_rd_error(ctx, msg, &error)))
	{
		WLog_ERR(tag, "KRB_ERROR: %s", error->text.data);
		krb5_free_error(ctx, error);
	}

	return rv;
}

BOOL krb5glue_authenticator_validate_chksum(krb5glue_authenticator authenticator, int cksumtype,
                                            uint32_t* flags)
{
	WINPR_ASSERT(flags);

	if (!authenticator || !authenticator->checksum ||
	    authenticator->checksum->checksum_type != cksumtype || authenticator->checksum->length < 24)
		return FALSE;
	Data_Read_UINT32((authenticator->checksum->contents + 20), (*flags));
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
	char* tmp_profile_path = create_temporary_file();
	profile_t profile = NULL;

	WINPR_ASSERT(ctx);

	krb5_get_init_creds_opt_alloc(ctx, &gic_opt);

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
		if (krb_settings->armorCache)
			krb5_get_init_creds_opt_set_fast_ccache_name(ctx, gic_opt, krb_settings->armorCache);
		if (krb_settings->pkinitX509Identity)
			krb5_get_init_creds_opt_set_pa(ctx, gic_opt, "X509_user_identity",
			                               krb_settings->pkinitX509Identity);
		if (krb_settings->pkinitX509Anchors)
			krb5_get_init_creds_opt_set_pa(ctx, gic_opt, "X509_anchors",
			                               krb_settings->pkinitX509Anchors);
		if (krb_settings->kdcUrl)
		{
			const char* names[4] = { 0 };
			char* realm = NULL;

			if ((rv = krb5_get_profile(ctx, &profile)))
				goto cleanup;

			names[0] = "realms";
			realm = calloc(princ->realm.length + 1, 1);
			if (!realm)
			{
				rv = ENOMEM;
				goto cleanup;
			}
			CopyMemory(realm, princ->realm.data, princ->realm.length);
			names[1] = realm;
			names[2] = "kdc";

			profile_clear_relation(profile, names);
			profile_add_relation(profile, names, krb_settings->kdcUrl);

			free(realm);

			if ((rv = profile_flush_to_file(profile, tmp_profile_path)))
				goto cleanup;

			profile_release(profile);
			profile = NULL;
			if ((rv = profile_init_path(tmp_profile_path, &profile)))
				goto cleanup;

			if ((rv = krb5_init_context_profile(profile, 0, &ctx)))
				goto cleanup;
		}
	}

	if (rv == 0)
		rv = krb5_get_init_creds_opt_set_in_ccache(ctx, gic_opt, ccache);
	if (rv == 0)
		rv = krb5_get_init_creds_opt_set_out_ccache(ctx, gic_opt, ccache);

	if (rv == 0)
		rv = krb5_init_creds_init(ctx, princ, prompter, password, start_time, gic_opt, &creds_ctx);
	if (rv == 0)
		rv = krb5_init_creds_get(ctx, creds_ctx);

cleanup:
	krb5_init_creds_free(ctx, creds_ctx);
#ifdef HAVE_AT_LEAST_KRB_V1_13
	krb5_get_init_creds_opt_free(ctx, gic_opt);
#endif
	winpr_DeleteFile(tmp_profile_path);
	free(tmp_profile_path);

	return rv;
}

#endif /* WITH_KRB5_MIT */
