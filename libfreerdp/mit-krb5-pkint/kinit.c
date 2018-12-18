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

#include "autoconf.h"
#include <k5-int.h>
#include "k5-platform.h"        /* For asprintf and getopt */
#include <krb5.h>
#include "extern.h"
#include <locale.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <com_err.h>

#ifndef _WIN32
#define GET_PROGNAME(x) (strrchr((x), '/') ? strrchr((x), '/') + 1 : (x))
#else
#define GET_PROGNAME(x) max(max(strrchr((x), '/'), strrchr((x), '\\')) + 1,(x))
#endif

#ifdef HAVE_PWD_H
#include <pwd.h>
static char *
get_name_from_os()
{
    struct passwd *pw;

    pw = getpwuid(getuid());
    return (pw != NULL) ? pw->pw_name : NULL;
}
#else /* HAVE_PWD_H */
#ifdef _WIN32
static char *
get_name_from_os()
{
    static char name[1024];
    DWORD name_size = sizeof(name);

    if (GetUserName(name, &name_size)) {
        name[sizeof(name) - 1] = '\0'; /* Just to be extra safe */
        return name;
    } else {
        return NULL;
    }
}
#else /* _WIN32 */
static char *
get_name_from_os()
{
    return NULL;
}
#endif /* _WIN32 */
#endif /* HAVE_PWD_H */

static char *progname;

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

    char *principal_name;
    char *service_name;
    char *keytab_name;
    char *k5_in_cache_name;
    char *k5_out_cache_name;
    char *armor_ccache;

    action_type action;
    int use_client_keytab;

    int num_pa_opts;
    krb5_gic_opt_pa_data *pa_opts;

    int canonicalize;
    int enterprise;
};

struct k5_data
{
    krb5_context ctx;
    krb5_ccache in_cc, out_cc;
    krb5_principal me;
    char *name;
    krb5_boolean switch_to_cache;
};

/*
 * If struct[2] == NULL, then long_getopt acts as if the short flag struct[3]
 * were specified.  If struct[2] != NULL, then struct[3] is stored in
 * *(struct[2]), the array index which was specified is stored in *index, and
 * long_getopt() returns 0.
 */
const char *shopts = "r:fpFPn54aAVl:s:c:kit:T:RS:vX:CEI:";

#define USAGE_BREAK "\n\t"

static void
usage()
{
    fprintf(stderr, "Usage: %s [-V] "
            "[-l lifetime] [-s start_time] "
            USAGE_BREAK
            "[-r renewable_life] "
            "[-f | -F | --forwardable | --noforwardable] "
            USAGE_BREAK
            "[-p | -P | --proxiable | --noproxiable] "
            USAGE_BREAK
            "-n "
            "[-a | -A | --addresses | --noaddresses] "
            USAGE_BREAK
            "[--request-pac | --no-request-pac] "
            USAGE_BREAK
            "[-C | --canonicalize] "
            USAGE_BREAK
            "[-E | --enterprise] "
            USAGE_BREAK
            "[-v] [-R] "
            "[-k [-i|-t keytab_file]] "
            "[-c cachename] "
            USAGE_BREAK
            "[-S service_name] [-T ticket_armor_cache]"
            USAGE_BREAK
            "[-X <attribute>[=<value>]] [principal]"
            "\n\n",
            progname);

    fprintf(stderr, "    options:\n");
    fprintf(stderr, _("\t-V verbose\n"));
    fprintf(stderr, _("\t-l lifetime\n"));
    fprintf(stderr, _("\t-s start time\n"));
    fprintf(stderr, _("\t-r renewable lifetime\n"));
    fprintf(stderr, _("\t-f forwardable\n"));
    fprintf(stderr, _("\t-F not forwardable\n"));
    fprintf(stderr, _("\t-p proxiable\n"));
    fprintf(stderr, _("\t-P not proxiable\n"));
    fprintf(stderr, _("\t-n anonymous\n"));
    fprintf(stderr, _("\t-a include addresses\n"));
    fprintf(stderr, _("\t-A do not include addresses\n"));
    fprintf(stderr, _("\t-v validate\n"));
    fprintf(stderr, _("\t-R renew\n"));
    fprintf(stderr, _("\t-C canonicalize\n"));
    fprintf(stderr, _("\t-E client is enterprise principal name\n"));
    fprintf(stderr, _("\t-k use keytab\n"));
    fprintf(stderr, _("\t-i use default client keytab (with -k)\n"));
    fprintf(stderr, _("\t-t filename of keytab to use\n"));
    fprintf(stderr, _("\t-c Kerberos 5 cache name\n"));
    fprintf(stderr, _("\t-S service\n"));
    fprintf(stderr, _("\t-T armor credential cache\n"));
    fprintf(stderr, _("\t-X <attribute>[=<value>]\n"));
    exit(2);
}

