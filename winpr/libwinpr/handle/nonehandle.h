/**
 * WinPR: Windows Portable Runtime
 * NoneHandle a.k.a. brathandle should be used where a handle is needed, but
 * functionality is not implemented yet or not implementable.
 *
 * Copyright 2014 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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

#ifndef WINPR_NONE_HANDLE_PRIVATE_H
#define WINPR_NONE_HANDLE_PRIVATE_H

#ifndef _WIN32

#include <winpr/handle.h>
#include "handle.h"

struct winpr_none_handle
{
	WINPR_HANDLE_DEF();
};

typedef struct winpr_none_handle WINPR_NONE_HANDLE;

HANDLE CreateNoneHandle();

#endif /*_WIN32*/

#endif /* WINPR_NONE_HANDLE_PRIVATE_H */
