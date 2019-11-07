/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Cliprdr common
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
 * Copyright 2019 Kobi Mizrachi <kmizrachi18@gmail.com>
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

#ifndef FREERDP_CHANNEL_RDPECLIP_COMMON_H
#define FREERDP_CHANNEL_RDPECLIP_COMMON_H

#include <winpr/crt.h>
#include <winpr/stream.h>

#include <freerdp/channels/cliprdr.h>
#include <freerdp/api.h>

FREERDP_LOCAL wStream* cliprdr_packet_new(UINT16 msgType, UINT16 msgFlags, UINT32 dataLen);
FREERDP_LOCAL wStream*
cliprdr_packet_lock_clipdata_new(const CLIPRDR_LOCK_CLIPBOARD_DATA* lockClipboardData);
FREERDP_LOCAL wStream*
cliprdr_packet_unlock_clipdata_new(const CLIPRDR_UNLOCK_CLIPBOARD_DATA* unlockClipboardData);
FREERDP_LOCAL wStream*
cliprdr_packet_file_contents_request_new(const CLIPRDR_FILE_CONTENTS_REQUEST* request);
FREERDP_LOCAL wStream*
cliprdr_packet_file_contents_response_new(const CLIPRDR_FILE_CONTENTS_RESPONSE* response);
FREERDP_LOCAL wStream* cliprdr_packet_format_list_new(const CLIPRDR_FORMAT_LIST* formatList,
                                                      BOOL useLongFormatNames);

FREERDP_LOCAL UINT cliprdr_read_lock_clipdata(wStream* s,
                                              CLIPRDR_LOCK_CLIPBOARD_DATA* lockClipboardData);
FREERDP_LOCAL UINT cliprdr_read_unlock_clipdata(wStream* s,
                                                CLIPRDR_UNLOCK_CLIPBOARD_DATA* unlockClipboardData);
FREERDP_LOCAL UINT cliprdr_read_format_data_request(wStream* s,
                                                    CLIPRDR_FORMAT_DATA_REQUEST* formatDataRequest);
FREERDP_LOCAL UINT cliprdr_read_format_data_response(wStream* s,
                                                     CLIPRDR_FORMAT_DATA_RESPONSE* response);
FREERDP_LOCAL UINT
cliprdr_read_file_contents_request(wStream* s, CLIPRDR_FILE_CONTENTS_REQUEST* fileContentsRequest);
FREERDP_LOCAL UINT cliprdr_read_file_contents_response(wStream* s,
                                                       CLIPRDR_FILE_CONTENTS_RESPONSE* response);
FREERDP_LOCAL UINT cliprdr_read_format_list(wStream* s, CLIPRDR_FORMAT_LIST* formatList,
                                            BOOL useLongFormatNames);

FREERDP_LOCAL void cliprdr_free_format_list(CLIPRDR_FORMAT_LIST* formatList);

#endif /* FREERDP_CHANNEL_RDPECLIP_COMMON_H */
