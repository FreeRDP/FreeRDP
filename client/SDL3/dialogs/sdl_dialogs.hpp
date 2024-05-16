/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL Client helper dialogs
 *
 * Copyright 2023 Armin Novak <armin.novak@thincast.com>
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

#pragma once

#include <winpr/wtypes.h>
#include <freerdp/freerdp.h>

#include "../sdl_types.hpp"
#include "../sdl_utils.hpp"

BOOL sdl_authenticate_ex(freerdp* instance, char** username, char** password, char** domain,
                         rdp_auth_reason reason);
BOOL sdl_choose_smartcard(freerdp* instance, SmartcardCertInfo** cert_list, DWORD count,
                          DWORD* choice, BOOL gateway);

SSIZE_T sdl_retry_dialog(freerdp* instance, const char* what, size_t current, void* userarg);

DWORD sdl_verify_certificate_ex(freerdp* instance, const char* host, UINT16 port,
                                const char* common_name, const char* subject, const char* issuer,
                                const char* fingerprint, DWORD flags);

DWORD sdl_verify_changed_certificate_ex(freerdp* instance, const char* host, UINT16 port,
                                        const char* common_name, const char* subject,
                                        const char* issuer, const char* new_fingerprint,
                                        const char* old_subject, const char* old_issuer,
                                        const char* old_fingerprint, DWORD flags);

int sdl_logon_error_info(freerdp* instance, UINT32 data, UINT32 type);

BOOL sdl_present_gateway_message(freerdp* instance, UINT32 type, BOOL isDisplayMandatory,
                                 BOOL isConsentMandatory, size_t length, const WCHAR* message);

BOOL sdl_message_dialog_show(const char* title, const char* message, Sint32 flags);
BOOL sdl_cert_dialog_show(const char* title, const char* message);
BOOL sdl_scard_dialog_show(const char* title, Sint32 count, const char** list);
BOOL sdl_auth_dialog_show(const SDL_UserAuthArg* args);
