/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Network Data Representation (NDR)
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

#include <freerdp/utils/memory.h>

#include "ndr.h"

/**
 * MSRPC NDR Types Technical Overview:
 * http://dvlabs.tippingpoint.com/blog/2007/11/24/msrpc-ndr-types/
 */

const char* FC_TYPE_STRINGS[] =
{
	"FC_ZERO",
	"FC_BYTE",
	"FC_CHAR",
	"FC_SMALL",
	"FC_USMALL",
	"FC_WCHAR",
	"FC_SHORT",
	"FC_USHORT",
	"FC_LONG",
	"FC_ULONG",
	"FC_FLOAT",
	"FC_HYPER",
	"FC_DOUBLE",
	"FC_ENUM16",
	"FC_ENUM32",
	"FC_IGNORE",
	"FC_ERROR_STATUS_T",
	"FC_RP",
	"FC_UP",
	"FC_OP",
	"FC_FP",
	"FC_STRUCT",
	"FC_PSTRUCT",
	"FC_CSTRUCT",
	"FC_CPSTRUCT",
	"FC_CVSTRUCT",
	"FC_BOGUS_STRUCT",
	"FC_CARRAY",
	"FC_CVARRAY",
	"FC_SMFARRAY",
	"FC_LGFARRAY",
	"FC_SMVARRAY",
	"FC_LGVARRAY",
	"FC_BOGUS_ARRAY",
	"FC_C_CSTRING",
	"FC_C_BSTRING",
	"FC_C_SSTRING",
	"FC_C_WSTRING",
	"FC_CSTRING",
	"FC_BSTRING",
	"FC_SSTRING",
	"FC_WSTRING",
	"FC_ENCAPSULATED_UNION",
	"FC_NON_ENCAPSULATED_UNION",
	"FC_BYTE_COUNT_POINTER",
	"FC_TRANSMIT_AS",
	"FC_REPRESENT_AS",
	"FC_IP",
	"FC_BIND_CONTEXT",
	"FC_BIND_GENERIC",
	"FC_BIND_PRIMITIVE",
	"FC_AUTO_HANDLE",
	"FC_CALLBACK_HANDLE",
	"FC_UNUSED1",
	"FC_POINTER",
	"FC_ALIGNM2",
	"FC_ALIGNM4",
	"FC_ALIGNM8",
	"FC_UNUSED2",
	"FC_UNUSED3",
	"FC_UNUSED4",
	"FC_STRUCTPAD1",
	"FC_STRUCTPAD2",
	"FC_STRUCTPAD3",
	"FC_STRUCTPAD4",
	"FC_STRUCTPAD5",
	"FC_STRUCTPAD6",
	"FC_STRUCTPAD7",
	"FC_STRING_SIZED",
	"FC_UNUSED5",
	"FC_NO_REPEAT",
	"FC_FIXED_REPEAT",
	"FC_VARIABLE_REPEAT",
	"FC_FIXED_OFFSET",
	"FC_VARIABLE_OFFSET",
	"FC_PP",
	"FC_EMBEDDED_COMPLEX",
	"FC_IN_PARAM",
	"FC_IN_PARAM_BASETYPE",
	"FC_IN_PARAM_NO_FREE_INST",
	"FC_IN_OUT_PARAM",
	"FC_OUT_PARAM",
	"FC_RETURN_PARAM",
	"FC_RETURN_PARAM_BASETYPE",
	"FC_DEREFERENCE",
	"FC_DIV_2",
	"FC_MULT_2",
	"FC_ADD_1",
	"FC_SUB_1",
	"FC_CALLBACK",
	"FC_CONSTANT_IID",
	"FC_END",
	"FC_PAD",
	"", "", "", "", "", "",
	"", "", "", "", "", "",
	"", "", "", "", "", "",
	"", "", "", "", "", "",
	"FC_SPLIT_DEREFERENCE",
	"FC_SPLIT_DIV_2",
	"FC_SPLIT_MULT_2",
	"FC_SPLIT_ADD_1",
	"FC_SPLIT_SUB_1",
	"FC_SPLIT_CALLBACK",
	"", "", "", "", "", "", "", "", "", "", "", "", "", "",
	"", "", "", "", "", "", "", "", "", "", "", "", "", "",
	"", "", "", "", "", "", "", "", "", "", "", "", "", "",
	"", "", "", "", "", "", "", "", "", "", "", "", "", "",
	"FC_HARD_STRUCT",
	"FC_TRANSMIT_AS_PTR",
	"FC_REPRESENT_AS_PTR",
	"FC_USER_MARSHAL",
	"FC_PIPE",
	"FC_BLKHOLE",
	"FC_RANGE",
	"FC_INT3264",
	"FC_UINT3264",
	"FC_END_OF_UNIVERSE",
};

typedef void (*NDR_TYPE_SIZE_ROUTINE)(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat);
typedef void (*NDR_TYPE_MARSHALL_ROUTINE)(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, unsigned char FormatChar);
typedef void (*NDR_TYPE_UNMARSHALL_ROUTINE)(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, unsigned char FormatChar);
typedef void (*NDR_TYPE_FREE_ROUTINE)(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat);

void NdrSimpleTypeBufferSize(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat);
void NdrSimpleTypeMarshall(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, unsigned char FormatChar);
void NdrSimpleTypeUnmarshall(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, unsigned char FormatChar);
void NdrSimpleTypeFree(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat);

void NdrPointerBufferSize(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat);

void NdrSimpleStructBufferSize(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat);

void NdrConformantStructBufferSize(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat);

void NdrConformantVaryingStructBufferSize(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat);

void NdrComplexStructBufferSize(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat);

void NdrConformantArrayBufferSize(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat);

void NdrConformantVaryingArrayBufferSize(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat);

void NdrFixedArrayBufferSize(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat);

void NdrVaryingArrayBufferSize(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat);

void NdrComplexArrayBufferSize(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat);

void NdrConformantStringBufferSize(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat);

void NdrNonConformantStringBufferSize(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat);

void NdrEncapsulatedUnionBufferSize(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat);

void NdrNonEncapsulatedUnionBufferSize(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat);

void NdrByteCountPointerBufferSize(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat);

