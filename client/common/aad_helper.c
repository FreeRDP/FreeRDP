/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Client-driven out-of-process browser helper (JSON-RPC over dedicated pipes)
 *
 * Copyright 2026 David Fort <contact@hardening-consulting.com>
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

#include <stdlib.h>
#include <string.h>

#include <winpr/assert.h>
#include <winpr/file.h>
#include <winpr/handle.h>
#include <winpr/json.h>
#include <winpr/pipe.h>
#include <winpr/string.h>
#include <winpr/synch.h>
#include <winpr/thread.h>

#include <freerdp/log.h>
#include <freerdp/client/aad_helper.h>

#define TAG CLIENT_TAG("common.aadauth")

struct AadAuthHelper
{
	HANDLE hCmdOutRead; /* parent's read end: helper -> FreeRDP responses/notifications */
	HANDLE hCmdInWrite; /* parent's write end: FreeRDP -> helper requests */
	HANDLE hProcess;
	UINT32 nextId;

	BYTE* buf;
	size_t bufLen;
};

/* ---- wire format helpers ------------------------------------------------------------- */

static char* build_hello_request(UINT32 id)
{
	WINPR_JSON* obj = WINPR_JSON_CreateObject();
	if (!obj)
		return nullptr;

	BOOL ok = WINPR_JSON_AddStringToObject(obj, "jsonrpc", "2.0") &&
	          WINPR_JSON_AddIntegerToObject(obj, "id", id) &&
	          WINPR_JSON_AddStringToObject(obj, "method", "hello");

	WINPR_JSON* params = ok ? WINPR_JSON_AddObjectToObject(obj, "params") : nullptr;
	ok = ok && params && WINPR_JSON_AddIntegerToObject(params, "protocol_version", 1) &&
	     WINPR_JSON_AddStringToObject(params, "client", "freerdp");

	char* str = ok ? WINPR_JSON_PrintUnformatted(obj) : nullptr;
	WINPR_JSON_Delete(obj);
	return str;
}

static char* build_navigate_request(UINT32 id, const char* title, const char* url,
                                    const char* redirect_uri, UINT32 timeout_ms)
{
	WINPR_JSON* obj = WINPR_JSON_CreateObject();
	if (!obj)
		return nullptr;

	BOOL ok = WINPR_JSON_AddStringToObject(obj, "jsonrpc", "2.0") &&
	          WINPR_JSON_AddIntegerToObject(obj, "id", id) &&
	          WINPR_JSON_AddStringToObject(obj, "method", "navigate");

	WINPR_JSON* params = ok ? WINPR_JSON_AddObjectToObject(obj, "params") : nullptr;
	ok = ok && params && WINPR_JSON_AddStringToObject(params, "title", title) &&
	     WINPR_JSON_AddStringToObject(params, "url", url) &&
	     WINPR_JSON_AddStringToObject(params, "redirect_uri", redirect_uri) &&
	     WINPR_JSON_AddIntegerToObject(params, "timeout_ms", timeout_ms);

	char* str = ok ? WINPR_JSON_PrintUnformatted(obj) : nullptr;
	WINPR_JSON_Delete(obj);
	return str;
}

static char* build_shutdown_request(UINT32 id)
{
	WINPR_JSON* obj = WINPR_JSON_CreateObject();
	if (!obj)
		return nullptr;

	BOOL ok = WINPR_JSON_AddStringToObject(obj, "jsonrpc", "2.0") &&
	          WINPR_JSON_AddIntegerToObject(obj, "id", id) &&
	          WINPR_JSON_AddStringToObject(obj, "method", "shutdown");

	char* str = ok ? WINPR_JSON_PrintUnformatted(obj) : nullptr;
	WINPR_JSON_Delete(obj);
	return str;
}

static char* build_exit_notification(void)
{
	WINPR_JSON* obj = WINPR_JSON_CreateObject();
	if (!obj)
		return nullptr;

	BOOL ok = WINPR_JSON_AddStringToObject(obj, "jsonrpc", "2.0") &&
	          WINPR_JSON_AddStringToObject(obj, "method", "exit");

	char* str = ok ? WINPR_JSON_PrintUnformatted(obj) : nullptr;
	WINPR_JSON_Delete(obj);
	return str;
}

