/**
 * WinPR: Windows Portable Runtime
 * OpenSSL Library Initialization
 *
 * Copyright 2014 Thincast Technologies GmbH
 * Copyright 2014 Norbert Federa <norbert.federa@thincast.com>
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

#ifndef WINPR_SSL_H
#define WINPR_SSL_H

#include <winpr/wtypes.h>

#define WINPR_SSL_INIT_DEFAULT 0x00
#define WINPR_SSL_INIT_ALREADY_INITIALIZED 0x01
#define WINPR_SSL_INIT_ENABLE_LOCKING 0x2
#define WINPR_SSL_INIT_ENABLE_FIPS 0x4

#define WINPR_SSL_CLEANUP_GLOBAL 0x01
#define WINPR_SSL_CLEANUP_THREAD 0x02

#ifdef	__cplusplus
extern "C" {
#endif

WINPR_API BOOL winpr_InitializeSSL(DWORD flags);
WINPR_API BOOL winpr_CleanupSSL(DWORD flags);

WINPR_API BOOL winpr_FIPSMode(void);

#ifdef	__cplusplus
}
#endif

#endif	/* WINPR_SSL_H */

