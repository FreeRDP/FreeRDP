/**
 * WinPR: Windows Portable Runtime
 * lsa4posix: TSSSP authentication package servicing
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

#ifndef WINPR_TOOLS_LSA4POSIX_AP_TSSSP_H
#define WINPR_TOOLS_LSA4POSIX_AP_TSSSP_H

#include <winpr/wtypes.h>
#include "lsa_proto.h"

typedef struct ap_tsssp_state ApTssspState;

ApTssspState* ap_tsssp_new(void);
void ap_tsssp_free(ApTssspState* state);

/* Each handler reads the request fields from r and writes the response fields to w (which the
 * caller has initialised with the response envelope and will finish/send). Returns FALSE only on
 * a fatal framing error (the connection is then dropped). */
WINPR_ATTR_NODISCARD BOOL ap_tsssp_acquire(ApTssspState* state, LsaProtoReader* r,
                                           LsaProtoWriter* w);
WINPR_ATTR_NODISCARD BOOL ap_tsssp_isc(ApTssspState* state, LsaProtoReader* r, LsaProtoWriter* w);
WINPR_ATTR_NODISCARD BOOL ap_tsssp_delete_ctx(ApTssspState* state, LsaProtoReader* r,
                                              LsaProtoWriter* w);
WINPR_ATTR_NODISCARD BOOL ap_tsssp_free_cred(ApTssspState* state, LsaProtoReader* r,
                                             LsaProtoWriter* w);

/* Service an LSA_CALL_PACKAGE for TSSSP: a redirected [MS-RDPEAR] packet whose TsPkgContext field
 * (offset 16) selects the Remote Credential Guard context. Decrypts the body with that context,
 * dispatches the redirected call and re-encrypts the reply. *ret (malloc'd, caller frees) and
 * *retLen hold the response packet; *protocolStatus is the [MS-RDPEAR] call status. Returns FALSE
 * only on an internal error, in which case *protocolStatus carries the NTSTATUS. */
WINPR_ATTR_NODISCARD BOOL ap_tsssp_call_package(const BYTE* submit, UINT32 submitLen,
                                                NTSTATUS* protocolStatus, BYTE** ret,
                                                UINT32* retLen);

#endif /* WINPR_TOOLS_LSA4POSIX_AP_TSSSP_H */
