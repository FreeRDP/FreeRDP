/*
   Android RAIL/RemoteApp Channel

   Copyright 2026 Ibrahim Sevinc <ibrahim.sevinc.mail@gmail.com>

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
   If a copy of the MPL was not distributed with this file, You can obtain one at
   http://mozilla.org/MPL/2.0/.
*/

#ifndef FREERDP_CLIENT_ANDROID_RAIL_H
#define FREERDP_CLIENT_ANDROID_RAIL_H

#include <freerdp/client/rail.h>
#include <freerdp/api.h>

#include "android_freerdp.h"

FREERDP_LOCAL BOOL android_rail_init(androidContext* afc, RailClientContext* rail);
FREERDP_LOCAL BOOL android_rail_uninit(androidContext* afc, RailClientContext* rail);

#endif /* FREERDP_CLIENT_ANDROID_RAIL_H */
