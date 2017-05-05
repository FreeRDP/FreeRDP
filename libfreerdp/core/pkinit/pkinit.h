/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP MIT Kerberos authentication for smartcard (PKINIT)
 *
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

#ifndef PKINIT_H
#define PKINIT_H

#include <freerdp/types.h>
#include <freerdp/settings.h>
#include <freerdp/log.h>

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

/* undef HAVE_XXX defined previously and also in pkinit/autoconf.h */
#undef HAVE_FCNTL_H
#undef HAVE_INTTYPES_H
#undef HAVE_POLL_H
#undef HAVE_SYSLOG_H
#undef HAVE_SYS_SELECT_H
#undef HAVE_UNISTD_H

/* k5-platform.h, k5-json.h, k5-thread.h, autoconf.h are not installed
 * by default in /usr/include. These files and their include files (autoconf.h, k5-thread.h)
 * have been copied from krb5 sources manually into local pkinit/ directory */
#include "k5-platform.h"
#include "k5-json.h"

//#define HANDLE_PINPAD_WITH_LOGIN_REQUIRED

#include <gssapi.h>
#include <krb5.h>

#include <krb5/clpreauth_plugin.h>
#include <pkcs11-helper-1.0/pkcs11.h>

typedef struct _kerberos_settings{
	krb5_error_code ret;
	krb5_context context;
	krb5_principal principal;
	char * address;
	krb5_ccache ccache;
	krb5_init_creds_context ctx;
	krb5_creds * creds;
	krb5_responder_context rctx;
	krb5_get_init_creds_opt * options;
	krb5_responder_pkinit_challenge * challenge;
	void * data;
	char * identity;
	UINT32 freeRDP_error;
}kerberos_settings;

typedef struct _data_kerberos{
	krb5_context context;
	krb5_responder_context rctx;
	krb5_get_init_creds_opt * options;
}data_kerberos;

typedef struct _pkinit_anchors{
	size_t length;
	char * anchor;
}pkinit_anchors;

#define TERMSRV_SPN_PREFIX	"TERMSRV/"

typedef enum { INIT_CREDS_PINPAD, INIT_CREDS_KEYBOARD } action_type;

struct k_opts
{
	/* in seconds */
	krb5_deltat starttime;
	krb5_deltat lifetime;
	krb5_deltat rlife;

	int forwardable;
	int proxiable;
	int anonymous;
	int addresses;

	int not_forwardable;
	int not_proxiable;
	int no_addresses;

	int verbose;

	char* principal_name;
	char* service_name;
	char* keytab_name;
	char* k5_in_cache_name;
	char* k5_out_cache_name;
	char *armor_ccache;
	pkinit_anchors ** pkinit_anchors;

	action_type action;
	int use_client_keytab;

	int num_pa_opts;
	krb5_gic_opt_pa_data *pa_opts;

	int canonicalize;
	int enterprise;

	krb5_data * outdata;
};

struct k5_data
{
	krb5_context ctx;
	krb5_ccache in_cc, out_cc;
	krb5_principal me;
	char* name;
	krb5_boolean switch_to_cache;
};

struct _responder_data {
	krb5_boolean called;
	krb5_boolean print_pkinit_challenge;
	char *challenge;
	char *response;
	char *pkinit_answer;
	char *otp_answer;
};

typedef struct _responder_data ty_responder_data;
typedef ty_responder_data* responder_data;

BOOL pkinit_acquire_krb5_TGT(rdpSettings * settings);
BOOL get_TGT_kerberos(rdpSettings * settings);
BOOL set_pkinit_identity(rdpSettings * settings);
pkinit_anchors ** parse_pkinit_anchors(char * list_pkinit_anchors);
int add_preauth_opt(struct k_opts *opts, char *av);
int init_responder_data(rdpSettings * settings, responder_data data);
int k5_begin(struct k_opts* opts, struct k5_data* k5, rdpSettings * rdpSettings);
int k5_kinit(struct k_opts* opts, struct k5_data* k5, responder_data response, rdpSettings * rdpSettings);
void k5_end(struct k5_data* k5);
BOOL init_cred_cache(rdpSettings * settings);

krb5_error_code KRB5_CALLCONV krb5_copy_data_add0(krb5_context context, const krb5_data *indata, krb5_data **outdata);
krb5_error_code krb5int_copy_data_contents_add0(krb5_context context, const krb5_data *indata, krb5_data *outdata);
void trace_callback(krb5_context context, const krb5_trace_info * info, void *cb);

#endif /* PKINIT_H */
