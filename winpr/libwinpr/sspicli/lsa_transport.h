/**
 * WinPR: Windows Portable Runtime
 * lsa4posix client transport (AF_UNIX connection to the daemon)
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

#ifndef WINPR_SSPICLI_LSA_TRANSPORT_H
#define WINPR_SSPICLI_LSA_TRANSPORT_H

#include <winpr/wtypes.h>
#include <winpr/nt.h>

#include "lsa_proto.h"

/* Connect to the per-user lsa4posix daemon. Returns a socket fd, or -1 if the daemon or its
 * private socket directory is not available. */
int lsa4posix_client_connect(void);

void lsa4posix_client_close(int fd);

/* Send one request payload (always freed) and receive one response. On success *response holds
 * the response buffer the reader decodes in place: read fields from reader, then call
 * lsa_proto_reader_done() and free *response. The envelope (type, response flag, request id) is
 * validated. */
WINPR_ATTR_NODISCARD
NTSTATUS lsa4posix_transact(int fd, UINT16 expectType, wStream* request, UINT32 requestId,
                            LsaProtoReader* reader, LSA_MSG_HDR* respHdr, wStream** response);

#endif /* WINPR_SSPICLI_LSA_TRANSPORT_H */
