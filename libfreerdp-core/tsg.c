/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Terminal Server Gateway (TSG)
 *
 * Copyright 2012 Fujitsu Technology Solutions GmbH
 * Copyright 2012 Dmitrij Jasnov <dmitrij.jasnov@ts.fujitsu.com>
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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freerdp/utils/sleep.h>
#include <freerdp/utils/stream.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/hexdump.h>
#include <freerdp/utils/unicode.h>

#include <winpr/ndr.h>

#include "tsg.h"

/**
 * RPC Functions: http://msdn.microsoft.com/en-us/library/windows/desktop/aa378623/
 * Remote Procedure Call: http://msdn.microsoft.com/en-us/library/windows/desktop/aa378651/
 * RPC NDR Interface Reference: http://msdn.microsoft.com/en-us/library/windows/desktop/hh802752/
 */

/**
 * START OF GENERATED CODE
 */

#define TYPE_FORMAT_STRING_SIZE   833
#define PROC_FORMAT_STRING_SIZE   449
#define EXPR_FORMAT_STRING_SIZE   1
#define TRANSMIT_AS_TABLE_SIZE    0
#define WIRE_MARSHAL_TABLE_SIZE   0

typedef struct _ms2Dtsgu_MIDL_TYPE_FORMAT_STRING
    {
    short          Pad;
    unsigned char  Format[ TYPE_FORMAT_STRING_SIZE ];
    } ms2Dtsgu_MIDL_TYPE_FORMAT_STRING;

typedef struct _ms2Dtsgu_MIDL_PROC_FORMAT_STRING
    {
    short          Pad;
    unsigned char  Format[ PROC_FORMAT_STRING_SIZE ];
    } ms2Dtsgu_MIDL_PROC_FORMAT_STRING;

typedef struct _ms2Dtsgu_MIDL_EXPR_FORMAT_STRING
    {
    long          Pad;
    unsigned char  Format[ EXPR_FORMAT_STRING_SIZE ];
    } ms2Dtsgu_MIDL_EXPR_FORMAT_STRING;


static const RPC_SYNTAX_IDENTIFIER  _RpcTransferSyntax =
{{0x8A885D04,0x1CEB,0x11C9,{0x9F,0xE8,0x08,0x00,0x2B,0x10,0x48,0x60}},{2,0}};

static const ms2Dtsgu_MIDL_TYPE_FORMAT_STRING ms2Dtsgu__MIDL_TypeFormatString;
static const ms2Dtsgu_MIDL_PROC_FORMAT_STRING ms2Dtsgu__MIDL_ProcFormatString;
static const ms2Dtsgu_MIDL_EXPR_FORMAT_STRING ms2Dtsgu__MIDL_ExprFormatString;

#define GENERIC_BINDING_TABLE_SIZE   0

static const RPC_CLIENT_INTERFACE TsProxyRpcInterface___RpcClientInterface =
    {
    sizeof(RPC_CLIENT_INTERFACE),
    {{0x44e265dd,0x7daf,0x42cd,{0x85,0x60,0x3c,0xdb,0x6e,0x7a,0x27,0x29}},{1,3}},
    {{0x8A885D04,0x1CEB,0x11C9,{0x9F,0xE8,0x08,0x00,0x2B,0x10,0x48,0x60}},{2,0}},
    0,
    0,
    0,
    0,
    0,
    0x00000000
    };
RPC_IF_HANDLE TsProxyRpcInterface_v1_3_c_ifspec = (RPC_IF_HANDLE)& TsProxyRpcInterface___RpcClientInterface;

static const MIDL_STUB_DESC TsProxyRpcInterface_StubDesc;

static RPC_BINDING_HANDLE TsProxyRpcInterface__MIDL_AutoBindHandle;


void Opnum0NotUsedOnWire(
    /* [in] */ handle_t IDL_handle)
{

    NdrClientCall2(
                  ( PMIDL_STUB_DESC  )&TsProxyRpcInterface_StubDesc,
                  (PFORMAT_STRING) &ms2Dtsgu__MIDL_ProcFormatString.Format[0],
                  ( unsigned char * )&IDL_handle);

}


HRESULT TsProxyCreateTunnel(
    /* [ref][in] */ PTSG_PACKET tsgPacket,
    /* [ref][out] */ PTSG_PACKET *tsgPacketResponse,
    /* [out] */ PTUNNEL_CONTEXT_HANDLE_SERIALIZE *tunnelContext,
    /* [out] */ unsigned long *tunnelId)
{

    CLIENT_CALL_RETURN _RetVal;

    _RetVal = NdrClientCall2(
                  ( PMIDL_STUB_DESC  )&TsProxyRpcInterface_StubDesc,
                  (PFORMAT_STRING) &ms2Dtsgu__MIDL_ProcFormatString.Format[28],
                  ( unsigned char * )&tsgPacket);
    return ( HRESULT  )_RetVal.Simple;

}


HRESULT TsProxyAuthorizeTunnel(
    /* [in] */ PTUNNEL_CONTEXT_HANDLE_NOSERIALIZE tunnelContext,
    /* [ref][in] */ PTSG_PACKET tsgPacket,
    /* [ref][out] */ PTSG_PACKET *tsgPacketResponse)
{

    CLIENT_CALL_RETURN _RetVal;

    _RetVal = NdrClientCall2(
                  ( PMIDL_STUB_DESC  )&TsProxyRpcInterface_StubDesc,
                  (PFORMAT_STRING) &ms2Dtsgu__MIDL_ProcFormatString.Format[82],
                  ( unsigned char * )&tunnelContext);
    return ( HRESULT  )_RetVal.Simple;

}


HRESULT TsProxyMakeTunnelCall(
    /* [in] */ PTUNNEL_CONTEXT_HANDLE_NOSERIALIZE tunnelContext,
    /* [in] */ unsigned long procId,
    /* [ref][in] */ PTSG_PACKET tsgPacket,
    /* [ref][out] */ PTSG_PACKET *tsgPacketResponse)
{

    CLIENT_CALL_RETURN _RetVal;

    _RetVal = NdrClientCall2(
                  ( PMIDL_STUB_DESC  )&TsProxyRpcInterface_StubDesc,
                  (PFORMAT_STRING) &ms2Dtsgu__MIDL_ProcFormatString.Format[136],
                  ( unsigned char * )&tunnelContext);
    return ( HRESULT  )_RetVal.Simple;

}


HRESULT TsProxyCreateChannel(
    /* [in] */ PTUNNEL_CONTEXT_HANDLE_NOSERIALIZE tunnelContext,
    /* [ref][in] */ PTSENDPOINTINFO tsEndPointInfo,
    /* [out] */ PCHANNEL_CONTEXT_HANDLE_SERIALIZE *channelContext,
    /* [out] */ unsigned long *channelId)
{

    CLIENT_CALL_RETURN _RetVal;

    _RetVal = NdrClientCall2(
                  ( PMIDL_STUB_DESC  )&TsProxyRpcInterface_StubDesc,
                  (PFORMAT_STRING) &ms2Dtsgu__MIDL_ProcFormatString.Format[196],
                  ( unsigned char * )&tunnelContext);
    return ( HRESULT  )_RetVal.Simple;

}


void Opnum5NotUsedOnWire(
    /* [in] */ handle_t IDL_handle)
{

    NdrClientCall2(
                  ( PMIDL_STUB_DESC  )&TsProxyRpcInterface_StubDesc,
                  (PFORMAT_STRING) &ms2Dtsgu__MIDL_ProcFormatString.Format[256],
                  ( unsigned char * )&IDL_handle);

}


HRESULT TsProxyCloseChannel(
    /* [out][in] */ PCHANNEL_CONTEXT_HANDLE_NOSERIALIZE *context)
{

    CLIENT_CALL_RETURN _RetVal;

    _RetVal = NdrClientCall2(
                  ( PMIDL_STUB_DESC  )&TsProxyRpcInterface_StubDesc,
                  (PFORMAT_STRING) &ms2Dtsgu__MIDL_ProcFormatString.Format[284],
                  ( unsigned char * )&context);
    return ( HRESULT  )_RetVal.Simple;

}


HRESULT TsProxyCloseTunnel(
    /* [out][in] */ PTUNNEL_CONTEXT_HANDLE_SERIALIZE *context)
{

    CLIENT_CALL_RETURN _RetVal;

    _RetVal = NdrClientCall2(
                  ( PMIDL_STUB_DESC  )&TsProxyRpcInterface_StubDesc,
                  (PFORMAT_STRING) &ms2Dtsgu__MIDL_ProcFormatString.Format[326],
                  ( unsigned char * )&context);
    return ( HRESULT  )_RetVal.Simple;

}


DWORD TsProxySetupReceivePipe(
    /* [in] */ handle_t IDL_handle,
    /* [max_is][in] */ byte pRpcMessage[  ])
{

    CLIENT_CALL_RETURN _RetVal;

    _RetVal = NdrClientCall2(
                  ( PMIDL_STUB_DESC  )&TsProxyRpcInterface_StubDesc,
                  (PFORMAT_STRING) &ms2Dtsgu__MIDL_ProcFormatString.Format[368],
                  ( unsigned char * )&IDL_handle);
    return ( DWORD  )_RetVal.Simple;

}

