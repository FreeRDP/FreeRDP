/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Smartcard logon
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

#ifndef SMARTCARD_LOGON_H
#define SMARTCARD_LOGON_H

#include "freerdp/freerdp.h"
#include <dlfcn.h>

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/objects.h>
#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/opensslv.h>
#include <openssl/pem.h>
#include <openssl/bn.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

#include <gssapi/gssapi.h>

#include <pkcs11-helper-1.0/pkcs11.h>
#include "pkcs11/cert_info.h"
#include "pkcs11/cert_vfy.h"
#include "pkcs11/error.h"

#define MAX_KEYS_PER_SLOT 15
#define NB_ENTRIES_MAX 20
#define SIZE_SPN_MAX 200
#define PIN_LENGTH 4
#define SIZE_TOKEN_LABEL_MAX 30
#define SIZE_NB_SLOT_ID_MAX 2 // "99" slots max
#define NB_TRY_MAX_LOGIN_TOKEN 3

#define FLAGS_TOKEN_USER_PIN_NOT_IMPLEMENTED    (0)
#define FLAGS_TOKEN_USER_PIN_OK         		(0)

/*
 * token flag values meet kerberos responder pkinit flags defined in krb5.h
 * KRB5_RESPONDER_PKINIT_FLAGS_TOKEN_USER_PIN_COUNT_LOW (1 << 0)
 * KRB5_RESPONDER_PKINIT_FLAGS_TOKEN_USER_PIN_FINAL_TRY (1 << 1)
 * KRB5_RESPONDER_PKINIT_FLAGS_TOKEN_USER_PIN_LOCKED	(1 << 2)
 */
#define FLAGS_TOKEN_USER_PIN_COUNT_LOW  		(1 << 0)
#define FLAGS_TOKEN_USER_PIN_FINAL_TRY  		(1 << 1)
#define FLAGS_TOKEN_USER_PIN_LOCKED     		(1 << 2)

#define MAGIC			0xd00bed00

struct pkcs11_module {
	unsigned int _magic;
	void *handle;
};
typedef struct pkcs11_module pkcs11_module_t;

struct _cert_object {
	CK_KEY_TYPE key_type;
	CK_CERTIFICATE_TYPE type;
	CK_BYTE *id_cert;
	CK_ULONG id_cert_length;
	CK_OBJECT_HANDLE private_key;
	X509 *x509;
};

typedef struct _cert_object cert_object;

struct _pkcs11_handle {
	pkcs11_module_t * p11_module_handle;
	CK_SLOT_ID slot_id;
	CK_ULONG slot_count;
	CK_SESSION_HANDLE session;
	CK_OBJECT_HANDLE private_key;
	cert_policy policy;
	cert_object **certs;
	cert_object * valid_cert;
	int cert_count;
};

typedef struct _pkcs11_handle pkcs11_handle;

#define AT_KEYEXCHANGE 1
#define AT_SIGNATURE   2

#include "nla.h"

errno_t memset_s(void *v, rsize_t smax, int c, rsize_t n);

CK_RV C_UnloadModule(void *module);
void * C_LoadModule(const char *mspec, CK_FUNCTION_LIST_PTR_PTR funcs);

struct flag_info {
	CK_FLAGS	value;
	const char *	name;
};

const char *p11_utf8_to_local(CK_UTF8CHAR *string, size_t len);
const char *p11_flag_names(struct flag_info *list, CK_FLAGS value);
const char *p11_token_info_flags(CK_FLAGS value);

int find_object(CK_SESSION_HANDLE sess, CK_OBJECT_CLASS cls,
		CK_OBJECT_HANDLE_PTR ret,
		const unsigned char *id, size_t id_len, int obj_index);

CK_RV find_object_with_attributes(CK_SESSION_HANDLE session,
			CK_OBJECT_HANDLE *out,
			CK_ATTRIBUTE *attrs, CK_ULONG attrsLen,
			CK_ULONG obj_index);

CK_ULONG get_private_key_length(CK_SESSION_HANDLE sess, CK_OBJECT_HANDLE prkey);

CK_ULONG get_mechanisms(CK_SLOT_ID slot, CK_MECHANISM_TYPE_PTR *pList, CK_FLAGS flags);

int find_mechanism(CK_SLOT_ID slot, CK_FLAGS flags,
		CK_MECHANISM_TYPE_PTR list, size_t list_len,
		CK_MECHANISM_TYPE_PTR result);

CK_RV init_authentication_pin(rdpNla * nla);
cert_object **get_list_certificate(pkcs11_handle * phdlsc, int *ncerts);
CK_RV get_info_smartcard(rdpNla * nla);
int get_valid_smartcard_cert(rdpNla * nla);
int get_valid_smartcard_UPN(rdpSettings * settings, X509 * x509);
int get_private_key(pkcs11_handle *h, cert_object *cert);
CK_RV pkcs11_do_login(CK_SESSION_HANDLE session, CK_SLOT_ID slot_id, rdpSettings * settings);
CK_RV pkcs11_login(CK_SESSION_HANDLE session, rdpSettings * settings, char *pin);
int find_valid_matching_cert(rdpSettings * settings, pkcs11_handle * phdlsc);
int match_id(rdpSettings * settings, cert_object * cert);
int get_id_private_key(pkcs11_handle * h, cert_object * cert);
int compare_id(rdpSettings * settings, cert_object * cert);
int crypto_init(cert_policy *policy);
int close_pkcs11_session(pkcs11_handle *h);
void release_pkcs11_module(pkcs11_handle * h);

#endif /* SMARTCARD_LOGON_H */