void NdrContextHandleBufferSize(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat);

const NDR_TYPE_SIZE_ROUTINE pfnSizeRoutines[] =
{
	NULL, /* FC_ZERO */
	NdrSimpleTypeBufferSize, /* FC_BYTE */
	NdrSimpleTypeBufferSize, /* FC_CHAR */
	NdrSimpleTypeBufferSize, /* FC_SMALL */
	NdrSimpleTypeBufferSize, /* FC_USMALL */
	NdrSimpleTypeBufferSize, /* FC_WCHAR */
	NdrSimpleTypeBufferSize, /* FC_SHORT */
	NdrSimpleTypeBufferSize, /* FC_USHORT */
	NdrSimpleTypeBufferSize, /* FC_LONG */
	NdrSimpleTypeBufferSize, /* FC_ULONG */
	NdrSimpleTypeBufferSize, /* FC_FLOAT */
	NdrSimpleTypeBufferSize, /* FC_HYPER */
	NdrSimpleTypeBufferSize, /* FC_DOUBLE */
	NdrSimpleTypeBufferSize, /* FC_ENUM16 */
	NdrSimpleTypeBufferSize, /* FC_ENUM32 */
	NdrSimpleTypeBufferSize, /* FC_IGNORE */
	NdrSimpleTypeBufferSize, /* FC_ERROR_STATUS_T */
	NdrPointerBufferSize, /* FC_RP */
	NdrPointerBufferSize, /* FC_UP */
	NdrPointerBufferSize, /* FC_OP */
	NdrPointerBufferSize, /* FC_FP */
	NdrSimpleStructBufferSize, /* FC_STRUCT */
	NdrSimpleStructBufferSize, /* FC_PSTRUCT */
	NdrConformantStructBufferSize, /* FC_CSTRUCT */
	NdrConformantStructBufferSize, /* FC_CPSTRUCT */
	NdrConformantVaryingStructBufferSize, /* FC_CVSTRUCT */
	NdrComplexStructBufferSize, /* FC_BOGUS_STRUCT */
	NdrConformantArrayBufferSize, /* FC_CARRAY */
	NdrConformantVaryingArrayBufferSize, /* FC_CVARRAY */
	NdrFixedArrayBufferSize, /* FC_SMFARRAY */
	NdrFixedArrayBufferSize, /* FC_LGFARRAY */
	NdrVaryingArrayBufferSize, /* FC_SMVARRAY */
	NdrVaryingArrayBufferSize, /* FC_LGVARRAY */
	NdrComplexArrayBufferSize, /* FC_BOGUS_ARRAY */
	NdrConformantStringBufferSize, /* FC_C_CSTRING */
	NULL, /* FC_C_BSTRING */
	NULL, /* FC_C_SSTRING */
	NdrConformantStringBufferSize, /* FC_C_WSTRING */
	NdrNonConformantStringBufferSize, /* FC_CSTRING */
	NULL, /* FC_BSTRING */
	NULL, /* FC_SSTRING */
	NULL, /* FC_WSTRING */
	NdrEncapsulatedUnionBufferSize, /* FC_ENCAPSULATED_UNION */
	NdrNonEncapsulatedUnionBufferSize, /* FC_NON_ENCAPSULATED_UNION */
	NdrByteCountPointerBufferSize, /* FC_BYTE_COUNT_POINTER */
	NULL, /* FC_TRANSMIT_AS */
	NULL, /* FC_REPRESENT_AS */
	NULL, /* FC_IP */
	NdrContextHandleBufferSize, /* FC_BIND_CONTEXT */
	NULL, /* FC_BIND_GENERIC */
	NULL, /* FC_BIND_PRIMITIVE */
	NULL, /* FC_AUTO_HANDLE */
	NULL, /* FC_CALLBACK_HANDLE */
	NULL, /* FC_UNUSED1 */
	NULL, /* FC_POINTER */
	NULL, /* FC_ALIGNM2 */
	NULL, /* FC_ALIGNM4 */
	NULL, /* FC_ALIGNM8 */
	NULL, /* FC_UNUSED2 */
	NULL, /* FC_UNUSED3 */
	NULL, /* FC_UNUSED4 */
	NULL, /* FC_STRUCTPAD1 */
	NULL, /* FC_STRUCTPAD2 */
	NULL, /* FC_STRUCTPAD3 */
	NULL, /* FC_STRUCTPAD4 */
	NULL, /* FC_STRUCTPAD5 */
	NULL, /* FC_STRUCTPAD6 */
	NULL, /* FC_STRUCTPAD7 */
	NULL, /* FC_STRING_SIZED */
	NULL, /* FC_UNUSED5 */
	NULL, /* FC_NO_REPEAT */
	NULL, /* FC_FIXED_REPEAT */
	NULL, /* FC_VARIABLE_REPEAT */
	NULL, /* FC_FIXED_OFFSET */
	NULL, /* FC_VARIABLE_OFFSET */
	NULL, /* FC_PP */
	NULL, /* FC_EMBEDDED_COMPLEX */
	NULL, /* FC_IN_PARAM */
	NULL, /* FC_IN_PARAM_BASETYPE */
	NULL, /* FC_IN_PARAM_NO_FREE_INST */
	NULL, /* FC_IN_OUT_PARAM */
	NULL, /* FC_OUT_PARAM */
	NULL, /* FC_RETURN_PARAM */
	NULL, /* FC_RETURN_PARAM_BASETYPE */
	NULL, /* FC_DEREFERENCE */
	NULL, /* FC_DIV_2 */
	NULL, /* FC_MULT_2 */
	NULL, /* FC_ADD_1 */
	NULL, /* FC_SUB_1 */
	NULL, /* FC_CALLBACK */
	NULL, /* FC_CONSTANT_IID */
	NULL, /* FC_END */
	NULL, /* FC_PAD */
};

