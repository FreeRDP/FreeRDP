#ifndef FREERDP_PROXY_CLIENT_H
#define FREERDP_PROXY_CLIENT_H

#include <freerdp/freerdp.h>
#include <freerdp/freerdp.h>
#include <freerdp/client/rdpei.h>
#include <freerdp/client/tsmf.h>
#include <freerdp/client/rail.h>
#include <freerdp/client/cliprdr.h>
#include <freerdp/client/rdpgfx.h>
#include <freerdp/client/encomsp.h>

#include "pfreerdp.h"

struct tf_context
{
	rdpContext context;

	/* Channels */
	RdpeiClientContext* rdpei;
	RdpgfxClientContext* gfx;
	EncomspClientContext* encomsp;
};
typedef struct tf_context tfContext;

rdpContext* proxy_client_create_context(proxyContext* pContext, char* host, DWORD port,
                                        char* username, char* password);
int proxy_client_start(rdpContext* context);

#endif /* FREERDP_PROXY_CLIENT_H */