static const ms2Dtsgu_MIDL_PROC_FORMAT_STRING ms2Dtsgu__MIDL_ProcFormatString =
    {
        0,
        {

	/* Procedure Opnum0NotUsedOnWire */

			0x0,		/* 0 */
			0x48,		/* Old Flags:  */
/*  2 */	NdrFcLong( 0x0 ),	/* 0 */
/*  6 */	NdrFcShort( 0x0 ),	/* 0 */
/*  8 */	NdrFcShort( 0x4 ),	/* x86 Stack size/offset = 4 */
/* 10 */	0x32,		/* FC_BIND_PRIMITIVE */
			0x0,		/* 0 */
/* 12 */	NdrFcShort( 0x0 ),	/* x86 Stack size/offset = 0 */
/* 14 */	NdrFcShort( 0x0 ),	/* 0 */
/* 16 */	NdrFcShort( 0x0 ),	/* 0 */
/* 18 */	0x40,		/* Oi2 Flags:  has ext, */
			0x0,		/* 0 */
/* 20 */	0x8,		/* 8 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 22 */	NdrFcShort( 0x0 ),	/* 0 */
/* 24 */	NdrFcShort( 0x0 ),	/* 0 */
/* 26 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Procedure TsProxyCreateTunnel */


	/* Parameter IDL_handle */

/* 28 */	0x33,		/* FC_AUTO_HANDLE */
			0x48,		/* Old Flags:  */
/* 30 */	NdrFcLong( 0x0 ),	/* 0 */
/* 34 */	NdrFcShort( 0x1 ),	/* 1 */
/* 36 */	NdrFcShort( 0x14 ),	/* x86 Stack size/offset = 20 */
/* 38 */	NdrFcShort( 0x0 ),	/* 0 */
/* 40 */	NdrFcShort( 0x5c ),	/* 92 */
/* 42 */	0x47,		/* Oi2 Flags:  srv must size, clt must size, has return, has ext, */
			0x5,		/* 5 */
/* 44 */	0x8,		/* 8 */
			0x7,		/* Ext Flags:  new corr desc, clt corr check, srv corr check, */
/* 46 */	NdrFcShort( 0x1 ),	/* 1 */
/* 48 */	NdrFcShort( 0x1 ),	/* 1 */
/* 50 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter tsgPacket */

/* 52 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 54 */	NdrFcShort( 0x0 ),	/* x86 Stack size/offset = 0 */
/* 56 */	NdrFcShort( 0x280 ),	/* Type Offset=640 */

	/* Parameter tsgPacketResponse */

/* 58 */	NdrFcShort( 0x2013 ),	/* Flags:  must size, must free, out, srv alloc size=8 */
/* 60 */	NdrFcShort( 0x4 ),	/* x86 Stack size/offset = 4 */
/* 62 */	NdrFcShort( 0x28e ),	/* Type Offset=654 */

	/* Parameter tunnelContext */

/* 64 */	NdrFcShort( 0x110 ),	/* Flags:  out, simple ref, */
/* 66 */	NdrFcShort( 0x8 ),	/* x86 Stack size/offset = 8 */
/* 68 */	NdrFcShort( 0x29a ),	/* Type Offset=666 */

	/* Parameter tunnelId */

/* 70 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 72 */	NdrFcShort( 0xc ),	/* x86 Stack size/offset = 12 */
/* 74 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 76 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 78 */	NdrFcShort( 0x10 ),	/* x86 Stack size/offset = 16 */
/* 80 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure TsProxyAuthorizeTunnel */

/* 82 */	0x0,		/* 0 */
			0x48,		/* Old Flags:  */
/* 84 */	NdrFcLong( 0x0 ),	/* 0 */
/* 88 */	NdrFcShort( 0x2 ),	/* 2 */
/* 90 */	NdrFcShort( 0x10 ),	/* x86 Stack size/offset = 16 */
/* 92 */	0x30,		/* FC_BIND_CONTEXT */
			0x40,		/* Ctxt flags:  in, */
/* 94 */	NdrFcShort( 0x0 ),	/* x86 Stack size/offset = 0 */
/* 96 */	0x1,		/* 1 */
			0x0,		/* 0 */
/* 98 */	NdrFcShort( 0x24 ),	/* 36 */
/* 100 */	NdrFcShort( 0x8 ),	/* 8 */
/* 102 */	0x47,		/* Oi2 Flags:  srv must size, clt must size, has return, has ext, */
			0x4,		/* 4 */
/* 104 */	0x8,		/* 8 */
			0x7,		/* Ext Flags:  new corr desc, clt corr check, srv corr check, */
/* 106 */	NdrFcShort( 0x1 ),	/* 1 */
/* 108 */	NdrFcShort( 0x1 ),	/* 1 */
/* 110 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter tunnelContext */

/* 112 */	NdrFcShort( 0x8 ),	/* Flags:  in, */
/* 114 */	NdrFcShort( 0x0 ),	/* x86 Stack size/offset = 0 */
/* 116 */	NdrFcShort( 0x2a2 ),	/* Type Offset=674 */

	/* Parameter tsgPacket */

/* 118 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 120 */	NdrFcShort( 0x4 ),	/* x86 Stack size/offset = 4 */
/* 122 */	NdrFcShort( 0x280 ),	/* Type Offset=640 */

	/* Parameter tsgPacketResponse */

/* 124 */	NdrFcShort( 0x2013 ),	/* Flags:  must size, must free, out, srv alloc size=8 */
/* 126 */	NdrFcShort( 0x8 ),	/* x86 Stack size/offset = 8 */
/* 128 */	NdrFcShort( 0x28e ),	/* Type Offset=654 */

	/* Return value */

/* 130 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 132 */	NdrFcShort( 0xc ),	/* x86 Stack size/offset = 12 */
/* 134 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure TsProxyMakeTunnelCall */

/* 136 */	0x0,		/* 0 */
			0x48,		/* Old Flags:  */
/* 138 */	NdrFcLong( 0x0 ),	/* 0 */
/* 142 */	NdrFcShort( 0x3 ),	/* 3 */
/* 144 */	NdrFcShort( 0x14 ),	/* x86 Stack size/offset = 20 */
/* 146 */	0x30,		/* FC_BIND_CONTEXT */
			0x40,		/* Ctxt flags:  in, */
/* 148 */	NdrFcShort( 0x0 ),	/* x86 Stack size/offset = 0 */
/* 150 */	0x1,		/* 1 */
			0x0,		/* 0 */
/* 152 */	NdrFcShort( 0x2c ),	/* 44 */
/* 154 */	NdrFcShort( 0x8 ),	/* 8 */
/* 156 */	0x47,		/* Oi2 Flags:  srv must size, clt must size, has return, has ext, */
			0x5,		/* 5 */
/* 158 */	0x8,		/* 8 */
			0x7,		/* Ext Flags:  new corr desc, clt corr check, srv corr check, */
/* 160 */	NdrFcShort( 0x1 ),	/* 1 */
/* 162 */	NdrFcShort( 0x1 ),	/* 1 */
/* 164 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter tunnelContext */

/* 166 */	NdrFcShort( 0x8 ),	/* Flags:  in, */
/* 168 */	NdrFcShort( 0x0 ),	/* x86 Stack size/offset = 0 */
/* 170 */	NdrFcShort( 0x2a2 ),	/* Type Offset=674 */

	/* Parameter procId */

/* 172 */	NdrFcShort( 0x48 ),	/* Flags:  in, base type, */
/* 174 */	NdrFcShort( 0x4 ),	/* x86 Stack size/offset = 4 */
/* 176 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Parameter tsgPacket */

/* 178 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 180 */	NdrFcShort( 0x8 ),	/* x86 Stack size/offset = 8 */
/* 182 */	NdrFcShort( 0x280 ),	/* Type Offset=640 */

	/* Parameter tsgPacketResponse */

/* 184 */	NdrFcShort( 0x2013 ),	/* Flags:  must size, must free, out, srv alloc size=8 */
/* 186 */	NdrFcShort( 0xc ),	/* x86 Stack size/offset = 12 */
/* 188 */	NdrFcShort( 0x28e ),	/* Type Offset=654 */

	/* Return value */

/* 190 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 192 */	NdrFcShort( 0x10 ),	/* x86 Stack size/offset = 16 */
/* 194 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure TsProxyCreateChannel */

/* 196 */	0x0,		/* 0 */
			0x48,		/* Old Flags:  */
/* 198 */	NdrFcLong( 0x0 ),	/* 0 */
/* 202 */	NdrFcShort( 0x4 ),	/* 4 */
/* 204 */	NdrFcShort( 0x14 ),	/* x86 Stack size/offset = 20 */
/* 206 */	0x30,		/* FC_BIND_CONTEXT */
			0x40,		/* Ctxt flags:  in, */
/* 208 */	NdrFcShort( 0x0 ),	/* x86 Stack size/offset = 0 */
/* 210 */	0x1,		/* 1 */
			0x0,		/* 0 */
/* 212 */	NdrFcShort( 0x24 ),	/* 36 */
/* 214 */	NdrFcShort( 0x5c ),	/* 92 */
/* 216 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x5,		/* 5 */
/* 218 */	0x8,		/* 8 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 220 */	NdrFcShort( 0x0 ),	/* 0 */
/* 222 */	NdrFcShort( 0x1 ),	/* 1 */
/* 224 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter tunnelContext */

/* 226 */	NdrFcShort( 0x8 ),	/* Flags:  in, */
/* 228 */	NdrFcShort( 0x0 ),	/* x86 Stack size/offset = 0 */
/* 230 */	NdrFcShort( 0x2a2 ),	/* Type Offset=674 */

	/* Parameter tsEndPointInfo */

/* 232 */	NdrFcShort( 0x10b ),	/* Flags:  must size, must free, in, simple ref, */
/* 234 */	NdrFcShort( 0x4 ),	/* x86 Stack size/offset = 4 */
/* 236 */	NdrFcShort( 0x2fe ),	/* Type Offset=766 */

	/* Parameter channelContext */

/* 238 */	NdrFcShort( 0x110 ),	/* Flags:  out, simple ref, */
/* 240 */	NdrFcShort( 0x8 ),	/* x86 Stack size/offset = 8 */
/* 242 */	NdrFcShort( 0x320 ),	/* Type Offset=800 */

	/* Parameter channelId */

/* 244 */	NdrFcShort( 0x2150 ),	/* Flags:  out, base type, simple ref, srv alloc size=8 */
/* 246 */	NdrFcShort( 0xc ),	/* x86 Stack size/offset = 12 */
/* 248 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Return value */

/* 250 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 252 */	NdrFcShort( 0x10 ),	/* x86 Stack size/offset = 16 */
/* 254 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure Opnum5NotUsedOnWire */

/* 256 */	0x0,		/* 0 */
			0x48,		/* Old Flags:  */
/* 258 */	NdrFcLong( 0x0 ),	/* 0 */
/* 262 */	NdrFcShort( 0x5 ),	/* 5 */
/* 264 */	NdrFcShort( 0x4 ),	/* x86 Stack size/offset = 4 */
/* 266 */	0x32,		/* FC_BIND_PRIMITIVE */
			0x0,		/* 0 */
/* 268 */	NdrFcShort( 0x0 ),	/* x86 Stack size/offset = 0 */
/* 270 */	NdrFcShort( 0x0 ),	/* 0 */
/* 272 */	NdrFcShort( 0x0 ),	/* 0 */
/* 274 */	0x40,		/* Oi2 Flags:  has ext, */
			0x0,		/* 0 */
/* 276 */	0x8,		/* 8 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 278 */	NdrFcShort( 0x0 ),	/* 0 */
/* 280 */	NdrFcShort( 0x0 ),	/* 0 */
/* 282 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Procedure TsProxyCloseChannel */


	/* Parameter IDL_handle */

/* 284 */	0x0,		/* 0 */
			0x48,		/* Old Flags:  */
/* 286 */	NdrFcLong( 0x0 ),	/* 0 */
/* 290 */	NdrFcShort( 0x6 ),	/* 6 */
/* 292 */	NdrFcShort( 0x8 ),	/* x86 Stack size/offset = 8 */
/* 294 */	0x30,		/* FC_BIND_CONTEXT */
			0xe0,		/* Ctxt flags:  via ptr, in, out, */
/* 296 */	NdrFcShort( 0x0 ),	/* x86 Stack size/offset = 0 */
/* 298 */	0x3,		/* 3 */
			0x0,		/* 0 */
/* 300 */	NdrFcShort( 0x38 ),	/* 56 */
/* 302 */	NdrFcShort( 0x40 ),	/* 64 */
/* 304 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 306 */	0x8,		/* 8 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 308 */	NdrFcShort( 0x0 ),	/* 0 */
/* 310 */	NdrFcShort( 0x0 ),	/* 0 */
/* 312 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter context */

/* 314 */	NdrFcShort( 0x118 ),	/* Flags:  in, out, simple ref, */
/* 316 */	NdrFcShort( 0x0 ),	/* x86 Stack size/offset = 0 */
/* 318 */	NdrFcShort( 0x328 ),	/* Type Offset=808 */

	/* Return value */

/* 320 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 322 */	NdrFcShort( 0x4 ),	/* x86 Stack size/offset = 4 */
/* 324 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure TsProxyCloseTunnel */

/* 326 */	0x0,		/* 0 */
			0x48,		/* Old Flags:  */
/* 328 */	NdrFcLong( 0x0 ),	/* 0 */
/* 332 */	NdrFcShort( 0x7 ),	/* 7 */
/* 334 */	NdrFcShort( 0x8 ),	/* x86 Stack size/offset = 8 */
/* 336 */	0x30,		/* FC_BIND_CONTEXT */
			0xe0,		/* Ctxt flags:  via ptr, in, out, */
/* 338 */	NdrFcShort( 0x0 ),	/* x86 Stack size/offset = 0 */
/* 340 */	0x0,		/* 0 */
			0x0,		/* 0 */
/* 342 */	NdrFcShort( 0x38 ),	/* 56 */
/* 344 */	NdrFcShort( 0x40 ),	/* 64 */
/* 346 */	0x44,		/* Oi2 Flags:  has return, has ext, */
			0x2,		/* 2 */
/* 348 */	0x8,		/* 8 */
			0x1,		/* Ext Flags:  new corr desc, */
/* 350 */	NdrFcShort( 0x0 ),	/* 0 */
/* 352 */	NdrFcShort( 0x0 ),	/* 0 */
/* 354 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter context */

/* 356 */	NdrFcShort( 0x118 ),	/* Flags:  in, out, simple ref, */
/* 358 */	NdrFcShort( 0x0 ),	/* x86 Stack size/offset = 0 */
/* 360 */	NdrFcShort( 0x330 ),	/* Type Offset=816 */

	/* Return value */

/* 362 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 364 */	NdrFcShort( 0x4 ),	/* x86 Stack size/offset = 4 */
/* 366 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure TsProxySetupReceivePipe */

/* 368 */	0x0,		/* 0 */
			0x48,		/* Old Flags:  */
/* 370 */	NdrFcLong( 0x0 ),	/* 0 */
/* 374 */	NdrFcShort( 0x8 ),	/* 8 */
/* 376 */	NdrFcShort( 0xc ),	/* x86 Stack size/offset = 12 */
/* 378 */	0x32,		/* FC_BIND_PRIMITIVE */
			0x0,		/* 0 */
/* 380 */	NdrFcShort( 0x0 ),	/* x86 Stack size/offset = 0 */
/* 382 */	NdrFcShort( 0x0 ),	/* 0 */
/* 384 */	NdrFcShort( 0x8 ),	/* 8 */
/* 386 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 388 */	0x8,		/* 8 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 390 */	NdrFcShort( 0x0 ),	/* 0 */
/* 392 */	NdrFcShort( 0x1 ),	/* 1 */
/* 394 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter IDL_handle */

/* 396 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 398 */	NdrFcShort( 0x4 ),	/* x86 Stack size/offset = 4 */
/* 400 */	NdrFcShort( 0x334 ),	/* Type Offset=820 */

	/* Parameter pRpcMessage */

/* 402 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 404 */	NdrFcShort( 0x8 ),	/* x86 Stack size/offset = 8 */
/* 406 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

	/* Procedure TsProxySendToServer */


	/* Return value */

/* 408 */	0x0,		/* 0 */
			0x48,		/* Old Flags:  */
/* 410 */	NdrFcLong( 0x0 ),	/* 0 */
/* 414 */	NdrFcShort( 0x9 ),	/* 9 */
/* 416 */	NdrFcShort( 0xc ),	/* x86 Stack size/offset = 12 */
/* 418 */	0x32,		/* FC_BIND_PRIMITIVE */
			0x0,		/* 0 */
/* 420 */	NdrFcShort( 0x0 ),	/* x86 Stack size/offset = 0 */
/* 422 */	NdrFcShort( 0x0 ),	/* 0 */
/* 424 */	NdrFcShort( 0x8 ),	/* 8 */
/* 426 */	0x46,		/* Oi2 Flags:  clt must size, has return, has ext, */
			0x2,		/* 2 */
/* 428 */	0x8,		/* 8 */
			0x5,		/* Ext Flags:  new corr desc, srv corr check, */
/* 430 */	NdrFcShort( 0x0 ),	/* 0 */
/* 432 */	NdrFcShort( 0x1 ),	/* 1 */
/* 434 */	NdrFcShort( 0x0 ),	/* 0 */

	/* Parameter IDL_handle */

/* 436 */	NdrFcShort( 0xb ),	/* Flags:  must size, must free, in, */
/* 438 */	NdrFcShort( 0x4 ),	/* x86 Stack size/offset = 4 */
/* 440 */	NdrFcShort( 0x334 ),	/* Type Offset=820 */

	/* Parameter pRpcMessage */

/* 442 */	NdrFcShort( 0x70 ),	/* Flags:  out, return, base type, */
/* 444 */	NdrFcShort( 0x8 ),	/* x86 Stack size/offset = 8 */
/* 446 */	0x8,		/* FC_LONG */
			0x0,		/* 0 */

			0x0
        }
    };

