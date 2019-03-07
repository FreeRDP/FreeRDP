#include "pf_common.h"

BOOL pf_common_connection_aborted_by_peer(proxyContext* context)
{
	return WaitForSingleObject(context->connectionClosed, 0) == WAIT_OBJECT_0;
}