#ifndef FREERDP_PROXY_CLIENT_H
#define FREERDP_PROXY_CLIENT_H

#include <freerdp/freerdp.h>
#include <freerdp/client/rdpei.h>
#include <freerdp/client/tsmf.h>
#include <freerdp/client/rail.h>
#include <freerdp/client/cliprdr.h>
#include <freerdp/client/rdpgfx.h>
#include <freerdp/client/encomsp.h>

struct tf_context
{
	rdpContext context;

	/* Channels */
	RdpeiClientContext* rdpei;
	RdpgfxClientContext* gfx;
	EncomspClientContext* encomsp;
};
typedef struct tf_context tfContext;

#endif /* FREERDP_PROXY_CLIENT_H */
