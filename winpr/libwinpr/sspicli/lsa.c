/**
 * WinPR: Windows Portable Runtime
 * LSA authentication package client (lsa4posix backend)
 *
 * Implements the winpr/secapi.h LSA client API on non-Windows platforms by
 * talking to the per-user lsa4posix daemon over an AF_UNIX stream socket.
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

#ifndef _WIN32

#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/nt.h>
#include <winpr/secapi.h>

#include "lsa_proto.h"
#include "lsa_transport.h"

typedef struct
{
	int fd;
	UINT32 nextRequestId;
	UINT64 daemonInstanceId;
} LsaConnection;

NTSTATUS LsaConnectUntrusted(PHANDLE LsaHandle)
{
	if (!LsaHandle)
		return STATUS_INVALID_PARAMETER;
	*LsaHandle = nullptr;

	const int fd = lsa4posix_client_connect();
	if (fd < 0)
		return STATUS_NO_SUCH_PACKAGE;

	LsaConnection* conn = (LsaConnection*)calloc(1, sizeof(LsaConnection));
	if (!conn)
	{
		lsa4posix_client_close(fd);
		return STATUS_NO_MEMORY;
	}
	conn->fd = fd;
	conn->nextRequestId = 1;

	const UINT32 requestId = conn->nextRequestId++;
	LSA_MSG_HDR hdr = { LSA_MSG_HELLO, 0, requestId };
	LsaProtoWriter w = { 0 };
	wStream* request = nullptr;
	if (!lsa_proto_writer_init(&w, &hdr) || !lsa_proto_write_u16(&w, LSA_PROTO_VERSION_MAJOR) ||
	    !lsa_proto_write_u16(&w, LSA_PROTO_VERSION_MINOR) || !lsa_proto_write_u32(&w, 0) ||
	    !lsa_proto_writer_finish(&w, &request))
	{
		lsa_proto_writer_abort(&w);
		lsa4posix_client_close(fd);
		free(conn);
		return STATUS_UNSUCCESSFUL;
	}

	LsaProtoReader reader = { 0 };
	LSA_MSG_HDR respHdr = { 0 };
	wStream* response = nullptr;
	NTSTATUS status =
	    lsa4posix_transact(fd, LSA_MSG_HELLO, request, requestId, &reader, &respHdr, &response);
	if (status != STATUS_SUCCESS)
	{
		lsa4posix_client_close(fd);
		free(conn);
		return status;
	}

	UINT32 respStatus = 0;
	UINT16 major = 0;
	UINT16 minor = 0;
	UINT32 serverFlags = 0;
	if (!lsa_proto_read_u32(&reader, &respStatus) || !lsa_proto_read_u16(&reader, &major) ||
	    !lsa_proto_read_u16(&reader, &minor) || !lsa_proto_read_u32(&reader, &serverFlags) ||
	    !lsa_proto_read_u64(&reader, &conn->daemonInstanceId))
		status = STATUS_UNSUCCESSFUL;
	else if (respStatus != STATUS_SUCCESS)
		status = (NTSTATUS)respStatus;

	lsa_proto_reader_done(&reader);
	Stream_Free(response, TRUE);

	if (status != STATUS_SUCCESS)
	{
		lsa4posix_client_close(fd);
		free(conn);
		return status;
	}

	*LsaHandle = (HANDLE)conn;
	return STATUS_SUCCESS;
}

NTSTATUS LsaLookupAuthenticationPackage(HANDLE LsaHandle, PLSA_STRING PackageName,
                                        PULONG AuthenticationPackage)
{
	if (!LsaHandle || !PackageName || !AuthenticationPackage)
		return STATUS_INVALID_PARAMETER;

	LsaConnection* conn = (LsaConnection*)LsaHandle;
	*AuthenticationPackage = 0;

	const UINT32 requestId = conn->nextRequestId++;
	LSA_MSG_HDR hdr = { LSA_MSG_LOOKUP_PACKAGE, 0, requestId };
	LsaProtoWriter w = { 0 };
	wStream* request = nullptr;
	if (!lsa_proto_writer_init(&w, &hdr) ||
	    !lsa_proto_write_blob(&w, (const BYTE*)PackageName->Buffer, PackageName->Length) ||
	    !lsa_proto_writer_finish(&w, &request))
	{
		lsa_proto_writer_abort(&w);
		return STATUS_UNSUCCESSFUL;
	}

	LsaProtoReader reader = { 0 };
	LSA_MSG_HDR respHdr = { 0 };
	wStream* response = nullptr;
	NTSTATUS status = lsa4posix_transact(conn->fd, LSA_MSG_LOOKUP_PACKAGE, request, requestId,
	                                     &reader, &respHdr, &response);
	if (status != STATUS_SUCCESS)
		return status;

	UINT32 respStatus = 0;
	UINT32 packageId = 0;
	if (!lsa_proto_read_u32(&reader, &respStatus) || !lsa_proto_read_u32(&reader, &packageId))
		status = STATUS_UNSUCCESSFUL;
	else if (respStatus != STATUS_SUCCESS)
		status = (NTSTATUS)respStatus;
	else
		*AuthenticationPackage = packageId;

	lsa_proto_reader_done(&reader);
	Stream_Free(response, TRUE);
	return status;
}

NTSTATUS LsaCallAuthenticationPackage(HANDLE LsaHandle, ULONG AuthenticationPackage,
                                      PVOID ProtocolSubmitBuffer, ULONG SubmitBufferLength,
                                      PVOID* ProtocolReturnBuffer, PULONG ReturnBufferLength,
                                      PNTSTATUS ProtocolStatus)
{
	if (!LsaHandle || (!ProtocolSubmitBuffer && (SubmitBufferLength > 0)))
		return STATUS_INVALID_PARAMETER;

	LsaConnection* conn = (LsaConnection*)LsaHandle;
	if (ProtocolReturnBuffer)
		*ProtocolReturnBuffer = nullptr;
	if (ReturnBufferLength)
		*ReturnBufferLength = 0;
	if (ProtocolStatus)
		*ProtocolStatus = STATUS_SUCCESS;

	const UINT32 requestId = conn->nextRequestId++;
	LSA_MSG_HDR hdr = { LSA_MSG_CALL_PACKAGE, 0, requestId };
	LsaProtoWriter w = { 0 };
	wStream* request = nullptr;
	if (!lsa_proto_writer_init(&w, &hdr) || !lsa_proto_write_u32(&w, AuthenticationPackage) ||
	    !lsa_proto_write_blob(&w, (const BYTE*)ProtocolSubmitBuffer, SubmitBufferLength) ||
	    !lsa_proto_writer_finish(&w, &request))
	{
		lsa_proto_writer_abort(&w);
		return STATUS_UNSUCCESSFUL;
	}

	LsaProtoReader reader = { 0 };
	LSA_MSG_HDR respHdr = { 0 };
	wStream* response = nullptr;
	NTSTATUS status = lsa4posix_transact(conn->fd, LSA_MSG_CALL_PACKAGE, request, requestId,
	                                     &reader, &respHdr, &response);
	if (status != STATUS_SUCCESS)
		return status;

	UINT32 respStatus = 0;
	UINT32 protocolStatus = 0;
	BYTE* ret = nullptr;
	UINT32 retLen = 0;
	if (!lsa_proto_read_u32(&reader, &respStatus) ||
	    !lsa_proto_read_u32(&reader, &protocolStatus) ||
	    !lsa_proto_read_blob(&reader, &ret, &retLen))
		status = STATUS_UNSUCCESSFUL;
	else if (respStatus != STATUS_SUCCESS)
		status = (NTSTATUS)respStatus;
	else
	{
		if (ProtocolStatus)
			*ProtocolStatus = (NTSTATUS)protocolStatus;
		if (ProtocolReturnBuffer)
			*ProtocolReturnBuffer = ret;
		else
			free(ret);
		if (ReturnBufferLength)
			*ReturnBufferLength = retLen;
		ret = nullptr;
	}

	free(ret);
	lsa_proto_reader_done(&reader);
	Stream_Free(response, TRUE);
	return status;
}

NTSTATUS LsaFreeReturnBuffer(PVOID Buffer)
{
	free(Buffer);
	return STATUS_SUCCESS;
}

NTSTATUS LsaDeregisterLogonProcess(HANDLE LsaHandle)
{
	LsaConnection* conn = (LsaConnection*)LsaHandle;
	if (!conn)
		return STATUS_INVALID_HANDLE;

	lsa4posix_client_close(conn->fd);
	free(conn);
	return STATUS_SUCCESS;
}

ULONG LsaNtStatusToWinError(NTSTATUS Status)
{
	switch (Status)
	{
		case STATUS_SUCCESS:
			return ERROR_SUCCESS;
		case STATUS_NO_SUCH_PACKAGE:
			return ERROR_NO_SUCH_PACKAGE;
		case STATUS_NO_MEMORY:
			return ERROR_NOT_ENOUGH_MEMORY;
		case STATUS_INVALID_PARAMETER:
			return ERROR_INVALID_PARAMETER;
		case STATUS_INVALID_HANDLE:
			return ERROR_INVALID_HANDLE;
		default:
			return ERROR_GEN_FAILURE;
	}
}

#endif /* _WIN32 */