static krb5_context errctx;
static void
extended_com_err_fn(const char *myprog, errcode_t code, const char *fmt,
                    va_list args)
{
    const char *emsg;

    emsg = krb5_get_error_message(errctx, code);
    fprintf(stderr, "%s: %s ", myprog, emsg);
    krb5_free_error_message(errctx, emsg);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
}

static int
add_preauth_opt(struct k_opts *opts, char *av)
{
    char *sep, *v;
    krb5_gic_opt_pa_data *p, *x;
    size_t newsize = (opts->num_pa_opts + 1) * sizeof(*opts->pa_opts);

    x = realloc(opts->pa_opts, newsize);
    if (x == NULL)
        return ENOMEM;
    opts->pa_opts = x;

    p = &opts->pa_opts[opts->num_pa_opts];
    sep = strchr(av, '=');
    if (sep) {
        *sep = '\0';
        v = ++sep;
        p->value = v;
    } else {
        p->value = "yes";
    }
    p->attr = av;
    opts->num_pa_opts++;
    return 0;
}

static char *
parse_options(int argc, char **argv, struct k_opts *opts)
{
    struct option long_options[] = {
        { "noforwardable", 0, NULL, 'F' },
        { "noproxiable", 0, NULL, 'P' },
        { "addresses", 0, NULL, 'a'},
        { "forwardable", 0, NULL, 'f' },
        { "proxiable", 0, NULL, 'p' },
        { "noaddresses", 0, NULL, 'A' },
        { "canonicalize", 0, NULL, 'C' },
        { "enterprise", 0, NULL, 'E' },
        { "request-pac", 0, &opts->request_pac, 1 },
        { "no-request-pac", 0, &opts->not_request_pac, 1 },
        { NULL, 0, NULL, 0 }
    };
    krb5_error_code ret;
    int errflg = 0;
    int i;

    while ((i = getopt_long(argc, argv, shopts, long_options, 0)) != -1) {
        switch (i) {
        case 'V':
            opts->verbose = 1;
            break;
        case 'l':
            /* Lifetime */
            ret = krb5_string_to_deltat(optarg, &opts->lifetime);
            if (ret || opts->lifetime == 0) {
                fprintf(stderr, _("Bad lifetime value %s\n"), optarg);
                errflg++;
            }
            break;
        case 'r':
            /* Renewable Time */
            ret = krb5_string_to_deltat(optarg, &opts->rlife);
            if (ret || opts->rlife == 0) {
                fprintf(stderr, _("Bad lifetime value %s\n"), optarg);
                errflg++;
            }
            break;
        case 'f':
            opts->forwardable = 1;
            break;
        case 'F':
            opts->not_forwardable = 1;
            break;
        case 'p':
            opts->proxiable = 1;
            break;
        case 'P':
            opts->not_proxiable = 1;
            break;
        case 'n':
            opts->anonymous = 1;
            break;
        case 'a':
            opts->addresses = 1;
            break;
        case 'A':
            opts->no_addresses = 1;
            break;
        case 's':
            ret = krb5_string_to_deltat(optarg, &opts->starttime);
            if (ret || opts->starttime == 0) {
                /* Parse as an absolute time; intentionally undocumented
                 * but left for backwards compatibility. */
                krb5_timestamp abs_starttime;

                ret = krb5_string_to_timestamp(optarg, &abs_starttime);
                if (ret || abs_starttime == 0) {
                    fprintf(stderr, _("Bad start time value %s\n"), optarg);
                    errflg++;
                } else {
                    opts->starttime = ts_delta(abs_starttime, time(NULL));
                }
            }
            break;
        case 'S':
            opts->service_name = optarg;
            break;
        case 'k':
            opts->action = INIT_KT;
            break;
        case 'i':
            opts->use_client_keytab = 1;
            break;
        case 't':
            if (opts->keytab_name != NULL) {
                fprintf(stderr, _("Only one -t option allowed.\n"));
                errflg++;
            } else {
                opts->keytab_name = optarg;
            }
            break;
        case 'T':
            if (opts->armor_ccache != NULL) {
                fprintf(stderr, _("Only one armor_ccache\n"));
                errflg++;
            } else {
                opts->armor_ccache = optarg;
            }
            break;
        case 'R':
            opts->action = RENEW;
            break;
        case 'v':
            opts->action = VALIDATE;
            break;
        case 'c':
            if (opts->k5_out_cache_name != NULL) {
                fprintf(stderr, _("Only one -c option allowed\n"));
                errflg++;
            } else {
                opts->k5_out_cache_name = optarg;
            }
            break;
        case 'I':
            if (opts->k5_in_cache_name != NULL) {
                fprintf(stderr, _("Only one -I option allowed\n"));
                errflg++;
            } else {
                opts->k5_in_cache_name = optarg;
            }
            break;
        case 'X':
            ret = add_preauth_opt(opts, optarg);
            if (ret) {
                com_err(progname, ret, _("while adding preauth option"));
                errflg++;
            }
            break;
        case 'C':
            opts->canonicalize = 1;
            break;
        case 'E':
            opts->enterprise = 1;
            break;
        case '4':
            fprintf(stderr, _("Kerberos 4 is no longer supported\n"));
            exit(3);
            break;
        case '5':
            break;
        case 0:
            /* If this option set a flag, do nothing else now. */
            break;
        default:
            errflg++;
            break;
        }
    }

    if (opts->forwardable && opts->not_forwardable) {
        fprintf(stderr, _("Only one of -f and -F allowed\n"));
        errflg++;
    }
    if (opts->proxiable && opts->not_proxiable) {
        fprintf(stderr, _("Only one of -p and -P allowed\n"));
        errflg++;
    }
    if (opts->request_pac && opts->not_request_pac) {
        fprintf(stderr, _("Only one of --request-pac and --no-request-pac "
                          "allowed\n"));
        errflg++;
    }
    if (opts->addresses && opts->no_addresses) {
        fprintf(stderr, _("Only one of -a and -A allowed\n"));
        errflg++;
    }
    if (opts->keytab_name != NULL && opts->use_client_keytab == 1) {
        fprintf(stderr, _("Only one of -t and -i allowed\n"));
        errflg++;
    }
    if ((opts->keytab_name != NULL || opts->use_client_keytab == 1) &&
        opts->action != INIT_KT) {
        opts->action = INIT_KT;
        fprintf(stderr, _("keytab specified, forcing -k\n"));
    }
    if (argc - optind > 1) {
        fprintf(stderr, _("Extra arguments (starting with \"%s\").\n"),
                argv[optind + 1]);
        errflg++;
    }

    if (errflg)
        usage();

    opts->principal_name = (optind == argc - 1) ? argv[optind] : 0;
    return opts->principal_name;
}

