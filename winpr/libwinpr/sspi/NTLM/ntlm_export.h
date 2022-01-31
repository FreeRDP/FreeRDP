/**
 * WinPR: Windows Portable Runtime
 * NTLM Security Package
 *
 * Copyright 2021 Armin Novak <armin.novak@thincast.com>
 * Copyright 2021 Thincast Technologies GmbH
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

#ifndef WINPR_SSPI_NTLM_EXPORT_H
#define WINPR_SSPI_NTLM_EXPORT_H

#include <winpr/sspi.h>

#ifdef __cplusplus
extern "C"
{
#endif

	extern const SecPkgInfoA NTLM_SecPkgInfoA;
	extern const SecPkgInfoW NTLM_SecPkgInfoW;
	extern const SecurityFunctionTableA NTLM_SecurityFunctionTableA;
	extern const SecurityFunctionTableW NTLM_SecurityFunctionTableW;

#ifdef __cplusplus
}
#endif

#endif