/* ---- transport: newline-delimited JSON over the helper's cmdIn/cmdOut pipes ----------- */

static BOOL helper_write_line(AadAuthHelper* helper, const char* json)
{
	WINPR_ASSERT(helper);
	WINPR_ASSERT(json);

	const size_t len = strlen(json);
	const size_t total = len + 1; /* trailing '\n' */
	char* line = malloc(total);
	if (!line)
		return FALSE;
	/* terminated with '\n', not '\0' - written as-is over the wire, never treated as a C string */
	// NOLINTNEXTLINE(bugprone-not-null-terminated-result)
	memcpy(line, json, len);
	line[len] = '\n';

	BOOL rc = TRUE;
	size_t written = 0;
	while (written < total)
	{
		DWORD dwWritten = 0;
		if (!WriteFile(helper->hCmdInWrite, line + written, (DWORD)(total - written), &dwWritten,
		               nullptr) ||
		    (dwWritten == 0))
		{
			WLog_ERR(TAG, "aad-auth-helper: failed writing to helper");
			rc = FALSE;
			break;
		}
		written += dwWritten;
	}

	free(line);
	return rc;
}

/** extracts one '\n'-terminated line already buffered in helper->buf, if any */
static char* linebuf_extract(AadAuthHelper* helper)
{
	if (!helper->buf || !helper->bufLen)
		return nullptr;

	const BYTE* nl = memchr(helper->buf, '\n', helper->bufLen);
	if (!nl)
		return nullptr;

	const size_t lineLen = (size_t)(nl - helper->buf);
	char* line = malloc(lineLen + 1);
	if (!line)
		return nullptr;
	memcpy(line, helper->buf, lineLen);
	line[lineLen] = '\0';

	const size_t consumed = lineLen + 1;
	const size_t remaining = helper->bufLen - consumed;
	memmove(helper->buf, helper->buf + consumed, remaining);
	helper->bufLen = remaining;
	return line;
}

static char* helper_read_line(AadAuthHelper* helper)
{
	WINPR_ASSERT(helper);

	char* line = linebuf_extract(helper);
	if (line)
		return line;

	while (TRUE)
	{
		BYTE chunk[4096];
		DWORD dwRead = 0;
		if (!ReadFile(helper->hCmdOutRead, chunk, sizeof(chunk), &dwRead, nullptr) || (dwRead == 0))
		{
			WLog_ERR(TAG, "aad-auth-helper: helper pipe closed or read error");
			return nullptr;
		}

		BYTE* nbuf = realloc(helper->buf, helper->bufLen + dwRead);
		if (!nbuf)
			return nullptr;
		helper->buf = nbuf;
		memcpy(helper->buf + helper->bufLen, chunk, dwRead);
		helper->bufLen += dwRead;

		line = linebuf_extract(helper);
		if (line)
			return line;
	}
}

/** reads and discards notifications (forwarding "log" ones to WLog) until the response with
 *  id == expectedId is found. Returns the parsed message (caller frees with WINPR_JSON_Delete),
 *  or nullptr on a transport failure. */
static WINPR_JSON* wait_for_response(AadAuthHelper* helper, UINT32 expectedId)
{
	while (TRUE)
	{
		char* line = helper_read_line(helper);
		if (!line)
			return nullptr;

		WINPR_JSON* msg = WINPR_JSON_Parse(line);
		free(line);
		if (!msg)
		{
			WLog_WARN(TAG, "aad-auth-helper: ignoring malformed line from helper");
			continue;
		}

		WINPR_JSON* method = WINPR_JSON_GetObjectItemCaseSensitive(msg, "method");
		if (method && WINPR_JSON_IsString(method))
		{
			const char* m = WINPR_JSON_GetStringValue(method);
			if (m && (strcmp(m, "log") == 0))
			{
				WINPR_JSON* params = WINPR_JSON_GetObjectItemCaseSensitive(msg, "params");
				WINPR_JSON* message =
				    params ? WINPR_JSON_GetObjectItemCaseSensitive(params, "message") : nullptr;
				const char* text = (message && WINPR_JSON_IsString(message))
				                       ? WINPR_JSON_GetStringValue(message)
				                       : "";
				WLog_INFO(TAG, "[helper] %s", text);
			}
			WINPR_JSON_Delete(msg);
			continue;
		}

		WINPR_JSON* id = WINPR_JSON_GetObjectItemCaseSensitive(msg, "id");
		const double idValue = (id && WINPR_JSON_IsNumber(id)) ? WINPR_JSON_GetNumberValue(id) : 0;
		if (!id || !WINPR_JSON_IsNumber(id) || ((UINT32)idValue != expectedId))
		{
			WLog_WARN(TAG, "aad-auth-helper: dropping response with unexpected id");
			WINPR_JSON_Delete(msg);
			continue;
		}

		return msg;
	}
}

