/**
 * WinPR: Windows Portable Runtime
 * Windows credentials
 *
 * Copyright 2022 David Fort <contact@hardening-consulting.com>
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
#ifndef WINPR_CRED_H_
#define WINPR_CRED_H_

#include <winpr/winpr.h>

#ifdef _WIN32
#include <wincred.h>
#else

#define CERT_HASH_LENGTH 20

typedef enum
{
	CertCredential,
	UsernameTargetCredential,
	BinaryBlobCredential,
	UsernameForPackedCredentials,
	BinaryBlobForSystem
} CRED_MARSHAL_TYPE,
    *PCRED_MARSHAL_TYPE;

typedef struct
{
	ULONG cbSize;
	UCHAR rgbHashOfCert[CERT_HASH_LENGTH];
} CERT_CREDENTIAL_INFO, *PCERT_CREDENTIAL_INFO;

#if 0 /* shall we implement these ? */
WINPR_API BOOL CredMarshalCredentialA(CRED_MARSHAL_TYPE CredType, PVOID Credential,
                                      LPSTR* MarshaledCredential);
WINPR_API BOOL CredMarshalCredentialW(CRED_MARSHAL_TYPE CredType, PVOID Credential,
                                      LPWSTR* MarshaledCredential);

#ifdef UNICODE
#define CredMarshalCredential CredMarshalCredentialW
#else
#define CredMarshalCredential CredMarshalCredentialA
#endif

#endif /* 0 */

#endif /* _WIN32 */

#endif /* WINPR_CRED_H_ */