const NDR_TYPE_MARSHALL_ROUTINE pfnMarshallRoutines[] =
{
	NULL, /* FC_ZERO */
	NdrSimpleTypeMarshall, /* FC_BYTE */
	NdrSimpleTypeMarshall, /* FC_CHAR */
	NdrSimpleTypeMarshall, /* FC_SMALL */
	NdrSimpleTypeMarshall, /* FC_USMALL */
	NdrSimpleTypeMarshall, /* FC_WCHAR */
	NdrSimpleTypeMarshall, /* FC_SHORT */
	NdrSimpleTypeMarshall, /* FC_USHORT */
	NdrSimpleTypeMarshall, /* FC_LONG */
	NdrSimpleTypeMarshall, /* FC_ULONG */
	NdrSimpleTypeMarshall, /* FC_FLOAT */
	NdrSimpleTypeMarshall, /* FC_HYPER */
	NdrSimpleTypeMarshall, /* FC_DOUBLE */
	NdrSimpleTypeMarshall, /* FC_ENUM16 */
	NdrSimpleTypeMarshall, /* FC_ENUM32 */
	NdrSimpleTypeMarshall, /* FC_IGNORE */
	NULL, /* FC_ERROR_STATUS_T */
	NULL, /* FC_RP */
	NULL, /* FC_UP */
	NULL, /* FC_OP */
	NULL, /* FC_FP */
	NULL, /* FC_STRUCT */
	NULL, /* FC_PSTRUCT */
	NULL, /* FC_CSTRUCT */
	NULL, /* FC_CPSTRUCT */
	NULL, /* FC_CVSTRUCT */
	NULL, /* FC_BOGUS_STRUCT */
	NULL, /* FC_CARRAY */
	NULL, /* FC_CVARRAY */
	NULL, /* FC_SMFARRAY */
	NULL, /* FC_LGFARRAY */
	NULL, /* FC_SMVARRAY */
	NULL, /* FC_LGVARRAY */
	NULL, /* FC_BOGUS_ARRAY */
	NULL, /* FC_C_CSTRING */
	NULL, /* FC_C_BSTRING */
	NULL, /* FC_C_SSTRING */
	NULL, /* FC_C_WSTRING */
	NULL, /* FC_CSTRING */
	NULL, /* FC_BSTRING */
	NULL, /* FC_SSTRING */
	NULL, /* FC_WSTRING */
	NULL, /* FC_ENCAPSULATED_UNION */
	NULL, /* FC_NON_ENCAPSULATED_UNION */
	NULL, /* FC_BYTE_COUNT_POINTER */
	NULL, /* FC_TRANSMIT_AS */
	NULL, /* FC_REPRESENT_AS */
	NULL, /* FC_IP */
	NULL, /* FC_BIND_CONTEXT */
	NULL, /* FC_BIND_GENERIC */
	NULL, /* FC_BIND_PRIMITIVE */
	NULL, /* FC_AUTO_HANDLE */
	NULL, /* FC_CALLBACK_HANDLE */
	NULL, /* FC_UNUSED1 */
	NULL, /* FC_POINTER */
	NULL, /* FC_ALIGNM2 */
	NULL, /* FC_ALIGNM4 */
	NULL, /* FC_ALIGNM8 */
	NULL, /* FC_UNUSED2 */
	NULL, /* FC_UNUSED3 */
	NULL, /* FC_UNUSED4 */
	NULL, /* FC_STRUCTPAD1 */
	NULL, /* FC_STRUCTPAD2 */
	NULL, /* FC_STRUCTPAD3 */
	NULL, /* FC_STRUCTPAD4 */
	NULL, /* FC_STRUCTPAD5 */
	NULL, /* FC_STRUCTPAD6 */
	NULL, /* FC_STRUCTPAD7 */
	NULL, /* FC_STRING_SIZED */
	NULL, /* FC_UNUSED5 */
	NULL, /* FC_NO_REPEAT */
	NULL, /* FC_FIXED_REPEAT */
	NULL, /* FC_VARIABLE_REPEAT */
	NULL, /* FC_FIXED_OFFSET */
	NULL, /* FC_VARIABLE_OFFSET */
	NULL, /* FC_PP */
	NULL, /* FC_EMBEDDED_COMPLEX */
	NULL, /* FC_IN_PARAM */
	NULL, /* FC_IN_PARAM_BASETYPE */
	NULL, /* FC_IN_PARAM_NO_FREE_INST */
	NULL, /* FC_IN_OUT_PARAM */
	NULL, /* FC_OUT_PARAM */
	NULL, /* FC_RETURN_PARAM */
	NULL, /* FC_RETURN_PARAM_BASETYPE */
	NULL, /* FC_DEREFERENCE */
	NULL, /* FC_DIV_2 */
	NULL, /* FC_MULT_2 */
	NULL, /* FC_ADD_1 */
	NULL, /* FC_SUB_1 */
	NULL, /* FC_CALLBACK */
	NULL, /* FC_CONSTANT_IID */
	NULL, /* FC_END */
	NULL, /* FC_PAD */
};

