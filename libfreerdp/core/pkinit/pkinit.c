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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <freerdp/error.h>

#include "pkinit.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#define TAG FREERDP_TAG("core.pkinit")
#define PKINIT_ANCHORS_MAX 10

static const char * PREFIX_X509_ANCHORS = "X509_anchors=";
static const char * PREFIX_PKINIT_FILE = "FILE:";
static const char * PREFIX_X509_USER_IDENTITY = "X509_user_identity=";
static const char * PREFIX_PKINIT_PKCS11 = "PKCS11:module_name=";
static const char * PREFIX_PKINIT_CERT_ID = ":certid=";

static const char * PREFIX_PKINIT_CHALLENGE = "pkinit"; /* "KRB5_RESPONDER_QUESTION_PKINIT" */
static const char * PREFIX_PKINIT_PKCS11_FORMAT_CHALLENGE = "={\"PKCS11:module_name=";
static const char * PREFIX_PKINIT_SLOT_ID = ":slotid=";
static const char * PREFIX_PKINIT_TOKEN_LABEL = ":token=";
static const char * SUFFIX_PKINIT_TOKEN_LABEL = "\":";
static const char * SUFFIX_PKINIT_FORMAT_CHALLENGE = "}";

/* Copy a data structure, with fresh allocation. */
krb5_error_code KRB5_CALLCONV
krb5_copy_data_add0(krb5_context context, const krb5_data *indata, krb5_data **outdata)
{
	krb5_data *tempdata;
	krb5_error_code retval;

	if (!indata) {
		*outdata = 0;
		return 0;
	}

	if (!(tempdata = (krb5_data *)malloc(sizeof(*tempdata))))
		return ENOMEM;

	retval = krb5int_copy_data_contents_add0(context, indata, tempdata);
	if (retval) {
		krb5_free_data_contents(context, tempdata);
		return retval;
	}

	*outdata = tempdata;

	return 0;
}

krb5_error_code
krb5int_copy_data_contents_add0(krb5_context context, const krb5_data *indata, krb5_data *outdata)
{
	if (!indata) {
		return EINVAL;
	}

	outdata->length = indata->length;
	if (outdata->length) {
		if (!(outdata->data = malloc(outdata->length + 1))) {
			return ENOMEM;
		}
		memcpy((char *)outdata->data, (char *)indata->data, outdata->length);
		outdata->data[outdata->length] = '\0';
	}
	else
		outdata->data = 0;
	outdata->magic = KV5M_DATA;

	return 0;
}

void trace_callback(krb5_context context, const krb5_trace_info * info, void *cb) {
    if(info)
    	WLog_INFO(TAG, "Kerberos : %s", info->message);
}

static char *progname = "pkinit";

#ifndef HAVE_PWD_H
#include <pwd.h>
static
char * get_name_from_os()
{
	struct passwd *pw;
	if ((pw = getpwuid((int) getuid())))
		return pw->pw_name;
	return 0;
}
#else /* HAVE_PWD_H */
static
char * get_name_from_os()
{
	return 0;
}
#endif


static krb5_context errctx;
static void extended_com_err_fn (const char *myprog, errcode_t code,
		const char *fmt, va_list args)
{
	const char *emsg;
	emsg = krb5_get_error_message (errctx, code);
	fprintf (stderr, "%s: %s ", myprog, emsg);
	krb5_free_error_message (errctx, emsg);
	vfprintf (stderr, fmt, args);
	fprintf (stderr, "\n");
}


BOOL set_pkinit_identity(rdpSettings * settings)
{
	unsigned int size_PkinitIdentity = strlen(PREFIX_X509_USER_IDENTITY) + strlen(PREFIX_PKINIT_PKCS11) + strlen(settings->Pkcs11Module) + strlen(PREFIX_PKINIT_CERT_ID) + (unsigned int) (settings->IdCertificateLength * 2);
	settings->PkinitIdentity = calloc( size_PkinitIdentity + 1, sizeof(char) );
	if( !settings->PkinitIdentity ){
		WLog_ERR(TAG, "Error allocation settings Pkinit Identity");
		return FALSE;
	}

	strncat(settings->PkinitIdentity, PREFIX_X509_USER_IDENTITY, strlen(PREFIX_X509_USER_IDENTITY) );
	strncat(settings->PkinitIdentity, PREFIX_PKINIT_PKCS11, strlen(PREFIX_PKINIT_PKCS11) );
	strncat(settings->PkinitIdentity, settings->Pkcs11Module, strlen(settings->Pkcs11Module) );
	strncat(settings->PkinitIdentity, PREFIX_PKINIT_CERT_ID, strlen(PREFIX_PKINIT_CERT_ID) );
	strncat(settings->PkinitIdentity, (char*) settings->IdCertificate, (unsigned int) (settings->IdCertificateLength * 2) );

	settings->PkinitIdentity[size_PkinitIdentity] = '\0';

	return TRUE;
}


