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

#ifndef FREERDP_CRYPTO_PRIVATEKEY_H
#define FREERDP_CRYPTO_PRIVATEKEY_H

#include <freerdp/api.h>

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct rdp_private_key rdpPrivateKey;

	FREERDP_API void freerdp_key_free(rdpPrivateKey* key);

	WINPR_ATTR_MALLOC(freerdp_key_free, 1)
	FREERDP_API rdpPrivateKey* freerdp_key_new(void);

	FREERDP_API rdpPrivateKey* freerdp_key_new_from_file(const char* keyfile);

	WINPR_ATTR_MALLOC(freerdp_key_free, 1)
	FREERDP_API rdpPrivateKey* freerdp_key_new_from_pem(const char* pem);

	/** @brief Create a private key from file \b keyfile with optional password \b password
	 *
	 *  @param keyfile The file to read the key from
	 *  @param password The optional password the key is enecrypted with, \b NULL for unencrypted
	 *  @return An allocated private key, \b NULL in case of failure.
	 *  @since version 3.16.0
	 */
	FREERDP_API rdpPrivateKey* freerdp_key_new_from_file_enc(const char* keyfile,
	                                                         const char* password);

	/** @brief Create a private key from a PEM file with optional \b password
	 *
	 *  @param pem The PEM string to use
	 *  @param password The optional password, use \b NULL if no encryption is used.
	 *  @return An allocated private key, \b NULL in case of failure.
	 *  @since version 3.16.0
	 */
	WINPR_ATTR_MALLOC(freerdp_key_free, 1)
	FREERDP_API rdpPrivateKey* freerdp_key_new_from_pem_enc(const char* pem, const char* password);

	FREERDP_API BOOL freerdp_key_is_rsa(const rdpPrivateKey* key);

	FREERDP_API size_t freerdp_key_get_bits(const rdpPrivateKey* key);

	/** @brief Create a PEM from a private key
	 *
	 *  @param key The key to convert
	 *  @param plen Optional pointer, value set to strlen of the PEM
	 *  @param password Optional password string. If \b NULL an unencrypted PEM is written.
	 *  @return A PEM string or \b NULL in case of errors
	 *
	 *  @since version 3.16.0
	 */
	WINPR_ATTR_MALLOC(free, 1)
	FREERDP_API char* freerdp_key_get_pem(const rdpPrivateKey* key, size_t* plen,
	                                      const char* password);

	/** @brief Create a new private key
	 *
	 *  @param key The key to initialize
	 *  @param type The key type (RSA, ...)
	 *  @param count The number of arguments following, depends on type
	 *  @return \b TRUE for success, \b FALSE otherwise
	 *  @since version 3.16.0
	 */
	FREERDP_API BOOL freerdp_key_generate(rdpPrivateKey* key, const char* type, size_t count, ...);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CRYPTO_PRIVATEKEY_H */
