/**
 * WinPR: Windows Portable Runtime
 * Windows Native System Services
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef WINPR_NT_PRIVATE_H
#define WINPR_NT_PRIVATE_H

#ifndef _WIN32

#include <winpr/nt.h>

#include "../handle/handle.h"

struct winpr_file
{
	WINPR_HANDLE_DEF();

	ACCESS_MASK DesiredAccess;
	OBJECT_ATTRIBUTES ObjectAttributes;
	ULONG FileAttributes;
	ULONG ShareAccess;
	ULONG CreateDisposition;
	ULONG CreateOptions;
};
typedef struct winpr_file WINPR_FILE;

#endif

#endif /* WINPR_NT_PRIVATE_H */
 