static const ms2Dtsgu_MIDL_TYPE_FORMAT_STRING ms2Dtsgu__MIDL_TypeFormatString =
    {
        0,
        {
			NdrFcShort( 0x0 ),	/* 0 */
/*  2 */
			0x11, 0x0,	/* FC_RP */
/*  4 */	NdrFcShort( 0x27c ),	/* Offset= 636 (640) */
/*  6 */
			0x2b,		/* FC_NON_ENCAPSULATED_UNION */
			0x9,		/* FC_ULONG */
/*  8 */	0x9,		/* Corr desc: FC_ULONG */
			0x0,		/*  */
/* 10 */	NdrFcShort( 0xfffc ),	/* -4 */
/* 12 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 14 */	NdrFcShort( 0x2 ),	/* Offset= 2 (16) */
/* 16 */	NdrFcShort( 0x4 ),	/* 4 */
/* 18 */	NdrFcShort( 0xb ),	/* 11 */
/* 20 */	NdrFcLong( 0x4844 ),	/* 18500 */
/* 24 */	NdrFcShort( 0x40 ),	/* Offset= 64 (88) */
/* 26 */	NdrFcLong( 0x5643 ),	/* 22083 */
/* 30 */	NdrFcShort( 0x46 ),	/* Offset= 70 (100) */
/* 32 */	NdrFcLong( 0x5143 ),	/* 20803 */
/* 36 */	NdrFcShort( 0xa8 ),	/* Offset= 168 (204) */
/* 38 */	NdrFcLong( 0x5152 ),	/* 20818 */
/* 42 */	NdrFcShort( 0xa6 ),	/* Offset= 166 (208) */
/* 44 */	NdrFcLong( 0x5052 ),	/* 20562 */
/* 48 */	NdrFcShort( 0xe8 ),	/* Offset= 232 (280) */
/* 50 */	NdrFcLong( 0x4552 ),	/* 17746 */
/* 54 */	NdrFcShort( 0x122 ),	/* Offset= 290 (344) */
/* 56 */	NdrFcLong( 0x4350 ),	/* 17232 */
/* 60 */	NdrFcShort( 0x160 ),	/* Offset= 352 (412) */
/* 62 */	NdrFcLong( 0x4752 ),	/* 18258 */
/* 66 */	NdrFcShort( 0x8a ),	/* Offset= 138 (204) */
/* 68 */	NdrFcLong( 0x4750 ),	/* 18256 */
/* 72 */	NdrFcShort( 0x1d4 ),	/* Offset= 468 (540) */
/* 74 */	NdrFcLong( 0x4054 ),	/* 16468 */
/* 78 */	NdrFcShort( 0x1d2 ),	/* Offset= 466 (544) */
/* 80 */	NdrFcLong( 0x5250 ),	/* 21072 */
/* 84 */	NdrFcShort( 0x1fc ),	/* Offset= 508 (592) */
/* 86 */	NdrFcShort( 0xffff ),	/* Offset= -1 (85) */
/* 88 */
			0x12, 0x0,	/* FC_UP */
/* 90 */	NdrFcShort( 0x2 ),	/* Offset= 2 (92) */
/* 92 */
			0x15,		/* FC_STRUCT */
			0x1,		/* 1 */
/* 94 */	NdrFcShort( 0x4 ),	/* 4 */
/* 96 */	0x6,		/* FC_SHORT */
			0x6,		/* FC_SHORT */
/* 98 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 100 */
			0x12, 0x0,	/* FC_UP */
/* 102 */	NdrFcShort( 0x4c ),	/* Offset= 76 (178) */
/* 104 */	0xb7,		/* FC_RANGE */
			0x8,		/* 8 */
/* 106 */	NdrFcLong( 0x0 ),	/* 0 */
/* 110 */	NdrFcLong( 0x20 ),	/* 32 */
/* 114 */
			0x2b,		/* FC_NON_ENCAPSULATED_UNION */
			0x9,		/* FC_ULONG */
/* 116 */	0x9,		/* Corr desc: FC_ULONG */
			0x0,		/*  */
/* 118 */	NdrFcShort( 0xfffc ),	/* -4 */
/* 120 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 122 */	NdrFcShort( 0x2 ),	/* Offset= 2 (124) */
/* 124 */	NdrFcShort( 0x4 ),	/* 4 */
/* 126 */	NdrFcShort( 0x1 ),	/* 1 */
/* 128 */	NdrFcLong( 0x1 ),	/* 1 */
/* 132 */	NdrFcShort( 0x4 ),	/* Offset= 4 (136) */
/* 134 */	NdrFcShort( 0xffff ),	/* Offset= -1 (133) */
/* 136 */
			0x15,		/* FC_STRUCT */
			0x3,		/* 3 */
/* 138 */	NdrFcShort( 0x4 ),	/* 4 */
/* 140 */	0x8,		/* FC_LONG */
			0x5b,		/* FC_END */
/* 142 */
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 144 */	NdrFcShort( 0x8 ),	/* 8 */
/* 146 */	NdrFcShort( 0x0 ),	/* 0 */
/* 148 */	NdrFcShort( 0x0 ),	/* Offset= 0 (148) */
/* 150 */	0x8,		/* FC_LONG */
			0x4c,		/* FC_EMBEDDED_COMPLEX */
/* 152 */	0x0,		/* 0 */
			NdrFcShort( 0xffd9 ),	/* Offset= -39 (114) */
			0x5b,		/* FC_END */
/* 156 */
			0x21,		/* FC_BOGUS_ARRAY */
			0x3,		/* 3 */
/* 158 */	NdrFcShort( 0x0 ),	/* 0 */
/* 160 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 162 */	NdrFcShort( 0x8 ),	/* 8 */
/* 164 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 166 */	NdrFcLong( 0xffffffff ),	/* -1 */
/* 170 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 172 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 174 */	NdrFcShort( 0xffe0 ),	/* Offset= -32 (142) */
/* 176 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 178 */
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 180 */	NdrFcShort( 0x14 ),	/* 20 */
/* 182 */	NdrFcShort( 0x0 ),	/* 0 */
/* 184 */	NdrFcShort( 0x10 ),	/* Offset= 16 (200) */
/* 186 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 188 */	NdrFcShort( 0xffa0 ),	/* Offset= -96 (92) */
/* 190 */	0x36,		/* FC_POINTER */
			0x4c,		/* FC_EMBEDDED_COMPLEX */
/* 192 */	0x0,		/* 0 */
			NdrFcShort( 0xffa7 ),	/* Offset= -89 (104) */
			0x6,		/* FC_SHORT */
/* 196 */	0x6,		/* FC_SHORT */
			0x6,		/* FC_SHORT */
/* 198 */	0x3e,		/* FC_STRUCTPAD2 */
			0x5b,		/* FC_END */
/* 200 */
			0x12, 0x0,	/* FC_UP */
/* 202 */	NdrFcShort( 0xffd2 ),	/* Offset= -46 (156) */
/* 204 */
			0x12, 0x0,	/* FC_UP */
/* 206 */	NdrFcShort( 0xffba ),	/* Offset= -70 (136) */
/* 208 */
			0x12, 0x0,	/* FC_UP */
/* 210 */	NdrFcShort( 0x2a ),	/* Offset= 42 (252) */
/* 212 */	0xb7,		/* FC_RANGE */
			0x8,		/* 8 */
/* 214 */	NdrFcLong( 0x0 ),	/* 0 */
/* 218 */	NdrFcLong( 0x201 ),	/* 513 */
/* 222 */	0xb7,		/* FC_RANGE */
			0x8,		/* 8 */
/* 224 */	NdrFcLong( 0x0 ),	/* 0 */
/* 228 */	NdrFcLong( 0x1f40 ),	/* 8000 */
/* 232 */
			0x25,		/* FC_C_WSTRING */
			0x44,		/* FC_STRING_SIZED */
/* 234 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 236 */	NdrFcShort( 0x8 ),	/* 8 */
/* 238 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 240 */
			0x1b,		/* FC_CARRAY */
			0x0,		/* 0 */
/* 242 */	NdrFcShort( 0x1 ),	/* 1 */
/* 244 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 246 */	NdrFcShort( 0x10 ),	/* 16 */
/* 248 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 250 */	0x1,		/* FC_BYTE */
			0x5b,		/* FC_END */
/* 252 */
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 254 */	NdrFcShort( 0x14 ),	/* 20 */
/* 256 */	NdrFcShort( 0x0 ),	/* 0 */
/* 258 */	NdrFcShort( 0xe ),	/* Offset= 14 (272) */
/* 260 */	0x8,		/* FC_LONG */
			0x36,		/* FC_POINTER */
/* 262 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 264 */	NdrFcShort( 0xffcc ),	/* Offset= -52 (212) */
/* 266 */	0x36,		/* FC_POINTER */
			0x4c,		/* FC_EMBEDDED_COMPLEX */
/* 268 */	0x0,		/* 0 */
			NdrFcShort( 0xffd1 ),	/* Offset= -47 (222) */
			0x5b,		/* FC_END */
/* 272 */
			0x12, 0x0,	/* FC_UP */
/* 274 */	NdrFcShort( 0xffd6 ),	/* Offset= -42 (232) */
/* 276 */
			0x12, 0x0,	/* FC_UP */
/* 278 */	NdrFcShort( 0xffda ),	/* Offset= -38 (240) */
/* 280 */
			0x12, 0x0,	/* FC_UP */
/* 282 */	NdrFcShort( 0x26 ),	/* Offset= 38 (320) */
/* 284 */	0xb7,		/* FC_RANGE */
			0x8,		/* 8 */
/* 286 */	NdrFcLong( 0x0 ),	/* 0 */
/* 290 */	NdrFcLong( 0x5dc0 ),	/* 24000 */
/* 294 */
			0x15,		/* FC_STRUCT */
			0x3,		/* 3 */
/* 296 */	NdrFcShort( 0x20 ),	/* 32 */
/* 298 */	0x8,		/* FC_LONG */
			0x8,		/* FC_LONG */
/* 300 */	0x8,		/* FC_LONG */
			0x8,		/* FC_LONG */
/* 302 */	0x8,		/* FC_LONG */
			0x8,		/* FC_LONG */
/* 304 */	0x8,		/* FC_LONG */
			0x8,		/* FC_LONG */
/* 306 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 308 */
			0x1b,		/* FC_CARRAY */
			0x0,		/* 0 */
/* 310 */	NdrFcShort( 0x1 ),	/* 1 */
/* 312 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 314 */	NdrFcShort( 0xc ),	/* 12 */
/* 316 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 318 */	0x1,		/* FC_BYTE */
			0x5b,		/* FC_END */
/* 320 */
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 322 */	NdrFcShort( 0x30 ),	/* 48 */
/* 324 */	NdrFcShort( 0x0 ),	/* 0 */
/* 326 */	NdrFcShort( 0xe ),	/* Offset= 14 (340) */
/* 328 */	0x8,		/* FC_LONG */
			0x8,		/* FC_LONG */
/* 330 */	0x36,		/* FC_POINTER */
			0x4c,		/* FC_EMBEDDED_COMPLEX */
/* 332 */	0x0,		/* 0 */
			NdrFcShort( 0xffcf ),	/* Offset= -49 (284) */
			0x4c,		/* FC_EMBEDDED_COMPLEX */
/* 336 */	0x0,		/* 0 */
			NdrFcShort( 0xffd5 ),	/* Offset= -43 (294) */
			0x5b,		/* FC_END */
/* 340 */
			0x12, 0x0,	/* FC_UP */
/* 342 */	NdrFcShort( 0xffde ),	/* Offset= -34 (308) */
/* 344 */
			0x12, 0x0,	/* FC_UP */
/* 346 */	NdrFcShort( 0x26 ),	/* Offset= 38 (384) */
/* 348 */	0xb7,		/* FC_RANGE */
			0x8,		/* 8 */
/* 350 */	NdrFcLong( 0x0 ),	/* 0 */
/* 354 */	NdrFcLong( 0x5dc0 ),	/* 24000 */
/* 358 */
			0x1d,		/* FC_SMFARRAY */
			0x0,		/* 0 */
/* 360 */	NdrFcShort( 0x8 ),	/* 8 */
/* 362 */	0x1,		/* FC_BYTE */
			0x5b,		/* FC_END */
/* 364 */
			0x15,		/* FC_STRUCT */
			0x3,		/* 3 */
/* 366 */	NdrFcShort( 0x10 ),	/* 16 */
/* 368 */	0x8,		/* FC_LONG */
			0x6,		/* FC_SHORT */
/* 370 */	0x6,		/* FC_SHORT */
			0x4c,		/* FC_EMBEDDED_COMPLEX */
/* 372 */	0x0,		/* 0 */
			NdrFcShort( 0xfff1 ),	/* Offset= -15 (358) */
			0x5b,		/* FC_END */
/* 376 */
			0x25,		/* FC_C_WSTRING */
			0x44,		/* FC_STRING_SIZED */
/* 378 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 380 */	NdrFcShort( 0x4 ),	/* 4 */
/* 382 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 384 */
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 386 */	NdrFcShort( 0x20 ),	/* 32 */
/* 388 */	NdrFcShort( 0x0 ),	/* 0 */
/* 390 */	NdrFcShort( 0xe ),	/* Offset= 14 (404) */
/* 392 */	0x8,		/* FC_LONG */
			0x4c,		/* FC_EMBEDDED_COMPLEX */
/* 394 */	0x0,		/* 0 */
			NdrFcShort( 0xffd1 ),	/* Offset= -47 (348) */
			0x36,		/* FC_POINTER */
/* 398 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 400 */	NdrFcShort( 0xffdc ),	/* Offset= -36 (364) */
/* 402 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 404 */
			0x12, 0x0,	/* FC_UP */
/* 406 */	NdrFcShort( 0xffe2 ),	/* Offset= -30 (376) */
/* 408 */
			0x12, 0x0,	/* FC_UP */
/* 410 */	NdrFcShort( 0xff18 ),	/* Offset= -232 (178) */
/* 412 */
			0x12, 0x0,	/* FC_UP */
/* 414 */	NdrFcShort( 0x6c ),	/* Offset= 108 (522) */
/* 416 */
			0x2b,		/* FC_NON_ENCAPSULATED_UNION */
			0x9,		/* FC_ULONG */
/* 418 */	0x9,		/* Corr desc: FC_ULONG */
			0x0,		/*  */
/* 420 */	NdrFcShort( 0xfff8 ),	/* -8 */
/* 422 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 424 */	NdrFcShort( 0x2 ),	/* Offset= 2 (426) */
/* 426 */	NdrFcShort( 0x4 ),	/* 4 */
/* 428 */	NdrFcShort( 0x3 ),	/* 3 */
/* 430 */	NdrFcLong( 0x1 ),	/* 1 */
/* 434 */	NdrFcShort( 0x10 ),	/* Offset= 16 (450) */
/* 436 */	NdrFcLong( 0x2 ),	/* 2 */
/* 440 */	NdrFcShort( 0xa ),	/* Offset= 10 (450) */
/* 442 */	NdrFcLong( 0x3 ),	/* 3 */
/* 446 */	NdrFcShort( 0x32 ),	/* Offset= 50 (496) */
/* 448 */	NdrFcShort( 0xffff ),	/* Offset= -1 (447) */
/* 450 */
			0x12, 0x0,	/* FC_UP */
/* 452 */	NdrFcShort( 0x18 ),	/* Offset= 24 (476) */
/* 454 */	0xb7,		/* FC_RANGE */
			0x8,		/* 8 */
/* 456 */	NdrFcLong( 0x0 ),	/* 0 */
/* 460 */	NdrFcLong( 0x10000 ),	/* 65536 */
/* 464 */
			0x1b,		/* FC_CARRAY */
			0x1,		/* 1 */
/* 466 */	NdrFcShort( 0x2 ),	/* 2 */
/* 468 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 470 */	NdrFcShort( 0x8 ),	/* 8 */
/* 472 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 474 */	0x5,		/* FC_WCHAR */
			0x5b,		/* FC_END */
/* 476 */
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 478 */	NdrFcShort( 0x10 ),	/* 16 */
/* 480 */	NdrFcShort( 0x0 ),	/* 0 */
/* 482 */	NdrFcShort( 0xa ),	/* Offset= 10 (492) */
/* 484 */	0x8,		/* FC_LONG */
			0x8,		/* FC_LONG */
/* 486 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 488 */	NdrFcShort( 0xffde ),	/* Offset= -34 (454) */
/* 490 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 492 */
			0x12, 0x0,	/* FC_UP */
/* 494 */	NdrFcShort( 0xffe2 ),	/* Offset= -30 (464) */
/* 496 */
			0x12, 0x0,	/* FC_UP */
/* 498 */	NdrFcShort( 0x2 ),	/* Offset= 2 (500) */
/* 500 */
			0x15,		/* FC_STRUCT */
			0x7,		/* 7 */
/* 502 */	NdrFcShort( 0x8 ),	/* 8 */
/* 504 */	0xb,		/* FC_HYPER */
			0x5b,		/* FC_END */
/* 506 */
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 508 */	NdrFcShort( 0x10 ),	/* 16 */
/* 510 */	NdrFcShort( 0x0 ),	/* 0 */
/* 512 */	NdrFcShort( 0x0 ),	/* Offset= 0 (512) */
/* 514 */	0x8,		/* FC_LONG */
			0x8,		/* FC_LONG */
/* 516 */	0x8,		/* FC_LONG */
			0x4c,		/* FC_EMBEDDED_COMPLEX */
/* 518 */	0x0,		/* 0 */
			NdrFcShort( 0xff99 ),	/* Offset= -103 (416) */
			0x5b,		/* FC_END */
/* 522 */
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 524 */	NdrFcShort( 0x30 ),	/* 48 */
/* 526 */	NdrFcShort( 0x0 ),	/* 0 */
/* 528 */	NdrFcShort( 0x0 ),	/* Offset= 0 (528) */
/* 530 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 532 */	NdrFcShort( 0xff6c ),	/* Offset= -148 (384) */
/* 534 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 536 */	NdrFcShort( 0xffe2 ),	/* Offset= -30 (506) */
/* 538 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 540 */
			0x12, 0x0,	/* FC_UP */
/* 542 */	NdrFcShort( 0xffdc ),	/* Offset= -36 (506) */
/* 544 */
			0x12, 0x0,	/* FC_UP */
/* 546 */	NdrFcShort( 0x18 ),	/* Offset= 24 (570) */
/* 548 */	0xb7,		/* FC_RANGE */
			0x8,		/* 8 */
/* 550 */	NdrFcLong( 0x0 ),	/* 0 */
/* 554 */	NdrFcLong( 0x10000 ),	/* 65536 */
/* 558 */
			0x1b,		/* FC_CARRAY */
			0x0,		/* 0 */
/* 560 */	NdrFcShort( 0x1 ),	/* 1 */
/* 562 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 564 */	NdrFcShort( 0x14 ),	/* 20 */
/* 566 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 568 */	0x1,		/* FC_BYTE */
			0x5b,		/* FC_END */
/* 570 */
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 572 */	NdrFcShort( 0x1c ),	/* 28 */
/* 574 */	NdrFcShort( 0x0 ),	/* 0 */
/* 576 */	NdrFcShort( 0xc ),	/* Offset= 12 (588) */
/* 578 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 580 */	NdrFcShort( 0xfe6e ),	/* Offset= -402 (178) */
/* 582 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 584 */	NdrFcShort( 0xffdc ),	/* Offset= -36 (548) */
/* 586 */	0x36,		/* FC_POINTER */
			0x5b,		/* FC_END */
/* 588 */
			0x12, 0x0,	/* FC_UP */
/* 590 */	NdrFcShort( 0xffe0 ),	/* Offset= -32 (558) */
/* 592 */
			0x12, 0x0,	/* FC_UP */
/* 594 */	NdrFcShort( 0x1e ),	/* Offset= 30 (624) */
/* 596 */
			0x2b,		/* FC_NON_ENCAPSULATED_UNION */
			0x9,		/* FC_ULONG */
/* 598 */	0x9,		/* Corr desc: FC_ULONG */
			0x0,		/*  */
/* 600 */	NdrFcShort( 0xfffc ),	/* -4 */
/* 602 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 604 */	NdrFcShort( 0x2 ),	/* Offset= 2 (606) */
/* 606 */	NdrFcShort( 0x4 ),	/* 4 */
/* 608 */	NdrFcShort( 0x2 ),	/* 2 */
/* 610 */	NdrFcLong( 0x5643 ),	/* 22083 */
/* 614 */	NdrFcShort( 0xfdfe ),	/* Offset= -514 (100) */
/* 616 */	NdrFcLong( 0x4054 ),	/* 16468 */
/* 620 */	NdrFcShort( 0xffb4 ),	/* Offset= -76 (544) */
/* 622 */	NdrFcShort( 0xffff ),	/* Offset= -1 (621) */
/* 624 */
			0x1a,		/* FC_BOGUS_STRUCT */
			0x7,		/* 7 */
/* 626 */	NdrFcShort( 0x10 ),	/* 16 */
/* 628 */	NdrFcShort( 0x0 ),	/* 0 */
/* 630 */	NdrFcShort( 0x0 ),	/* Offset= 0 (630) */
/* 632 */	0xb,		/* FC_HYPER */
			0x8,		/* FC_LONG */
/* 634 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 636 */	NdrFcShort( 0xffd8 ),	/* Offset= -40 (596) */
/* 638 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 640 */
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 642 */	NdrFcShort( 0x8 ),	/* 8 */
/* 644 */	NdrFcShort( 0x0 ),	/* 0 */
/* 646 */	NdrFcShort( 0x0 ),	/* Offset= 0 (646) */
/* 648 */	0x8,		/* FC_LONG */
			0x4c,		/* FC_EMBEDDED_COMPLEX */
/* 650 */	0x0,		/* 0 */
			NdrFcShort( 0xfd7b ),	/* Offset= -645 (6) */
			0x5b,		/* FC_END */
/* 654 */
			0x11, 0x14,	/* FC_RP [alloced_on_stack] [pointer_deref] */
/* 656 */	NdrFcShort( 0x2 ),	/* Offset= 2 (658) */
/* 658 */
			0x12, 0x0,	/* FC_UP */
/* 660 */	NdrFcShort( 0xffec ),	/* Offset= -20 (640) */
/* 662 */
			0x11, 0x4,	/* FC_RP [alloced_on_stack] */
/* 664 */	NdrFcShort( 0x2 ),	/* Offset= 2 (666) */
/* 666 */	0x30,		/* FC_BIND_CONTEXT */
			0xa0,		/* Ctxt flags:  via ptr, out, */
/* 668 */	0x0,		/* 0 */
			0x0,		/* 0 */
/* 670 */
			0x11, 0xc,	/* FC_RP [alloced_on_stack] [simple_pointer] */
/* 672 */	0x8,		/* FC_LONG */
			0x5c,		/* FC_PAD */
/* 674 */	0x30,		/* FC_BIND_CONTEXT */
			0x41,		/* Ctxt flags:  in, can't be null */
/* 676 */	0x1,		/* 1 */
			0x0,		/* 0 */
/* 678 */
			0x11, 0x0,	/* FC_RP */
/* 680 */	NdrFcShort( 0x56 ),	/* Offset= 86 (766) */
/* 682 */	0xb7,		/* FC_RANGE */
			0x8,		/* 8 */
/* 684 */	NdrFcLong( 0x0 ),	/* 0 */
/* 688 */	NdrFcLong( 0x32 ),	/* 50 */
/* 692 */	0xb7,		/* FC_RANGE */
			0x6,		/* 6 */
/* 694 */	NdrFcLong( 0x0 ),	/* 0 */
/* 698 */	NdrFcLong( 0x3 ),	/* 3 */
/* 702 */
			0x1b,		/* FC_CARRAY */
			0x3,		/* 3 */
/* 704 */	NdrFcShort( 0x4 ),	/* 4 */
/* 706 */	0x19,		/* Corr desc:  field pointer, FC_ULONG */
			0x0,		/*  */
/* 708 */	NdrFcShort( 0x4 ),	/* 4 */
/* 710 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 712 */
			0x4b,		/* FC_PP */
			0x5c,		/* FC_PAD */
/* 714 */
			0x48,		/* FC_VARIABLE_REPEAT */
			0x49,		/* FC_FIXED_OFFSET */
/* 716 */	NdrFcShort( 0x4 ),	/* 4 */
/* 718 */	NdrFcShort( 0x0 ),	/* 0 */
/* 720 */	NdrFcShort( 0x1 ),	/* 1 */
/* 722 */	NdrFcShort( 0x0 ),	/* 0 */
/* 724 */	NdrFcShort( 0x0 ),	/* 0 */
/* 726 */	0x12, 0x8,	/* FC_UP [simple_pointer] */
/* 728 */
			0x25,		/* FC_C_WSTRING */
			0x5c,		/* FC_PAD */
/* 730 */
			0x5b,		/* FC_END */

			0x8,		/* FC_LONG */
/* 732 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 734 */
			0x1b,		/* FC_CARRAY */
			0x3,		/* 3 */
/* 736 */	NdrFcShort( 0x4 ),	/* 4 */
/* 738 */	0x17,		/* Corr desc:  field pointer, FC_USHORT */
			0x0,		/*  */
/* 740 */	NdrFcShort( 0xc ),	/* 12 */
/* 742 */	NdrFcShort( 0x0 ),	/* Corr flags:  */
/* 744 */
			0x4b,		/* FC_PP */
			0x5c,		/* FC_PAD */
/* 746 */
			0x48,		/* FC_VARIABLE_REPEAT */
			0x49,		/* FC_FIXED_OFFSET */
/* 748 */	NdrFcShort( 0x4 ),	/* 4 */
/* 750 */	NdrFcShort( 0x0 ),	/* 0 */
/* 752 */	NdrFcShort( 0x1 ),	/* 1 */
/* 754 */	NdrFcShort( 0x0 ),	/* 0 */
/* 756 */	NdrFcShort( 0x0 ),	/* 0 */
/* 758 */	0x12, 0x8,	/* FC_UP [simple_pointer] */
/* 760 */
			0x25,		/* FC_C_WSTRING */
			0x5c,		/* FC_PAD */
/* 762 */
			0x5b,		/* FC_END */

			0x8,		/* FC_LONG */
/* 764 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 766 */
			0x1a,		/* FC_BOGUS_STRUCT */
			0x3,		/* 3 */
/* 768 */	NdrFcShort( 0x14 ),	/* 20 */
/* 770 */	NdrFcShort( 0x0 ),	/* 0 */
/* 772 */	NdrFcShort( 0x10 ),	/* Offset= 16 (788) */
/* 774 */	0x36,		/* FC_POINTER */
			0x4c,		/* FC_EMBEDDED_COMPLEX */
/* 776 */	0x0,		/* 0 */
			NdrFcShort( 0xffa1 ),	/* Offset= -95 (682) */
			0x36,		/* FC_POINTER */
/* 780 */	0x4c,		/* FC_EMBEDDED_COMPLEX */
			0x0,		/* 0 */
/* 782 */	NdrFcShort( 0xffa6 ),	/* Offset= -90 (692) */
/* 784 */	0x3e,		/* FC_STRUCTPAD2 */
			0x8,		/* FC_LONG */
/* 786 */	0x5c,		/* FC_PAD */
			0x5b,		/* FC_END */
/* 788 */
			0x12, 0x0,	/* FC_UP */
/* 790 */	NdrFcShort( 0xffa8 ),	/* Offset= -88 (702) */
/* 792 */
			0x12, 0x0,	/* FC_UP */
/* 794 */	NdrFcShort( 0xffc4 ),	/* Offset= -60 (734) */
/* 796 */
			0x11, 0x4,	/* FC_RP [alloced_on_stack] */
/* 798 */	NdrFcShort( 0x2 ),	/* Offset= 2 (800) */
/* 800 */	0x30,		/* FC_BIND_CONTEXT */
			0xa0,		/* Ctxt flags:  via ptr, out, */
/* 802 */	0x2,		/* 2 */
			0x1,		/* 1 */
/* 804 */
			0x11, 0x4,	/* FC_RP [alloced_on_stack] */
/* 806 */	NdrFcShort( 0x2 ),	/* Offset= 2 (808) */
/* 808 */	0x30,		/* FC_BIND_CONTEXT */
			0xe1,		/* Ctxt flags:  via ptr, in, out, can't be null */
/* 810 */	0x3,		/* 3 */
			0x0,		/* 0 */
/* 812 */
			0x11, 0x4,	/* FC_RP [alloced_on_stack] */
/* 814 */	NdrFcShort( 0x2 ),	/* Offset= 2 (816) */
/* 816 */	0x30,		/* FC_BIND_CONTEXT */
			0xe1,		/* Ctxt flags:  via ptr, in, out, can't be null */
/* 818 */	0x0,		/* 0 */
			0x0,		/* 0 */
/* 820 */
			0x1b,		/* FC_CARRAY */
			0x0,		/* 0 */
/* 822 */	NdrFcShort( 0x1 ),	/* 1 */
/* 824 */	0x40,		/* Corr desc:  constant, val=32768 */
			0x0,		/* 0 */
/* 826 */	NdrFcShort( 0x8000 ),	/* -32768 */
/* 828 */	NdrFcShort( 0x1 ),	/* Corr flags:  early, */
/* 830 */	0x1,		/* FC_BYTE */
			0x5b,		/* FC_END */

			0x0
        }
    };