pkinit_anchors ** parse_pkinit_anchors(char * list_pkinit_anchors)
{
	pkinit_anchors ** array_pkinit_anchors = NULL;
	array_pkinit_anchors = (pkinit_anchors**) calloc( PKINIT_ANCHORS_MAX, sizeof(pkinit_anchors *));
	if(array_pkinit_anchors == NULL)
		return NULL;

	int i=0, j=0;
	for(i=0; i<PKINIT_ANCHORS_MAX; i++){
		array_pkinit_anchors[i] = (pkinit_anchors*) calloc( 1, sizeof(pkinit_anchors));
		if(array_pkinit_anchors[i] == NULL){
			while(i){
				free(array_pkinit_anchors[i-1]);
				i--;
			}
			free(array_pkinit_anchors);
			return NULL;
		}
	}

	WLog_DBG(TAG, "list pkinit anchors : %s", list_pkinit_anchors);

	char * pch;
	pch = strtok (list_pkinit_anchors,",");

	if(pch==NULL){
		for(i=0; i<PKINIT_ANCHORS_MAX; i++)
			free(array_pkinit_anchors[i]);
		free(array_pkinit_anchors);
		return NULL;
	}

	i=0;

	while (pch != NULL)
	{
		array_pkinit_anchors[i]->anchor = _strdup( pch );

		if( array_pkinit_anchors[i]->anchor == NULL ){
			WLog_ERR(TAG, "Error _strdup");
			j = i + 1;
			while(j > 0){
				free(array_pkinit_anchors[j-1]->anchor);
				j--;
			}
			while(j < PKINIT_ANCHORS_MAX){
				free(array_pkinit_anchors[j]);
				j++;
			}
			free(array_pkinit_anchors);
			return NULL;
		}

		array_pkinit_anchors[i]->length = strlen( array_pkinit_anchors[i]->anchor );
		size_t new_size_array_anchors = strlen( array_pkinit_anchors[i]->anchor ) + strlen(PREFIX_X509_ANCHORS) + strlen(PREFIX_PKINIT_FILE);

		array_pkinit_anchors[i]->anchor = realloc( array_pkinit_anchors[i]->anchor, new_size_array_anchors + 1);
		if( array_pkinit_anchors[i]->anchor == NULL) {
			j = i + 1;
			while(j > 0){
				free(array_pkinit_anchors[j-1]->anchor);
				j--;
			}
			while(j < PKINIT_ANCHORS_MAX){
				free(array_pkinit_anchors[j]);
				j++;
			}
			free(array_pkinit_anchors);
			return NULL;
		}

		memmove( array_pkinit_anchors[i]->anchor + strlen(PREFIX_X509_ANCHORS) + strlen(PREFIX_PKINIT_FILE), array_pkinit_anchors[i]->anchor, array_pkinit_anchors[i]->length + 1);
		memcpy( array_pkinit_anchors[i]->anchor + 0, PREFIX_X509_ANCHORS, strlen(PREFIX_X509_ANCHORS) );
		memcpy( array_pkinit_anchors[i]->anchor + strlen(PREFIX_X509_ANCHORS), PREFIX_PKINIT_FILE, strlen(PREFIX_PKINIT_FILE) );

		*(array_pkinit_anchors[i]->anchor + new_size_array_anchors) = '\0';

		pch = strtok (NULL, ",");

		i++;

		if( pch != NULL && i == PKINIT_ANCHORS_MAX ){
			WLog_ERR(TAG, "Error : too much anchors given");
			for( i=PKINIT_ANCHORS_MAX; i>0 ; i-- )
				free(array_pkinit_anchors[i-1]->anchor);
			free(array_pkinit_anchors);
			return NULL;
		}
	}

	if(i)
		return array_pkinit_anchors;
	else{
		free(array_pkinit_anchors);
		return NULL;
	}
}


char ** integer_to_string_token_flags_responder(INT32 tokenFlags)
{
	/* when not specified or not applicable token flags set to 0 */
	static char * token_flags_pkinit_formatted = NULL;

	/* Kerberos responder pkinit flags not applicable or no PIN error while logging */
	if( tokenFlags == 0 ){
		token_flags_pkinit_formatted = "0";
	}

	if( tokenFlags & KRB5_RESPONDER_PKINIT_FLAGS_TOKEN_USER_PIN_COUNT_LOW ){
		token_flags_pkinit_formatted = "1"; /* (1 << 0) = 1 */
	}
	if( tokenFlags & KRB5_RESPONDER_PKINIT_FLAGS_TOKEN_USER_PIN_FINAL_TRY ){
		token_flags_pkinit_formatted = "2"; /* (1 << 1) = 2 */
	}
	if( (tokenFlags & KRB5_RESPONDER_PKINIT_FLAGS_TOKEN_USER_PIN_COUNT_LOW) && (tokenFlags & KRB5_RESPONDER_PKINIT_FLAGS_TOKEN_USER_PIN_FINAL_TRY) ){
		token_flags_pkinit_formatted = "3"; /* (1 << 0) & (1 << 1) = 3 */
	}
	if( tokenFlags & KRB5_RESPONDER_PKINIT_FLAGS_TOKEN_USER_PIN_LOCKED ){
		token_flags_pkinit_formatted = "4"; /* (1 << 2) = 4 */
	}
	if( (tokenFlags & KRB5_RESPONDER_PKINIT_FLAGS_TOKEN_USER_PIN_COUNT_LOW) && (tokenFlags & KRB5_RESPONDER_PKINIT_FLAGS_TOKEN_USER_PIN_LOCKED) ){
		token_flags_pkinit_formatted = "5"; /* (1 << 0) & (1 << 2) = 5 */
	}
	if( (tokenFlags & KRB5_RESPONDER_PKINIT_FLAGS_TOKEN_USER_PIN_FINAL_TRY) && (tokenFlags & KRB5_RESPONDER_PKINIT_FLAGS_TOKEN_USER_PIN_LOCKED) ){
		token_flags_pkinit_formatted = "6"; /* (1 << 1) & (1 << 2) = 6 */
	}
	if( (tokenFlags & KRB5_RESPONDER_PKINIT_FLAGS_TOKEN_USER_PIN_COUNT_LOW) && (tokenFlags & KRB5_RESPONDER_PKINIT_FLAGS_TOKEN_USER_PIN_FINAL_TRY)
			&& (tokenFlags & KRB5_RESPONDER_PKINIT_FLAGS_TOKEN_USER_PIN_LOCKED) ){
		token_flags_pkinit_formatted = "7"; /* (1 << 0) & (1 << 1) & (1 << 2) = 7 */
	}

	WLog_DBG(TAG, "%s %d : formatted pkinit token flags = %s", __FILENAME__, __LINE__, token_flags_pkinit_formatted);

	return &token_flags_pkinit_formatted;
}


