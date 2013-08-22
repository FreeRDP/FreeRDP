/**
 * WinPR: Windows Portable Runtime
 * Security Definitions
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

#ifndef WINPR_SECURITY_H
#define WINPR_SECURITY_H

#include <winpr/winpr.h>
#include <winpr/wtypes.h>

/**
 * Windows Integrity Mechanism Design:
 * http://msdn.microsoft.com/en-us/library/bb625963.aspx
 */

#ifndef _WIN32

#include <winpr/nt.h>

#define SECURITY_MANDATORY_UNTRUSTED_RID	0x0000
#define SECURITY_MANDATORY_LOW_RID		0x1000
#define SECURITY_MANDATORY_MEDIUM_RID		0x2000
#define SECURITY_MANDATORY_HIGH_RID		0x3000
#define SECURITY_MANDATORY_SYSTEM_RID		0x4000

#endif

#endif /* WINPR_SECURITY_H */