static const unsigned short TsProxyRpcInterface_FormatStringOffsetTable[] =
    {
    0,
    28,
    82,
    136,
    196,
    256,
    284,
    326,
    368,
    408
    };


static const MIDL_STUB_DESC TsProxyRpcInterface_StubDesc =
    {
    (void *)& TsProxyRpcInterface___RpcClientInterface,
    MIDL_user_allocate,
    MIDL_user_free,
    { &TsProxyRpcInterface__MIDL_AutoBindHandle },
    0,
    0,
    0,
    0,
    ms2Dtsgu__MIDL_TypeFormatString.Format,
    1, /* -error bounds_check flag */
    0x50002, /* Ndr library version */
    0,
    0x700022b, /* MIDL Version 7.0.555 */
    0,
    0,
    0,  /* notify & notify_flag routine table */
    0x1, /* MIDL flag */
    0, /* cs routines */
    0,   /* proxy/server info */
    0
    };

/**
 * END OF GENERATED CODE
 */

uint8 tsg_packet1[108] =
{
	0x43, 0x56, 0x00, 0x00, 0x43, 0x56, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x52, 0x54, 0x43, 0x56,
	0x04, 0x00, 0x02, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00,
	0x8A, 0xE3, 0x13, 0x71, 0x02, 0xF4, 0x36, 0x71, 0x01, 0x00, 0x04, 0x00, 0x01, 0x00, 0x00, 0x00,
	0x02, 0x40, 0x28, 0x00, 0xDD, 0x65, 0xE2, 0x44, 0xAF, 0x7D, 0xCD, 0x42, 0x85, 0x60, 0x3C, 0xDB,
	0x6E, 0x7A, 0x27, 0x29, 0x01, 0x00, 0x03, 0x00, 0x04, 0x5D, 0x88, 0x8A, 0xEB, 0x1C, 0xC9, 0x11,
	0x9F, 0xE8, 0x08, 0x00, 0x2B, 0x10, 0x48, 0x60, 0x02, 0x00, 0x00, 0x00
};

