/*
 RDP ui callbacks

 Copyright 2013 Thincast Technologies GmbH, Authors: Martin Fleisz, Dorian Johnson

 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 If a copy of the MPL was not distributed with this file, You can obtain one at
 http://mozilla.org/MPL/2.0/.
 */

#import "ios_freerdp.h"

#include <freerdp/freerdp.h>

WINPR_ATTR_NODISCARD
BOOL ios_ui_begin_paint(rdpContext* context);

WINPR_ATTR_NODISCARD
BOOL ios_ui_end_paint(rdpContext* context);

WINPR_ATTR_NODISCARD
BOOL ios_ui_resize_window(rdpContext* context);

WINPR_ATTR_NODISCARD
BOOL ios_ui_authenticate_ex(freerdp* instance, char** username, char** password, char** domain,
                            rdp_auth_reason reason);
WINPR_ATTR_NODISCARD
DWORD ios_ui_verify_certificate_ex(freerdp* instance, const char* host, UINT16 port,
                                   const char* common_name, const char* subject, const char* issuer,
                                   const char* fingerprint, DWORD flags);

WINPR_ATTR_NODISCARD
DWORD ios_ui_verify_changed_certificate_ex(freerdp* instance, const char* host, UINT16 port,
                                           const char* common_name, const char* subject,
                                           const char* issuer, const char* fingerprint,
                                           const char* old_subject, const char* old_issuer,
                                           const char* old_fingerprint, DWORD flags);
WINPR_ATTR_NODISCARD
SSIZE_T ios_ui_retry_dialog(freerdp* instance, const char* what, size_t current, void* userarg);

void ios_ui_set_clipboard_text(rdpContext* context, const char* text, UINT32 size);

WINPR_ATTR_MALLOC(free, 1)
char* ios_ui_get_clipboard_text(rdpContext* context);

void ios_allocate_display_buffer(mfInfo* mfi);
