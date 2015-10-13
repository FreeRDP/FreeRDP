/**
 * WinPR: Windows Portable Runtime
 * Cryptography API (CryptoAPI)
 *
 * Copyright 2012-2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef WINPR_CRYPTO_PRIVATE_H
#define WINPR_CRYPTO_PRIVATE_H

#ifndef _WIN32

struct _WINPR_PROTECTED_MEMORY_BLOCK
{
	BYTE* pData;
	DWORD cbData;
	DWORD dwFlags;
	BYTE key[32];
	BYTE iv[32];
	BYTE salt[8];
};
typedef struct _WINPR_PROTECTED_MEMORY_BLOCK WINPR_PROTECTED_MEMORY_BLOCK;

struct _WINPR_CERTSTORE
{
	LPCSTR lpszStoreProvider;
	DWORD dwMsgAndCertEncodingType;
};
typedef struct _WINPR_CERTSTORE WINPR_CERTSTORE;

#endif

#endif /* WINPR_CRYPTO_PRIVATE_H */
