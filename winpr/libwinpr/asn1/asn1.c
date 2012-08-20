/**
 * WinPR: Windows Portable Runtime
 * ASN.1 Encoding & Decoding Engine
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

#include <winpr/asn1.h>

#include <winpr/crt.h>

#ifndef _WIN32

ASN1module_t ASN1_CreateModule(ASN1uint32_t nVersion, ASN1encodingrule_e eRule,
	ASN1uint32_t dwFlags, ASN1uint32_t cPDU, const ASN1GenericFun_t apfnEncoder[],
	const ASN1GenericFun_t apfnDecoder[], const ASN1FreeFun_t apfnFreeMemory[],
	const ASN1uint32_t acbStructSize[], ASN1magic_t nModuleName)
{
	ASN1module_t module = NULL;

	if (!((apfnEncoder) && (apfnDecoder) && (apfnFreeMemory) && (acbStructSize)))
		return NULL;

	module = (ASN1module_t) malloc(sizeof(struct tagASN1module_t));
	ZeroMemory(module, sizeof(struct tagASN1module_t));

	if (module)
	{
		module->nModuleName = nModuleName;
		module->dwFlags = dwFlags;
		module->eRule = eRule;
		module->cPDUs = cPDU;
		module->apfnFreeMemory = apfnFreeMemory;
		module->acbStructSize = acbStructSize;

		if (eRule & ASN1_BER_RULE)
		{
			module->BER.apfnEncoder = (const ASN1BerEncFun_t*) apfnEncoder;
			module->BER.apfnDecoder = (const ASN1BerDecFun_t*) apfnDecoder;
		}
	}

	return module;
}

void ASN1_CloseModule(ASN1module_t pModule)
{
	if (!pModule)
		free(pModule);
}

ASN1error_e ASN1_CreateEncoder(ASN1module_t pModule, ASN1encoding_t* ppEncoderInfo,
	ASN1octet_t* pbBuf, ASN1uint32_t cbBufSize, ASN1encoding_t pParent)
{
	ASN1error_e status;
	ASN1encoding_t encoder;
	ASN1encodingrule_e rule;

	if (pModule && ppEncoderInfo)
	{
		*ppEncoderInfo = 0;
		encoder = (ASN1encoding_t) malloc(sizeof(struct ASN1encoding_s));

		if (encoder)
		{
			ZeroMemory(encoder, sizeof(struct ASN1encoding_s));
			encoder->magic = 0x44434E45;
			encoder->err = 0;
			encoder->dwFlags = pModule->dwFlags;
			encoder->module = pModule;

			if (pbBuf && cbBufSize)
			{
				encoder->dwFlags |= ASN1ENCODE_SETBUFFER;
				encoder->buf = pbBuf;
				encoder->pos = pbBuf;
				encoder->size = cbBufSize;
			}

			if (pParent)
			{
				encoder[1].magic = (ASN1magic_t) pParent;
				rule = pParent->eRule;
			}
			else
			{
				encoder[1].magic = (ASN1magic_t) encoder;
				rule = pModule->eRule;
			}

			encoder->eRule = rule;

			if (encoder->dwFlags & ASN1ENCODE_SETBUFFER)
				goto LABEL_SET_BUFFER;

			if (!pParent)
			{
LABEL_ENCODER_COMPLETE:
				*ppEncoderInfo = encoder;
				return 0;
			}

			if (rule & ASN1_BER_RULE)
			{
				//if (ASN1BEREncCheck(encoder, 1))
				{
					*encoder->buf = 0;
LABEL_SET_BUFFER:
					if (pParent)
						pParent[1].version = (ASN1uint32_t) encoder;

					goto LABEL_ENCODER_COMPLETE;
				}

				status = ASN1_ERR_MEMORY;
			}
			else
			{
				status = ASN1_ERR_RULE;
			}

			free(encoder);
		}
		else
		{
			status = ASN1_ERR_MEMORY;
		}
	}
	else
	{
		status = ASN1_ERR_BADARGS;
	}

	return status;
}

ASN1error_e ASN1_Encode(ASN1encoding_t pEncoderInfo, void* pDataStruct, ASN1uint32_t nPduNum,
	ASN1uint32_t dwFlags, ASN1octet_t* pbBuf, ASN1uint32_t cbBufSize)
{
	int flags;
	ASN1error_e status;
	ASN1module_t module;
	ASN1BerEncFun_t pfnEncoder;

	if (!pEncoderInfo)
		return ASN1_ERR_BADARGS;

	ASN1EncSetError(pEncoderInfo, ASN1_SUCCESS);

	if (dwFlags & 8)
	{
		pEncoderInfo->dwFlags |= 8;
		pEncoderInfo->buf = pbBuf;
		pEncoderInfo->pos = pbBuf;
		pEncoderInfo->size = cbBufSize;
	}
	else
	{
		flags = dwFlags | pEncoderInfo->dwFlags;

		if (flags & 0x10)
		{
			pEncoderInfo->dwFlags &= 0xFFFFFFF7;
			pEncoderInfo->buf = 0;
			pEncoderInfo->pos = 0;
			pEncoderInfo->size = 0;
		}
		else
		{
			if (!(dwFlags & ASN1ENCODE_REUSEBUFFER) && (flags & ASN1ENCODE_APPEND))
				goto LABEL_MODULE;

			pEncoderInfo->pos = pEncoderInfo->buf;
		}
	}

	pEncoderInfo->len = 0;
	pEncoderInfo->bit = 0;

LABEL_MODULE:
	module = pEncoderInfo->module;

	if (nPduNum >= module->cPDUs)
		goto LABEL_BAD_PDU;

	if (!(pEncoderInfo->eRule & ASN1_BER_RULE))
	{
		status = ASN1_ERR_RULE;
		return ASN1EncSetError(pEncoderInfo, status);
	}

	pfnEncoder = module->BER.apfnEncoder[nPduNum];

	if (!pfnEncoder)
	{
LABEL_BAD_PDU:
		status = ASN1_ERR_BADPDU;
		return ASN1EncSetError(pEncoderInfo, status);
	}

	if (pfnEncoder(pEncoderInfo, 0, pDataStruct))
	{
		//ASN1BEREncFlush(pEncoderInfo);
	}
	else
	{
		if (pEncoderInfo[1].err >= 0)
			ASN1EncSetError(pEncoderInfo, ASN1_ERR_CORRUPT);
	}

	if (pEncoderInfo[1].err < 0)
	{
		if (((dwFlags & 0xFF) | (pEncoderInfo->dwFlags & 0xFF)) & 0x10)
		{
			ASN1_FreeEncoded(pEncoderInfo, pEncoderInfo->buf);
			pEncoderInfo->buf = 0;
			pEncoderInfo->pos = 0;
			pEncoderInfo->bit = 0;
			pEncoderInfo->len = 0;
			pEncoderInfo->size = 0;
		}
	}

	return pEncoderInfo[1].err;
}

void ASN1_CloseEncoder(ASN1encoding_t pEncoderInfo)
{
	ASN1magic_t magic;

	if (pEncoderInfo)
	{
		magic = pEncoderInfo[1].magic;

		if (pEncoderInfo != (ASN1encoding_t) magic)
			pEncoderInfo[1].version = 0;

		free(pEncoderInfo);
	}
}

ASN1error_e ASN1EncSetError(ASN1encoding_t enc, ASN1error_e err)
{
	ASN1error_e status;
	ASN1encoding_t encoder;
	ASN1encoding_t nextEncoder;

	status = err;
	encoder = enc;

	while (encoder)
	{
		nextEncoder = (ASN1encoding_t) &encoder[1];

		encoder->err = err;

		if (encoder == nextEncoder)
			break;

		encoder = nextEncoder;
	}

	return status;
}

ASN1error_e ASN1DecSetError(ASN1decoding_t dec, ASN1error_e err)
{
	ASN1error_e status;
	ASN1decoding_t decoder;
	ASN1decoding_t nextDecoder;

	status = err;
	decoder = dec;

	while (decoder)
	{
		nextDecoder = (ASN1decoding_t) &decoder[1];

		decoder->err = err;

		if (decoder == nextDecoder)
			break;

		decoder = nextDecoder;
	}

	return status;
}

void ASN1_FreeEncoded(ASN1encoding_t pEncoderInfo, void* pBuf)
{
	return;
}

void ASN1_FreeDecoded(ASN1decoding_t pDecoderInfo, void* pDataStruct, ASN1uint32_t nPduNum)
{
	return;
}

#endif
