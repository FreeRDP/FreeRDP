/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* clients/kinit/kinit.c - Initialize a credential cache */
/*
 * Copyright 1990, 2008 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */


//  #include "autoconf.h"
//  #include <k5-int.h>
// #include "k5-platform.h"        /* For asprintf and getopt */
#include <krb5.h>
#include <locale.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <com_err.h>

#include <freerdp/log.h>
#include <freerdp/settings.h>
#include <winpr/strlst.h>

#include "extern.h"
#include "kinit.h"

#define TAG CLIENT_TAG("kinit")

#define _(s) s

krb5_error_code k5_kt_get_principal(krb5_context context, krb5_keytab keytab,
                                    krb5_principal* princ_out);

krb5_error_code
krb5int_copy_data_contents_add0(krb5_context, const krb5_data*, krb5_data*);

/* Return the delta between two timestamps (a - b) as a signed 32-bit value,
 * without relying on undefined behavior. */
static inline krb5_deltat
ts_delta(krb5_timestamp a, krb5_timestamp b)
{
	return (krb5_deltat)((uint32_t)a - (uint32_t)b);
}

/* Copy a data structure, with fresh allocation. */
krb5_error_code KRB5_CALLCONV
krb5_copy_data_add0(krb5_context context, const krb5_data* indata, krb5_data** outdata)
{
	krb5_data* tempdata;
	krb5_error_code retval;

	if (!indata)
	{
		*outdata = 0;
		return 0;
	}

	if (!(tempdata = (krb5_data*)malloc(sizeof(*tempdata))))
		return ENOMEM;

	retval = krb5int_copy_data_contents_add0(context, indata, tempdata);

	if (retval)
	{
		krb5_free_data_contents(context, tempdata);
		return retval;
	}

	*outdata = tempdata;
	return 0;
}



#ifdef HAVE_PWD_H
#include <pwd.h>
static char* get_name_from_os()
{
	struct passwd* pw;
	pw = getpwuid(getuid());
	return (pw != NULL) ? pw->pw_name : NULL;
}
#else /* HAVE_PWD_H */
#ifdef _WIN32
static char* get_name_from_os()
{
	static char name[1024];
	DWORD name_size = sizeof(name);

	if (GetUserName(name, &name_size))
	{
		name[sizeof(name) - 1] = '\0'; /* Just to be extra safe */
		return name;
	}
	else
	{
		return NULL;
	}
}
#else /* _WIN32 */
static char* get_name_from_os()
{
	return NULL;
}
#endif /* _WIN32 */
#endif /* HAVE_PWD_H */


typedef enum { INIT_PW, INIT_KT, RENEW, VALIDATE } action_type;

struct k_opts
{
	/* In seconds */
	krb5_deltat starttime;
	krb5_deltat lifetime;
	krb5_deltat rlife;

	int forwardable;
	int proxiable;
	int request_pac;
	int anonymous;
	int addresses;

	int not_forwardable;
	int not_proxiable;
	int not_request_pac;
	int no_addresses;

	int verbose;

	char* principal_name;
	char* service_name;
	char* keytab_name;
	char* k5_in_cache_name;
	char* k5_out_cache_name;
	char* armor_ccache;

	action_type action;
	int use_client_keytab;

	int num_pa_opts;
	krb5_gic_opt_pa_data* pa_opts;

	int canonicalize;
	int enterprise;

	/* output */
	krb5_data* outdata;
};

void pa_opts_free(int num_pa_opts, krb5_gic_opt_pa_data* pa_opts)
{
	int i;

	for (i = 0; i < num_pa_opts; i ++)
	{
		free(pa_opts->attr);
		free(pa_opts->value);
	}

	free(pa_opts);
}

void k_opts_free_fields(struct k_opts* opts)
{
	free(opts->principal_name);
	free(opts->service_name);
	free(opts->keytab_name);
	free(opts->k5_in_cache_name);
	free(opts->k5_out_cache_name);
	free(opts->armor_ccache);
	pa_opts_free(opts->num_pa_opts, opts->pa_opts);
	memset(opts, 0, sizeof(* opts));
}

