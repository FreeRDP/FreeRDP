/**
 * WinPR: Windows Portable Runtime
 * Schannel Security Package (OpenSSL)
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>
#include <winpr/sspi.h>
#include <winpr/print.h>

#include "schannel_openssl.h"

char* openssl_get_ssl_error_string(int ssl_error)
{
	switch (ssl_error)
	{
		case SSL_ERROR_ZERO_RETURN:
			return "SSL_ERROR_ZERO_RETURN";

		case SSL_ERROR_WANT_READ:
			return "SSL_ERROR_WANT_READ";

		case SSL_ERROR_WANT_WRITE:
			return "SSL_ERROR_WANT_WRITE";

		case SSL_ERROR_SYSCALL:
			return "SSL_ERROR_SYSCALL";

		case SSL_ERROR_SSL:
			return "SSL_ERROR_SSL";
	}

	return "SSL_ERROR_UNKNOWN";
}

int schannel_openssl_client_init(SCHANNEL_OPENSSL* context)
{
	int status;
	long options = 0;

	context->ctx = SSL_CTX_new(TLSv1_client_method());

	if (!context->ctx)
	{
		printf("SSL_CTX_new failed\n");
		return -1;
	}

	/**
	 * SSL_OP_NO_COMPRESSION:
	 *
	 * The Microsoft RDP server does not advertise support
	 * for TLS compression, but alternative servers may support it.
	 * This was observed between early versions of the FreeRDP server
	 * and the FreeRDP client, and caused major performance issues,
	 * which is why we're disabling it.
	 */
#ifdef SSL_OP_NO_COMPRESSION
	options |= SSL_OP_NO_COMPRESSION;
#endif

	/**
	 * SSL_OP_TLS_BLOCK_PADDING_BUG:
	 *
	 * The Microsoft RDP server does *not* support TLS padding.
	 * It absolutely needs to be disabled otherwise it won't work.
	 */
	options |= SSL_OP_TLS_BLOCK_PADDING_BUG;

	/**
	 * SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS:
	 *
	 * Just like TLS padding, the Microsoft RDP server does not
	 * support empty fragments. This needs to be disabled.
	 */
	options |= SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS;

	SSL_CTX_set_options(context->ctx, options);

	context->ssl = SSL_new(context->ctx);

	if (!context->ssl)
	{
		printf("SSL_new failed\n");
		return -1;
	}

	context->bioRead = BIO_new(BIO_s_mem());

	if (!context->bioRead)
	{
		printf("BIO_new failed\n");
		return -1;
	}

	status = BIO_set_write_buf_size(context->bioRead, SCHANNEL_CB_MAX_TOKEN);

	context->bioWrite = BIO_new(BIO_s_mem());

	if (!context->bioWrite)
	{
		printf("BIO_new failed\n");
		return -1;
	}

	status = BIO_set_write_buf_size(context->bioWrite, SCHANNEL_CB_MAX_TOKEN);

	status = BIO_make_bio_pair(context->bioRead, context->bioWrite);

	SSL_set_bio(context->ssl, context->bioRead, context->bioWrite);

	context->ReadBuffer = (BYTE*) malloc(SCHANNEL_CB_MAX_TOKEN);
	context->WriteBuffer = (BYTE*) malloc(SCHANNEL_CB_MAX_TOKEN);

	return 0;
}

int schannel_openssl_server_init(SCHANNEL_OPENSSL* context)
{
	int status;
	long options = 0;

	context->ctx = SSL_CTX_new(SSLv23_server_method());

	if (!context->ctx)
	{
		printf("SSL_CTX_new failed\n");
		return -1;
	}

	/*
	 * SSL_OP_NO_SSLv2:
	 *
	 * We only want SSLv3 and TLSv1, so disable SSLv2.
	 * SSLv3 is used by, eg. Microsoft RDC for Mac OS X.
	 */
	options |= SSL_OP_NO_SSLv2;

	/**
	 * SSL_OP_NO_COMPRESSION:
	 *
	 * The Microsoft RDP server does not advertise support
	 * for TLS compression, but alternative servers may support it.
	 * This was observed between early versions of the FreeRDP server
	 * and the FreeRDP client, and caused major performance issues,
	 * which is why we're disabling it.
	 */
#ifdef SSL_OP_NO_COMPRESSION
	options |= SSL_OP_NO_COMPRESSION;
#endif

	/**
	 * SSL_OP_TLS_BLOCK_PADDING_BUG:
	 *
	 * The Microsoft RDP server does *not* support TLS padding.
	 * It absolutely needs to be disabled otherwise it won't work.
	 */
	options |= SSL_OP_TLS_BLOCK_PADDING_BUG;

	/**
	 * SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS:
	 *
	 * Just like TLS padding, the Microsoft RDP server does not
	 * support empty fragments. This needs to be disabled.
	 */
	options |= SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS;

	SSL_CTX_set_options(context->ctx, options);

	context->ssl = SSL_new(context->ctx);

	if (!context->ssl)
	{
		printf("SSL_new failed\n");
		return -1;
	}

#if 0
	if (SSL_CTX_use_RSAPrivateKey_file(context->ctx, privatekey_file, SSL_FILETYPE_PEM) <= 0)
	{
		printf("SSL_CTX_use_RSAPrivateKey_file failed\n");
		return -1;
	}

	if (SSL_use_certificate_file(context->ssl, certificate_file, SSL_FILETYPE_PEM) <= 0)
	{
		printf("SSL_use_certificate_file failed\n");
		return -1;
	}
#endif

	context->bioRead = BIO_new(BIO_s_mem());

	if (!context->bioRead)
	{
		printf("BIO_new failed\n");
		return -1;
	}

	status = BIO_set_write_buf_size(context->bioRead, SCHANNEL_CB_MAX_TOKEN);

	context->bioWrite = BIO_new(BIO_s_mem());

	if (!context->bioWrite)
	{
		printf("BIO_new failed\n");
		return -1;
	}

	status = BIO_set_write_buf_size(context->bioWrite, SCHANNEL_CB_MAX_TOKEN);

	status = BIO_make_bio_pair(context->bioRead, context->bioWrite);

	SSL_set_bio(context->ssl, context->bioRead, context->bioWrite);

	context->ReadBuffer = (BYTE*) malloc(SCHANNEL_CB_MAX_TOKEN);
	context->WriteBuffer = (BYTE*) malloc(SCHANNEL_CB_MAX_TOKEN);

	return 0;
}

