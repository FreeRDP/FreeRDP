#ifndef FREERDP_SERVER_PROXY_PFCOMMON_H
#define FREERDP_SERVER_PROXY_PFCOMMON_H

#include <freerdp/freerdp.h>
#include "pf_context.h"

BOOL pf_common_connection_aborted_by_peer(proxyContext* context);
void pf_common_copy_settings(rdpSettings* dst, rdpSettings* src);

#endif /* FREERDP_SERVER_PROXY_PFCOMMON_H */