struct k5_data
{
	krb5_context ctx;
	krb5_ccache in_cc, out_cc;
	krb5_principal me;
	char* name;
	krb5_boolean switch_to_cache;
};

#define CHECK_MEMORY(pointer, error_action)                             \
	do                                                                  \
	{                                                                   \
		if (!(pointer))                                                 \
		{                                                               \
			WLog_ERR(TAG, "%s:%d: out of memory",                       \
			         __FUNCTION__, __LINE__);                                \
			error_action;                                               \
		}                                                               \
	}while (0)

#define CHECK_STRING_PRESENT(string, name, error_action)                \
	do                                                                  \
	{                                                                   \
		if((string) == NULL)                                            \
		{                                                               \
			WLog_ERR(TAG, "Missing %s", name);                          \
			error_action;                                               \
		}                                                               \
	}while (0)

static krb5_context errctx;
static void extended_com_err_fn(const char* myprog, errcode_t code, const char* fmt,
                                va_list args)
{
	const char* emsg;
	char*  buffer = NULL;
	int size = 0;
	emsg = krb5_get_error_message(errctx, code);
	WLog_ERR(myprog, "%s", emsg);
	krb5_free_error_message(errctx, emsg);
	size = vsnprintf(NULL, 0, fmt, args);
	CHECK_MEMORY(buffer = malloc(1 + size), return);
	size = vsnprintf(buffer, 1 + size, fmt, args);
	WLog_ERR(myprog, "%s", buffer);
}

static int add_preauth_opt(struct k_opts* opts, char* attribute, char* value)
{
	size_t newsize = (opts->num_pa_opts + 1) * sizeof(*opts->pa_opts);
	krb5_gic_opt_pa_data* p;
	krb5_gic_opt_pa_data* x = realloc(opts->pa_opts, newsize);

	if (x == NULL)
	{
		return ENOMEM;
	}

	opts->pa_opts = x;
	p = &opts->pa_opts[opts->num_pa_opts];
	p->attr = attribute;
	p->value = value;
	opts->num_pa_opts++;
	return 0;
}

