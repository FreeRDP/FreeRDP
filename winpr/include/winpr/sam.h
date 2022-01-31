/**
 * WinPR: Windows Portable Runtime
 * Security Accounts Manager (SAM)
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef WINPR_UTILS_SAM_H
#define WINPR_UTILS_SAM_H

#include <winpr/winpr.h>
#include <winpr/wtypes.h>

typedef struct winpr_sam WINPR_SAM;

struct winpr_sam_entry
{
	LPSTR User;
	UINT32 UserLength;
	LPSTR Domain;
	UINT32 DomainLength;
	BYTE LmHash[16];
	BYTE NtHash[16];
};
typedef struct winpr_sam_entry WINPR_SAM_ENTRY;

#ifdef __cplusplus
extern "C"
{
#endif

	WINPR_API WINPR_SAM_ENTRY* SamLookupUserA(WINPR_SAM* sam, LPCSTR User, UINT32 UserLength,
	                                          LPCSTR Domain, UINT32 DomainLength);
	WINPR_API WINPR_SAM_ENTRY* SamLookupUserW(WINPR_SAM* sam, LPCWSTR User, UINT32 UserLength,
	                                          LPCWSTR Domain, UINT32 DomainLength);

	WINPR_API void SamResetEntry(WINPR_SAM_ENTRY* entry);
	WINPR_API void SamFreeEntry(WINPR_SAM* sam, WINPR_SAM_ENTRY* entry);

	WINPR_API WINPR_SAM* SamOpen(const char* filename, BOOL readOnly);
	WINPR_API void SamClose(WINPR_SAM* sam);

#ifdef __cplusplus
}
#endif

#endif /* WINPR_UTILS_SAM_H */