/**
	TsProxyCreateTunnel

	0x43, 0x56, 0x00, 0x00, packetId (TSG_PACKET_TYPE_VERSIONCAPS)

	TSG_PACKET
	0x43, 0x56, 0x00, 0x00, SwitchValue (TSG_PACKET_TYPE_VERSIONCAPS)

	0x00, 0x00, 0x02, 0x00, NdrPtr

	0x52, 0x54, componentId
	0x43, 0x56, packetId

	0x04, 0x00, 0x02, 0x00, NdrPtr TsgCapsPtr
	0x01, 0x00, 0x00, 0x00, numCapabilities

	0x01, 0x00, MajorVersion
	0x01, 0x00, MinorVersion
	0x00, 0x00, QuarantineCapabilities

	0x00, 0x00, alignment pad?

	0x01, 0x00, 0x00, 0x00, MaximumCount
	0x01, 0x00, 0x00, 0x00, TSG_CAPABILITY_TYPE_NAP

	0x01, 0x00, 0x00, 0x00, SwitchValue (TSG_NAP_CAPABILITY_QUAR_SOH)
	0x1F, 0x00, 0x00, 0x00, idle value in minutes?

	0x8A, 0xE3, 0x13, 0x71, 0x02, 0xF4, 0x36, 0x71, 0x01, 0x00, 0x04, 0x00, 0x01, 0x00, 0x00, 0x00,
	0x02, 0x40, 0x28, 0x00, 0xDD, 0x65, 0xE2, 0x44, 0xAF, 0x7D, 0xCD, 0x42, 0x85, 0x60, 0x3C, 0xDB,
	0x6E, 0x7A, 0x27, 0x29, 0x01, 0x00, 0x03, 0x00, 0x04, 0x5D, 0x88, 0x8A, 0xEB, 0x1C, 0xC9, 0x11,
	0x9F, 0xE8, 0x08, 0x00, 0x2B, 0x10, 0x48, 0x60, 0x02, 0x00, 0x00, 0x00
 */