static int k5_begin(struct k_opts* opts, struct k5_data* k5)
{
	krb5_error_code ret;
	int success = 0;
	int flags = opts->enterprise ? KRB5_PRINCIPAL_PARSE_ENTERPRISE : 0;
	krb5_ccache defcache = NULL;
	krb5_principal defcache_princ = NULL, princ;
	krb5_keytab keytab;
	const char* deftype = NULL;
	char* defrealm, *name;
	ret = krb5_init_context(&k5->ctx);

	if (ret)
	{
		com_err(TAG, ret, _("while initializing Kerberos 5 library"));
		return 0;
	}

	errctx = k5->ctx;

	if (opts->k5_out_cache_name)
	{
		ret = krb5_cc_resolve(k5->ctx, opts->k5_out_cache_name, &k5->out_cc);

		if (ret)
		{
			com_err(TAG, ret, _("resolving ccache %s"),
			        opts->k5_out_cache_name);
			goto cleanup;
		}

		if (opts->verbose)
		{
			fprintf(stderr, _("Using specified cache: %s\n"),
			        opts->k5_out_cache_name);
		}
	}
	else
	{
		/* Resolve the default ccache and get its type and default principal
		 * (if it is initialized). */
		ret = krb5_cc_default(k5->ctx, &defcache);

		if (ret)
		{
			com_err(TAG, ret, _("while getting default ccache"));
			goto cleanup;
		}

		deftype = krb5_cc_get_type(k5->ctx, defcache);

		if (krb5_cc_get_principal(k5->ctx, defcache, &defcache_princ) != 0)
			defcache_princ = NULL;
	}

	/* Choose a client principal name. */
	if (opts->principal_name != NULL)
	{
		/* Use the specified principal name. */
		ret = krb5_parse_name_flags(k5->ctx, opts->principal_name, flags,
		                            &k5->me);

		if (ret)
		{
			com_err(TAG, ret, _("when parsing name %s"),
			        opts->principal_name);
			goto cleanup;
		}
	}
	else if (opts->anonymous)
	{
		/* Use the anonymous principal for the local realm. */
		ret = krb5_get_default_realm(k5->ctx, &defrealm);

		if (ret)
		{
			com_err(TAG, ret, _("while getting default realm"));
			goto cleanup;
		}

		ret = krb5_build_principal_ext(k5->ctx, &k5->me,
		                               strlen(defrealm), defrealm,
		                               strlen(KRB5_WELLKNOWN_NAMESTR),
		                               KRB5_WELLKNOWN_NAMESTR,
		                               strlen(KRB5_ANONYMOUS_PRINCSTR),
		                               KRB5_ANONYMOUS_PRINCSTR, 0);
		krb5_free_default_realm(k5->ctx, defrealm);

		if (ret)
		{
			com_err(TAG, ret, _("while building principal"));
			goto cleanup;
		}
	}
	else if (opts->action == INIT_KT && opts->use_client_keytab)
	{
		/* Use the first entry from the client keytab. */
		ret = krb5_kt_client_default(k5->ctx, &keytab);

		if (ret)
		{
			com_err(TAG, ret,
			        _("When resolving the default client keytab"));
			goto cleanup;
		}

		ret = k5_kt_get_principal(k5->ctx, keytab, &k5->me);
		krb5_kt_close(k5->ctx, keytab);

		if (ret)
		{
			com_err(TAG, ret,
			        _("When determining client principal name from keytab"));
			goto cleanup;
		}
	}
	else if (opts->action == INIT_KT)
	{
		/* Use the default host/service name. */
		ret = krb5_sname_to_principal(k5->ctx, NULL, NULL, KRB5_NT_SRV_HST,
		                              &k5->me);

		if (ret)
		{
			com_err(TAG, ret,
			        _("when creating default server principal name"));
			goto cleanup;
		}

		if (k5->me->realm.data[0] == 0)
		{
			ret = krb5_unparse_name(k5->ctx, k5->me, &k5->name);

			if (ret == 0)
			{
				com_err(TAG, KRB5_ERR_HOST_REALM_UNKNOWN,
				        _("(principal %s)"), k5->name);
			}
			else
			{
				com_err(TAG, KRB5_ERR_HOST_REALM_UNKNOWN,
				        _("for local services"));
			}

			goto cleanup;
		}
	}
	else if (k5->out_cc != NULL)
	{
		/* If the output ccache is initialized, use its principal. */
		if (krb5_cc_get_principal(k5->ctx, k5->out_cc, &princ) == 0)
			k5->me = princ;
	}
	else if (defcache_princ != NULL)
	{
		/* Use the default cache's principal, and use the default cache as the
		 * output cache. */
		k5->out_cc = defcache;
		defcache = NULL;
		k5->me = defcache_princ;
		defcache_princ = NULL;
	}

	/* If we still haven't chosen, use the local username. */
	if (k5->me == NULL)
	{
		name = get_name_from_os();

		if (name == NULL)
		{
			fprintf(stderr, _("Unable to identify user\n"));
			goto cleanup;
		}

		ret = krb5_parse_name_flags(k5->ctx, name, flags, &k5->me);

		if (ret)
		{
			com_err(TAG, ret, _("when parsing name %s"), name);
			goto cleanup;
		}
	}

	if (k5->out_cc == NULL && krb5_cc_support_switch(k5->ctx, deftype))
	{
		/* Use an existing cache for the client principal if we can. */
		ret = krb5_cc_cache_match(k5->ctx, k5->me, &k5->out_cc);

		if (ret && ret != KRB5_CC_NOTFOUND)
		{
			com_err(TAG, ret, _("while searching for ccache for %s"),
			        opts->principal_name);
			goto cleanup;
		}

		if (!ret)
		{
			if (opts->verbose)
			{
				fprintf(stderr, _("Using existing cache: %s\n"),
				        krb5_cc_get_name(k5->ctx, k5->out_cc));
			}

			k5->switch_to_cache = 1;
		}
		else if (defcache_princ != NULL)
		{
			/* Create a new cache to avoid overwriting the initialized default
			 * cache. */
			ret = krb5_cc_new_unique(k5->ctx, deftype, NULL, &k5->out_cc);

			if (ret)
			{
				com_err(TAG, ret, _("while generating new ccache"));
				goto cleanup;
			}

			if (opts->verbose)
			{
				fprintf(stderr, _("Using new cache: %s\n"),
				        krb5_cc_get_name(k5->ctx, k5->out_cc));
			}

			k5->switch_to_cache = 1;
		}
	}

	/* Use the default cache if we haven't picked one yet. */
	if (k5->out_cc == NULL)
	{
		k5->out_cc = defcache;
		defcache = NULL;

		if (opts->verbose)
		{
			fprintf(stderr, _("Using default cache: %s\n"),
			        krb5_cc_get_name(k5->ctx, k5->out_cc));
		}
	}

	if (opts->k5_in_cache_name)
	{
		ret = krb5_cc_resolve(k5->ctx, opts->k5_in_cache_name, &k5->in_cc);

		if (ret)
		{
			com_err(TAG, ret, _("resolving ccache %s"),
			        opts->k5_in_cache_name);
			goto cleanup;
		}

		if (opts->verbose)
		{
			fprintf(stderr, _("Using specified input cache: %s\n"),
			        opts->k5_in_cache_name);
		}
	}

	ret = krb5_unparse_name(k5->ctx, k5->me, &k5->name);

	if (ret)
	{
		com_err(TAG, ret, _("when unparsing name"));
		goto cleanup;
	}

	if (opts->verbose)
		fprintf(stderr, _("Using principal: %s\n"), k5->name);

	opts->principal_name = k5->name;
	success = 1;
cleanup:

	if (defcache != NULL)
		krb5_cc_close(k5->ctx, defcache);

	krb5_free_principal(k5->ctx, defcache_princ);
	return success;
}

