/* FreeRDP: Licensed under the Apache License, Version 2.0. */
#include "../transport.h"

#include <limits.h>
#include <stdio.h>

int TestTransportLayerBIO(int argc, char* argv[])
{
	(void)argc;
	(void)argv;
	int status = 1;
	BIO* bio = BIO_new(BIO_s_transport_layer());
	if (!bio)
		return 1;
	rdpTransportLayer* layer = transport_layer_new(NULL, 0);
	if (!layer)
		goto out;
	BIO_set_data(bio, layer); /* The BIO owns the layer from here. */

	/* OpenSSL asks these optional capability questions even for ordinary TLS BIOs.
	 * This callback-based layer has no kernel-TLS implementation. */
#ifdef BIO_CTRL_GET_KTLS_SEND
	if (BIO_ctrl(bio, BIO_CTRL_GET_KTLS_SEND, 0, NULL) != 0)
	{
		fprintf(stderr, "Transport layer must report KTLS send unavailable\n");
		goto out;
	}
#endif
#ifdef BIO_CTRL_GET_KTLS_RECV
	if (BIO_ctrl(bio, BIO_CTRL_GET_KTLS_RECV, 0, NULL) != 0)
	{
		fprintf(stderr, "Transport layer must report KTLS receive unavailable\n");
		goto out;
	}
#endif
	/* The new capability handling must leave ordinary controls functional. */
	if (BIO_ctrl(bio, BIO_CTRL_FLUSH, 0, NULL) != 1 ||
	    BIO_ctrl(bio, BIO_CTRL_SET_CLOSE, BIO_CLOSE, NULL) != 1 ||
	    BIO_ctrl(bio, BIO_CTRL_GET_CLOSE, 0, NULL) != BIO_CLOSE)
		goto out;
	/* Optional capability probes must not weaken rejection of unsupported controls. */
	if (BIO_ctrl(bio, BIO_C_GET_FD, 0, NULL) != -1 || BIO_ctrl(bio, BIO_C_SET_FD, -1, NULL) != -1 ||
	    BIO_ctrl(bio, INT_MAX, 0, NULL) != -1)
		goto out;
	status = 0;
out:
	BIO_free(bio);
	return status;
}
