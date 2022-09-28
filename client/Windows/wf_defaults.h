/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2022 Stefan Koell
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

#ifndef FREERDP_CLIENT_WIN_DEFAULTS_H
#define FREERDP_CLIENT_WIN_DEFAULTS_H

#include <freerdp/settings.h>

void WINAPI AddDefaultSettings(_Inout_ rdpSettings* settings);

#endif /* FREERDP_CLIENT_WIN_DEFAULTS_H */