static void k5_end(struct k5_data* k5)
{
	krb5_free_unparsed_name(k5->ctx, k5->name);
	krb5_free_principal(k5->ctx, k5->me);

	if (k5->in_cc != NULL)
		krb5_cc_close(k5->ctx, k5->in_cc);

	if (k5->out_cc != NULL)
		krb5_cc_close(k5->ctx, k5->out_cc);

	krb5_free_context(k5->ctx);
	errctx = NULL;
	memset(k5, 0, sizeof(*k5));
}

static krb5_error_code KRB5_CALLCONV
kinit_prompter(krb5_context ctx, void* data, const char* name,
               const char* banner, int num_prompts, krb5_prompt prompts[])
{
	krb5_boolean* pwprompt = data;
	krb5_prompt_type* ptypes;
	int i;
	/* Make a note if we receive a password prompt. */
	ptypes = krb5_get_prompt_types(ctx);

	for (i = 0; i < num_prompts; i++)
	{
		if (ptypes != NULL && ptypes[i] == KRB5_PROMPT_TYPE_PASSWORD)
			*pwprompt = TRUE;
	}

	return krb5_prompter_posix(ctx, data, name, banner, num_prompts, prompts);
}

static int
k5_kinit(struct k_opts* opts, struct k5_data* k5)
{
	int notix = 1;
	krb5_keytab keytab = 0;
	krb5_creds my_creds;
	krb5_error_code ret;
	krb5_get_init_creds_opt* options = NULL;
	krb5_boolean pwprompt = FALSE;
	krb5_address** addresses = NULL;
	int i;
	memset(&my_creds, 0, sizeof(my_creds));
	ret = krb5_get_init_creds_opt_alloc(k5->ctx, &options);

	if (ret)
		goto cleanup;

	if (opts->lifetime)
		krb5_get_init_creds_opt_set_tkt_life(options, opts->lifetime);

	if (opts->rlife)
		krb5_get_init_creds_opt_set_renew_life(options, opts->rlife);

	if (opts->forwardable)
		krb5_get_init_creds_opt_set_forwardable(options, 1);

	if (opts->not_forwardable)
		krb5_get_init_creds_opt_set_forwardable(options, 0);

	if (opts->proxiable)
		krb5_get_init_creds_opt_set_proxiable(options, 1);

	if (opts->not_proxiable)
		krb5_get_init_creds_opt_set_proxiable(options, 0);

	if (opts->canonicalize)
		krb5_get_init_creds_opt_set_canonicalize(options, 1);

	if (opts->anonymous)
		krb5_get_init_creds_opt_set_anonymous(options, 1);

	if (opts->addresses)
	{
		ret = krb5_os_localaddr(k5->ctx, &addresses);

		if (ret)
		{
			com_err(TAG, ret, _("getting local addresses"));
			goto cleanup;
		}

		krb5_get_init_creds_opt_set_address_list(options, addresses);
	}

	if (opts->no_addresses)
		krb5_get_init_creds_opt_set_address_list(options, NULL);

	if (opts->armor_ccache != NULL)
	{
		krb5_get_init_creds_opt_set_fast_ccache_name(k5->ctx, options,
		        opts->armor_ccache);
	}

	if (opts->request_pac)
		krb5_get_init_creds_opt_set_pac_request(k5->ctx, options, TRUE);

	if (opts->not_request_pac)
		krb5_get_init_creds_opt_set_pac_request(k5->ctx, options, FALSE);

	if (opts->action == INIT_KT && opts->keytab_name != NULL)
	{
#ifndef _WIN32

		if (strncmp(opts->keytab_name, "KDB:", 4) == 0)
		{
			ret = kinit_kdb_init(&k5->ctx, k5->me->realm.data);

			if (ret)
			{
				com_err(TAG, ret,
				        _("while setting up KDB keytab for realm %s"),
				        k5->me->realm.data);
				goto cleanup;
			}
		}

#endif
		ret = krb5_kt_resolve(k5->ctx, opts->keytab_name, &keytab);

		if (ret)
		{
			com_err(TAG, ret, _("resolving keytab %s"),
			        opts->keytab_name);
			goto cleanup;
		}

		if (opts->verbose)
			fprintf(stderr, _("Using keytab: %s\n"), opts->keytab_name);
	}
	else if (opts->action == INIT_KT && opts->use_client_keytab)
	{
		ret = krb5_kt_client_default(k5->ctx, &keytab);

		if (ret)
		{
			com_err(TAG, ret, _("resolving default client keytab"));
			goto cleanup;
		}
	}

	for (i = 0; i < opts->num_pa_opts; i++)
	{
		ret = krb5_get_init_creds_opt_set_pa(k5->ctx, options,
		                                     opts->pa_opts[i].attr,
		                                     opts->pa_opts[i].value);

		if (ret)
		{
			com_err(TAG, ret, _("while setting '%s'='%s'"),
			        opts->pa_opts[i].attr, opts->pa_opts[i].value);
			goto cleanup;
		}

		if (opts->verbose)
		{
			fprintf(stderr, _("PA Option %s = %s\n"), opts->pa_opts[i].attr,
			        opts->pa_opts[i].value);
		}
	}

	if (k5->in_cc)
	{
		ret = krb5_get_init_creds_opt_set_in_ccache(k5->ctx, options,
		        k5->in_cc);

		if (ret)
			goto cleanup;
	}

	ret = krb5_get_init_creds_opt_set_out_ccache(k5->ctx, options, k5->out_cc);

	if (ret)
		goto cleanup;

	switch (opts->action)
	{
		case INIT_PW:
			ret = krb5_get_init_creds_password(k5->ctx, &my_creds, k5->me, 0,
			                                   kinit_prompter, &pwprompt,
			                                   opts->starttime, opts->service_name,
			                                   options);
			break;

		case INIT_KT:
			ret = krb5_get_init_creds_keytab(k5->ctx, &my_creds, k5->me, keytab,
			                                 opts->starttime, opts->service_name,
			                                 options);
			break;

		case VALIDATE:
			ret = krb5_get_validated_creds(k5->ctx, &my_creds, k5->me, k5->out_cc,
			                               opts->service_name);
			break;

		case RENEW:
			ret = krb5_get_renewed_creds(k5->ctx, &my_creds, k5->me, k5->out_cc,
			                             opts->service_name);
			break;
	}

	if (ret)
	{
		char* doing = NULL;

		switch (opts->action)
		{
			case INIT_PW:
			case INIT_KT:
				doing = _("getting initial credentials");
				break;

			case VALIDATE:
				doing = _("validating credentials");
				break;

			case RENEW:
				doing = _("renewing credentials");
				break;
		}

		/* If reply decryption failed, or if pre-authentication failed and we
		 * were prompted for a password, assume the password was wrong. */
		if (ret == KRB5KRB_AP_ERR_BAD_INTEGRITY ||
		    (pwprompt && ret == KRB5KDC_ERR_PREAUTH_FAILED))
		{
			fprintf(stderr, _("%s: Password incorrect while %s\n"), TAG,
			        doing);
		}
		else
		{
			com_err(TAG, ret, _("while %s"), doing);
		}

		goto cleanup;
	}

	if (opts->action != INIT_PW && opts->action != INIT_KT)
	{
		ret = krb5_cc_initialize(k5->ctx, k5->out_cc, opts->canonicalize ?
		                         my_creds.client : k5->me);

		if (ret)
		{
			com_err(TAG, ret, _("when initializing cache %s"),
			        opts->k5_out_cache_name ? opts->k5_out_cache_name : "");
			goto cleanup;
		}

		if (opts->verbose)
			fprintf(stderr, _("Initialized cache\n"));

		ret = krb5_cc_store_cred(k5->ctx, k5->out_cc, &my_creds);

		if (ret)
		{
			com_err(TAG, ret, _("while storing credentials"));
			goto cleanup;
		}

		if (opts->verbose)
			fprintf(stderr, _("Stored credentials\n"));
	}

	/* Get canonicalized principal name for credentials delegation (CredSSP) */
	krb5_copy_data_add0(k5->ctx, my_creds.client->data, &(opts->outdata));
	notix = 0;

	if (k5->switch_to_cache)
	{
		ret = krb5_cc_switch(k5->ctx, k5->out_cc);

		if (ret)
		{
			com_err(TAG, ret, _("while switching to new ccache"));
			goto cleanup;
		}
	}

cleanup:
#ifndef _WIN32
	kinit_kdb_fini();
#endif

	if (options)
		krb5_get_init_creds_opt_free(k5->ctx, options);

	if (my_creds.client == k5->me)
		my_creds.client = 0;

	if (opts->pa_opts)
	{
		free(opts->pa_opts);
		opts->pa_opts = NULL;
		opts->num_pa_opts = 0;
	}

	krb5_free_cred_contents(k5->ctx, &my_creds);

	if (keytab != NULL)
		krb5_kt_close(k5->ctx, keytab);

	return notix ? 0 : 1;
}

