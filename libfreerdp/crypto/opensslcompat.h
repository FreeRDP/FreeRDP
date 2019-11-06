/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * OpenSSL Compatibility
 *
 * Copyright (C) 2016 Norbert Federa <norbert.federa@thincast.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *		 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FREERDP_LIB_CRYPTO_OPENSSLCOMPAT_H
#define FREERDP_LIB_CRYPTO_OPENSSLCOMPAT_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <freerdp/api.h>

#ifdef WITH_OPENSSL

#include <openssl/opensslv.h>

#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)

#include <openssl/bio.h>
#include <openssl/rsa.h>
#include <openssl/bn.h>

#define BIO_get_data(b) (b)->ptr
#define BIO_set_data(b, v) (b)->ptr = v
#define BIO_get_init(b) (b)->init
#define BIO_set_init(b, v) (b)->init = v
#define BIO_get_next(b, v) (b)->next_bio
#define BIO_set_next(b, v) (b)->next_bio = v
#define BIO_get_shutdown(b) (b)->shutdown
#define BIO_set_shutdown(b, v) (b)->shutdown = v
#define BIO_get_retry_reason(b) (b)->retry_reason
#define BIO_set_retry_reason(b, v) (b)->retry_reason = v

#define BIO_meth_set_write(b, f) (b)->bwrite = (f)
#define BIO_meth_set_read(b, f) (b)->bread = (f)
#define BIO_meth_set_puts(b, f) (b)->bputs = (f)
#define BIO_meth_set_gets(b, f) (b)->bgets = (f)
#define BIO_meth_set_ctrl(b, f) (b)->ctrl = (f)
#define BIO_meth_set_create(b, f) (b)->create = (f)
#define BIO_meth_set_destroy(b, f) (b)->destroy = (f)
#define BIO_meth_set_callback_ctrl(b, f) (b)->callback_ctrl = (f)

BIO_METHOD* BIO_meth_new(int type, const char* name);
void RSA_get0_key(const RSA* r, const BIGNUM** n, const BIGNUM** e, const BIGNUM** d);

#endif /* OPENSSL < 1.1.0 || LIBRESSL */
#endif /* WITH_OPENSSL */

#endif /* FREERDP_LIB_CRYPTO_OPENSSLCOMPAT_H */