const NDR_TYPE_UNMARSHALL_ROUTINE pfnUnmarshallRoutines[] =
{
	NULL, /* FC_ZERO */
	NdrSimpleTypeUnmarshall, /* FC_BYTE */
	NdrSimpleTypeUnmarshall, /* FC_CHAR */
	NdrSimpleTypeUnmarshall, /* FC_SMALL */
	NdrSimpleTypeUnmarshall, /* FC_USMALL */
	NdrSimpleTypeUnmarshall, /* FC_WCHAR */
	NdrSimpleTypeUnmarshall, /* FC_SHORT */
	NdrSimpleTypeUnmarshall, /* FC_USHORT */
	NdrSimpleTypeUnmarshall, /* FC_LONG */
	NdrSimpleTypeUnmarshall, /* FC_ULONG */
	NdrSimpleTypeUnmarshall, /* FC_FLOAT */
	NdrSimpleTypeUnmarshall, /* FC_HYPER */
	NdrSimpleTypeUnmarshall, /* FC_DOUBLE */
	NdrSimpleTypeUnmarshall, /* FC_ENUM16 */
	NdrSimpleTypeUnmarshall, /* FC_ENUM32 */
	NdrSimpleTypeUnmarshall, /* FC_IGNORE */
	NULL, /* FC_ERROR_STATUS_T */
	NULL, /* FC_RP */
	NULL, /* FC_UP */
	NULL, /* FC_OP */
	NULL, /* FC_FP */
	NULL, /* FC_STRUCT */
	NULL, /* FC_PSTRUCT */
	NULL, /* FC_CSTRUCT */
	NULL, /* FC_CPSTRUCT */
	NULL, /* FC_CVSTRUCT */
	NULL, /* FC_BOGUS_STRUCT */
	NULL, /* FC_CARRAY */
	NULL, /* FC_CVARRAY */
	NULL, /* FC_SMFARRAY */
	NULL, /* FC_LGFARRAY */
	NULL, /* FC_SMVARRAY */
	NULL, /* FC_LGVARRAY */
	NULL, /* FC_BOGUS_ARRAY */
	NULL, /* FC_C_CSTRING */
	NULL, /* FC_C_BSTRING */
	NULL, /* FC_C_SSTRING */
	NULL, /* FC_C_WSTRING */
	NULL, /* FC_CSTRING */
	NULL, /* FC_BSTRING */
	NULL, /* FC_SSTRING */
	NULL, /* FC_WSTRING */
	NULL, /* FC_ENCAPSULATED_UNION */
	NULL, /* FC_NON_ENCAPSULATED_UNION */
	NULL, /* FC_BYTE_COUNT_POINTER */
	NULL, /* FC_TRANSMIT_AS */
	NULL, /* FC_REPRESENT_AS */
	NULL, /* FC_IP */
	NULL, /* FC_BIND_CONTEXT */
	NULL, /* FC_BIND_GENERIC */
	NULL, /* FC_BIND_PRIMITIVE */
	NULL, /* FC_AUTO_HANDLE */
	NULL, /* FC_CALLBACK_HANDLE */
	NULL, /* FC_UNUSED1 */
	NULL, /* FC_POINTER */
	NULL, /* FC_ALIGNM2 */
	NULL, /* FC_ALIGNM4 */
	NULL, /* FC_ALIGNM8 */
	NULL, /* FC_UNUSED2 */
	NULL, /* FC_UNUSED3 */
	NULL, /* FC_UNUSED4 */
	NULL, /* FC_STRUCTPAD1 */
	NULL, /* FC_STRUCTPAD2 */
	NULL, /* FC_STRUCTPAD3 */
	NULL, /* FC_STRUCTPAD4 */
	NULL, /* FC_STRUCTPAD5 */
	NULL, /* FC_STRUCTPAD6 */
	NULL, /* FC_STRUCTPAD7 */
	NULL, /* FC_STRING_SIZED */
	NULL, /* FC_UNUSED5 */
	NULL, /* FC_NO_REPEAT */
	NULL, /* FC_FIXED_REPEAT */
	NULL, /* FC_VARIABLE_REPEAT */
	NULL, /* FC_FIXED_OFFSET */
	NULL, /* FC_VARIABLE_OFFSET */
	NULL, /* FC_PP */
	NULL, /* FC_EMBEDDED_COMPLEX */
	NULL, /* FC_IN_PARAM */
	NULL, /* FC_IN_PARAM_BASETYPE */
	NULL, /* FC_IN_PARAM_NO_FREE_INST */
	NULL, /* FC_IN_OUT_PARAM */
	NULL, /* FC_OUT_PARAM */
	NULL, /* FC_RETURN_PARAM */
	NULL, /* FC_RETURN_PARAM_BASETYPE */
	NULL, /* FC_DEREFERENCE */
	NULL, /* FC_DIV_2 */
	NULL, /* FC_MULT_2 */
	NULL, /* FC_ADD_1 */
	NULL, /* FC_SUB_1 */
	NULL, /* FC_CALLBACK */
	NULL, /* FC_CONSTANT_IID */
	NULL, /* FC_END */
	NULL, /* FC_PAD */
};

