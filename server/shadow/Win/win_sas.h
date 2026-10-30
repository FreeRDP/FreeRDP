/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2026 Cong Zhang
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

#ifndef FREERDP_SERVER_SHADOW_WIN_SAS_H
#define FREERDP_SERVER_SHADOW_WIN_SAS_H

/* Ctrl+Alt+Del from a remote keyboard is the secure attention sequence:
 * SendInput cannot produce it and only a service may call SendSAS, so the
 * shadow server sets this event and freerdp-shadow-service, which started
 * it, sends the sequence. */
#define WIN_SHADOW_SAS_EVENT L"Global\\FreeRDP-Shadow-SAS"

#endif /* FREERDP_SERVER_SHADOW_WIN_SAS_H */
