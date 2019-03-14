#ifndef FREERDP_SERVER_PROXY_PFCOMMON_H
#define FREERDP_SERVER_PROXY_PFCOMMON_H

#include <freerdp/freerdp.h>
#include "pf_context.h"

BOOL pf_common_connection_aborted_by_peer(proxyContext* context);
void proxy_settings_mirror(rdpSettings* settings, rdpSettings* baseSettings);

#endif /* FREERDP_SERVER_PROXY_PFCOMMON_H */
