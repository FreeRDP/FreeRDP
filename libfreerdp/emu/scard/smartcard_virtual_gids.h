/**
 * WinPR: Windows Portable Runtime
 * Virtual GIDS implementation
 *
 * Copyright 2021 Martin Fleisz <martin.fleisz@thincast.com>
 * Copyright 2021 Thincast Technologies GmbH
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

#ifndef WINPR_SMARTCARD_VIRTUAL_GIDS_H
#define WINPR_SMARTCARD_VIRTUAL_GIDS_H

#include <winpr/winpr.h>
#include <winpr/wtypes.h>
#include <freerdp/channels/log.h>

/* Virtual GIDS context */
typedef struct vgids_context vgidsContext;

/* Creates a new virtual gids context */
vgidsContext* vgids_new(void);

/*
   Initializes the virtual gids context.
     cert: PEM encoded smartcard certificate
     privateKey: PEM encoded private key for the smartcard certificate
     pin: Pin protecting the usage of the private key
     Returns: TRUE on success, FALSE in case of an error
*/
BOOL vgids_init(vgidsContext* ctx, const char* cert, const char* privateKey, const char* pin);

/*
   Processes the provided APDU returning a response for each processed command.
     data: APDU byte stream
     dataSize: size of the APDU provided in data
     response: Pointer where the response buffer is stored to. Must be freed by caller!
     responseSize: Size of the returned data buffer
     Returns: TRUE on success, FALSE in case of an error
*/
BOOL vgids_process_apdu(vgidsContext* context, const BYTE* data, DWORD dataSize, BYTE** response,
                        DWORD* responseSize);

/* frees a previously created virtual gids context */
void vgids_free(vgidsContext* context);

#endif /* WINPR_SMARTCARD_VIRTUAL_GIDS_H */
