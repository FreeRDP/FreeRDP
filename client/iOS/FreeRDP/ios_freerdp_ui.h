/*
 RDP ui callbacks
 
 Copyright 2013 Thinstuff Technologies GmbH, Authors: Martin Fleisz, Dorian Johnson
 
 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#import "ios_freerdp.h"

void ios_ui_begin_paint(rdpContext * context);
void ios_ui_end_paint(rdpContext * context);
void ios_ui_resize_window(rdpContext * context);

BOOL ios_ui_authenticate(freerdp * instance, char** username, char** password, char** domain);
BOOL ios_ui_check_certificate(freerdp * instance, char * subject, char * issuer, char * fingerprint);
BOOL ios_ui_check_changed_certificate(freerdp * instance, char * subject, char * issuer, char * new_fingerprint, char * old_fingerprint);

void ios_allocate_display_buffer(mfInfo* mfi);
void ios_resize_display_buffer(mfInfo* mfi);