const NDR_TYPE_FREE_ROUTINE pfnFreeRoutines[] =
{
	NULL, /* FC_ZERO */
	NdrSimpleTypeFree, /* FC_BYTE */
	NdrSimpleTypeFree, /* FC_CHAR */
	NdrSimpleTypeFree, /* FC_SMALL */
	NdrSimpleTypeFree, /* FC_USMALL */
	NdrSimpleTypeFree, /* FC_WCHAR */
	NdrSimpleTypeFree, /* FC_SHORT */
	NdrSimpleTypeFree, /* FC_USHORT */
	NdrSimpleTypeFree, /* FC_LONG */
	NdrSimpleTypeFree, /* FC_ULONG */
	NdrSimpleTypeFree, /* FC_FLOAT */
	NdrSimpleTypeFree, /* FC_HYPER */
	NdrSimpleTypeFree, /* FC_DOUBLE */
	NdrSimpleTypeFree, /* FC_ENUM16 */
	NdrSimpleTypeFree, /* FC_ENUM32 */
	NdrSimpleTypeFree, /* FC_IGNORE */
	NULL, /* FC_ERROR_STATUS_T */
	NULL, /* FC_RP */
	NULL, /* FC_UP */
	NULL, /* FC_OP */
	NULL, /* FC_FP */
	NULL, /* FC_STRUCT */
	NULL, /* FC_PSTRUCT */
	NULL, /* FC_CSTRUCT */
	NULL, /* FC_CPSTRUCT */
	NULL, /* FC_CVSTRUCT */
	NULL, /* FC_BOGUS_STRUCT */
	NULL, /* FC_CARRAY */
	NULL, /* FC_CVARRAY */
	NULL, /* FC_SMFARRAY */
	NULL, /* FC_LGFARRAY */
	NULL, /* FC_SMVARRAY */
	NULL, /* FC_LGVARRAY */
	NULL, /* FC_BOGUS_ARRAY */
	NULL, /* FC_C_CSTRING */
	NULL, /* FC_C_BSTRING */
	NULL, /* FC_C_SSTRING */
	NULL, /* FC_C_WSTRING */
	NULL, /* FC_CSTRING */
	NULL, /* FC_BSTRING */
	NULL, /* FC_SSTRING */
	NULL, /* FC_WSTRING */
	NULL, /* FC_ENCAPSULATED_UNION */
	NULL, /* FC_NON_ENCAPSULATED_UNION */
	NULL, /* FC_BYTE_COUNT_POINTER */
	NULL, /* FC_TRANSMIT_AS */
	NULL, /* FC_REPRESENT_AS */
	NULL, /* FC_IP */
	NULL, /* FC_BIND_CONTEXT */
	NULL, /* FC_BIND_GENERIC */
	NULL, /* FC_BIND_PRIMITIVE */
	NULL, /* FC_AUTO_HANDLE */
	NULL, /* FC_CALLBACK_HANDLE */
	NULL, /* FC_UNUSED1 */
	NULL, /* FC_POINTER */
	NULL, /* FC_ALIGNM2 */
	NULL, /* FC_ALIGNM4 */
	NULL, /* FC_ALIGNM8 */
	NULL, /* FC_UNUSED2 */
	NULL, /* FC_UNUSED3 */
	NULL, /* FC_UNUSED4 */
	NULL, /* FC_STRUCTPAD1 */
	NULL, /* FC_STRUCTPAD2 */
	NULL, /* FC_STRUCTPAD3 */
	NULL, /* FC_STRUCTPAD4 */
	NULL, /* FC_STRUCTPAD5 */
	NULL, /* FC_STRUCTPAD6 */
	NULL, /* FC_STRUCTPAD7 */
	NULL, /* FC_STRING_SIZED */
	NULL, /* FC_UNUSED5 */
	NULL, /* FC_NO_REPEAT */
	NULL, /* FC_FIXED_REPEAT */
	NULL, /* FC_VARIABLE_REPEAT */
	NULL, /* FC_FIXED_OFFSET */
	NULL, /* FC_VARIABLE_OFFSET */
	NULL, /* FC_PP */
	NULL, /* FC_EMBEDDED_COMPLEX */
	NULL, /* FC_IN_PARAM */
	NULL, /* FC_IN_PARAM_BASETYPE */
	NULL, /* FC_IN_PARAM_NO_FREE_INST */
	NULL, /* FC_IN_OUT_PARAM */
	NULL, /* FC_OUT_PARAM */
	NULL, /* FC_RETURN_PARAM */
	NULL, /* FC_RETURN_PARAM_BASETYPE */
	NULL, /* FC_DEREFERENCE */
	NULL, /* FC_DIV_2 */
	NULL, /* FC_MULT_2 */
	NULL, /* FC_ADD_1 */
	NULL, /* FC_SUB_1 */
	NULL, /* FC_CALLBACK */
	NULL, /* FC_CONSTANT_IID */
	NULL, /* FC_END */
	NULL, /* FC_PAD */
};

static void AlignLength(unsigned long* length, unsigned int alignment)
{
	*length = (*length + alignment - 1) & ~(alignment - 1);
}

static void IncrementLength(unsigned long* length, unsigned int size)
{
	*length += size;
}

void NdrSimpleTypeBufferSize(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat)
{
	switch (*pFormat)
	{
		case FC_BYTE:
		case FC_CHAR:
		case FC_SMALL:
		case FC_USMALL:
			IncrementLength(&(pStubMsg->BufferLength), sizeof(BYTE));
			break;

		case FC_WCHAR:
		case FC_SHORT:
		case FC_USHORT:
		case FC_ENUM16:
			AlignLength(&(pStubMsg->BufferLength), sizeof(USHORT));
			IncrementLength(&(pStubMsg->BufferLength), sizeof(USHORT));
			break;

		case FC_LONG:
		case FC_ULONG:
		case FC_ENUM32:
		case FC_INT3264:
		case FC_UINT3264:
			AlignLength(&(pStubMsg->BufferLength), sizeof(ULONG));
			IncrementLength(&(pStubMsg->BufferLength), sizeof(ULONG));
			break;

		case FC_FLOAT:
			AlignLength(&(pStubMsg->BufferLength), sizeof(FLOAT));
			IncrementLength(&(pStubMsg->BufferLength), sizeof(FLOAT));
			break;

		case FC_DOUBLE:
			AlignLength(&(pStubMsg->BufferLength), sizeof(DOUBLE));
			IncrementLength(&(pStubMsg->BufferLength), sizeof(DOUBLE));
			break;

		case FC_HYPER:
			AlignLength(&(pStubMsg->BufferLength), sizeof(ULONGLONG));
			IncrementLength(&(pStubMsg->BufferLength), sizeof(ULONGLONG));
			break;

		case FC_ERROR_STATUS_T:
			AlignLength(&(pStubMsg->BufferLength), sizeof(error_status_t));
			IncrementLength(&(pStubMsg->BufferLength), sizeof(error_status_t));
			break;

		case FC_IGNORE:
			break;
	}
}

/* Pointers: http://msdn.microsoft.com/en-us/library/windows/desktop/hh802750/ */

void NdrPointerBufferSize(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat)
{
	/**
	 * pointer_type<1>
	 * pointer_attributes<1>
	 * simple_type<1>
	 * FC_PAD
	 */

	/**
	 * pointer_type<1>
	 * pointer_attributes<1>
	 * offset_to_complex_description<2>
	 */

	unsigned char type;
	unsigned char attributes;
	PFORMAT_STRING pNextFormat;
	NDR_TYPE_SIZE_ROUTINE pfnSizeRoutine;

	type = pFormat[0];
	attributes = pFormat[1];
	pFormat += 2;

	if (type != FC_RP)
	{
		AlignLength((&pStubMsg->BufferLength), 4);
		IncrementLength((&pStubMsg->BufferLength), 4);
	}

	if (attributes & FC_SIMPLE_POINTER)
		pNextFormat = pFormat;
	else
		pNextFormat = pFormat + *(SHORT*) pFormat;

	switch (type)
	{
		case FC_RP: /* Reference Pointer */
			break;

		case FC_UP: /* Unique Pointer */
		case FC_OP: /* Unique Pointer in an object interface */

			if (!pMemory)
				return;

			break;

		case FC_FP: /* Full Pointer */
			printf("warning: FC_FP unimplemented\n");
			break;
	}

	if (attributes & FC_POINTER_DEREF)
		pMemory = *(unsigned char**) pMemory;

	pfnSizeRoutine = pfnSizeRoutines[*pNextFormat];

	if (pfnSizeRoutine)
		pfnSizeRoutine(pStubMsg, pMemory, pNextFormat);
}