/* ---- public API ------------------------------------------------------------------------ */

AadAuthHelper* aad_auth_helper_start(const char* helper_path)
{
	WINPR_ASSERT(helper_path);

	AadAuthHelper* helper = calloc(1, sizeof(AadAuthHelper));
	if (!helper)
		return nullptr;

	SECURITY_ATTRIBUTES saAttr = { 0 };
	STARTUPINFOEXA siStartInfoEx = { 0 };
	PROCESS_INFORMATION procInfo = { 0 };
	LPPROC_THREAD_ATTRIBUTE_LIST attrList = nullptr;
	HANDLE hCmdInRead = nullptr;   /* child's end, handed away via --cmdInFd= */
	HANDLE hCmdOutWrite = nullptr; /* child's end, handed away via --cmdOutFd= */
	char* cmdline = nullptr;
	BOOL created = FALSE;

	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = TRUE;
	saAttr.lpSecurityDescriptor = nullptr;

	siStartInfoEx.StartupInfo.cb = sizeof(siStartInfoEx);
	/* the JSON-RPC channel travels over two dedicated pipes handed to the helper via
	 * --cmdInFd=/--cmdOutFd= command line arguments (see winpr_exportHandleToString() below),
	 * not stdin/stdout - so the helper's stdio is left as a plain passthrough of this process'
	 * own, the same way hStdError already was. This keeps the protocol immune to anything the
	 * helper (or a library it links, e.g. Chromium/Qt) happens to print to stdout/stderr for its
	 * own diagnostics, and lets that output reach the user's terminal normally. */
	siStartInfoEx.StartupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
	siStartInfoEx.StartupInfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
	siStartInfoEx.StartupInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
	siStartInfoEx.StartupInfo.dwFlags = STARTF_USESTDHANDLES;

	if (!CreatePipe(&helper->hCmdOutRead, &hCmdOutWrite, &saAttr, 0))
	{
		WLog_ERR(TAG, "aad-auth-helper: cmdOut CreatePipe failed");
		goto cleanup;
	}
	if (!SetHandleInformation(helper->hCmdOutRead, HANDLE_FLAG_INHERIT, 0))
	{
		WLog_ERR(TAG, "aad-auth-helper: cmdOut SetHandleInformation failed");
		goto cleanup;
	}

	if (!CreatePipe(&hCmdInRead, &helper->hCmdInWrite, &saAttr, 0))
	{
		WLog_ERR(TAG, "aad-auth-helper: cmdIn CreatePipe failed");
		goto cleanup;
	}
	if (!SetHandleInformation(helper->hCmdInWrite, HANDLE_FLAG_INHERIT, 0))
	{
		WLog_ERR(TAG, "aad-auth-helper: cmdIn SetHandleInformation failed");
		goto cleanup;
	}

	char cmdInArg[64] = { 0 };
	char cmdOutArg[64] = { 0 };
	if (!winpr_exportHandleToString(hCmdInRead, "--cmdInFd={}", cmdInArg, sizeof(cmdInArg)))
	{
		WLog_ERR(TAG, "aad-auth-helper: failed to export the cmdIn handle");
		goto cleanup;
	}
	if (!winpr_exportHandleToString(hCmdOutWrite, "--cmdOutFd={}", cmdOutArg, sizeof(cmdOutArg)))
	{
		WLog_ERR(TAG, "aad-auth-helper: failed to export the cmdOut handle");
		goto cleanup;
	}

	/* explicit allowlist: only these handles are inherited by the spawned helper, regardless of
	 * anything else in this process that happens to also be marked inheritable (e.g. by another
	 * component linked into the same client). See winpr's CreateProcess /
	 * PROC_THREAD_ATTRIBUTE_HANDLE_LIST support - without this, WinPR's CreateProcess already
	 * defaults to closing everything not wired via STARTUPINFO, so this list exists to make that
	 * contract explicit and portable to real Windows builds of this same file.
	 *
	 * UpdateProcThreadAttribute() only stores a pointer to `handles`, it does not copy it (matches
	 * real Windows - see winpr's own DeleteProcThreadAttributeList() comment) - so `handles` must
	 * stay alive until the CreateProcessA() call below returns. Deliberately kept in the same
	 * block as that call rather than a narrower nested scope: a variable can be read as
	 * use-after-scope by AddressSanitizer once its enclosing block ends even while its storage is
	 * technically still on the stack, and even though CreateProcessA() only reads it through this
	 * still-live block, an earlier version of this function that closed `handles`' scope before
	 * calling CreateProcessA() (relying only on `attrList`/`siStartInfoEx` still being valid)
	 * tripped exactly that. */
	HANDLE handles[5] = { siStartInfoEx.StartupInfo.hStdOutput, siStartInfoEx.StartupInfo.hStdInput,
		                  siStartInfoEx.StartupInfo.hStdError, hCmdInRead, hCmdOutWrite };
	{
		SIZE_T size = 0;

		if (InitializeProcThreadAttributeList(nullptr, 1, 0, &size) || (size == 0))
		{
			WLog_ERR(TAG, "aad-auth-helper: unexpected attribute list sizing result");
			goto cleanup;
		}

		attrList = (LPPROC_THREAD_ATTRIBUTE_LIST)malloc(size);
		if (!attrList || !InitializeProcThreadAttributeList(attrList, 1, 0, &size))
		{
			WLog_ERR(TAG, "aad-auth-helper: InitializeProcThreadAttributeList failed");
			goto cleanup;
		}

		if (!UpdateProcThreadAttribute(attrList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
		                               (PVOID)handles, sizeof(handles), nullptr, nullptr))
		{
			WLog_ERR(TAG, "aad-auth-helper: UpdateProcThreadAttribute failed");
			goto cleanup;
		}

		siStartInfoEx.lpAttributeList = attrList;
	}

	{
		size_t cmdlineLen = 0;
		if (winpr_asprintf(&cmdline, &cmdlineLen, "\"%s\" %s %s", helper_path, cmdInArg,
		                   cmdOutArg) < 0)
			goto cleanup;

		created =
		    CreateProcessA(nullptr, cmdline, nullptr, nullptr, TRUE, EXTENDED_STARTUPINFO_PRESENT,
		                   nullptr, nullptr, (LPSTARTUPINFOA)&siStartInfoEx, &procInfo);
	}

	if (!created)
		WLog_ERR(TAG, "aad-auth-helper: failed to spawn '%s'", helper_path);

cleanup:
	free(cmdline);
	if (attrList)
	{
		DeleteProcThreadAttributeList(attrList);
		free(attrList);
	}
	(void)CloseHandle(procInfo.hThread);
	if (hCmdInRead)
		(void)CloseHandle(hCmdInRead);
	if (hCmdOutWrite)
		(void)CloseHandle(hCmdOutWrite);

	if (!created)
	{
		aad_auth_helper_stop(helper);
		return nullptr;
	}

	helper->hProcess = procInfo.hProcess;

	{
		const UINT32 id = ++helper->nextId;
		char* req = build_hello_request(id);
		BOOL ok = req && helper_write_line(helper, req);
		free(req);

		if (ok)
		{
			WINPR_JSON* resp = wait_for_response(helper, id);
			ok = resp && WINPR_JSON_HasObjectItem(resp, "result");
			if (resp)
				WINPR_JSON_Delete(resp);
		}

		if (!ok)
		{
			WLog_ERR(TAG, "aad-auth-helper: hello handshake failed");
			aad_auth_helper_stop(helper);
			return nullptr;
		}
	}

	return helper;
}