int init_responder_data(rdpSettings * settings, responder_data response)
{
	/* Check that a particular question has a specific challenge */
	size_t challenge_size = 0;
	challenge_size = strlen(PREFIX_PKINIT_CHALLENGE) + strlen(PREFIX_PKINIT_PKCS11_FORMAT_CHALLENGE) + strlen(settings->Pkcs11Module) + strlen(PREFIX_PKINIT_SLOT_ID)
							+ strlen(settings->SlotID)
							+ strlen(PREFIX_PKINIT_TOKEN_LABEL)
							+ strlen(settings->TokenLabel)
							+ strlen(SUFFIX_PKINIT_TOKEN_LABEL)
							+ 1 /* always 1 : possible values of TokenFlags range from "1" to "7" */
							+ strlen(SUFFIX_PKINIT_FORMAT_CHALLENGE);

	response->challenge = calloc( challenge_size + 1, sizeof(char) );
	if( response->challenge == NULL ){
		WLog_ERR(TAG, "Error allocation data challenge");
		goto get_error;
	}

	strncat(response->challenge, PREFIX_PKINIT_CHALLENGE, strlen(PREFIX_PKINIT_CHALLENGE) );
	strncat(response->challenge, PREFIX_PKINIT_PKCS11_FORMAT_CHALLENGE, strlen(PREFIX_PKINIT_PKCS11_FORMAT_CHALLENGE) );
	strncat(response->challenge, settings->Pkcs11Module, strlen(settings->Pkcs11Module) );
	strncat(response->challenge, PREFIX_PKINIT_SLOT_ID, strlen(PREFIX_PKINIT_SLOT_ID) );
	strncat(response->challenge, settings->SlotID, strlen(settings->SlotID) );
	strncat(response->challenge, PREFIX_PKINIT_TOKEN_LABEL, strlen(PREFIX_PKINIT_TOKEN_LABEL) );
	strncat(response->challenge, settings->TokenLabel, strlen(settings->TokenLabel) );
	strncat(response->challenge, SUFFIX_PKINIT_TOKEN_LABEL, strlen(SUFFIX_PKINIT_TOKEN_LABEL) );
	strncat(response->challenge, *integer_to_string_token_flags_responder(settings->TokenFlags), 1 );
	strncat(response->challenge, SUFFIX_PKINIT_FORMAT_CHALLENGE, strlen(SUFFIX_PKINIT_FORMAT_CHALLENGE) );
	response->challenge[ challenge_size ] = '\0';

	/* Set a PKINIT answer for a specific PKINIT identity. */
	size_t size_pkinit_answer = strlen(PREFIX_PKINIT_PKCS11) + strlen(settings->Pkcs11Module) + strlen(PREFIX_PKINIT_SLOT_ID)
									+ strlen(settings->SlotID) + strlen(PREFIX_PKINIT_TOKEN_LABEL) + strlen(settings->TokenLabel)
									+ strlen(PREFIX_PKINIT_TOKEN_LABEL) + 1 + strlen(settings->Pin);

	response->pkinit_answer = calloc( size_pkinit_answer + 1, sizeof(char) );
	if( response->pkinit_answer == NULL ){
		WLog_ERR(TAG, "Error allocation pkinit answer");
		goto get_error;
	}

	strncat(response->pkinit_answer, PREFIX_PKINIT_PKCS11, strlen(PREFIX_PKINIT_PKCS11) );
	strncat(response->pkinit_answer, settings->Pkcs11Module, strlen(settings->Pkcs11Module) );
	strncat(response->pkinit_answer, PREFIX_PKINIT_SLOT_ID, strlen(PREFIX_PKINIT_SLOT_ID) );
	strncat(response->pkinit_answer, settings->SlotID, strlen(settings->SlotID) );
	strncat(response->pkinit_answer, PREFIX_PKINIT_TOKEN_LABEL, strlen(PREFIX_PKINIT_TOKEN_LABEL) );
	strncat(response->pkinit_answer, settings->TokenLabel, strlen(settings->TokenLabel) );
	strncat(response->pkinit_answer, "=", 1 );
	strncat(response->pkinit_answer, settings->Pin, strlen(settings->Pin) );
	response->pkinit_answer[ size_pkinit_answer ] = '\0';

	return 0;

get_error:
	free(response->challenge);
	free(response->pkinit_answer);
	free(response);
	return -1;
}


