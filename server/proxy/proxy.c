#include "proxy.h"
#include "pf_config.h"

rdpProxyServer* proxy_server_new()
{
	rdpProxyServer* server;
	server = (rdpProxyServer*) calloc(1, sizeof(rdpProxyServer));

	if (!server)
		return NULL;

	server->config = (proxyConfig*) calloc(1, sizeof(proxyConfig));

	if (!server->config)
		return NULL;

	return server;
}

void proxy_server_free(rdpProxyServer* server)
{
	pf_server_config_free(server->config);
	free(server);
}