AadAuthHelperNavigateStatus aad_auth_helper_navigate(AadAuthHelper* helper, const char* title,
                                                     const char* url, const char* redirect_uri,
                                                     UINT32 timeout_ms, char** redirect_url)
{
	WINPR_ASSERT(helper);
	WINPR_ASSERT(url);
	WINPR_ASSERT(redirect_uri);
	WINPR_ASSERT(redirect_url);

	const UINT32 id = ++helper->nextId;
	char* req = build_navigate_request(id, title ? title : "", url, redirect_uri, timeout_ms);
	if (!req)
		return AAD_AUTH_HELPER_NAVIGATE_ERROR;

	BOOL ok = helper_write_line(helper, req);
	free(req);
	if (!ok)
		return AAD_AUTH_HELPER_NAVIGATE_ERROR;

	WINPR_JSON* resp = wait_for_response(helper, id);
	if (!resp)
		return AAD_AUTH_HELPER_NAVIGATE_ERROR;

	WINPR_JSON* error = WINPR_JSON_GetObjectItemCaseSensitive(resp, "error");
	if (error)
	{
		WINPR_JSON* message = WINPR_JSON_GetObjectItemCaseSensitive(error, "message");
		const char* msg = (message && WINPR_JSON_IsString(message))
		                      ? WINPR_JSON_GetStringValue(message)
		                      : "unknown error";
		WLog_WARN(TAG, "aad-auth-helper: navigate failed: %s", msg);

		AadAuthHelperNavigateStatus status = AAD_AUTH_HELPER_NAVIGATE_ERROR;
		if (strcmp(msg, "user_cancelled") == 0)
			status = AAD_AUTH_HELPER_NAVIGATE_CANCELLED;
		else if (strcmp(msg, "timeout") == 0)
			status = AAD_AUTH_HELPER_NAVIGATE_TIMEOUT;

		WINPR_JSON_Delete(resp);
		return status;
	}

	WINPR_JSON* result = WINPR_JSON_GetObjectItemCaseSensitive(resp, "result");
	WINPR_JSON* urlItem =
	    result ? WINPR_JSON_GetObjectItemCaseSensitive(result, "redirect_url") : nullptr;
	const char* value =
	    (urlItem && WINPR_JSON_IsString(urlItem)) ? WINPR_JSON_GetStringValue(urlItem) : nullptr;

	if (!value)
	{
		WLog_ERR(TAG, "aad-auth-helper: malformed navigate result");
		WINPR_JSON_Delete(resp);
		return AAD_AUTH_HELPER_NAVIGATE_ERROR;
	}

	*redirect_url = _strdup(value);
	WINPR_JSON_Delete(resp);
	return (*redirect_url != nullptr) ? AAD_AUTH_HELPER_NAVIGATE_OK
	                                  : AAD_AUTH_HELPER_NAVIGATE_ERROR;
}

void aad_auth_helper_stop(AadAuthHelper* helper)
{
	if (!helper)
		return;

	if (helper->hProcess)
	{
		const UINT32 id = ++helper->nextId;
		char* req = build_shutdown_request(id);
		if (req && helper_write_line(helper, req))
		{
			WINPR_JSON* resp = wait_for_response(helper, id);
			if (resp)
				WINPR_JSON_Delete(resp);
		}
		free(req);

		char* notif = build_exit_notification();
		if (notif)
			(void)helper_write_line(helper, notif);
		free(notif);

		if (WaitForSingleObject(helper->hProcess, 3000) != WAIT_OBJECT_0)
		{
			WLog_WARN(TAG, "aad-auth-helper: did not exit in time, terminating");
			(void)TerminateProcess(helper->hProcess, 0);
		}
		(void)CloseHandle(helper->hProcess);
	}

	if (helper->hCmdInWrite)
		(void)CloseHandle(helper->hCmdInWrite);
	if (helper->hCmdOutRead)
		(void)CloseHandle(helper->hCmdOutRead);

	free(helper->buf);
	free(helper);
}