int add_preauth_opt(struct k_opts *opts, char *av)
{
	char *sep, *v;
	krb5_gic_opt_pa_data *p, *x;

	if (opts->num_pa_opts == 0) {
		opts->pa_opts = malloc(sizeof(krb5_gic_opt_pa_data));
		if (opts->pa_opts == NULL)
			return ENOMEM;
	} else {
		size_t newsize = (opts->num_pa_opts + 1) * sizeof(krb5_gic_opt_pa_data);
		x = realloc(opts->pa_opts, newsize);
		if (x == NULL){
			free(opts->pa_opts);
			opts->pa_opts = NULL;
			return ENOMEM;
		}
		opts->pa_opts = x;
	}
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


static krb5_error_code
responder(krb5_context ctx, void *rawdata, krb5_responder_context rctx)
{
	krb5_error_code err;
	char *key, *value, *pin, *encoded1, *encoded2;
	const char *challenge;
	k5_json_value decoded1, decoded2;
	k5_json_object ids;
	k5_json_number val;
	krb5_int32 token_flags;
	responder_data data = rawdata;
	krb5_responder_pkinit_challenge *chl;
	krb5_responder_otp_challenge *ochl;
	unsigned int i, n;

	data->called = TRUE;

	if( strncmp( *krb5_responder_list_questions(ctx, rctx), "pkinit", 6) )
	{
		WLog_ERR(TAG, "No PKINIT question available");
		return 0;
	}

	/* Check that a particular challenge has the specified expected value. */
	if (data->challenge != NULL) {
		/* Separate the challenge name and its expected value. */
		key = strdup(data->challenge);
		if (key == NULL)
			return ENOMEM;
		value = key + strcspn(key, "=");
		if (*value != '\0')
			*value++ = '\0';
		/* Read the challenge. */
		challenge = krb5_responder_get_challenge(ctx, rctx, key);
		err = k5_json_decode(value, &decoded1);
		/* Check for "no challenge". */
		if (challenge == NULL && *value == '\0') {
			WLog_ERR(TAG, "OK: (no challenge) == (no challenge)");
		} else if (err != 0) {
			/* It's not JSON, so assume we're just after a string compare. */
			if (strcmp(challenge, value) == 0) {
				WLog_ERR(TAG, "OK: \"%s\" == \"%s\" ", challenge, value);
			} else {
				WLog_ERR(TAG, "ERROR: \"%s\" != \"%s\" ", challenge, value);
				return -1;
			}
		} else {
			/* Assume we're after a JSON compare - decode the actual value. */
			err = k5_json_decode(challenge, &decoded2);
			if (err != 0) {
				WLog_ERR(TAG, "error decoding \"%s\" ", challenge);
				return -1;
			}
			/* Re-encode the expected challenge and the actual challenge... */
			err = k5_json_encode(decoded1, &encoded1);
			if (err != 0) {
				WLog_ERR(TAG, "error encoding json data");
				return -1;
			}
			err = k5_json_encode(decoded2, &encoded2);
			if (err != 0) {
				WLog_ERR(TAG, "error encoding json data");
				return -1;
			}
			k5_json_release(decoded1);
			k5_json_release(decoded2);
			/* ... and see if they look the same. */
			if (strcmp(encoded1, encoded2) == 0) {
				WLog_DBG(TAG, "OK: \"%s\" == \"%s\"\n", encoded1, encoded2);
			} else {
				WLog_ERR(TAG, "ERROR: \"%s\" != \"%s\" ", encoded1,
						encoded2);
				return -1;
			}
			free(encoded1);
			free(encoded2);
		}
		free(key);
	}

	/* Provide a particular response for a challenge. */
	if (data->response != NULL) {
		/* Separate the challenge and its data content... */
		key = strdup(data->response);
		if (key == NULL)
			return ENOMEM;
		value = key + strcspn(key, "=");
		if (*value != '\0')
			*value++ = '\0';
		/* ... and pass it in. */
		err = krb5_responder_set_answer(ctx, rctx, key, value);
		if (err != 0) {
			WLog_ERR(TAG, "error setting response");
			return -1;
		}
		free(key);
	}

	if (data->print_pkinit_challenge) {
		/* Read the PKINIT challenge, formatted as a structure. */
		err = krb5_responder_pkinit_get_challenge(ctx, rctx, &chl);
		if (err != 0) {
			WLog_ERR(TAG, "error getting pkinit challenge");
			return -1;
		}
		if (chl != NULL) {
			for (n = 0; chl->identities[n] != NULL; n++)
				continue;
			for (i = 0; chl->identities[i] != NULL; i++) {
				if (chl->identities[i]->token_flags != -1) {
					WLog_DBG(TAG, "identity %u/%u: %s (flags=0x%lx)\n", i + 1, n,
							chl->identities[i]->identity,
							(long)chl->identities[i]->token_flags);
				} else {
					WLog_DBG(TAG, "identity %u/%u: %s\n", i + 1, n,
							chl->identities[i]->identity);
				}
			}
		}
		krb5_responder_pkinit_challenge_free(ctx, rctx, chl);
	}

	/* Provide a particular response for the PKINIT challenge. */
	if (data->pkinit_answer != NULL) {
		/* Read the PKINIT challenge, formatted as a structure. */
		err = krb5_responder_pkinit_get_challenge(ctx, rctx, &chl);
		if (err != 0) {
			WLog_ERR(TAG, "error getting pkinit challenge");
			return -1;
		}
		/*
		 * In case order matters, if the identity starts with "FILE:", exercise
		 * the set_answer function, with the real answer second.
		 */
		if (chl != NULL &&
				chl->identities != NULL &&
				chl->identities[0] != NULL) {
			if (strncmp(chl->identities[0]->identity, "FILE:", 5) == 0)
				krb5_responder_pkinit_set_answer(ctx, rctx, "foo", "bar");
		}
		/* Provide the real answer. */
		key = strdup(data->pkinit_answer);
		if (key == NULL)
			return ENOMEM;
		value = strrchr(key, '=');
		if (value != NULL)
			*value++ = '\0';
		else
			value = "";
		err = krb5_responder_pkinit_set_answer(ctx, rctx, key, value);
		if (err != 0) {
			WLog_ERR(TAG, "error setting response");
			return -1;
		}
		free(key);
		/*
		 * In case order matters, if the identity starts with "PKCS12:",
		 * exercise the set_answer function, with the real answer first.
		 */
		if (chl != NULL &&
				chl->identities != NULL &&
				chl->identities[0] != NULL) {
			if (strncmp(chl->identities[0]->identity, "PKCS12:", 7) == 0)
				krb5_responder_pkinit_set_answer(ctx, rctx, "foo", "bar");
		}
		krb5_responder_pkinit_challenge_free(ctx, rctx, chl);
	}

	/*
	 * Something we always check: read the PKINIT challenge, both as a
	 * structure and in JSON form, reconstruct the JSON form from the
	 * structure's contents, and check that they're the same.
	 */
	challenge = krb5_responder_get_challenge(ctx, rctx,
			KRB5_RESPONDER_QUESTION_PKINIT);
	if (challenge != NULL) {
		krb5_responder_pkinit_get_challenge(ctx, rctx, &chl);
		if (chl == NULL) {
			WLog_ERR(TAG, "pkinit raw challenge set, "
					"but structure is NULL");
			return -1;
		}
		if (k5_json_object_create(&ids) != 0) {
			WLog_ERR(TAG, "error creating json objects");
			return -1;
		}
		for (i = 0; chl->identities[i] != NULL; i++) {
			token_flags = chl->identities[i]->token_flags;
			if (k5_json_number_create(token_flags, &val) != 0) {
				WLog_ERR(TAG, "error creating json number");
				return -1;
			}
			if (k5_json_object_set(ids, chl->identities[i]->identity,
					val) != 0) {
				WLog_ERR(TAG, "error adding json number to object");
				return -1;
			}
			k5_json_release(val);
		}
		/* Encode the structure... */
		err = k5_json_encode(ids, &encoded1);
		if (err != 0) {
			WLog_ERR(TAG, "error encoding json data");
			return -1;
		}
		k5_json_release(ids);
		/* ... and see if they look the same. */
		if (strcmp(encoded1, challenge) != 0) {
			WLog_ERR(TAG, "\"%s\" != \"%s\" ", encoded1, challenge);
			return -1;
		}
		krb5_responder_pkinit_challenge_free(ctx, rctx, chl);
		free(encoded1);
	}

	/* Provide a particular response for an OTP challenge. */
	if (data->otp_answer != NULL) {
		if (krb5_responder_otp_get_challenge(ctx, rctx, &ochl) == 0) {
			key = strchr(data->otp_answer, '=');
			if (key != NULL) {
				/* Make a copy of the answer that we can chop up. */
				key = strdup(data->otp_answer);
				if (key == NULL)
					return ENOMEM;
				/* Isolate the ti value. */
				value = strchr(key, '=');
				*value++ = '\0';
				n = atoi(key);
				/* Break the value and PIN apart. */
				pin = strchr(value, ':');
				if (pin != NULL)
					*pin++ = '\0';
				err = krb5_responder_otp_set_answer(ctx, rctx, n, value, pin);
				if (err != 0) {
					WLog_ERR(TAG, "error setting response");
					return -1;
				}
				free(key);
			}
			krb5_responder_otp_challenge_free(ctx, rctx, ochl);
		}
	}

	return 0;
}


int k5_kinit(struct k_opts* opts, struct k5_data* k5, responder_data response, rdpSettings * settings)
{
	int notix = 1, i=0;
	krb5_creds * my_creds = NULL;
	krb5_error_code code = 0;
	krb5_get_init_creds_opt *options = NULL;
	BOOL pinPadMode = settings->PinPadIsPresent;
	BOOL loginRequired = settings->PinLoginRequired;

	my_creds = calloc( 1, sizeof(krb5_creds) );
	if( !my_creds ) goto cleanup;

	code = krb5_get_init_creds_opt_alloc(k5->ctx, &options);
	if (code){
		goto cleanup;
	}

	/*
    	From this point on, we can goto cleanup because my_creds is
        initialized.
	 */
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
		krb5_address **addresses = NULL;
		code = krb5_os_localaddr(k5->ctx, &addresses);
		if (code != 0) {
			WLog_ERR(TAG, "%s : Error %d getting local addresses", progname, code);
			goto cleanup;
		}
		krb5_get_init_creds_opt_set_address_list(options, addresses);
	}
	if (opts->no_addresses)
		krb5_get_init_creds_opt_set_address_list(options, NULL);
	if (opts->armor_ccache)
		krb5_get_init_creds_opt_set_fast_ccache_name(k5->ctx, options, opts->armor_ccache);

	for (i = 0; i < opts->num_pa_opts; i++) {
		code = krb5_get_init_creds_opt_set_pa(k5->ctx, options,
				opts->pa_opts[i].attr,
				opts->pa_opts[i].value);
		if (code != 0) {
			WLog_ERR(TAG, "%s : Error %d while setting '%s'='%s'",
					progname, code, opts->pa_opts[i].attr, opts->pa_opts[i].value);
			goto cleanup;
		}

		if (opts->verbose) {
			WLog_INFO(TAG, "PA Option %s = %s ", opts->pa_opts[i].attr,
					opts->pa_opts[i].value);
		}
	}

	if (k5->in_cc) {
		code = krb5_get_init_creds_opt_set_in_ccache(k5->ctx, options,
				k5->in_cc);
		if (code)
			goto cleanup;
	}

	code = krb5_get_init_creds_opt_set_out_ccache(k5->ctx, options,
			k5->out_cc);
	if (code)
		goto cleanup;

	char *doing = 0;

	if( pinPadMode && !loginRequired ){
		opts->action = INIT_CREDS_PINPAD;
		doing = "getting initial credentials with pinpad";
	}
	else if( !pinPadMode ){
		opts->action = INIT_CREDS_KEYBOARD;
		doing = "getting initial credentials with keyboard or command line";
	}
#ifdef HANDLE_PINPAD_WITH_LOGIN_REQUIRED
	else if( pinPadMode && loginRequired ){
		opts->action = INIT_CREDS_PINPAD;
		doing = "getting initial credentials with pinpad (login required)";
	}
#endif

	switch (opts->action) {
	case INIT_CREDS_PINPAD:
		code = krb5_get_init_creds_password(k5->ctx, my_creds, k5->me,
				0, /*kinit_prompter*/ 0, 0,
				opts->starttime,
				opts->service_name,
				options);
		break;
	case INIT_CREDS_KEYBOARD:
		code = krb5_get_init_creds_opt_set_responder(k5->ctx, options,
				responder, response);

		if (code) {
			WLog_ERR(TAG, "%s : Error while setting responder: %s", progname, error_message(code));
			goto cleanup;
		}

		code = krb5_get_init_creds_password(k5->ctx, my_creds, k5->me,
				NULL, NULL, NULL,
				opts->starttime,
				opts->service_name,
				options);

		if (!response->called) {
			WLog_ERR(TAG, "%s : Responder callback wasn't called", progname);
			goto cleanup;
		}

		break;
	}

	if (code) {
		switch (code){
		case KRB5KRB_AP_ERR_BAD_INTEGRITY:
			WLog_ERR(TAG, "%s: Password incorrect while %s", progname, doing);
			break;
		case KRB5KDC_ERR_KEY_EXP:
			WLog_ERR(TAG, "%s: Password has expired while %s", progname, doing);
			break;
		case KRB5KDC_ERR_PREAUTH_FAILED:
			WLog_ERR(TAG, "%s: Preauthentication failed while %s", progname, doing);
			break;
		case KRB5KDC_ERR_POLICY:
			WLog_ERR(TAG, "%s: KDC policy rejects request while %s", progname, doing);
			break;
		case KRB5KDC_ERR_BADOPTION:
			WLog_ERR(TAG, "%s: KDC can't fulfill requested option while %s", progname, doing);
			break;
		case KRB5KDC_ERR_CLIENT_REVOKED:
			WLog_ERR(TAG, "%s: Client's credentials have been revoked while %s", progname, doing);
			break;
		case KRB5KDC_ERR_SERVICE_REVOKED:
			WLog_ERR(TAG, "%s: Credentials for server have been revoked while %s", progname, doing);
			break;
		case KRB5KDC_ERR_CANNOT_POSTDATE:
			WLog_ERR(TAG, "%s: Ticket is ineligible for postdating while %s", progname, doing);
			break;
		case KRB5_RCACHE_BADVNO:
			WLog_ERR(TAG, "%s: Unsupported replay cache format version number while %s", progname, doing);
			break;
		default:
			WLog_ERR(TAG, "%s : Error %d while %s : %s",
					progname, code, doing, krb5_get_error_message(k5->ctx, code));
			break;
		}
		goto cleanup;
	}

	/* Conditional validation of the credentials obtained if KDC is able to perform it */
	if( opts->starttime ){
		code = krb5_get_validated_creds(k5->ctx, my_creds, k5->me, k5->out_cc,
				opts->service_name);

		if(code){
			switch(code){
			case KRB5KDC_ERR_BADOPTION:
				WLog_ERR(TAG, "%s: KDC can't fulfill requested option while validating credentials", progname);
				break;
			case KRB5_KDCREP_MODIFIED:
				WLog_ERR(TAG, "%s: KDC reply did not match expectations while validating credentials", progname);
				break;
			case KRB5KRB_AP_ERR_TKT_NYV:
				WLog_ERR(TAG, "%s: Ticket not yet valid", progname);
				break;
			case KRB5KRB_AP_ERR_SKEW:
				WLog_ERR(TAG, "%s: Clock skew too great", progname);
				break;
			default:
				WLog_ERR(TAG, "%s : Error %d while validating credentials : %s",
						progname, code, krb5_get_error_message(k5->ctx, code));
				break;
			}
			goto cleanup;
		}

		code = krb5_cc_initialize(k5->ctx, k5->out_cc, opts->canonicalize ?
				my_creds->client : k5->me);
		if (code) {
			WLog_ERR(TAG, "%s : Error %d when initializing cache %s", progname,
					code, opts->k5_out_cache_name?opts->k5_out_cache_name:"");
			goto cleanup;
		}

		if (opts->verbose)
			WLog_INFO(TAG, "%s : Initialized cache", progname);

		code = krb5_cc_store_cred(k5->ctx, k5->out_cc, my_creds);
		if (code) {
			WLog_ERR(TAG, "%s : Error %d while storing credentials", progname, code);
			goto cleanup;
		}
		if (opts->verbose)
			WLog_INFO(TAG, "%s : Stored credentials", progname);
	}

	/* Get canonicalized principal name for credentials delegation (CredSSP) */
	krb5_copy_data_add0(k5->ctx, my_creds->client->data, &(opts->outdata) );

	notix = 0;

	if (k5->switch_to_cache) {
		code = krb5_cc_switch(k5->ctx, k5->out_cc);
		if (code) {
			WLog_ERR(TAG, "%s : Error %d while switching to new ccache", progname, code);
			goto cleanup;
		}
	}

cleanup:
	if (options)
		krb5_get_init_creds_opt_free(k5->ctx, options);
	if( !my_creds && (my_creds->client == k5->me) ) {
		my_creds->client = 0;
	}
	krb5_free_cred_contents(k5->ctx, my_creds);
	free(my_creds);
	my_creds = NULL;
	if(opts->pa_opts!=NULL){
		free(opts->pa_opts);
		opts->pa_opts = NULL;
	}
	opts->num_pa_opts = 0;
	if(opts->pkinit_anchors){
		for( i=PKINIT_ANCHORS_MAX; i>0 ; i-- ){
			free(opts->pkinit_anchors[i-1]->anchor);
			opts->pkinit_anchors[i-1]->anchor = NULL;
			free(opts->pkinit_anchors[i-1]);
			opts->pkinit_anchors[i-1] = NULL;
		}
	}
	free(opts->pkinit_anchors);
	opts->pkinit_anchors = NULL;

	return notix ? 0 : 1; /* return 0 if error, 1 otherwise */
}