static int
k5_begin(struct k_opts *opts, struct k5_data *k5)
{
    krb5_error_code ret;
    int success = 0;
    int flags = opts->enterprise ? KRB5_PRINCIPAL_PARSE_ENTERPRISE : 0;
    krb5_ccache defcache = NULL;
    krb5_principal defcache_princ = NULL, princ;
    krb5_keytab keytab;
    const char *deftype = NULL;
    char *defrealm, *name;

    ret = krb5_init_context(&k5->ctx);
    if (ret) {
        com_err(progname, ret, _("while initializing Kerberos 5 library"));
        return 0;
    }
    errctx = k5->ctx;

    if (opts->k5_out_cache_name) {
        ret = krb5_cc_resolve(k5->ctx, opts->k5_out_cache_name, &k5->out_cc);
        if (ret) {
            com_err(progname, ret, _("resolving ccache %s"),
                    opts->k5_out_cache_name);
            goto cleanup;
        }
        if (opts->verbose) {
            fprintf(stderr, _("Using specified cache: %s\n"),
                    opts->k5_out_cache_name);
        }
    } else {
        /* Resolve the default ccache and get its type and default principal
         * (if it is initialized). */
        ret = krb5_cc_default(k5->ctx, &defcache);
        if (ret) {
            com_err(progname, ret, _("while getting default ccache"));
            goto cleanup;
        }
        deftype = krb5_cc_get_type(k5->ctx, defcache);
        if (krb5_cc_get_principal(k5->ctx, defcache, &defcache_princ) != 0)
            defcache_princ = NULL;
    }

    /* Choose a client principal name. */
    if (opts->principal_name != NULL) {
        /* Use the specified principal name. */
        ret = krb5_parse_name_flags(k5->ctx, opts->principal_name, flags,
                                    &k5->me);
        if (ret) {
            com_err(progname, ret, _("when parsing name %s"),
                    opts->principal_name);
            goto cleanup;
        }
    } else if (opts->anonymous) {
        /* Use the anonymous principal for the local realm. */
        ret = krb5_get_default_realm(k5->ctx, &defrealm);
        if (ret) {
            com_err(progname, ret, _("while getting default realm"));
            goto cleanup;
        }
        ret = krb5_build_principal_ext(k5->ctx, &k5->me,
                                       strlen(defrealm), defrealm,
                                       strlen(KRB5_WELLKNOWN_NAMESTR),
                                       KRB5_WELLKNOWN_NAMESTR,
                                       strlen(KRB5_ANONYMOUS_PRINCSTR),
                                       KRB5_ANONYMOUS_PRINCSTR, 0);
        krb5_free_default_realm(k5->ctx, defrealm);
        if (ret) {
            com_err(progname, ret, _("while building principal"));
            goto cleanup;
        }
    } else if (opts->action == INIT_KT && opts->use_client_keytab) {
        /* Use the first entry from the client keytab. */
        ret = krb5_kt_client_default(k5->ctx, &keytab);
        if (ret) {
            com_err(progname, ret,
                    _("When resolving the default client keytab"));
            goto cleanup;
        }
        ret = k5_kt_get_principal(k5->ctx, keytab, &k5->me);
        krb5_kt_close(k5->ctx, keytab);
        if (ret) {
            com_err(progname, ret,
                    _("When determining client principal name from keytab"));
            goto cleanup;
        }
    } else if (opts->action == INIT_KT) {
        /* Use the default host/service name. */
        ret = krb5_sname_to_principal(k5->ctx, NULL, NULL, KRB5_NT_SRV_HST,
                                      &k5->me);
        if (ret) {
            com_err(progname, ret,
                    _("when creating default server principal name"));
            goto cleanup;
        }
        if (k5->me->realm.data[0] == 0) {
            ret = krb5_unparse_name(k5->ctx, k5->me, &k5->name);
            if (ret == 0) {
                com_err(progname, KRB5_ERR_HOST_REALM_UNKNOWN,
                        _("(principal %s)"), k5->name);
            } else {
                com_err(progname, KRB5_ERR_HOST_REALM_UNKNOWN,
                        _("for local services"));
            }
            goto cleanup;
        }
    } else if (k5->out_cc != NULL) {
        /* If the output ccache is initialized, use its principal. */
        if (krb5_cc_get_principal(k5->ctx, k5->out_cc, &princ) == 0)
            k5->me = princ;
    } else if (defcache_princ != NULL) {
        /* Use the default cache's principal, and use the default cache as the
         * output cache. */
        k5->out_cc = defcache;
        defcache = NULL;
        k5->me = defcache_princ;
        defcache_princ = NULL;
    }

    /* If we still haven't chosen, use the local username. */
    if (k5->me == NULL) {
        name = get_name_from_os();
        if (name == NULL) {
            fprintf(stderr, _("Unable to identify user\n"));
            goto cleanup;
        }
        ret = krb5_parse_name_flags(k5->ctx, name, flags, &k5->me);
        if (ret) {
            com_err(progname, ret, _("when parsing name %s"), name);
            goto cleanup;
        }
    }

    if (k5->out_cc == NULL && krb5_cc_support_switch(k5->ctx, deftype)) {
        /* Use an existing cache for the client principal if we can. */
        ret = krb5_cc_cache_match(k5->ctx, k5->me, &k5->out_cc);
        if (ret && ret != KRB5_CC_NOTFOUND) {
            com_err(progname, ret, _("while searching for ccache for %s"),
                    opts->principal_name);
            goto cleanup;
        }
        if (!ret) {
            if (opts->verbose) {
                fprintf(stderr, _("Using existing cache: %s\n"),
                        krb5_cc_get_name(k5->ctx, k5->out_cc));
            }
            k5->switch_to_cache = 1;
        } else if (defcache_princ != NULL) {
            /* Create a new cache to avoid overwriting the initialized default
             * cache. */
            ret = krb5_cc_new_unique(k5->ctx, deftype, NULL, &k5->out_cc);
            if (ret) {
                com_err(progname, ret, _("while generating new ccache"));
                goto cleanup;
            }
            if (opts->verbose) {
                fprintf(stderr, _("Using new cache: %s\n"),
                        krb5_cc_get_name(k5->ctx, k5->out_cc));
            }
            k5->switch_to_cache = 1;
        }
    }

    /* Use the default cache if we haven't picked one yet. */
    if (k5->out_cc == NULL) {
        k5->out_cc = defcache;
        defcache = NULL;
        if (opts->verbose) {
            fprintf(stderr, _("Using default cache: %s\n"),
                    krb5_cc_get_name(k5->ctx, k5->out_cc));
        }
    }

    if (opts->k5_in_cache_name) {
        ret = krb5_cc_resolve(k5->ctx, opts->k5_in_cache_name, &k5->in_cc);
        if (ret) {
            com_err(progname, ret, _("resolving ccache %s"),
                    opts->k5_in_cache_name);
            goto cleanup;
        }
        if (opts->verbose) {
            fprintf(stderr, _("Using specified input cache: %s\n"),
                    opts->k5_in_cache_name);
        }
    }

    ret = krb5_unparse_name(k5->ctx, k5->me, &k5->name);
    if (ret) {
        com_err(progname, ret, _("when unparsing name"));
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

static void
k5_end(struct k5_data *k5)
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
kinit_prompter(krb5_context ctx, void *data, const char *name,
               const char *banner, int num_prompts, krb5_prompt prompts[])
{
    krb5_boolean *pwprompt = data;
    krb5_prompt_type *ptypes;
    int i;

    /* Make a note if we receive a password prompt. */
    ptypes = krb5_get_prompt_types(ctx);
    for (i = 0; i < num_prompts; i++) {
        if (ptypes != NULL && ptypes[i] == KRB5_PROMPT_TYPE_PASSWORD)
            *pwprompt = TRUE;
    }
    return krb5_prompter_posix(ctx, data, name, banner, num_prompts, prompts);
}

static int
k5_kinit(struct k_opts *opts, struct k5_data *k5)
{
    int notix = 1;
    krb5_keytab keytab = 0;
    krb5_creds my_creds;
    krb5_error_code ret;
    krb5_get_init_creds_opt *options = NULL;
    krb5_boolean pwprompt = FALSE;
    krb5_address **addresses = NULL;
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
    if (opts->addresses) {
        ret = krb5_os_localaddr(k5->ctx, &addresses);
        if (ret) {
            com_err(progname, ret, _("getting local addresses"));
            goto cleanup;
        }
        krb5_get_init_creds_opt_set_address_list(options, addresses);
    }
    if (opts->no_addresses)
        krb5_get_init_creds_opt_set_address_list(options, NULL);
    if (opts->armor_ccache != NULL) {
        krb5_get_init_creds_opt_set_fast_ccache_name(k5->ctx, options,
                                                     opts->armor_ccache);
    }
    if (opts->request_pac)
        krb5_get_init_creds_opt_set_pac_request(k5->ctx, options, TRUE);
    if (opts->not_request_pac)
        krb5_get_init_creds_opt_set_pac_request(k5->ctx, options, FALSE);


    if (opts->action == INIT_KT && opts->keytab_name != NULL) {
#ifndef _WIN32
        if (strncmp(opts->keytab_name, "KDB:", 4) == 0) {
            ret = kinit_kdb_init(&k5->ctx, k5->me->realm.data);
            if (ret) {
                com_err(progname, ret,
                        _("while setting up KDB keytab for realm %s"),
                        k5->me->realm.data);
                goto cleanup;
            }
        }
#endif

        ret = krb5_kt_resolve(k5->ctx, opts->keytab_name, &keytab);
        if (ret) {
            com_err(progname, ret, _("resolving keytab %s"),
                    opts->keytab_name);
            goto cleanup;
        }
        if (opts->verbose)
            fprintf(stderr, _("Using keytab: %s\n"), opts->keytab_name);
    } else if (opts->action == INIT_KT && opts->use_client_keytab) {
        ret = krb5_kt_client_default(k5->ctx, &keytab);
        if (ret) {
            com_err(progname, ret, _("resolving default client keytab"));
            goto cleanup;
        }
    }

    for (i = 0; i < opts->num_pa_opts; i++) {
        ret = krb5_get_init_creds_opt_set_pa(k5->ctx, options,
                                             opts->pa_opts[i].attr,
                                             opts->pa_opts[i].value);
        if (ret) {
            com_err(progname, ret, _("while setting '%s'='%s'"),
                    opts->pa_opts[i].attr, opts->pa_opts[i].value);
            goto cleanup;
        }
        if (opts->verbose) {
            fprintf(stderr, _("PA Option %s = %s\n"), opts->pa_opts[i].attr,
                    opts->pa_opts[i].value);
        }
    }
    if (k5->in_cc) {
        ret = krb5_get_init_creds_opt_set_in_ccache(k5->ctx, options,
                                                    k5->in_cc);
        if (ret)
            goto cleanup;
    }
    ret = krb5_get_init_creds_opt_set_out_ccache(k5->ctx, options, k5->out_cc);
    if (ret)
        goto cleanup;

    switch (opts->action) {
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

    if (ret) {
        char *doing = NULL;
        switch (opts->action) {
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
            (pwprompt && ret == KRB5KDC_ERR_PREAUTH_FAILED)) {
            fprintf(stderr, _("%s: Password incorrect while %s\n"), progname,
                    doing);
        } else {
            com_err(progname, ret, _("while %s"), doing);
        }
        goto cleanup;
    }

    if (opts->action != INIT_PW && opts->action != INIT_KT) {
        ret = krb5_cc_initialize(k5->ctx, k5->out_cc, opts->canonicalize ?
                                 my_creds.client : k5->me);
        if (ret) {
            com_err(progname, ret, _("when initializing cache %s"),
                    opts->k5_out_cache_name ? opts->k5_out_cache_name : "");
            goto cleanup;
        }
        if (opts->verbose)
            fprintf(stderr, _("Initialized cache\n"));

        ret = krb5_cc_store_cred(k5->ctx, k5->out_cc, &my_creds);
        if (ret) {
            com_err(progname, ret, _("while storing credentials"));
            goto cleanup;
        }
        if (opts->verbose)
            fprintf(stderr, _("Stored credentials\n"));
    }
    notix = 0;
    if (k5->switch_to_cache) {
        ret = krb5_cc_switch(k5->ctx, k5->out_cc);
        if (ret) {
            com_err(progname, ret, _("while switching to new ccache"));
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
    if (opts->pa_opts) {
        free(opts->pa_opts);
        opts->pa_opts = NULL;
        opts->num_pa_opts = 0;
    }
    krb5_free_cred_contents(k5->ctx, &my_creds);
    if (keytab != NULL)
        krb5_kt_close(k5->ctx, keytab);
    return notix ? 0 : 1;
}

int
main(int argc, char *argv[])
{
    struct k_opts opts;
    struct k5_data k5;
    int authed_k5 = 0;

    setlocale(LC_ALL, "");
    progname = GET_PROGNAME(argv[0]);

    /* Ensure we can be driven from a pipe */
    if (!isatty(fileno(stdin)))
        setvbuf(stdin, 0, _IONBF, 0);
    if (!isatty(fileno(stdout)))
        setvbuf(stdout, 0, _IONBF, 0);
    if (!isatty(fileno(stderr)))
        setvbuf(stderr, 0, _IONBF, 0);

    memset(&opts, 0, sizeof(opts));
    opts.action = INIT_PW;

    memset(&k5, 0, sizeof(k5));

    set_com_err_hook(extended_com_err_fn);

    parse_options(argc, argv, &opts);

    if (k5_begin(&opts, &k5))
        authed_k5 = k5_kinit(&opts, &k5);

    if (authed_k5 && opts.verbose)
        fprintf(stderr, _("Authenticated to Kerberos v5\n"));

    k5_end(&k5);

    if (!authed_k5)
        exit(1);
    return 0;
}