/* Structures: http://msdn.microsoft.com/en-us/library/windows/desktop/aa378695/ */

void NdrSimpleStructBufferSize(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat)
{
	/**
	 * FC_STRUCT alignment<1>
	 * memory_size<2>
	 * member_layout<>
	 * FC_END
	 */

	/**
	 * FC_PSTRUCT alignment<1>
	 * memory_size<2>
	 * pointer_layout<>
	 * member_layout<>
	 * FC_END
	 */

	printf("warning: NdrSimpleStructBufferSize unimplemented\n");
}

void NdrConformantStructBufferSize(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat)
{
	/**
	 * FC_CSTRUCT alignment<1>
	 * memory_size<2>
	 * offset_to_array_description<2>
	 * member_layout<>
	 * FC_END
	 */

	/**
	 * FC_CPSTRUCT alignment<1>
	 * memory_size<2>
	 * offset_to_array_description<2>
	 * pointer_layout<>
	 * member_layout<> FC_END
	 */

	printf("warning: NdrConformantStructBufferSize unimplemented\n");
}

void NdrConformantVaryingStructBufferSize(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat)
{
	/**
	 * FC_CVSTRUCT alignment<1>
	 * memory_size<2>
	 * offset_to_array_description<2>
	 * [pointer_layout<>]
	 * layout<>
	 * FC_END
	 */

	printf("warning: NdrConformantVaryingStructBufferSize unimplemented\n");
}

void NdrComplexStructBufferSize(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat)
{
	/**
	 * FC_BOGUS_STRUCT
	 * alignment<1>
	 * memory_size<2>
	 * offset_to_conformant_array_description<2>
	 * offset_to_pointer_layout<2>
	 * member_layout<>
	 * FC_END
	 * [pointer_layout<>]
	 */

	unsigned char type;
	unsigned char alignment;
	unsigned short memory_size;

	type = pFormat[0];

	if (type != FC_BOGUS_STRUCT)
	{
		printf("error: expected FC_BOGUS_STRUCT, got 0x%02X\n", type);
	}

	alignment = pFormat[1] + 1;
	memory_size = *(unsigned short*) &pFormat[2];

	AlignLength(&(pStubMsg->BufferLength), alignment);

	if (!pStubMsg->IgnoreEmbeddedPointers && !pStubMsg->PointerLength)
	{

	}

	printf("warning: NdrComplexStructBufferSize unimplemented\n");
}

void NdrConformantArrayBufferSize(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat)
{
	/**
	 * FC_CARRAY alignment<1>
	 * element_size<2>
	 * conformance_description<>
	 * [pointer_layout<>]
	 * element_description<>
	 * FC_END
	 */

	printf("warning: NdrConformantArrayBufferSize unimplemented\n");
}

void NdrConformantVaryingArrayBufferSize(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat)
{
	/**
	 * FC_CVARRAY alignment<1>
	 * element_size<2>
	 * conformance_description<>
	 * variance_description<>
	 * [pointer_layout<>]
	 * element_description<>
	 * FC_END
	 */

	printf("warning: NdrConformantVaryingArrayBufferSize unimplemented\n");
}

void NdrFixedArrayBufferSize(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat)
{
	/**
	 * FC_SMFARRAY alignment<1>
	 * total_size<2>
	 * [pointer_layout<>]
	 * element_description<>
	 * FC_END
	 */

	/**
	 * FC_LGFARRAY alignment<1>
	 * total_size<4>
	 * [pointer_layout<>]
	 * element_description<>
	 * FC_END
	 */

	printf("warning: NdrFixedArrayBufferSize unimplemented\n");
}

void NdrVaryingArrayBufferSize(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat)
{
	/**
	 * FC_SMVARRAY alignment<1>
	 * total_size<2>
	 * number_elements<2>
	 * element_size<2>
	 * variance_description<>
	 * [pointer_layout<>]
	 * element_description<>
	 * FC_END
	 */

	/**
	 * FC_LGVARRAY alignment<1>
	 * total_size<4>
	 * number_elements<4>
	 * element_size<2>
	 * variance_description<4>
	 * [pointer_layout<>]
	 * element_description<>
	 * FC_END
	 */

	printf("warning: NdrVaryingArrayBufferSize unimplemented\n");
}

void NdrComplexArrayBufferSize(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat)
{
	/**
	 * FC_BOGUS_ARRAY alignment<1>
	 * number_of_elements<2>
	 * conformance_description<>
	 * variance_description<>
	 * element_description<>
	 * FC_END
	 */

	printf("warning: NdrComplexArrayBufferSize unimplemented\n");
}

void NdrConformantStringBufferSize(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat)
{
	printf("warning: NdrConformantStringBufferSize unimplemented\n");
}

void NdrNonConformantStringBufferSize(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat)
{
	printf("warning: NdrNonConformantStringBufferSize unimplemented\n");
}

void NdrEncapsulatedUnionBufferSize(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat)
{
	printf("warning: NdrEncapsulatedUnionBufferSize unimplemented\n");
}

void NdrNonEncapsulatedUnionBufferSize(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat)
{
	printf("warning: NdrNonEncapsulatedUnionBufferSize unimplemented\n");
}

void NdrByteCountPointerBufferSize(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat)
{
	printf("warning: NdrByteCountPointerBufferSize unimplemented\n");
}

void NdrContextHandleBufferSize(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat)
{
	unsigned char type = *pFormat;

	if (type == FC_BIND_PRIMITIVE)
	{
		/**
		 * FC_BIND_PRIMITIVE
		 * flag<1>
		 * offset<2>
		 */

		printf("warning: NdrContextHandleBufferSize FC_BIND_PRIMITIVE unimplemented\n");
	}
	else if (type == FC_BIND_GENERIC)
	{
		/**
		 * FC_BIND_GENERIC
		 * flag_and_size<1>
		 * offset<2>
		 * binding_routine_pair_index<1>
		 * FC_PAD
		 */

		printf("warning: NdrContextHandleBufferSize FC_BIND_GENERIC unimplemented\n");
	}
	else if (type == FC_BIND_CONTEXT)
	{
		/**
		 * FC_BIND_CONTEXT
		 * flags<1>
		 * offset<2>
		 * context_rundown_routine_index<1>
		 * param_num<1>
		 */

		AlignLength(&(pStubMsg->BufferLength), 4);
		IncrementLength(&(pStubMsg->BufferLength), 20);
	}
}