static int kinit(struct k_opts*   opts, char** canonicalized_user)
{
	int authed_k5 = 0;
	struct k5_data k5;
	set_com_err_hook(extended_com_err_fn);
	memset(&k5, 0, sizeof(k5));

	if (k5_begin(opts, &k5))
	{
		authed_k5 = k5_kinit(opts, &k5);
	}

	if (authed_k5)
	{
		if (opts->verbose)
		{
			WLog_INFO(TAG, "Authenticated to Kerberos v5");

			if (opts->outdata->data)
			{
				(*canonicalized_user) = strdup(opts->outdata->data);

				if ((*canonicalized_user) == NULL)
				{
					WLog_ERR(TAG, "Error cannot strdup outdata into canonicalized user hint.");
					authed_k5 = FALSE;
				}

				krb5_free_data(k5.ctx, opts->outdata);
			}
		}
	}

	k5_end(&k5);
	return authed_k5 ? 0 : 1;
}

static int convert_deltat(char* timestring, krb5_deltat* deltat, int try_absolute, const char* what)
{
	if (timestring == NULL)
	{
		return 0;
	}

	if ((krb5_string_to_deltat(timestring, deltat) != 0)
	    || (*deltat == 0))
	{
		krb5_timestamp abs_starttime;

		if (!try_absolute)
		{
			goto bad;
		}

		/* Parse as an absolute time; intentionally undocumented
		* but left for backwards compatibility. */
		if ((krb5_string_to_timestamp(timestring, &abs_starttime) != 0)
		    || (abs_starttime == 0))
		{
			goto bad;
		}

		(*deltat) = ts_delta(abs_starttime, time(NULL));
	}

	return 0;
bad:
	WLog_ERR(TAG, "Bad %s option %s", what, timestring);
	return 1;
}

