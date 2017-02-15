/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Windows Server
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

#ifndef WF_SETTINGS_H
#define WF_SETTINGS_H

#include "wf_interface.h"

BOOL wf_settings_read_dword(HKEY key, LPCSTR subkey, LPTSTR name, DWORD* value);
BOOL wf_settings_read_string_ascii(HKEY key, LPCSTR subkey, LPTSTR name, char** value);

#endif /* WF_SETTINGS_H */
