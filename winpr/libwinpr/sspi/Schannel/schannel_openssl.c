/**
 * WinPR: Windows Portable Runtime
 * Schannel Security Package (OpenSSL)
 *
 * Copyright 2012-2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <winpr/config.h>

#include "schannel_openssl.h"

#ifdef WITH_OPENSSL

#include <winpr/crt.h>
#include <winpr/sspi.h>
#include <winpr/ssl.h>
#include <winpr/print.h>
#include <winpr/crypto.h>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>

struct S_SCHANNEL_OPENSSL
{
	SSL* ssl;
	SSL_CTX* ctx;
	BOOL connected;
	BIO* bioRead;
	BIO* bioWrite;
	BYTE* ReadBuffer;
	BYTE* WriteBuffer;
};

#include "../../log.h"
#define TAG WINPR_TAG("sspi.schannel")

static char* openssl_get_ssl_error_string(int ssl_error)
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
	context->ctx = SSL_CTX_new(SSLv23_client_method());

	if (!context->ctx)
	{
		WLog_ERR(TAG, "SSL_CTX_new failed");
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
		WLog_ERR(TAG, "SSL_new failed");
		goto out_ssl_new_failed;
	}

	context->bioRead = BIO_new(BIO_s_mem());

	if (!context->bioRead)
	{
		WLog_ERR(TAG, "BIO_new failed");
		goto out_bio_read_failed;
	}

	status = BIO_set_write_buf_size(context->bioRead, SCHANNEL_CB_MAX_TOKEN);

	if (status != 1)
	{
		WLog_ERR(TAG, "BIO_set_write_buf_size on bioRead failed");
		goto out_set_write_buf_read;
	}

	context->bioWrite = BIO_new(BIO_s_mem());

	if (!context->bioWrite)
	{
		WLog_ERR(TAG, "BIO_new failed");
		goto out_bio_write_failed;
	}

	status = BIO_set_write_buf_size(context->bioWrite, SCHANNEL_CB_MAX_TOKEN);

	if (status != 1)
	{
		WLog_ERR(TAG, "BIO_set_write_buf_size on bioWrite failed");
		goto out_set_write_buf_write;
	}

	status = BIO_make_bio_pair(context->bioRead, context->bioWrite);

	if (status != 1)
	{
		WLog_ERR(TAG, "BIO_make_bio_pair failed");
		goto out_bio_pair;
	}

	SSL_set_bio(context->ssl, context->bioRead, context->bioWrite);
	context->ReadBuffer = (BYTE*)malloc(SCHANNEL_CB_MAX_TOKEN);

	if (!context->ReadBuffer)
	{
		WLog_ERR(TAG, "Failed to allocate ReadBuffer");
		goto out_read_alloc;
	}

	context->WriteBuffer = (BYTE*)malloc(SCHANNEL_CB_MAX_TOKEN);

	if (!context->WriteBuffer)
	{
		WLog_ERR(TAG, "Failed to allocate ReadBuffer");
		goto out_write_alloc;
	}

	return 0;
out_write_alloc:
	free(context->ReadBuffer);
out_read_alloc:
out_bio_pair:
out_set_write_buf_write:
	BIO_free_all(context->bioWrite);
out_bio_write_failed:
out_set_write_buf_read:
	BIO_free_all(context->bioRead);
out_bio_read_failed:
	SSL_free(context->ssl);
out_ssl_new_failed:
	SSL_CTX_free(context->ctx);
	return -1;
}

int schannel_openssl_server_init(SCHANNEL_OPENSSL* context)
{
	int status;
	long options = 0;
	context->ctx = SSL_CTX_new(SSLv23_server_method());

	if (!context->ctx)
	{
		WLog_ERR(TAG, "SSL_CTX_new failed");
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

	if (SSL_CTX_use_RSAPrivateKey_file(context->ctx, "/tmp/localhost.key", SSL_FILETYPE_PEM) <= 0)
	{
		WLog_ERR(TAG, "SSL_CTX_use_RSAPrivateKey_file failed");
		goto out_rsa_key;
	}

	context->ssl = SSL_new(context->ctx);

	if (!context->ssl)
	{
		WLog_ERR(TAG, "SSL_new failed");
		goto out_ssl_new;
	}

	if (SSL_use_certificate_file(context->ssl, "/tmp/localhost.crt", SSL_FILETYPE_PEM) <= 0)
	{
		WLog_ERR(TAG, "SSL_use_certificate_file failed");
		goto out_use_certificate;
	}

	context->bioRead = BIO_new(BIO_s_mem());

	if (!context->bioRead)
	{
		WLog_ERR(TAG, "BIO_new failed");
		goto out_bio_read;
	}

	status = BIO_set_write_buf_size(context->bioRead, SCHANNEL_CB_MAX_TOKEN);

	if (status != 1)
	{
		WLog_ERR(TAG, "BIO_set_write_buf_size failed for bioRead");
		goto out_set_write_buf_read;
	}

	context->bioWrite = BIO_new(BIO_s_mem());

	if (!context->bioWrite)
	{
		WLog_ERR(TAG, "BIO_new failed");
		goto out_bio_write;
	}

	status = BIO_set_write_buf_size(context->bioWrite, SCHANNEL_CB_MAX_TOKEN);

	if (status != 1)
	{
		WLog_ERR(TAG, "BIO_set_write_buf_size failed for bioWrite");
		goto out_set_write_buf_write;
	}

	status = BIO_make_bio_pair(context->bioRead, context->bioWrite);

	if (status != 1)
	{
		WLog_ERR(TAG, "BIO_make_bio_pair failed");
		goto out_bio_pair;
	}

	SSL_set_bio(context->ssl, context->bioRead, context->bioWrite);
	context->ReadBuffer = (BYTE*)malloc(SCHANNEL_CB_MAX_TOKEN);

	if (!context->ReadBuffer)
	{
		WLog_ERR(TAG, "Failed to allocate memory for ReadBuffer");
		goto out_read_buffer;
	}

	context->WriteBuffer = (BYTE*)malloc(SCHANNEL_CB_MAX_TOKEN);

	if (!context->WriteBuffer)
	{
		WLog_ERR(TAG, "Failed to allocate memory for WriteBuffer");
		goto out_write_buffer;
	}

	return 0;
out_write_buffer:
	free(context->ReadBuffer);
out_read_buffer:
out_bio_pair:
out_set_write_buf_write:
	BIO_free_all(context->bioWrite);
out_bio_write:
out_set_write_buf_read:
	BIO_free_all(context->bioRead);
out_bio_read:
out_use_certificate:
	SSL_free(context->ssl);
out_ssl_new:
out_rsa_key:
	SSL_CTX_free(context->ctx);
	return -1;
}

SECURITY_STATUS schannel_openssl_client_process_tokens(SCHANNEL_OPENSSL* context,
                                                       PSecBufferDesc pInput,
                                                       PSecBufferDesc pOutput)
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

			pBuffer = sspi_FindSecBuffer(pInput, SECBUFFER_TOKEN);

			if (!pBuffer)
				return SEC_E_INVALID_TOKEN;

			ERR_clear_error();
			status = BIO_write(context->bioRead, pBuffer->pvBuffer, pBuffer->cbBuffer);
			if (status < 0)
				return SEC_E_INVALID_TOKEN;
		}

		status = SSL_connect(context->ssl);

		if (status < 0)
		{
			ssl_error = SSL_get_error(context->ssl, status);
			WLog_ERR(TAG, "SSL_connect error: %s", openssl_get_ssl_error_string(ssl_error));
		}

		if (status == 1)
			context->connected = TRUE;

		ERR_clear_error();
		status = BIO_read(context->bioWrite, context->ReadBuffer, SCHANNEL_CB_MAX_TOKEN);

		if (pOutput->cBuffers < 1)
			return SEC_E_INVALID_TOKEN;

		pBuffer = sspi_FindSecBuffer(pOutput, SECBUFFER_TOKEN);

		if (!pBuffer)
			return SEC_E_INVALID_TOKEN;

		if (status > 0)
		{
			if (pBuffer->cbBuffer < (unsigned long)status)
				return SEC_E_INSUFFICIENT_MEMORY;

			CopyMemory(pBuffer->pvBuffer, context->ReadBuffer, status);
			pBuffer->cbBuffer = status;
			return (context->connected) ? SEC_E_OK : SEC_I_CONTINUE_NEEDED;
		}
		else
		{
			pBuffer->cbBuffer = 0;
			return (context->connected) ? SEC_E_OK : SEC_I_CONTINUE_NEEDED;
		}
	}

	return SEC_E_OK;
}

SECURITY_STATUS schannel_openssl_server_process_tokens(SCHANNEL_OPENSSL* context,
                                                       PSecBufferDesc pInput,
                                                       PSecBufferDesc pOutput)
{
	int status;
	int ssl_error;
	PSecBuffer pBuffer;

	if (!context->connected)
	{
		if (pInput->cBuffers < 1)
			return SEC_E_INVALID_TOKEN;

		pBuffer = sspi_FindSecBuffer(pInput, SECBUFFER_TOKEN);

		if (!pBuffer)
			return SEC_E_INVALID_TOKEN;

		ERR_clear_error();
		status = BIO_write(context->bioRead, pBuffer->pvBuffer, pBuffer->cbBuffer);
		if (status >= 0)
			status = SSL_accept(context->ssl);

		if (status < 0)
		{
			ssl_error = SSL_get_error(context->ssl, status);
			WLog_ERR(TAG, "SSL_accept error: %s", openssl_get_ssl_error_string(ssl_error));
			return SEC_E_INVALID_TOKEN;
		}

		if (status == 1)
			context->connected = TRUE;

		ERR_clear_error();
		status = BIO_read(context->bioWrite, context->ReadBuffer, SCHANNEL_CB_MAX_TOKEN);
		if (status < 0)
		{
			ssl_error = SSL_get_error(context->ssl, status);
			WLog_ERR(TAG, "BIO_read: %s", openssl_get_ssl_error_string(ssl_error));
			return SEC_E_INVALID_TOKEN;
		}

		if (pOutput->cBuffers < 1)
			return SEC_E_INVALID_TOKEN;

		pBuffer = sspi_FindSecBuffer(pOutput, SECBUFFER_TOKEN);

		if (!pBuffer)
			return SEC_E_INVALID_TOKEN;

		if (status > 0)
		{
			if (pBuffer->cbBuffer < (unsigned long)status)
				return SEC_E_INSUFFICIENT_MEMORY;

			CopyMemory(pBuffer->pvBuffer, context->ReadBuffer, status);
			pBuffer->cbBuffer = status;
			return (context->connected) ? SEC_E_OK : SEC_I_CONTINUE_NEEDED;
		}
		else
		{
			pBuffer->cbBuffer = 0;
			return (context->connected) ? SEC_E_OK : SEC_I_CONTINUE_NEEDED;
		}
	}

	return SEC_E_OK;
}

SECURITY_STATUS schannel_openssl_encrypt_message(SCHANNEL_OPENSSL* context, PSecBufferDesc pMessage)
{
	int status;
	int ssl_error;
	PSecBuffer pStreamBodyBuffer;
	PSecBuffer pStreamHeaderBuffer;
	PSecBuffer pStreamTrailerBuffer;
	pStreamHeaderBuffer = sspi_FindSecBuffer(pMessage, SECBUFFER_STREAM_HEADER);
	pStreamBodyBuffer = sspi_FindSecBuffer(pMessage, SECBUFFER_DATA);
	pStreamTrailerBuffer = sspi_FindSecBuffer(pMessage, SECBUFFER_STREAM_TRAILER);

	if ((!pStreamHeaderBuffer) || (!pStreamBodyBuffer) || (!pStreamTrailerBuffer))
		return SEC_E_INVALID_TOKEN;

	status = SSL_write(context->ssl, pStreamBodyBuffer->pvBuffer, pStreamBodyBuffer->cbBuffer);

	if (status < 0)
	{
		ssl_error = SSL_get_error(context->ssl, status);
		WLog_ERR(TAG, "SSL_write: %s", openssl_get_ssl_error_string(ssl_error));
	}

	ERR_clear_error();
	status = BIO_read(context->bioWrite, context->ReadBuffer, SCHANNEL_CB_MAX_TOKEN);

	if (status > 0)
	{
		size_t ustatus = (size_t)status;
		size_t length;
		size_t offset = 0;

		length =
		    (pStreamHeaderBuffer->cbBuffer > ustatus) ? ustatus : pStreamHeaderBuffer->cbBuffer;
		CopyMemory(pStreamHeaderBuffer->pvBuffer, &context->ReadBuffer[offset], length);
		ustatus -= length;
		offset += length;
		length = (pStreamBodyBuffer->cbBuffer > ustatus) ? ustatus : pStreamBodyBuffer->cbBuffer;
		CopyMemory(pStreamBodyBuffer->pvBuffer, &context->ReadBuffer[offset], length);
		ustatus -= length;
		offset += length;
		length =
		    (pStreamTrailerBuffer->cbBuffer > ustatus) ? ustatus : pStreamTrailerBuffer->cbBuffer;
		CopyMemory(pStreamTrailerBuffer->pvBuffer, &context->ReadBuffer[offset], length);
	}

	return SEC_E_OK;
}

SECURITY_STATUS schannel_openssl_decrypt_message(SCHANNEL_OPENSSL* context, PSecBufferDesc pMessage)
{
	int status;
	int length;
	BYTE* buffer;
	int ssl_error;
	PSecBuffer pBuffer;
	pBuffer = sspi_FindSecBuffer(pMessage, SECBUFFER_DATA);

	if (!pBuffer)
		return SEC_E_INVALID_TOKEN;

	ERR_clear_error();
	status = BIO_write(context->bioRead, pBuffer->pvBuffer, pBuffer->cbBuffer);
	if (status > 0)
		status = SSL_read(context->ssl, pBuffer->pvBuffer, pBuffer->cbBuffer);

	if (status < 0)
	{
		ssl_error = SSL_get_error(context->ssl, status);
		WLog_ERR(TAG, "SSL_read: %s", openssl_get_ssl_error_string(ssl_error));
	}

	length = status;
	buffer = pBuffer->pvBuffer;
	pMessage->pBuffers[0].BufferType = SECBUFFER_STREAM_HEADER;
	pMessage->pBuffers[0].cbBuffer = 5;
	pMessage->pBuffers[1].BufferType = SECBUFFER_DATA;
	pMessage->pBuffers[1].pvBuffer = buffer;
	pMessage->pBuffers[1].cbBuffer = length;
	pMessage->pBuffers[2].BufferType = SECBUFFER_STREAM_TRAILER;
	pMessage->pBuffers[2].cbBuffer = 36;
	pMessage->pBuffers[3].BufferType = SECBUFFER_EMPTY;
	pMessage->pBuffers[3].cbBuffer = 0;
	return SEC_E_OK;
}

SCHANNEL_OPENSSL* schannel_openssl_new()
{
	SCHANNEL_OPENSSL* context;
	context = (SCHANNEL_OPENSSL*)calloc(1, sizeof(SCHANNEL_OPENSSL));

	if (context != NULL)
	{
		winpr_InitializeSSL(WINPR_SSL_INIT_DEFAULT);
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

#else

int schannel_openssl_client_init(SCHANNEL_OPENSSL* context)
{
	return 0;
}

int schannel_openssl_server_init(SCHANNEL_OPENSSL* context)
{
	return 0;
}

SECURITY_STATUS schannel_openssl_client_process_tokens(SCHANNEL_OPENSSL* context,
                                                       PSecBufferDesc pInput,
                                                       PSecBufferDesc pOutput)
{
	return SEC_E_OK;
}

SECURITY_STATUS schannel_openssl_server_process_tokens(SCHANNEL_OPENSSL* context,
                                                       PSecBufferDesc pInput,
                                                       PSecBufferDesc pOutput)
{
	return SEC_E_OK;
}

SECURITY_STATUS schannel_openssl_encrypt_message(SCHANNEL_OPENSSL* context, PSecBufferDesc pMessage)
{
	return SEC_E_OK;
}

SECURITY_STATUS schannel_openssl_decrypt_message(SCHANNEL_OPENSSL* context, PSecBufferDesc pMessage)
{
	return SEC_E_OK;
}

SCHANNEL_OPENSSL* schannel_openssl_new(void)
{
	return NULL;
}

void schannel_openssl_free(SCHANNEL_OPENSSL* context)
{
}

#endif