SECURITY_STATUS schannel_openssl_client_process_tokens(SCHANNEL_OPENSSL* context, PSecBufferDesc pInput, PSecBufferDesc pOutput)
{
	int status;
	int ssl_error;
	PSecBuffer pBuffer;

	if (!context->connected)
	{
		if (pInput)
		{
			if (pInput->cBuffers < 1)
				return SEC_E_INVALID_TOKEN;

			pBuffer = &pInput->pBuffers[0];

			if (pBuffer->BufferType != SECBUFFER_TOKEN)
				return SEC_E_INVALID_TOKEN;

			status = BIO_write(context->bioRead, pBuffer->pvBuffer, pBuffer->cbBuffer);
		}

		status = SSL_connect(context->ssl);

		if (status < 0)
		{
			ssl_error = SSL_get_error(context->ssl, status);
			printf("SSL_connect error: %s\n", openssl_get_ssl_error_string(ssl_error));
		}

		status = BIO_read(context->bioWrite, context->ReadBuffer, SCHANNEL_CB_MAX_TOKEN);

		if (status >= 0)
		{
			winpr_HexDump(context->ReadBuffer, status);
		}

		if (pOutput->cBuffers < 1)
			return SEC_E_INVALID_TOKEN;

		pBuffer = &pOutput->pBuffers[0];

		if (pBuffer->BufferType != SECBUFFER_TOKEN)
			return SEC_E_INVALID_TOKEN;

		if (pBuffer->cbBuffer < status)
			return SEC_E_INSUFFICIENT_MEMORY;

		CopyMemory(pBuffer->pvBuffer, context->ReadBuffer, status);
		pBuffer->cbBuffer = status;

		return SEC_I_CONTINUE_NEEDED;
	}

	return SEC_E_OK;
}

SECURITY_STATUS schannel_openssl_server_process_tokens(SCHANNEL_OPENSSL* context, PSecBufferDesc pInput, PSecBufferDesc pOutput)
{
	int status;
	int ssl_error;
	PSecBuffer pBuffer;

	if (!context->connected)
	{
		if (pInput->cBuffers < 1)
			return SEC_E_INVALID_TOKEN;

		pBuffer = &pInput->pBuffers[0];

		if (pBuffer->BufferType != SECBUFFER_TOKEN)
			return SEC_E_INVALID_TOKEN;

		status = BIO_write(context->bioRead, pBuffer->pvBuffer, pBuffer->cbBuffer);

		status = SSL_accept(context->ssl);

		if (status < 0)
		{
			ssl_error = SSL_get_error(context->ssl, status);
			printf("SSL_accept error: %s\n", openssl_get_ssl_error_string(ssl_error));
		}

		status = BIO_read(context->bioWrite, context->ReadBuffer, SCHANNEL_CB_MAX_TOKEN);

		if (status >= 0)
		{
			winpr_HexDump(context->ReadBuffer, status);
		}

		if (pOutput->cBuffers < 1)
			return SEC_E_INVALID_TOKEN;

		pBuffer = &pOutput->pBuffers[0];

		if (pBuffer->BufferType != SECBUFFER_TOKEN)
			return SEC_E_INVALID_TOKEN;

		if (pBuffer->cbBuffer < status)
			return SEC_E_INSUFFICIENT_MEMORY;

		CopyMemory(pBuffer->pvBuffer, context->ReadBuffer, status);
		pBuffer->cbBuffer = status;

		return SEC_I_CONTINUE_NEEDED;
	}

	return SEC_E_OK;
}

SCHANNEL_OPENSSL* schannel_openssl_new()
{
	SCHANNEL_OPENSSL* context;

	context = (SCHANNEL_OPENSSL*) malloc(sizeof(SCHANNEL_OPENSSL));

	if (context != NULL)
	{
		ZeroMemory(context, sizeof(SCHANNEL_OPENSSL));

		SSL_load_error_strings();
		SSL_library_init();

		context->connected = FALSE;
	}

	return context;
}

void schannel_openssl_free(SCHANNEL_OPENSSL* context)
{
	if (context)
	{
		free(context->ReadBuffer);
		free(context->WriteBuffer);

		free(context);
	}
}
