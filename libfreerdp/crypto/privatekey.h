/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Private key Handling
 *
 * Copyright 2023 Armin Novak <anovak@thincast.com>
 * Copyright 2023 Thincast Technologies GmbH
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

#ifndef FREERDP_LIB_CORE_PRIVATEKEY_H
#define FREERDP_LIB_CORE_PRIVATEKEY_H

#include <freerdp/api.h>
#include <freerdp/crypto/crypto.h>
#include <freerdp/crypto/privatekey.h>

#include <openssl/rsa.h>
#include <openssl/evp.h>

#ifdef __cplusplus
extern "C"
{
#endif

	enum FREERDP_KEY_PARAM
	{
		FREERDP_KEY_PARAM_RSA_D,
		FREERDP_KEY_PARAM_RSA_E,
		FREERDP_KEY_PARAM_RSA_N
	};

	FREERDP_LOCAL rdpPrivateKey* freerdp_key_clone(const rdpPrivateKey* key);

	FREERDP_LOCAL const rdpCertInfo* freerdp_key_get_info(const rdpPrivateKey* key);
	FREERDP_LOCAL const BYTE* freerdp_key_get_exponent(const rdpPrivateKey* key, size_t* plength);

	FREERDP_LOCAL BOOL freerdp_key_generate(rdpPrivateKey* key, size_t bits);

	/** \brief returns a pointer to a EVP_PKEY structure.
	 *  Call EVP_PKEY_free when done.
	 */
	FREERDP_LOCAL EVP_PKEY* freerdp_key_get_evp_pkey(const rdpPrivateKey* key);

	FREERDP_LOCAL BYTE* freerdp_key_get_param(const rdpPrivateKey* key,
	                                          enum FREERDP_KEY_PARAM param, size_t* plength);

	FREERDP_LOCAL WINPR_DIGEST_CTX* freerdp_key_digest_sign(rdpPrivateKey* key,
	                                                        WINPR_MD_TYPE digest);

	FREERDP_LOCAL extern const rdpPrivateKey* priv_key_tssk;

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_LIB_CORE_PRIVATEKEY_H */
