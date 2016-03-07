/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Wayland Input
 *
 * Copyright 2014 Manuel Bachmann <tarnyko@tarnyko.net>
 * Copyright 2015 David Fort <contact@hardening-consulting.com>
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

#ifndef __WLF_INPUT_H
#define __WLF_INPUT_H

#include <freerdp/freerdp.h>
#include <uwac/uwac.h>

BOOL wlf_handle_pointer_enter(freerdp* instance, UwacPointerEnterLeaveEvent *ev);
BOOL wlf_handle_pointer_motion(freerdp* instance, UwacPointerMotionEvent *ev);
BOOL wlf_handle_pointer_buttons(freerdp* instance, UwacPointerButtonEvent *ev);
BOOL wlf_handle_pointer_axis(freerdp* instance, UwacPointerAxisEvent *ev);

BOOL wlf_handle_key(freerdp* instance, UwacKeyEvent *ev);
BOOL wlf_keyboard_enter(freerdp *instance, UwacKeyboardEnterLeaveEvent *ev);

#endif /* __WLF_INPUT_H */
