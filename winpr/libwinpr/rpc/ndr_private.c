/**
 * WinPR: Windows Portable Runtime
 * Microsoft Remote Procedure Call (MSRPC)
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

#include <stdio.h>
#include <stdlib.h>

#include <winpr/rpc.h>

#ifndef _WIN32

#include "ndr_array.h"
#include "ndr_context.h"
#include "ndr_pointer.h"
#include "ndr_simple.h"
#include "ndr_string.h"
#include "ndr_structure.h"
#include "ndr_union.h"

#include "ndr_private.h"

void NdrpAlignLength(ULONG* length, unsigned int alignment)
{
	*length = (*length + alignment - 1) & ~(alignment - 1);
}

void NdrpIncrementLength(ULONG* length, unsigned int size)
{
	*length += size;
}

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

#endif