/**
 * TsProxyCreateTunnelResponse
 *
	05 00 02 03 10 00 00 00 50 09 10 00 01 00 00 00
	14 09 00 00 00 00 00 00 00 00 02 00

	50 43 00 00 TSG_PACKET_TYPE_CAPS_RESPONSE
	50 43 00 00 TSG_PACKET_TYPE_CAPS_RESPONSE
	Ptr:		04 00 02 00
	flags:		00 00 00 00
	certChainLen:	39 04 00 00 (1081 * 2 = 2162)
	certChainDataPtr: 08 00 02 00

	GUID?
	95 87 a1 e6 51 6e 6e 4f 90 66 49 4f 9b c2 40 ff
	0c 00 02 00 01 00 00 00 01 00 00 00 00 00 00 00
	01 00 00 00

	Ptr:		14 00 02 00
	MaxCount:	39 04 00 00
	Offset:		00 00 00 00
	ActualCount:	39 04 00 00

	end offset = 105 + 2162 = 2267 (0x08DB)

	                             2d 00 2d 00 2d 00 2d 00 ....9...-.-.-.-.
	0070 2d 00 42 00 45 00 47 00 49 00 4e 00 20 00 43 00 -.B.E.G.I.N. .C.
	0080 45 00 52 00 54 00 49 00 46 00 49 00 43 00 41 00 E.R.T.I.F.I.C.A.
	0090 54 00 45 00 2d 00 2d 00 2d 00 2d 00 2d 00 0d 00 T.E.-.-.-.-.-...
	00a0 0a 00 4d 00 49 00 49 00 43 00 34 00 6a 00 43 00 ..M.I.I.C.4.j.C.
	00b0 43 00 41 00 63 00 71 00 67 00 41 00 77 00 49 00 C.A.c.q.g.A.w.I.
	00c0 42 00 41 00 67 00 49 00 51 00 4a 00 38 00 4f 00 B.A.g.I.Q.J.8.O.
	00d0 48 00 6a 00 65 00 66 00 37 00 4e 00 34 00 78 00 H.j.e.f.7.N.4.x.
	00e0 43 00 39 00 30 00 36 00 49 00 53 00 33 00 34 00 C.9.0.6.I.S.3.4.
	00f0 75 00 76 00 54 00 41 00 4e 00 42 00 67 00 6b 00 u.v.T.A.N.B.g.k.
	0100 71 00 68 00 6b 00 69 00 47 00 39 00 77 00 30 00 q.h.k.i.G.9.w.0.
	0110 42 00 41 00 51 00 55 00 46 00 41 00 44 00 41 00 B.A.Q.U.F.A.D.A.
	0120 61 00 0d 00 0a 00 4d 00 52 00 67 00 77 00 46 00 a.....M.R.g.w.F.
	0130 67 00 59 00 44 00 56 00 51 00 51 00 44 00 45 00 g.Y.D.V.Q.Q.D.E.
	0140 77 00 39 00 58 00 53 00 55 00 34 00 74 00 4e 00 w.9.X.S.U.4.t.N.
	0150 7a 00 56 00 46 00 52 00 45 00 4e 00 51 00 52 00 z.V.F.R.E.N.Q.R.
	0160 30 00 64 00 4e 00 52 00 7a 00 6b 00 77 00 48 00 0.d.N.R.z.k.w.H.
	0170 68 00 63 00 4e 00 4d 00 54 00 49 00 77 00 4d 00 h.c.N.M.T.I.w.M.
	0180 7a 00 49 00 34 00 4d 00 44 00 4d 00 77 00 4d 00 z.I.4.M.D.M.w.M.
	0190 54 00 45 00 32 00 57 00 68 00 63 00 4e 00 4d 00 T.E.2.W.h.c.N.M.
	01a0 6a 00 49 00 77 00 0d 00 0a 00 4d 00 7a 00 49 00 j.I.w.....M.z.I.
	01b0 33 00 4d 00 44 00 41 00 77 00 4d 00 44 00 41 00 3.M.D.A.w.M.D.A.
	01c0 77 00 57 00 6a 00 41 00 61 00 4d 00 52 00 67 00 w.W.j.A.a.M.R.g.
	01d0 77 00 46 00 67 00 59 00 44 00 56 00 51 00 51 00 w.F.g.Y.D.V.Q.Q.
	01e0 44 00 45 00 77 00 39 00 58 00 53 00 55 00 34 00 D.E.w.9.X.S.U.4.
	01f0 74 00 4e 00 7a 00 56 00 46 00 52 00 45 00 4e 00 t.N.z.V.F.R.E.N.
	0200 51 00 52 00 30 00 64 00 4e 00 52 00 7a 00 6b 00 Q.R.0.d.N.R.z.k.
	0210 77 00 67 00 67 00 45 00 69 00 4d 00 41 00 30 00 w.g.g.E.i.M.A.0.
	0220 47 00 43 00 53 00 71 00 47 00 0d 00 0a 00 53 00 G.C.S.q.G.....S.
	0230 49 00 62 00 33 00 44 00 51 00 45 00 42 00 41 00 I.b.3.D.Q.E.B.A.
	0240 51 00 55 00 41 00 41 00 34 00 49 00 42 00 44 00 Q.U.A.A.4.I.B.D.
	0250 77 00 41 00 77 00 67 00 67 00 45 00 4b 00 41 00 w.A.w.g.g.E.K.A.
	0260 6f 00 49 00 42 00 41 00 51 00 43 00 6e 00 78 00 o.I.B.A.Q.C.n.x.
	0270 55 00 63 00 32 00 68 00 2b 00 4c 00 5a 00 64 00 U.c.2.h.+.L.Z.d.
	0280 42 00 65 00 56 00 61 00 79 00 35 00 52 00 36 00 B.e.V.a.y.5.R.6.
	0290 68 00 70 00 36 00 47 00 31 00 2b 00 75 00 2f 00 h.p.6.G.1.+.u./.
	02a0 59 00 33 00 7a 00 63 00 33 00 33 00 47 00 0d 00 Y.3.z.c.3.3.G...
	02b0 0a 00 66 00 4c 00 62 00 4f 00 53 00 62 00 48 00 ..f.L.b.O.S.b.H.
	02c0 71 00 6d 00 4e 00 31 00 42 00 4e 00 57 00 79 00 q.m.N.1.B.N.W.y.
	02d0 44 00 7a 00 51 00 66 00 5a 00 71 00 30 00 54 00 D.z.Q.f.Z.q.0.T.
	02e0 35 00 30 00 4b 00 70 00 54 00 61 00 49 00 71 00 5.0.K.p.T.a.I.q.
	02f0 65 00 33 00 58 00 65 00 51 00 4f 00 45 00 63 00 e.3.X.e.Q.O.E.c.
	0300 42 00 33 00 4b 00 78 00 56 00 6a 00 78 00 46 00 B.3.K.x.V.j.x.F.
	0310 46 00 75 00 47 00 67 00 6d 00 57 00 4d 00 55 00 F.u.G.g.m.W.M.U.
	0320 6d 00 7a 00 37 00 79 00 77 00 49 00 49 00 75 00 m.z.7.y.w.I.I.u.
	0330 38 00 0d 00 0a 00 33 00 52 00 56 00 44 00 39 00 8.....3.R.V.D.9.
	0340 36 00 73 00 42 00 30 00 6b 00 31 00 6a 00 37 00 6.s.B.0.k.1.j.7.
	0350 70 00 30 00 4d 00 54 00 6f 00 4a 00 6a 00 71 00 p.0.M.T.o.J.j.q.
	0360 4a 00 45 00 78 00 51 00 56 00 6d 00 48 00 44 00 J.E.x.Q.V.m.H.D.
	0370 72 00 56 00 46 00 2f 00 63 00 4f 00 77 00 6a 00 r.V.F./.c.O.w.j.
	0380 35 00 59 00 69 00 35 00 42 00 33 00 47 00 57 00 5.Y.i.5.B.3.G.W.
	0390 38 00 65 00 65 00 37 00 5a 00 45 00 52 00 30 00 8.e.e.7.Z.E.R.0.
	03a0 76 00 63 00 62 00 4f 00 34 00 59 00 70 00 4c 00 v.c.b.O.4.Y.p.L.
	03b0 64 00 58 00 41 00 0d 00 0a 00 65 00 6f 00 6f 00 d.X.A.....e.o.o.
	03c0 62 00 48 00 6f 00 37 00 7a 00 73 00 65 00 59 00 b.H.o.7.z.s.e.Y.
	03d0 57 00 31 00 37 00 72 00 54 00 4b 00 79 00 73 00 W.1.7.r.T.K.y.s.
	03e0 65 00 59 00 52 00 69 00 6a 00 32 00 6a 00 76 00 e.Y.R.i.j.2.j.v.
	03f0 63 00 6e 00 75 00 6f 00 52 00 72 00 4a 00 48 00 c.n.u.o.R.r.J.H.
	0400 58 00 78 00 36 00 41 00 44 00 64 00 6f 00 57 00 X.x.6.A.D.d.o.W.
	0410 37 00 58 00 4e 00 69 00 39 00 59 00 75 00 55 00 7.X.N.i.9.Y.u.U.
	0420 4a 00 46 00 35 00 6b 00 51 00 46 00 34 00 64 00 J.F.5.k.Q.F.4.d.
	0430 6b 00 73 00 6c 00 5a 00 72 00 0d 00 0a 00 49 00 k.s.l.Z.r.....I.
	0440 44 00 50 00 50 00 6b 00 30 00 68 00 44 00 78 00 D.P.P.k.0.h.D.x.
	0450 6d 00 61 00 49 00 6c 00 5a 00 6a 00 47 00 6a 00 m.a.I.l.Z.j.G.j.
	0460 70 00 55 00 65 00 69 00 47 00 50 00 2b 00 57 00 p.U.e.i.G.P.+.W.
	0470 46 00 68 00 72 00 4d 00 6d 00 6f 00 6b 00 6f 00 F.h.r.M.m.o.k.o.
	0480 46 00 78 00 7a 00 2f 00 70 00 7a 00 61 00 38 00 F.x.z./.p.z.a.8.
	0490 5a 00 4c 00 50 00 4f 00 4a 00 64 00 51 00 76 00 Z.L.P.O.J.d.Q.v.
	04a0 6c 00 31 00 52 00 78 00 34 00 61 00 6e 00 64 00 l.1.R.x.4.a.n.d.
	04b0 43 00 38 00 4d 00 79 00 59 00 47 00 2b 00 0d 00 C.8.M.y.Y.G.+...
	04c0 0a 00 50 00 57 00 62 00 74 00 62 00 44 00 43 00 ..P.W.b.t.b.D.C.
	04d0 31 00 33 00 41 00 71 00 2f 00 44 00 4d 00 4c 00 1.3.A.q./.D.M.L.
	04e0 49 00 56 00 6b 00 6c 00 41 00 65 00 4e 00 6f 00 I.V.k.l.A.e.N.o.
	04f0 78 00 32 00 43 00 61 00 4a 00 65 00 67 00 30 00 x.2.C.a.J.e.g.0.
	0500 56 00 2b 00 48 00 6d 00 46 00 6b 00 70 00 59 00 V.+.H.m.F.k.p.Y.
	0510 68 00 75 00 34 00 6f 00 33 00 6b 00 38 00 6e 00 h.u.4.o.3.k.8.n.
	0520 58 00 5a 00 37 00 7a 00 35 00 41 00 67 00 4d 00 X.Z.7.z.5.A.g.M.
	0530 42 00 41 00 41 00 47 00 6a 00 4a 00 44 00 41 00 B.A.A.G.j.J.D.A.
	0540 69 00 0d 00 0a 00 4d 00 41 00 73 00 47 00 41 00 i.....M.A.s.G.A.
	0550 31 00 55 00 64 00 44 00 77 00 51 00 45 00 41 00 1.U.d.D.w.Q.E.A.
	0560 77 00 49 00 45 00 4d 00 44 00 41 00 54 00 42 00 w.I.E.M.D.A.T.B.
	0570 67 00 4e 00 56 00 48 00 53 00 55 00 45 00 44 00 g.N.V.H.S.U.E.D.
	0580 44 00 41 00 4b 00 42 00 67 00 67 00 72 00 42 00 D.A.K.B.g.g.r.B.
	0590 67 00 45 00 46 00 42 00 51 00 63 00 44 00 41 00 g.E.F.B.Q.c.D.A.
	05a0 54 00 41 00 4e 00 42 00 67 00 6b 00 71 00 68 00 T.A.N.B.g.k.q.h.
	05b0 6b 00 69 00 47 00 39 00 77 00 30 00 42 00 41 00 k.i.G.9.w.0.B.A.
	05c0 51 00 55 00 46 00 0d 00 0a 00 41 00 41 00 4f 00 Q.U.F.....A.A.O.
	05d0 43 00 41 00 51 00 45 00 41 00 52 00 33 00 74 00 C.A.Q.E.A.R.3.t.
	05e0 67 00 2f 00 6e 00 41 00 69 00 73 00 41 00 46 00 g./.n.A.i.s.A.F.
	05f0 42 00 50 00 66 00 5a 00 42 00 68 00 5a 00 31 00 B.P.f.Z.B.h.Z.1.
	0600 71 00 55 00 53 00 74 00 55 00 52 00 32 00 5a 00 q.U.S.t.U.R.2.Z.
	0610 32 00 5a 00 6a 00 55 00 49 00 42 00 70 00 64 00 2.Z.j.U.I.B.p.d.
	0620 68 00 5a 00 4e 00 32 00 64 00 50 00 6b 00 6f 00 h.Z.N.2.d.P.k.o.
	0630 4d 00 6c 00 32 00 4b 00 4f 00 6b 00 66 00 4d 00 M.l.2.K.O.k.f.M.
	0640 4e 00 6d 00 45 00 44 00 45 00 0d 00 0a 00 68 00 N.m.E.D.E.....h.
	0650 72 00 6e 00 56 00 74 00 71 00 54 00 79 00 65 00 r.n.V.t.q.T.y.e.
	0660 50 00 32 00 4e 00 52 00 71 00 78 00 67 00 48 00 P.2.N.R.q.x.g.H.
	0670 46 00 2b 00 48 00 2f 00 6e 00 4f 00 78 00 37 00 F.+.H./.n.O.x.7.
	0680 78 00 6d 00 66 00 49 00 72 00 4c 00 31 00 77 00 x.m.f.I.r.L.1.w.
	0690 45 00 6a 00 63 00 37 00 41 00 50 00 6c 00 37 00 E.j.c.7.A.P.l.7.
	06a0 4b 00 61 00 39 00 4b 00 6a 00 63 00 65 00 6f 00 K.a.9.K.j.c.e.o.
	06b0 4f 00 4f 00 6f 00 68 00 31 00 32 00 6c 00 2f 00 O.O.o.h.1.2.l./.
	06c0 39 00 48 00 53 00 70 00 38 00 30 00 37 00 0d 00 9.H.S.p.8.0.7...
	06d0 0a 00 4f 00 57 00 4d 00 69 00 48 00 41 00 4a 00 ..O.W.M.i.H.A.J.
	06e0 6e 00 66 00 47 00 32 00 46 00 46 00 37 00 6e 00 n.f.G.2.F.F.7.n.
	06f0 51 00 61 00 74 00 63 00 35 00 4e 00 53 00 42 00 Q.a.t.c.5.N.S.B.
	0700 38 00 4e 00 75 00 4f 00 5a 00 67 00 64 00 62 00 8.N.u.O.Z.g.d.b.
	0710 67 00 51 00 2b 00 43 00 42 00 62 00 39 00 76 00 g.Q.+.C.B.b.9.v.
	0720 2b 00 4d 00 56 00 55 00 33 00 43 00 67 00 39 00 +.M.V.U.3.C.g.9.
	0730 4c 00 57 00 65 00 54 00 53 00 2b 00 51 00 78 00 L.W.e.T.S.+.Q.x.
	0740 79 00 59 00 6c 00 37 00 4f 00 30 00 4c 00 43 00 y.Y.l.7.O.0.L.C.
	0750 4d 00 0d 00 0a 00 76 00 45 00 37 00 77 00 4a 00 M.....v.E.7.w.J.
	0760 58 00 53 00 70 00 70 00 4a 00 4c 00 42 00 6b 00 X.S.p.p.J.L.B.k.
	0770 69 00 52 00 72 00 63 00 73 00 51 00 6e 00 34 00 i.R.r.c.s.Q.n.4.
	0780 61 00 39 00 62 00 64 00 67 00 56 00 67 00 6b 00 a.9.b.d.g.V.g.k.
	0790 57 00 32 00 78 00 70 00 7a 00 56 00 4e 00 6d 00 W.2.x.p.z.V.N.m.
	07a0 6e 00 62 00 42 00 35 00 73 00 49 00 49 00 38 00 n.b.B.5.s.I.I.8.
	07b0 35 00 75 00 7a 00 37 00 78 00 76 00 46 00 47 00 5.u.z.7.x.v.F.G.
	07c0 50 00 47 00 65 00 70 00 4c 00 55 00 55 00 55 00 P.G.e.p.L.U.U.U.
	07d0 76 00 66 00 66 00 0d 00 0a 00 36 00 76 00 68 00 v.f.f.....6.v.h.
	07e0 56 00 46 00 5a 00 76 00 62 00 53 00 47 00 73 00 V.F.Z.v.b.S.G.s.
	07f0 77 00 6f 00 72 00 32 00 5a 00 6c 00 54 00 6c 00 w.o.r.2.Z.l.T.l.
	0800 79 00 57 00 70 00 79 00 67 00 5a 00 67 00 71 00 y.W.p.y.g.Z.g.q.
	0810 49 00 46 00 56 00 6f 00 73 00 43 00 4f 00 33 00 I.F.V.o.s.C.O.3.
	0820 34 00 39 00 53 00 65 00 2b 00 55 00 45 00 72 00 4.9.S.e.+.U.E.r.
	0830 72 00 54 00 48 00 46 00 7a 00 71 00 38 00 63 00 r.T.H.F.z.q.8.c.
	0840 76 00 7a 00 4b 00 76 00 63 00 6b 00 4f 00 75 00 v.z.K.v.c.k.O.u.
	0850 4f 00 6b 00 42 00 72 00 42 00 0d 00 0a 00 6c 00 O.k.B.r.B.....l.
	0860 69 00 79 00 4e 00 32 00 47 00 42 00 41 00 6f 00 i.y.N.2.G.B.A.o.
	0870 50 00 45 00 67 00 79 00 4d 00 48 00 41 00 35 00 P.E.g.y.M.H.A.5.
	0880 53 00 78 00 39 00 5a 00 39 00 37 00 35 00 75 00 S.x.9.Z.9.7.5.u.
	0890 6e 00 7a 00 50 00 74 00 77 00 3d 00 3d 00 0d 00 n.z.P.t.w.=.=...
	08a0 0a 00 2d 00 2d 00 2d 00 2d 00 2d 00 45 00 4e 00 ..-.-.-.-.-.E.N.
	08b0 44 00 20 00 43 00 45 00 52 00 54 00 49 00 46 00 D. .C.E.R.T.I.F.
	08c0 49 00 43 00 41 00 54 00 45 00 2d 00 2d 00 2d 00 I.C.A.T.E.-.-.-.
	08d0 2d 00 2d 00 0d 00 0a 00 00 00

	00 00 alignment pad?

	52 54 componentId
	43 56 packetId (TSG_PACKET_TYPE_VERSIONCAPS)
	TsgCapsPtr:		10 00 02 00
	NumCapabilities:	01 00 00 00
	MajorVersion:		01 00
	MinorVersion:		01 00
	quarantineCapabilities:	01 00
	pad:			00 00
	NumCapabilitiesConf:
	MaxCount: 01 00 00 00

		tsgCaps:	01 00 00 00 (TSG_CAPABILITY_TYPE_NAP)
		SwitchValue:	01 00 00 00 (TSG_CAPABILITY_TYPE_NAP)
			capabilities: 1f 00 00 00 =
				TSG_NAP_CAPABILITY_QUAR_SOH |
				TSG_NAP_CAPABILITY_IDLE_TIMEOUT |
				TSG_MESSAGING_CAP_CONSENT_SIGN |
				TSG_MESSAGING_CAP_SERVICE_MSG |
				TSG_MESSAGING_CAP_REAUTH

	00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ??

	TunnelContext:
		ContextType: 00 00 00 00
		ContextUuid: 81 1d 32 9f 3f ff 8d 41 ae 54 ba e4 7b b7 ef 43

	30 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ??
	00 00 00 00 0a 05 0c 00 00 00 00 00 01 00 00 00
	9d 13 89 41

	UINT32 TunnelId:	2e 85 76 3f
	HRESULT ReturnValue:	00 00 00 00
 */