void NdrSimpleTypeMarshall(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, unsigned char FormatChar)
{

}

void NdrSimpleTypeUnmarshall(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, unsigned char FormatChar)
{

}

void NdrSimpleTypeFree(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat)
{

}

void NdrPrintParamAttributes(PARAM_ATTRIBUTES attributes)
{
	if (attributes.ServerAllocSize)
		printf("ServerAllocSize, ");
	if (attributes.SaveForAsyncFinish)
		printf("SaveForAsyncFinish, ");
	if (attributes.IsDontCallFreeInst)
		printf("IsDontCallFreeInst, ");
	if (attributes.IsSimpleRef)
		printf("IsSimpleRef, ");
	if (attributes.IsByValue)
		printf("IsByValue, ");
	if (attributes.IsBasetype)
		printf("IsBaseType, ");
	if (attributes.IsReturn)
		printf("IsReturn, ");
	if (attributes.IsOut)
		printf("IsOut, ");
	if (attributes.IsIn)
		printf("IsIn, ");
	if (attributes.IsPipe)
		printf("IsPipe, ");
	if (attributes.MustFree)
		printf("MustFree, ");
	if (attributes.MustSize)
		printf("MustSize, ");
}

void NdrProcessParam(PMIDL_STUB_MESSAGE pStubMsg, NDR_PHASE phase, unsigned char* pMemory, NDR_PARAM* param)
{
	unsigned char type;
	PFORMAT_STRING pFormat;

	/* Parameter Descriptors: http://msdn.microsoft.com/en-us/library/windows/desktop/aa374362/ */

	if (param->Attributes.IsBasetype)
	{
		pFormat = &param->Type.FormatChar;

		if (param->Attributes.IsSimpleRef)
			pMemory = *(unsigned char**) pMemory;
	}
	else
	{
		pFormat = &pStubMsg->StubDesc->pFormatTypes[param->Type.Offset];

		if (!(param->Attributes.IsByValue))
			pMemory = *(unsigned char**) pMemory;
	}

	type = (pFormat[0] & 0x7F);

	if (type > FC_PAD)
		return;

	if (phase == NDR_PHASE_SIZE)
	{
		NDR_TYPE_SIZE_ROUTINE pfnSizeRoutine = pfnSizeRoutines[type];

		if (pfnSizeRoutine)
			pfnSizeRoutine(pStubMsg, pMemory, pFormat);
	}
	else if (phase == NDR_PHASE_MARSHALL)
	{
		NDR_TYPE_MARSHALL_ROUTINE pfnMarshallRoutine = pfnMarshallRoutines[type];

		if (pfnMarshallRoutine)
			pfnMarshallRoutine(pStubMsg, pMemory, *pFormat);
	}
	else if (phase == NDR_PHASE_UNMARSHALL)
	{
		NDR_TYPE_UNMARSHALL_ROUTINE pfnUnmarshallRoutine = pfnUnmarshallRoutines[type];

		if (pfnUnmarshallRoutine)
			pfnUnmarshallRoutine(pStubMsg, pMemory, *pFormat);
	}
	else if (phase == NDR_PHASE_FREE)
	{
		NDR_TYPE_FREE_ROUTINE pfnFreeRoutine = pfnFreeRoutines[type];

		if (pfnFreeRoutine)
			pfnFreeRoutine(pStubMsg, pMemory, pFormat);
	}
}

void NdrProcessParams(PMIDL_STUB_MESSAGE pStubMsg, PFORMAT_STRING pFormat, NDR_PHASE phase, void** fpuArgs, unsigned short numberParams)
{
	unsigned int i;
	NDR_PARAM* params;
	PFORMAT_STRING fmt;
	unsigned char* arg;
	unsigned char type;

	params = (NDR_PARAM*) pFormat;

	printf("Params = \n{\n");

	for (i = 0; i < numberParams; i++)
	{
		float tmp;
		arg = pStubMsg->StackTop + params[i].StackOffset;
		fmt = (PFORMAT_STRING) &pStubMsg->StubDesc->pFormatTypes[params[i].Type.Offset];

#ifdef __x86_64__
		if ((params[i].Attributes.IsBasetype) &&
				!(params[i].Attributes.IsSimpleRef) &&
				((params[i].Type.FormatChar) == FC_FLOAT) && !fpuArgs)
		{
			tmp = *(double*) arg;
			arg = (unsigned char*) &tmp;
		}
#endif

		printf("\t#%d\t", i);

		type = (params[i].Attributes.IsBasetype) ? params[i].Type.FormatChar : *fmt;

		printf(" type %s (0x%02X) ", FC_TYPE_STRINGS[type], type);

		NdrPrintParamAttributes(params[i].Attributes);

		if (params[i].Attributes.IsIn)
		{
			NdrProcessParam(pStubMsg, phase, arg, &params[i]);
		}

		printf("\n");
	}

	printf("}\n");
}

void NdrClientInitializeNew(PRPC_MESSAGE pRpcMessage, PMIDL_STUB_MESSAGE pStubMsg,
		PMIDL_STUB_DESC pStubDesc, unsigned int ProcNum)
{
	pRpcMessage->Handle = NULL;
	pRpcMessage->RpcFlags = 0;
	pRpcMessage->ProcNum = ProcNum;
	pRpcMessage->DataRepresentation = 0;
	pRpcMessage->ReservedForRuntime = NULL;
	pRpcMessage->RpcInterfaceInformation = pStubDesc->RpcInterfaceInformation;

	pStubMsg->RpcMsg = pRpcMessage;
	pStubMsg->BufferStart = NULL;
	pStubMsg->BufferEnd = NULL;
	pStubMsg->BufferLength = 0;
	pStubMsg->StackTop = NULL;
	pStubMsg->StubDesc = pStubDesc;
	pStubMsg->IgnoreEmbeddedPointers = 0;
	pStubMsg->PointerLength = 0;
}

