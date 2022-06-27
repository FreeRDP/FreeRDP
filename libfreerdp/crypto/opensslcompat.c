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

#include "opensslcompat.h"

#if OPENSSL_VERSION_NUMBER < 0x10100000L || \
    (defined(LIBRESSL_VERSION_NUMBER) && LIBRESSL_VERSION_NUMBER < 0x2070000fL)

BIO_METHOD* BIO_meth_new(int type, const char* name)
{
	BIO_METHOD* m;
	if (!(m = calloc(1, sizeof(BIO_METHOD))))
		return NULL;
	m->type = type;
	m->name = name;
	return m;
}

void RSA_get0_key(const RSA* r, const BIGNUM** n, const BIGNUM** e, const BIGNUM** d)
{
	if (n != NULL)
		*n = r->n;
	if (e != NULL)
		*e = r->e;
	if (d != NULL)
		*d = r->d;
}

#endif /* OPENSSL < 1.1.0 || LIBRESSL */