uint8 tsg_packet2[112] =
{
	0x00, 0x00, 0x00, 0x00, 0x6A, 0x78, 0xE9, 0xAB, 0x02, 0x90, 0x1C, 0x44, 0x8D, 0x99, 0x29, 0x30,
	0x53, 0x6C, 0x04, 0x33, 0x52, 0x51, 0x00, 0x00, 0x52, 0x51, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x02, 0x00, 0x15, 0x00, 0x00, 0x00, 0x08, 0x00, 0x02, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x15, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x15, 0x00, 0x00, 0x00,
	0x61, 0x00, 0x62, 0x00, 0x63, 0x00, 0x2D, 0x00, 0x4E, 0x00, 0x48, 0x00, 0x35, 0x00, 0x37, 0x00,
	0x30, 0x00, 0x2E, 0x00, 0x43, 0x00, 0x53, 0x00, 0x4F, 0x00, 0x44, 0x00, 0x2E, 0x00, 0x6C, 0x00,
	0x6F, 0x00, 0x63, 0x00, 0x61, 0x00, 0x6C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/**
	TsProxyAuthorizeTunnel

	TunnelContext:
		ContextType:	0x00, 0x00, 0x00, 0x00,
		ContextUuid:	0x6A, 0x78, 0xE9, 0xAB, 0x02, 0x90, 0x1C, 0x44,
				0x8D, 0x99, 0x29, 0x30, 0x53, 0x6C, 0x04, 0x33,

	TsgPacket:
		PacketId:	0x52, 0x51, 0x00, 0x00,
		SwitchValue:	0x52, 0x51, 0x00, 0x00,

		PacketQuarRequestPtr: 0x00, 0x00, 0x02, 0x00,
		PacketQuarRequest:
			Flags:	0x00, 0x00, 0x00, 0x00,
			MachineNamePtr: 0x04, 0x00, 0x02, 0x00,
			NameLength:	0x15, 0x00, 0x00, 0x00,
			DataPtr:	0x08, 0x00, 0x02, 0x00,
			DataLen:	0x00, 0x00, 0x00, 0x00,
			MachineName:
				MaxCount:	0x15, 0x00, 0x00, 0x00, (21 elements)
				Offset:		0x00, 0x00, 0x00, 0x00,
				ActualCount:	0x15, 0x00, 0x00, 0x00, (21 elements)
				Array:		0x61, 0x00, 0x62, 0x00, 0x63, 0x00, 0x2D, 0x00,
						0x4E, 0x00, 0x48, 0x00, 0x35, 0x00, 0x37, 0x00,
						0x30, 0x00, 0x2E, 0x00, 0x43, 0x00, 0x53, 0x00,
						0x4F, 0x00, 0x44, 0x00, 0x2E, 0x00, 0x6C, 0x00,
						0x6F, 0x00, 0x63, 0x00, 0x61, 0x00, 0x6C, 0x00,
						0x00, 0x00,

			DataLenConf:
				MaxCount: 0x00, 0x00, 0x00, 0x00,
						0x00, 0x00
 */

uint8 tsg_packet3[40] =
{
	0x00, 0x00, 0x00, 0x00, 0x6A, 0x78, 0xE9, 0xAB, 0x02, 0x90, 0x1C, 0x44, 0x8D, 0x99, 0x29, 0x30,
	0x53, 0x6C, 0x04, 0x33, 0x01, 0x00, 0x00, 0x00, 0x52, 0x47, 0x00, 0x00, 0x52, 0x47, 0x00, 0x00,
	0x00, 0x00, 0x02, 0x00, 0x01, 0x00, 0x00, 0x00
};

/**
	TsProxyMakeTunnelCall

	0x00, 0x00, 0x00, 0x00, 0x6A, 0x78, 0xE9, 0xAB, 0x02, 0x90, 0x1C, 0x44, 0x8D, 0x99, 0x29, 0x30,
	0x53, 0x6C, 0x04, 0x33, 0x01, 0x00, 0x00, 0x00,

	0x52, 0x47, 0x00, 0x00,
	0x52, 0x47, 0x00, 0x00,
	0x00, 0x00, 0x02, 0x00,
	0x01, 0x00, 0x00, 0x00
 */

uint8 tsg_packet4[48] =
{
	0x00, 0x00, 0x00, 0x00, 0x6A, 0x78, 0xE9, 0xAB, 0x02, 0x90, 0x1C, 0x44, 0x8D, 0x99, 0x29, 0x30,
	0x53, 0x6C, 0x04, 0x33, 0x00, 0x00, 0x02, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x04, 0x00, 0x02, 0x00
};

/**
	TsProxyCreateChannel

	0x00, 0x00, 0x00, 0x00, 0x6A, 0x78, 0xE9, 0xAB, 0x02, 0x90, 0x1C, 0x44, 0x8D, 0x99, 0x29, 0x30,
	0x53, 0x6C, 0x04, 0x33, 0x00, 0x00, 0x02, 0x00,

	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
	0x01, 0x00, 0x00, 0x00, 0x04, 0x00, 0x02, 0x00
 */

uint8 tsg_packet5[20] =
{
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
};

DWORD TsProxySendToServer(handle_t IDL_handle, byte pRpcMessage[], uint32 count, uint32* lengths)
{
	STREAM* s;
	int status;
	int length;
	rdpTsg* tsg;
	byte* buffer1;
	byte* buffer2;
	byte* buffer3;
	uint32 buffer1Length;
	uint32 buffer2Length;
	uint32 buffer3Length;
	uint32 numBuffers = 0;
	uint32 totalDataBytes = 0;

	tsg = (rdpTsg*) IDL_handle;
	buffer1Length = buffer2Length = buffer3Length = 0;

	if (count > 0)
	{
		numBuffers++;
		buffer1 = &pRpcMessage[0];
		buffer1Length = lengths[0];
		totalDataBytes += lengths[0] + 4;
	}

	if (count > 1)
	{
		numBuffers++;
		buffer2 = &pRpcMessage[1];
		buffer2Length = lengths[1];
		totalDataBytes += lengths[1] + 4;
	}

	if (count > 2)
	{
		numBuffers++;
		buffer3 = &pRpcMessage[2];
		buffer3Length = lengths[2];
		totalDataBytes += lengths[2] + 4;
	}

	s = stream_new(28 + totalDataBytes);

	/* PCHANNEL_CONTEXT_HANDLE_NOSERIALIZE_NR (20 bytes) */
	stream_write_uint32(s, 0); /* ContextType (4 bytes) */
	stream_write(s, tsg->ChannelContext, 16); /* ContextUuid (4 bytes) */

	stream_write_uint32_be(s, totalDataBytes); /* totalDataBytes (4 bytes) */
	stream_write_uint32_be(s, numBuffers); /* numBuffers (4 bytes) */

	if (buffer1Length > 0)
		stream_write_uint32_be(s, buffer1Length); /* buffer1Length (4 bytes) */
	if (buffer2Length > 0)
		stream_write_uint32_be(s, buffer2Length); /* buffer2Length (4 bytes) */
	if (buffer3Length > 0)
		stream_write_uint32_be(s, buffer3Length); /* buffer3Length (4 bytes) */

	if (buffer1Length > 0)
		stream_write(s, buffer1, buffer1Length); /* buffer1 (variable) */
	if (buffer2Length > 0)
		stream_write(s, buffer2, buffer2Length); /* buffer2 (variable) */
	if (buffer3Length > 0)
		stream_write(s, buffer3, buffer3Length); /* buffer3 (variable) */

	stream_seal(s);

	length = s->size;
	status = rpc_tsg_write(tsg->rpc, s->data, s->size, 9);

	stream_free(s);

	if (status <= 0)
	{
		printf("rpc_tsg_write failed!\n");
		return -1;
	}

	return length;
}

#ifndef WITH_MSRPC
boolean tsg_connect(rdpTsg* tsg, const char* hostname, uint16 port)
{
	uint8* data;
	uint32 length;
	STREAM* s_p4;
	int status = -1;
	UNICONV* tsg_uniconv;
	rdpRpc* rpc = tsg->rpc;
	uint8* dest_addr_unic;
	uint32 dest_addr_unic_len;

	if (!rpc_connect(rpc))
	{
		printf("rpc_connect failed!\n");
		return false;
	}

	DEBUG_TSG("rpc_connect success");

	/**
	 * OpNum = 1
	 *
	 * HRESULT TsProxyCreateTunnel(
	 * [in, ref] PTSG_PACKET tsgPacket,
	 * [out, ref] PTSG_PACKET* tsgPacketResponse,
	 * [out] PTUNNEL_CONTEXT_HANDLE_SERIALIZE* tunnelContext,
	 * [out] unsigned long* tunnelId
	 * );
	 */

	DEBUG_TSG("TsProxyCreateTunnel");
	status = rpc_tsg_write(rpc, tsg_packet1, sizeof(tsg_packet1), 1);

	if (status <= 0)
	{
		printf("rpc_write opnum=1 failed!\n");
		return false;
	}

	length = 0x8FFF;
	data = xmalloc(length);
	status = rpc_read(rpc, data, length);

	if (status <= 0)
	{
		printf("rpc_recv failed!\n");
		return false;
	}

	memcpy(tsg->TunnelContext, data + (status - 24), 16);

#ifdef WITH_DEBUG_TSG
	printf("TSG TunnelContext:\n");
	freerdp_hexdump(tsg->TunnelContext, 16);
	printf("\n");
#endif

	memcpy(tsg_packet2 + 4, tsg->TunnelContext, 16);

	/**
	 * OpNum = 2
	 *
	 * HRESULT TsProxyAuthorizeTunnel(
	 * [in] PTUNNEL_CONTEXT_HANDLE_NOSERIALIZE tunnelContext,
	 * [in, ref] PTSG_PACKET tsgPacket,
	 * [out, ref] PTSG_PACKET* tsgPacketResponse
	 * );
	 *
	 */

	DEBUG_TSG("TsProxyAuthorizeTunnel");
	status = rpc_tsg_write(rpc, tsg_packet2, sizeof(tsg_packet2), 2);

	if (status <= 0)
	{
		printf("rpc_write opnum=2 failed!\n");
		return false;
	}

	status = rpc_read(rpc, data, length);

	if (status <= 0)
	{
		printf("rpc_recv failed!\n");
		return false;
	}

	memcpy(tsg_packet3 + 4, tsg->TunnelContext, 16);

	/**
	 * OpNum = 3
	 *
	 * HRESULT TsProxyMakeTunnelCall(
	 * [in] PTUNNEL_CONTEXT_HANDLE_NOSERIALIZE tunnelContext,
	 * [in] unsigned long procId,
	 * [in, ref] PTSG_PACKET tsgPacket,
	 * [out, ref] PTSG_PACKET* tsgPacketResponse
	 * );
	 */

	DEBUG_TSG("TsProxyMakeTunnelCall");
	status = rpc_tsg_write(rpc, tsg_packet3, sizeof(tsg_packet3), 3);

	if (status <= 0)
	{
		printf("rpc_write opnum=3 failed!\n");
		return false;
	}
	status = -1;

	tsg_uniconv = freerdp_uniconv_new();
	dest_addr_unic = (uint8*) freerdp_uniconv_out(tsg_uniconv, hostname, (size_t*) &dest_addr_unic_len);
	freerdp_uniconv_free(tsg_uniconv);

	memcpy(tsg_packet4 + 4, tsg->TunnelContext, 16);
	memcpy(tsg_packet4 + 38, &port, 2);

	s_p4 = stream_new(60 + dest_addr_unic_len + 2);
	stream_write(s_p4, tsg_packet4, 48);
	stream_write_uint32(s_p4, (dest_addr_unic_len / 2) + 1); /* MaximumCount */
	stream_write_uint32(s_p4, 0x00000000); /* Offset */
	stream_write_uint32(s_p4, (dest_addr_unic_len / 2) + 1);/* ActualCount */
	stream_write(s_p4, dest_addr_unic, dest_addr_unic_len);
	stream_write_uint16(s_p4, 0x0000); /* unicode zero to terminate hostname string */

	/**
	 * OpNum = 4
	 *
	 * HRESULT TsProxyCreateChannel(
	 * [in] PTUNNEL_CONTEXT_HANDLE_NOSERIALIZE tunnelContext,
	 * [in, ref] PTSENDPOINTINFO tsEndPointInfo,
	 * [out] PCHANNEL_CONTEXT_HANDLE_SERIALIZE* channelContext,
	 * [out] unsigned long* channelId
	 * );
	 */

	DEBUG_TSG("TsProxyCreateChannel");
	status = rpc_tsg_write(rpc, s_p4->data, s_p4->size, 4);

	if (status <= 0)
	{
		printf("rpc_write opnum=4 failed!\n");
		return false;
	}
	xfree(dest_addr_unic);

	status = rpc_read(rpc, data, length);

	if (status < 0)
	{
		printf("rpc_recv failed!\n");
		return false;
	}

	memcpy(tsg->ChannelContext, data + 4, 16);

#ifdef WITH_DEBUG_TSG
	printf("TSG ChannelContext:\n");
	freerdp_hexdump(tsg->ChannelContext, 16);
	printf("\n");
#endif

	memcpy(tsg_packet5 + 4, tsg->ChannelContext, 16);

	/**
	 * OpNum = 8
	 *
	 * DWORD TsProxySetupReceivePipe(
	 * [in, max_is(32767)] byte pRpcMessage[]
	 * );
	 */

	DEBUG_TSG("TsProxySetupReceivePipe");
	status = rpc_tsg_write(rpc, tsg_packet5, sizeof(tsg_packet5), 8);

	if (status <= 0)
	{
		printf("rpc_write opnum=8 failed!\n");
		return false;
	}

	return true;
}
#else
boolean tsg_connect(rdpTsg* tsg, const char* hostname, uint16 port)
{
	uint8* data;
	uint32 length;
	STREAM* s_p4;
	int status = -1;
	UNICONV* tsg_uniconv;
	rdpRpc* rpc = tsg->rpc;
	uint8* dest_addr_unic;
	uint32 dest_addr_unic_len;

	if (!rpc_connect(rpc))
	{
		printf("rpc_connect failed!\n");
		return false;
	}

	DEBUG_TSG("rpc_connect success");

	/**
	 * OpNum = 1
	 *
	 * HRESULT TsProxyCreateTunnel(
	 * [in, ref] PTSG_PACKET tsgPacket,
	 * [out, ref] PTSG_PACKET* tsgPacketResponse,
	 * [out] PTUNNEL_CONTEXT_HANDLE_SERIALIZE* tunnelContext,
	 * [out] unsigned long* tunnelId
	 * );
	 */

	DEBUG_TSG("TsProxyCreateTunnel");

	{
		TSG_PACKET tsgPacket;
		PTSG_PACKET tsgPacketResponse;
		PTUNNEL_CONTEXT_HANDLE_SERIALIZE tunnelContext;
		unsigned long tunnelId;

		TsProxyCreateTunnel(&tsgPacket, &tsgPacketResponse, &tunnelContext, &tunnelId);
	}

	/**
	 * OpNum = 2
	 *
	 * HRESULT TsProxyAuthorizeTunnel(
	 * [in] PTUNNEL_CONTEXT_HANDLE_NOSERIALIZE tunnelContext,
	 * [in, ref] PTSG_PACKET tsgPacket,
	 * [out, ref] PTSG_PACKET* tsgPacketResponse
	 * );
	 *
	 */

	DEBUG_TSG("TsProxyAuthorizeTunnel");

	{
		TSG_PACKET tsgPacket;
		PTSG_PACKET tsgPacketResponse;
		PTUNNEL_CONTEXT_HANDLE_NOSERIALIZE tunnelContext;

		TsProxyAuthorizeTunnel(&tunnelContext, &tsgPacket, &tsgPacketResponse);
	}

	/**
	 * OpNum = 3
	 *
	 * HRESULT TsProxyMakeTunnelCall(
	 * [in] PTUNNEL_CONTEXT_HANDLE_NOSERIALIZE tunnelContext,
	 * [in] unsigned long procId,
	 * [in, ref] PTSG_PACKET tsgPacket,
	 * [out, ref] PTSG_PACKET* tsgPacketResponse
	 * );
	 */

	DEBUG_TSG("TsProxyMakeTunnelCall");

	{
		TSG_PACKET tsgPacket;
		PTSG_PACKET tsgPacketResponse;
		PTUNNEL_CONTEXT_HANDLE_NOSERIALIZE tunnelContext;

		TsProxyMakeTunnelCall(&tunnelContext, 0, &tsgPacket, &tsgPacketResponse);
	}

	/**
	 * OpNum = 4
	 *
	 * HRESULT TsProxyCreateChannel(
	 * [in] PTUNNEL_CONTEXT_HANDLE_NOSERIALIZE tunnelContext,
	 * [in, ref] PTSENDPOINTINFO tsEndPointInfo,
	 * [out] PCHANNEL_CONTEXT_HANDLE_SERIALIZE* channelContext,
	 * [out] unsigned long* channelId
	 * );
	 */

	DEBUG_TSG("TsProxyCreateChannel");

	{
		unsigned long channelId;
		TSENDPOINTINFO tsEndPointInfo;
		PTUNNEL_CONTEXT_HANDLE_NOSERIALIZE tunnelContext;
		PCHANNEL_CONTEXT_HANDLE_SERIALIZE channelContext;

		TsProxyCreateChannel(&tunnelContext, &tsEndPointInfo, &channelContext, &channelId);
	}

	/**
	 * OpNum = 8
	 *
	 * DWORD TsProxySetupReceivePipe(
	 * [in, max_is(32767)] byte pRpcMessage[]
	 * );
	 */

	DEBUG_TSG("TsProxySetupReceivePipe");

	return true;
}
#endif

int tsg_read(rdpTsg* tsg, uint8* data, uint32 length)
{
	int status;

	status = rpc_read(tsg->rpc, data, length);

	return status;
}

int tsg_write(rdpTsg* tsg, uint8* data, uint32 length)
{
	return TsProxySendToServer((handle_t) tsg, data, 1, &length);
}

rdpTsg* tsg_new(rdpTransport* transport)
{
	rdpTsg* tsg;

	tsg = xnew(rdpTsg);

	if (tsg != NULL)
	{
		tsg->transport = transport;
		tsg->settings = transport->settings;
		tsg->rpc = rpc_new(tsg->transport);
	}

	return tsg;
}

void tsg_free(rdpTsg* tsg)
{
	if (tsg != NULL)
	{
		rpc_free(tsg->rpc);
		xfree(tsg);
	}
}