static int fill_opts_with_settings(rdpSettings* settings, struct k_opts*   opts)
{
	char* attribute = NULL;
	char* value = NULL;
	char** anchors = NULL;
	memset(opts, 0, sizeof(* opts));
	opts->verbose = settings->Krb5Trace;
	opts->canonicalize = 1;
	opts->enterprise = 1;
	opts->action = INIT_KT;

	if (!convert_deltat(settings->KerberosStartTime, &opts->starttime, 1, "start time") ||
	    !convert_deltat(settings->KerberosLifeTime, &opts->lifetime,  0, "life time")  ||
	    !convert_deltat(settings->KerberosRenewableLifeTime, &opts->rlife, 0, "renewable time"))
	{
		goto error;
	}

	CHECK_STRING_PRESENT(settings->UserPrincipalName, "user principal name setting", goto error);
	CHECK_STRING_PRESENT(settings->Domain,            "domain name setting",         goto error);
	CHECK_STRING_PRESENT(settings->PkinitIdentity,    "pkinit Identity setting",     goto error);
	CHECK_MEMORY(opts->principal_name = strdup(settings->UserPrincipalName),         goto error);
	CHECK_MEMORY(opts->service_name   = strdup(settings->Domain),                    goto error);
	CHECK_MEMORY(attribute            = strdup("X509_user_identity"),                goto error);
	CHECK_MEMORY(value                = strdup(settings->PkinitIdentity),            goto error);

	if (!add_preauth_opt(opts, attribute, value))
	{
		WLog_ERR(TAG, "Could not add preauth option %s = %s", attribute, value);
		goto error;
	}

	if (settings->PkinitAnchors != NULL)
	{
		anchors = string_list_split_string(settings->PkinitAnchors, ",", /* remove_empty_substring = */  1);
		int i;

		for (i = 0; anchors[i] != NULL; i++)
		{
			CHECK_MEMORY(attribute  = strdup("X509_anchors"),                         goto error);
			CHECK_MEMORY(value      = string_concatenate("FILE:", anchors[i], NULL),  goto error);

			if (!add_preauth_opt(opts, attribute, value))
			{
				WLog_ERR(TAG, "Could not add preauth option %s = %s", attribute, value);
				goto error;
			}
		}
	}

	return 0;
error:
	free(attribute);
	free(value);
	string_list_free(anchors);
	k_opts_free_fields(opts);
	return 1;
}

int kerberos_get_tgt(rdpSettings* settings)
{
	int ret = 0;
	struct k_opts  opts;

	if (fill_opts_with_settings(settings, &opts) != 0)
	{
		return 1;
	}

	ret = kinit(&opts, &settings->CanonicalizedUserHint);
	k_opts_free_fields(&opts);
	return ret;
}



/**** THE END ****/
