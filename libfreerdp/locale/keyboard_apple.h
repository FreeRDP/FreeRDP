/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Apple Core Foundation Keyboard Mapping
 *
 * Copyright 2021 Thincast Technologies GmbH
 * Copyright 2021 Martin Fleisz <martin.fleisz@thincast.com>
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

#ifndef FREERDP_LOCALE_KEYBOARD_APPLE_H
#define FREERDP_LOCALE_KEYBOARD_APPLE_H

#include <freerdp/api.h>

FREERDP_LOCAL int freerdp_detect_keyboard_layout_from_cf(DWORD* keyboardLayoutId);

#endif /* FREERDP_LOCALE_KEYBOARD_APPLE_H */