void NdrPrintOptFlags(INTERPRETER_OPT_FLAGS optFlags)
{
	if (optFlags.ClientMustSize)
		printf("ClientMustSize, ");
	if (optFlags.ServerMustSize)
		printf("ServerMustSize, ");
	if (optFlags.HasAsyncUuid)
		printf("HasAsyncUiid, ");
	if (optFlags.HasAsyncHandle)
		printf("HasAsyncHandle, ");
	if (optFlags.HasReturn)
		printf("HasReturn, ");
	if (optFlags.HasPipes)
		printf("HasPipes, ");
	if (optFlags.HasExtensions)
		printf("HasExtensions, ");
}

void NdrPrintExtFlags(INTERPRETER_OPT_FLAGS2 extFlags)
{
	if (extFlags.HasNewCorrDesc)
		printf("HasNewCorrDesc, ");
	if (extFlags.ClientCorrCheck)
		printf("ClientCorrCheck, ");
	if (extFlags.ServerCorrCheck)
		printf("ServerCorrCheck, ");
	if (extFlags.HasNotify)
		printf("HasNotify, ");
	if (extFlags.HasNotify2)
		printf("HasNotify2, ");
}

CLIENT_CALL_RETURN NdrClientCall(PMIDL_STUB_DESC pStubDescriptor, PFORMAT_STRING pFormat, void** stackTop, void** fpuStack)
{
	RPC_MESSAGE rpcMsg;
	unsigned short procNum;
	unsigned short stackSize;
	unsigned char numberParams;
	unsigned char handleType;
	MIDL_STUB_MESSAGE stubMsg;
	INTERPRETER_FLAGS flags;
	INTERPRETER_OPT_FLAGS optFlags;
	INTERPRETER_OPT_FLAGS2 extFlags;
	NDR_PROC_HEADER* procHeader;
	NDR_OI2_PROC_HEADER* oi2ProcHeader;
	CLIENT_CALL_RETURN client_call_return;

	procNum = stackSize = numberParams = 0;
	procHeader = (NDR_PROC_HEADER*) &pFormat[0];

	client_call_return.Pointer = NULL;

	handleType = procHeader->HandleType;
	flags = procHeader->OldOiFlags;
	procNum = procHeader->ProcNum;
	stackSize = procHeader->StackSize;
	pFormat += sizeof(NDR_PROC_HEADER);

	/* The Header: http://msdn.microsoft.com/en-us/library/windows/desktop/aa378707/ */
	/* Procedure Header Descriptor: http://msdn.microsoft.com/en-us/library/windows/desktop/aa374387/ */
	/* Handles: http://msdn.microsoft.com/en-us/library/windows/desktop/aa373932/ */

	printf("Oi Header: HandleType: 0x%02X OiFlags: 0x%02X ProcNum: %d StackSize: 0x%04X\n",
			handleType, *((unsigned char*) &flags),
			(unsigned short) procNum, (unsigned short) stackSize);

	if (handleType > 0)
	{
		/* implicit handle */
		printf("Implicit Handle\n");
		oi2ProcHeader = (NDR_OI2_PROC_HEADER*) &pFormat[0];
		pFormat += sizeof(NDR_OI2_PROC_HEADER);
	}
	else
	{
		/* explicit handle */
		printf("Explicit Handle\n");
		oi2ProcHeader = (NDR_OI2_PROC_HEADER*) &pFormat[6];
		pFormat += sizeof(NDR_OI2_PROC_HEADER) + 6;
	}

	optFlags = oi2ProcHeader->Oi2Flags;
	numberParams = oi2ProcHeader->NumberParams;

	printf("Oi2 Header: Oi2Flags: 0x%02X, NumberParams: %d ClientBufferSize: %d ServerBufferSize: %d\n",
			*((unsigned char*) &optFlags),
			(unsigned char) numberParams,
			oi2ProcHeader->ClientBufferSize,
			oi2ProcHeader->ServerBufferSize);

	printf("Oi2Flags: ");
	NdrPrintOptFlags(optFlags);
	printf("\n");

	NdrClientInitializeNew(&rpcMsg, &stubMsg, pStubDescriptor, procNum);

	if (optFlags.HasExtensions)
	{
		NDR_PROC_HEADER_EXTS* extensions = (NDR_PROC_HEADER_EXTS*) pFormat;

		pFormat += extensions->Size;
		extFlags = extensions->Flags2;

		printf("Extensions: Size: %d, flags2: 0x%02X\n",
				extensions->Size, *((unsigned char*) &extensions->Flags2));

#ifdef __x86_64__
		if (extensions->Size > sizeof(*extensions) && fpuStack)
		{
			int i;
			unsigned short fpuMask = *(unsigned short*) (extensions + 1);

			for (i = 0; i < 4; i++, fpuMask >>= 2)
			{
				switch (fpuMask & 3)
				{
					case 1: *(float*) &stackTop[i] = *(float*) &fpuStack[i];
						break;

					case 2: *(double*) &stackTop[i] = *(double*) &fpuStack[i];
						break;
				}
			}
		}
#endif
	}

	stubMsg.StackTop = (unsigned char*) stackTop;

	printf("ExtFlags: ");
	NdrPrintExtFlags(extFlags);
	printf("\n");

	NdrProcessParams(&stubMsg, pFormat, NDR_PHASE_SIZE, fpuStack, numberParams);

	printf("stubMsg BufferLength: %d\n", (int) stubMsg.BufferLength);

	return client_call_return;
}

CLIENT_CALL_RETURN NdrClientCall2(PMIDL_STUB_DESC pStubDescriptor, PFORMAT_STRING pFormat, ...)
{
	va_list args;
	CLIENT_CALL_RETURN client_call_return;

	va_start(args, pFormat);
	client_call_return = NdrClientCall(pStubDescriptor, pFormat, va_arg(args, void**), NULL);
	va_end(args);

	return client_call_return;
}

void* MIDL_user_allocate(size_t cBytes)
{
    return (xmalloc(cBytes));
}

void MIDL_user_free(void* p)
{
	xfree(p);
}
