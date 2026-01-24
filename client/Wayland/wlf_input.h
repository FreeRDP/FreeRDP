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

#ifndef FREERDP_CLIENT_WAYLAND_INPUT_H
#define FREERDP_CLIENT_WAYLAND_INPUT_H

#include <freerdp/freerdp.h>
#include <freerdp/gdi/gdi.h>
#include <freerdp/gdi/gfx.h>
#include <uwac/uwac.h>

WINPR_ATTR_NODISCARD BOOL wlf_handle_pointer_enter(freerdp* instance,
                                                   const UwacPointerEnterLeaveEvent* ev);
WINPR_ATTR_NODISCARD BOOL wlf_handle_pointer_motion(freerdp* instance,
                                                    const UwacPointerMotionEvent* ev);
WINPR_ATTR_NODISCARD BOOL wlf_handle_pointer_buttons(freerdp* instance,
                                                     const UwacPointerButtonEvent* ev);
WINPR_ATTR_NODISCARD BOOL wlf_handle_pointer_axis(freerdp* instance,
                                                  const UwacPointerAxisEvent* ev);
WINPR_ATTR_NODISCARD BOOL wlf_handle_pointer_axis_discrete(freerdp* instance,
                                                           const UwacPointerAxisEvent* ev);
WINPR_ATTR_NODISCARD BOOL wlf_handle_pointer_frame(freerdp* instance,
                                                   const UwacPointerFrameEvent* ev);
WINPR_ATTR_NODISCARD BOOL wlf_handle_pointer_source(freerdp* instance,
                                                    const UwacPointerSourceEvent* ev);
WINPR_ATTR_NODISCARD BOOL wlf_handle_touch_up(freerdp* instance, const UwacTouchUp* ev);
WINPR_ATTR_NODISCARD BOOL wlf_handle_touch_down(freerdp* instance, const UwacTouchDown* ev);
WINPR_ATTR_NODISCARD BOOL wlf_handle_touch_motion(freerdp* instance, const UwacTouchMotion* ev);

WINPR_ATTR_NODISCARD BOOL wlf_handle_key(freerdp* instance, const UwacKeyEvent* ev);
WINPR_ATTR_NODISCARD BOOL wlf_handle_ungrab_key(freerdp* instance, const UwacKeyEvent* ev);
WINPR_ATTR_NODISCARD BOOL wlf_keyboard_enter(freerdp* instance,
                                             const UwacKeyboardEnterLeaveEvent* ev);
WINPR_ATTR_NODISCARD BOOL wlf_keyboard_modifiers(freerdp* instance,
                                                 const UwacKeyboardModifiersEvent* ev);

#endif /* FREERDP_CLIENT_WAYLAND_INPUT_H */