int k5_begin(struct k_opts* opts, struct k5_data* k5, rdpSettings * settings)
{
	krb5_error_code code = 0;
	int success = 0;
	int id_init = 0;
	int i = 0;
	int anchors_init = 1;
	krb5_ccache defcache = NULL;
	krb5_principal defcache_princ = NULL, princ = NULL;
	const char * deftype = NULL;
	char * defrealm=NULL, *name=NULL;
	char * pkinit_identity = settings->PkinitIdentity;
	char * list_pkinit_anchors = settings->PkinitAnchors;
	char ** domain = &settings->Domain;

	/* set opts */
	opts->lifetime = settings->LifeTime;
	opts->rlife = settings->RenewableLifeTime;
	opts->forwardable = 1;
	opts->not_forwardable = 0;
	opts->canonicalize = 1; /* Canonicalized UPN is required for credentials delegation (CredSSP) */

	int flags = opts->enterprise ? KRB5_PRINCIPAL_PARSE_ENTERPRISE : 0;

	/* set k5 string principal name */
	k5->name = opts->principal_name;

	/* set pkinit identities */
	id_init = add_preauth_opt(opts, pkinit_identity);
	if( id_init != 0 ){
		WLog_ERR(TAG, "%s : Error while setting pkinit identities", progname);
		goto cleanup;
	}

	/* set pkinit anchors */
	if(list_pkinit_anchors == NULL || (list_pkinit_anchors != NULL && (strlen(list_pkinit_anchors)==0)) ){
		WLog_WARN(TAG, "%s : /pkinit-anchors missing. Retrieve anchors via krb5.conf", progname);
	}
	else {
		opts->pkinit_anchors = parse_pkinit_anchors(list_pkinit_anchors);

		if(opts->pkinit_anchors == NULL){
			WLog_ERR(TAG, "%s : Fail to get pkinit anchors", progname);
			goto cleanup;
		}

		while(opts->pkinit_anchors && opts->pkinit_anchors[i]->anchor)
		{
			anchors_init = add_preauth_opt( opts, opts->pkinit_anchors[i]->anchor );
			if( anchors_init != 0 ){
				WLog_ERR(TAG, "%s : Error while setting pkinit anchors", progname);
				goto cleanup;
			}
			i++;
		}
	}

	code = krb5_init_context(&k5->ctx);
	if (code) {
		WLog_ERR(TAG, "%s : Error %d while initializing Kerberos 5 library", progname, code);
		goto cleanup;
	}
	errctx = k5->ctx;

	if(opts->verbose == 1){
		WLog_INFO(TAG, "%s : Krb5 trace activated", progname);
		int ret = krb5_set_trace_callback(k5->ctx, &trace_callback, NULL);
		if (ret == KRB5_TRACE_NOSUPP)
			WLog_ERR(TAG, "%s : KRB5_TRACE_NOSUPP", __FUNCTION__);
	}

	if (opts->k5_out_cache_name) {
		code = krb5_cc_resolve(k5->ctx, opts->k5_out_cache_name, &k5->out_cc);
		if (code != 0) {
			WLog_ERR(TAG, "%s : Error %d resolving ccache %s",
					progname, code, opts->k5_out_cache_name);
			goto cleanup;
		}
		if (opts->verbose) {
			WLog_INFO(TAG, "%s : Using specified cache: %s",
					progname, opts->k5_out_cache_name);
		}
	} else {
		/* Resolve the default ccache and get its type and default principal
		 * (if it is initialized). */
		code = krb5_cc_default(k5->ctx, &defcache);
		if (code) {
			WLog_ERR(TAG, "%s : Error %d while getting default ccache", progname, code);
			goto cleanup;
		}
		deftype = krb5_cc_get_type(k5->ctx, defcache);
		if (krb5_cc_get_principal(k5->ctx, defcache, &defcache_princ) != 0)
			defcache_princ = NULL;
	}

	/* Choose a client principal name. */
	if (k5->name != NULL) {
		/* Use the specified principal name. */
		code = krb5_parse_name_flags(k5->ctx, k5->name, flags,
				&k5->me);
		if (code) {
			WLog_ERR(TAG, "%s : %d : Error %d when parsing name %s",
					progname, __LINE__, code, k5->name);
			goto cleanup;
		}
	} else if (opts->anonymous) {
		/* Use the anonymous principal for the local realm. */
		code = krb5_get_default_realm(k5->ctx, &defrealm);
		if (code) {
			WLog_ERR(TAG, "%s : Error %d while getting default realm", progname, code);
			goto cleanup;
		}
		code = krb5_build_principal_ext(k5->ctx, &k5->me,
				strlen(defrealm), defrealm,
				strlen(KRB5_WELLKNOWN_NAMESTR),
				KRB5_WELLKNOWN_NAMESTR,
				strlen(KRB5_ANONYMOUS_PRINCSTR),
				KRB5_ANONYMOUS_PRINCSTR,
				0);
		krb5_free_default_realm(k5->ctx, defrealm);
		if (code) {
			WLog_ERR(TAG, "%s : Error %d while building principal", progname, code);
			goto cleanup;
		}
	}
	else if (k5->out_cc != NULL) {
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
			WLog_ERR(TAG, "Unable to identify user" );
			goto cleanup;
		}
		code = krb5_parse_name_flags(k5->ctx, name, flags, &k5->me);
		if (code) {
			WLog_ERR(TAG, "%s : %d : Error %d when parsing name %s",
					progname, __LINE__, code, name);
			goto cleanup;
		}
	}

	if (k5->out_cc == NULL && krb5_cc_support_switch(k5->ctx, deftype)) {
		/* Use an existing cache for the client principal if we can. */
		code = krb5_cc_cache_match(k5->ctx, k5->me, &k5->out_cc);
		if (code != 0 && code != KRB5_CC_NOTFOUND) {
			WLog_ERR(TAG, "%s : Error %d while searching for ccache for %s",
					progname, code, k5->name);
			goto cleanup;
		}
		if (code == 0) {
			if (opts->verbose) {
				WLog_INFO(TAG, "Using existing cache: %s",
						krb5_cc_get_name(k5->ctx, k5->out_cc));
			}
			k5->switch_to_cache = 1;
		} else if (defcache_princ != NULL) {
			/* Create a new cache to avoid overwriting the initialized default
			 * cache. */
			code = krb5_cc_new_unique(k5->ctx, deftype, NULL, &k5->out_cc);
			if (code) {
				WLog_ERR(TAG, "%s : Error %d while generating new ccache", progname, code);
				goto cleanup;
			}
			if (opts->verbose) {
				WLog_INFO(TAG, "Using new cache: %s",
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
			WLog_INFO(TAG, "Using default cache: %s",
					krb5_cc_get_name(k5->ctx, k5->out_cc));
		}
	}

	if (opts->k5_in_cache_name) {
		code = krb5_cc_resolve(k5->ctx, opts->k5_in_cache_name, &k5->in_cc);
		if (code != 0) {
			WLog_ERR(TAG, "%s : Error %d resolving ccache %s",
					progname, code, opts->k5_in_cache_name);
			goto cleanup;
		}
		if (opts->verbose) {
			WLog_INFO(TAG, "Using specified input cache: %s",
					opts->k5_in_cache_name);
		}
	}

	/* free before krb5_unparse_name change its address */
	free(k5->name);

	code = krb5_unparse_name(k5->ctx, k5->me, &k5->name);
	if (code) {
		WLog_ERR(TAG, "%s : Error %d when unparsing name", progname, code);
		goto cleanup;
	}
	if (opts->verbose)
		WLog_INFO(TAG, "Using principal: %s", k5->name);

	/* get back domain in settings if not specified in command line */
	if(*domain == NULL){
		char * find_domain = strrchr(k5->name, '@');
		if(find_domain != NULL){
			*find_domain++ = '\0';
			*domain = calloc( strlen(find_domain) + 1, sizeof(char) );
			if(!(*domain)){
				WLog_ERR(TAG, "Error allocation domain");
				goto cleanup;
			}
			strncpy(*domain, find_domain, strlen(find_domain) + 1);
		}
		else{
			WLog_ERR(TAG, "Error getting back domain");
			goto cleanup;
		}
	}
	else{
		WLog_DBG(TAG, "Domain already specified in command line");
	}

	success = 1;

	return success;

cleanup:
	if(opts->pa_opts!=NULL){
		free(opts->pa_opts);
		opts->pa_opts = NULL;
	}
	opts->num_pa_opts = 0;
	if(opts->pkinit_anchors != NULL){
		for( i=PKINIT_ANCHORS_MAX; i>0 ; i-- ){
			free(opts->pkinit_anchors[i-1]->anchor);
			opts->pkinit_anchors[i-1]->anchor = NULL;
			free(opts->pkinit_anchors[i-1]);
			opts->pkinit_anchors[i-1] = NULL;
		}
	}
	free(opts->pkinit_anchors);
	opts->pkinit_anchors = NULL;
	if (defcache != NULL)
		krb5_cc_close(k5->ctx, defcache);
	krb5_free_principal(k5->ctx, defcache_princ);
	return success;
}


void k5_end(struct k5_data* k5)
{
	if (k5->name)
		krb5_free_unparsed_name(k5->ctx, k5->name);
	if (k5->me)
		krb5_free_principal(k5->ctx, k5->me);
	if (k5->in_cc)
		krb5_cc_close(k5->ctx, k5->in_cc);
	if (k5->out_cc)
		krb5_cc_close(k5->ctx, k5->out_cc);
	if (k5->ctx)
		krb5_free_context(k5->ctx);
	errctx = NULL;
	memset(k5, 0, sizeof(*k5));
}


BOOL init_cred_cache(rdpSettings * settings)
{
	struct k_opts opts;
	struct k5_data k5;
	int authed_k5 = 0;
	responder_data response = NULL;

	memset(&opts, 0, sizeof(opts));
	memset(&k5, 0, sizeof(k5));

	set_com_err_hook (extended_com_err_fn);

	/* make KRB5 PKINIT verbose */
	if( settings->Krb5Trace )
		opts.verbose = 1;

	opts.principal_name = calloc(strlen(settings->UserPrincipalName)+1, sizeof(char) );
	if(opts.principal_name == NULL)
		return FALSE;
	strncpy(opts.principal_name, settings->UserPrincipalName, strlen(settings->UserPrincipalName) );
	opts.principal_name[strlen(settings->UserPrincipalName)] = '\0';

	/* if /d:domain is specified in command line, set it as Kerberos default realm */
	if( settings->Domain ){
		opts.principal_name = realloc( opts.principal_name, strlen(settings->UserPrincipalName) + 1 + strlen(settings->Domain) + 1 );
		if(opts.principal_name == NULL){
			free(opts.principal_name);
			return FALSE;
		}
		strncat(opts.principal_name, "@", 1);
		strncat(opts.principal_name, settings->Domain, strlen(settings->Domain) );
		opts.principal_name[strlen(settings->UserPrincipalName) + 1 + strlen(settings->Domain)] = '\0';
	}

	char * pstr = NULL;
	pstr = strchr(settings->UserPrincipalName, '@');
	if(pstr != NULL){
		opts.enterprise = KRB5_PRINCIPAL_PARSE_ENTERPRISE;
	}

	/* Start time is the time when ticket (TGT) issued by the KDC become valid.
	 * It needs to be different from 0 to request a postdated ticket.
	 * And thus, enable validation of credentials by the KDC, that can only validate postdated ticket */
	opts.starttime = settings->StartTime;

	/* set data responder callback if no PINPAD present */
	if( strncmp(settings->Pin, "NULL", 4) )
	{
		response = calloc( 1, sizeof(ty_responder_data) );
		if(response == NULL){
			WLog_ERR(TAG, "Error allocation data responder");
			free(opts.principal_name);
			return FALSE;
		}

		if( init_responder_data(settings, response) ){
			free(opts.principal_name);
			return FALSE;
		}
	}

	if ( k5_begin(&opts, &k5, settings) )
		authed_k5 = k5_kinit(&opts, &k5, response, settings);

	if( authed_k5 && opts.outdata->data ){
		settings->CanonicalizedUserHint = _strdup(opts.outdata->data);
		if(settings->CanonicalizedUserHint == NULL){
			WLog_ERR(TAG, "Error _strdup outdata into canonicalized user hint");
			authed_k5 = 0;
		}
		krb5_free_data(k5.ctx, opts.outdata);
	}

	if (authed_k5)
		WLog_INFO(TAG, "Authenticated to Kerberos v5 via smartcard");

	/* free */
	k5_end(&k5);
	if(response){
		free(response->challenge);
		free(response->pkinit_answer);
		free(response);
	}

	if (!authed_k5){
		WLog_ERR(TAG, "Credentials cache initialization failed !");
		return FALSE;
	}

	return TRUE;
}

/** pkinit_acquire_krb5_TGT is used to acquire credentials via Kerberos.
 *  This function is actually called in get_TGT_kerberos().
 *  @param krb_settings - pointer to the kerberos_settings structure
 *  @return TRUE if valid TGT acquired, FALSE otherwise
 */
BOOL pkinit_acquire_krb5_TGT(rdpSettings * settings)
{
	WLog_DBG(TAG, "PKINIT starting...");

	if( !set_pkinit_identity(settings) )
	{
		WLog_ERR(TAG, "%s %d : Error while setting pkinit_identity", __FUNCTION__,  __LINE__);
		return FALSE;
	}

	BOOL ret_pkinit = init_cred_cache(settings);

	if(ret_pkinit == FALSE)
		return FALSE;

	return TRUE;
}

/** get_TGT_kerberos is used to get TGT from KDC.
 *  This function is actually called in nla_client_init().
 *  @param settings - pointer to rdpSettings structure that contains the settings
 *  @return TRUE if the Kerberos negotiation was successful.
 */
BOOL get_TGT_kerberos(rdpSettings * settings)
{
	if( pkinit_acquire_krb5_TGT(settings) == FALSE )
		return FALSE;
	else
		WLog_DBG(TAG, "PKINIT : successfully acquired TGT");

	return TRUE;
}
