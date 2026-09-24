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
#include <stdarg.h>

#include <winpr/assert.h>
#include <winpr/file.h>
#include <winpr/handle.h>
#include <winpr/json.h>
#include <winpr/pipe.h>
#include <winpr/string.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/library.h>
#include <winpr/path.h>

#include <freerdp/build-config.h>
#include <freerdp/utils/helpers.h>
#include <freerdp/log.h>
#include <freerdp/client/aad_helper.h>

#define TAG CLIENT_TAG("common.aadauth")

struct AadAuthHelper
{
	rdpClientContext* context;
	HANDLE hCmdOutRead; /* parent's read end: helper -> FreeRDP responses/notifications */
	HANDLE hCmdInWrite; /* parent's write end: FreeRDP -> helper requests */
	HANDLE hProcess;
	UINT32 nextId;

	BYTE* buf;
	size_t bufLen;
};

WINPR_ATTR_MALLOC(free, 1)
static char* aad_auth_helper_detect_helper(void);

/* ---- wire format helpers ------------------------------------------------------------- */
WINPR_ATTR_MALLOC(free, 1)
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

WINPR_ATTR_MALLOC(free, 1)
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

WINPR_ATTR_MALLOC(free, 1)
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

WINPR_ATTR_MALLOC(free, 1)
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
WINPR_ATTR_NODISCARD
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
WINPR_ATTR_MALLOC(free, 1)
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

WINPR_ATTR_MALLOC(free, 1)
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
WINPR_ATTR_MALLOC(WINPR_JSON_Delete, 1)
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

static void updateBoolFromConfig(WINPR_JSON* obj, const char* what, BOOL* pVal)
{
	WINPR_ASSERT(obj);
	WINPR_ASSERT(what);
	WINPR_ASSERT(pVal);
	WINPR_JSON* val = WINPR_JSON_GetObjectItemCaseSensitive(obj, what);
	if (!val)
		return;
	if (!WINPR_JSON_IsBool(val))
		return;
	*pVal = WINPR_JSON_IsTrue(val);
}

static void updateStringFromConfig(WINPR_JSON* obj, const char* what, char** pVal)
{
	WINPR_ASSERT(obj);
	WINPR_ASSERT(what);
	WINPR_ASSERT(pVal);
	WINPR_JSON* val = WINPR_JSON_GetObjectItemCaseSensitive(obj, what);
	if (!val)
		return;
	if (!WINPR_JSON_IsString(val))
		return;
	free(*pVal);
	*pVal = _strdup(WINPR_JSON_GetStringValue(val));
}

WINPR_ATTR_MALLOC(free, 1)
static char* getHelperBinary(const rdpClientContext* context)
{
	/* TODO: Detection of helper binary:
	 *
	 * 1. TODO system wide config file? (config value/deny user level/deny command line/deny
	 * auto-detect)
	 * 2. TODO user level config file? (config value/deny command line/deny auto-detect)
	 * 3. command line parameter
	 * 4. auto detection (default)
	 */
	char* exe = nullptr;

	BOOL useArg = TRUE;
	BOOL useDetect = TRUE;
	BOOL useUserConfig = TRUE;

	const char config[] = "freerdp-client-aad.json";
	WINPR_JSON* sys = freerdp_GetJSONConfigFile(TRUE, config);
	if (sys)
	{
		updateBoolFromConfig(sys, "allow-commandline", &useArg);
		updateBoolFromConfig(sys, "allow-autodetect", &useDetect);
		updateBoolFromConfig(sys, "allow-user-config", &useUserConfig);
		updateStringFromConfig(sys, "helper-binary", &exe);
		WINPR_JSON_Delete(sys);
	}
	if (useUserConfig)
	{
		WINPR_JSON* user = freerdp_GetJSONConfigFile(FALSE, config);
		if (user)
		{
			updateBoolFromConfig(sys, "allow-commandline", &useArg);
			updateBoolFromConfig(sys, "allow-autodetect", &useDetect);
			updateStringFromConfig(sys, "helper-binary", &exe);
		}
		WINPR_JSON_Delete(user);
	}

	if (!exe && useArg)
	{
		const char* args =
		    freerdp_settings_get_string(context->context.settings, FreeRDP_AadAuthHelper);
		if (args && (strcmp("autodetect", args) == 0))
			exe = aad_auth_helper_detect_helper();
		else if (args)
			exe = _strdup(args);
	}

	if (!exe && useDetect)
		exe = aad_auth_helper_detect_helper();

	if (!exe)
	{
		WLog_ERR(TAG, "aad-auth-helper: no helper application detected, aborting");
		return nullptr;
	}
	return exe;
}

/* ---- public API ------------------------------------------------------------------------ */

AadAuthHelper* aad_auth_helper_start(rdpClientContext* context)
{
	WINPR_ASSERT(context);

	char* exe = nullptr;
	AadAuthHelper* helper = calloc(1, sizeof(AadAuthHelper));
	if (!helper)
		return nullptr;
	helper->context = context;

	PROCESS_INFORMATION procInfo = WINPR_C_ARRAY_INIT;
	LPPROC_THREAD_ATTRIBUTE_LIST attrList = nullptr;
	HANDLE hCmdInRead = nullptr;   /* child's end, handed away via --cmdInFd= */
	HANDLE hCmdOutWrite = nullptr; /* child's end, handed away via --cmdOutFd= */
	char* cmdline = nullptr;
	BOOL created = FALSE;

	SECURITY_ATTRIBUTES saAttr = { .nLength = sizeof(SECURITY_ATTRIBUTES),
		                           .bInheritHandle = TRUE,
		                           .lpSecurityDescriptor = nullptr };

	STARTUPINFOEXA siStartInfoEx = {
		.StartupInfo.cb = sizeof(siStartInfoEx),
		/* the JSON-RPC channel travels over two dedicated pipes handed to the helper via
		 * --cmdInFd=/--cmdOutFd= command line arguments (see winpr_exportHandleToString() below),
		 * not stdin/stdout - so the helper's stdio is left as a plain passthrough of this process'
		 * own, the same way hStdError already was. This keeps the protocol immune to anything the
		 * helper (or a library it links, e.g. Chromium/Qt) happens to print to stdout/stderr for
		 * its own diagnostics, and lets that output reach the user's terminal normally. */
		.StartupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE),
		.StartupInfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE),
		.StartupInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE),
		.StartupInfo.dwFlags = STARTF_USESTDHANDLES
	};

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

	char cmdInArg[64] = WINPR_C_ARRAY_INIT;
	char cmdOutArg[64] = WINPR_C_ARRAY_INIT;
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
		exe = getHelperBinary(context);
		if (!exe)
			goto cleanup;
		const int rc =
		    winpr_asprintf(&cmdline, &cmdlineLen, "\"%s\" %s %s", exe, cmdInArg, cmdOutArg);
		if (rc < 0)
			goto cleanup;

		created =
		    CreateProcessA(nullptr, cmdline, nullptr, nullptr, TRUE, EXTENDED_STARTUPINFO_PRESENT,
		                   nullptr, nullptr, (LPSTARTUPINFOA)&siStartInfoEx, &procInfo);
	}

	if (!created)
		WLog_ERR(TAG, "aad-auth-helper: failed to spawn '%s'", exe);

cleanup:
	free(exe);
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

static AadAuthHelperNavigateStatus aad_auth_helper_navigate(AadAuthHelper* helper,
                                                            const char* title, const char* url,
                                                            const char* redirect_uri,
                                                            UINT32 timeout_ms, char** redirect_url,
                                                            size_t* redirect_url_len)
{
	WINPR_ASSERT(helper);
	WINPR_ASSERT(url);
	WINPR_ASSERT(redirect_uri);
	WINPR_ASSERT(redirect_url);
	WINPR_ASSERT(redirect_url_len);

	*redirect_url = nullptr;
	*redirect_url_len = 0;

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
	if (*redirect_url)
		*redirect_url_len = strlen(*redirect_url);
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

WINPR_ATTR_MALLOC(winpr_zfree, 1)
static char* aad_auth_helper_extract_query_param(const char* url, const char* name)
{
	if (!url || !name)
		return nullptr;

	const char* start = strchr(url, '?');
	if (!start)
		return nullptr;

	const char* param = strstr(start, name);
	if (!param)
		return nullptr;

	const size_t len = strlen(name);
	if (param[len] != '=')
		return nullptr;

	char* str = _strdup(&param[len + 1]);
	if (!str)
		return nullptr;

	char* end = strchr(str, '&');
	if (end)
		*end = '\0';
	const size_t slen = strlen(str);
	char* decoded = winpr_str_url_decode(str, slen);
	winpr_zfree(str);
	return decoded;
}

/** drives the out-of-process helper to show \b url and waits for the OAuth2 redirect. On
 * AAD_AUTH_HELPER_NAVIGATE_ERROR (helper unreachable/unusable), callers fall back to the
 * terminal copy/paste flow rather than hard-failing the connection - but NOT on
 * AAD_AUTH_HELPER_NAVIGATE_CANCELLED (the user closed the popup) or _TIMEOUT (the user didn't
 * complete the flow in time): falling back in either of those cases would silently override an
 * outcome the user already determined, by prompting them to do it all over again via the
 * terminal instead of respecting that the attempt is over. */
WINPR_ATTR_NODISCARD
static AadAuthHelperNavigateStatus aad_helper_navigate(AadAuthHelper* helper, const char* title,
                                                       const char* url, char** pRedirectUrl,
                                                       size_t* pRedirectUrlLen)
{
	WINPR_ASSERT(helper);
	WINPR_ASSERT(title);
	WINPR_ASSERT(url);
	WINPR_ASSERT(pRedirectUrl);
	WINPR_ASSERT(pRedirectUrlLen);

	*pRedirectUrl = nullptr;
	*pRedirectUrlLen = 0;

	char* redirectUri = aad_auth_helper_extract_query_param(url, "redirect_uri");
	if (!redirectUri)
	{
		WLog_ERR(TAG, "[aad-auth] url %s has no redirect_uri parameter", url);
		return AAD_AUTH_HELPER_NAVIGATE_ERROR;
	}

	char* out = nullptr;
	size_t outLen = 0;
	const AadAuthHelperNavigateStatus status =
	    aad_auth_helper_navigate(helper, title, url, redirectUri, 180000, &out, &outLen);
	winpr_zfree(redirectUri);
	if (status != AAD_AUTH_HELPER_NAVIGATE_OK)
	{
		free(out);
		return status;
	}

	*pRedirectUrl = out;
	*pRedirectUrlLen = outLen;
	return AAD_AUTH_HELPER_NAVIGATE_OK;
}

WINPR_ATTR_NODISCARD
static BOOL aad_auth_helper_get_rdsaad_access_token(AadAuthHelper* helper,
                                                    freerdp_client_aad_type requestType,
                                                    freerdp_client_aad_type tokenType,
                                                    const char* scope, const char* req_cnf,
                                                    char** token)
{
	WINPR_ASSERT(helper);
	WINPR_ASSERT(scope);
	WINPR_ASSERT(req_cnf);
	WINPR_ASSERT(token);

	rdpClientContext* cctx = helper->context;
	WINPR_ASSERT(cctx);

	const char* title = "FreeRDP WebView - AAD access token";
	if (requestType == FREERDP_CLIENT_AAD_AVD_AUTH_REQUEST)
		title = "FreeRDP WebView - AVD access token";

	char* request = freerdp_client_get_aad_url(cctx, requestType, scope);
	if (!request)
	{
		WLog_ERR(TAG, "[aad-auth] authentication failed, could not construct request");
		return FALSE;
	}

	char* redirectUrl = nullptr;
	size_t redirectUrlLen = 0;
	const AadAuthHelperNavigateStatus status =
	    aad_helper_navigate(helper, title, request, &redirectUrl, &redirectUrlLen);
	winpr_zfree(request);

	if (status == AAD_AUTH_HELPER_NAVIGATE_CANCELLED)
	{
		winpr_znfree(redirectUrl, redirectUrlLen);
		WLog_INFO(TAG, "[aad-auth] user cancelled the authentication");
		return FALSE;
	}
	if (status == AAD_AUTH_HELPER_NAVIGATE_TIMEOUT)
	{
		winpr_znfree(redirectUrl, redirectUrlLen);
		WLog_ERR(TAG, "[aad-auth] authentication timed out");
		return FALSE;
	}
	if (status != AAD_AUTH_HELPER_NAVIGATE_OK)
	{
		winpr_znfree(redirectUrl, redirectUrlLen);
		WLog_ERR(TAG, "[aad-auth] authentication failed");
		return FALSE;
	}

	char* code = freerdp_client_extract_aad_code(cctx, redirectUrl, redirectUrlLen);
	winpr_znfree(redirectUrl, redirectUrlLen);

	if (!code)
	{
		WLog_ERR(TAG, "[aad-auth] authentication failed, could not find code parameter");
		return FALSE;
	}

	char* token_request = nullptr;
	if (tokenType == FREERDP_CLIENT_AAD_TOKEN_REQUEST)
		token_request = freerdp_client_get_aad_url(cctx, tokenType, scope, code, req_cnf);
	else
		token_request = freerdp_client_get_aad_url(cctx, tokenType, code);
	winpr_zfree(code);
	if (!token_request)
	{
		WLog_ERR(TAG, "[aad-auth] authentication failed, could not get token");
		return FALSE;
	}

	const BOOL rc = client_common_get_access_token(cctx->context.instance, token_request, token);
	winpr_zfree(token_request);
	return rc;
}

BOOL aad_auth_helper_get_access_token_v(AadAuthHelper* helper, AccessTokenType tokenType,
                                        char** token, size_t count, va_list args)
{
	WINPR_ASSERT(token);
	switch (tokenType)
	{
		case ACCESS_TOKEN_TYPE_AAD:
		{
			if (count < 2)
			{
				WLog_ERR(TAG,
				         "ACCESS_TOKEN_TYPE_AAD expected 2 additional arguments, but got %" PRIuz
				         ", aborting",
				         count);
				return FALSE;
			}
			else if (count > 2)
				WLog_WARN(TAG,
				          "ACCESS_TOKEN_TYPE_AAD expected 2 additional arguments, but got %" PRIuz
				          ", ignoring",
				          count);
			const char* scope = va_arg(args, const char*);
			const char* req_cnf = va_arg(args, const char*);
			return aad_auth_helper_get_rdsaad_access_token(helper, FREERDP_CLIENT_AAD_AUTH_REQUEST,
			                                               FREERDP_CLIENT_AAD_TOKEN_REQUEST, scope,
			                                               req_cnf, token);
		}
		case ACCESS_TOKEN_TYPE_AVD:
			if (count != 0)
				WLog_WARN(TAG,
				          "ACCESS_TOKEN_TYPE_AVD expected 0 additional arguments, but got %" PRIuz
				          ", ignoring",
				          count);
			return aad_auth_helper_get_rdsaad_access_token(
			    helper, FREERDP_CLIENT_AAD_AVD_AUTH_REQUEST, FREERDP_CLIENT_AAD_AVD_TOKEN_REQUEST,
			    "", "", token);
		default:
			WLog_ERR(TAG, "Unexpected value for AccessTokenType [%" PRIu32 "], aborting",
			         tokenType);
			return FALSE;
	}
}

BOOL aad_auth_helper_get_access_token(AadAuthHelper* helper, AccessTokenType tokenType,
                                      char** token, size_t count, ...)
{
	va_list ap = WINPR_C_ARRAY_INIT;
	va_start(ap, count);
	const BOOL rc = aad_auth_helper_get_access_token_v(helper, tokenType, token, count, ap);
	va_end(ap);
	return rc;
}

/* whether any auto-detectable helper was enabled at build time at all - see
 * WITH_XDG_AAD_AUTH_HELPER / WITH_WEBVIEW_AAD_AUTH_HELPER / WITH_QT_AAD_AUTH_HELPER in
 * client/common/CMakeLists.txt, propagated here as compile definitions by
 * client/SDL/common/CMakeLists.txt. Guards kHelperCandidates below: with none of the three
 * defined there's nothing to list, and a zero-size array isn't valid standard C++. */

/* auto-pick order for /azure:auth-helper:autodetect (or the option omitted entirely): xdg-open
 * first (drives the user's actual default browser, so it inherits whatever SSO session/cookies
 * are already there instead of prompting again), then the embedded webview (lighter, native OS
 * look), then Qt. */
static const char* kHelperCandidates[] = { "freerdp-xdg-aad-helper", "freerdp-qt-aad-helper",
	                                       "freerdp-webview-aad-helper" };

static void helper_binary_dirs_free(char** dirs, size_t count)
{
	if (!dirs)
		return;
	for (size_t x = 0; x < count; x++)
	{
		char* dir = dirs[x];
		free(dir);
	}
	free((void*)dirs);
}

WINPR_ATTR_MALLOC(free, 1)
static char* aad_auth_helper_get_binary_dir(void)
{
	DWORD len = 4096;
	char* path = nullptr;
	do
	{
		char* tmp = realloc(path, len);
		if (!tmp)
		{
			WLog_ERR(TAG, "[aad-auth] GetModuleFileNameA failed");
			free(path);
			return nullptr;
		}
		path = tmp;

		const DWORD rc = GetModuleFileNameA(nullptr, path, len);
		if (rc == 0)
		{
			WLog_ERR(TAG, "[aad-auth] GetModuleFileNameA failed");
			free(path);
			return nullptr;
		}

		if (rc == len)
		{
			if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
			{
				len += 4096;
				continue;
			}
			WLog_ERR(TAG, "[aad-auth] GetModuleFileNameA failed");
			free(path);
			return nullptr;
		}
		else
			break;
	} while (TRUE);

	char* sep = strrchr(path, '/');
#ifdef _WIN32
	char* sepWin = strrchr(path, '\\');
	if (!sep || (sepWin && (sepWin > sep)))
		sep = sepWin;
#endif
	if (!sep)
	{
		free(path);
		return nullptr;
	}
	*sep = '\0';
	return path;
}

/* directory this client binary itself lives in - where an installed (or freshly built) helper
 * binary is expected to sit alongside it. */
WINPR_ATTR_MALLOC(helper_binary_dirs_free, 1)
static char** aad_auth_helper_binary_dirs(size_t* count)
{
	WINPR_ASSERT(count);

	*count = 0;
	char** dirs = (char**)calloc(32, sizeof(char*));
	if (!dirs)
		return nullptr;
	{
		char* libexec = GetCombinedPath(FREERDP_INSTALL_PREFIX, FREERDP_LIBEXEC_PATH);
		if (libexec)
			dirs[(*count)++] = libexec;
	}

	char* app = aad_auth_helper_get_binary_dir();
	if (app)
	{
		dirs[(*count)++] = app;

		char* libexec = GetCombinedPath(app, FREERDP_LIBEXEC_REL_PATH);
		if (libexec)
			dirs[(*count)++] = libexec;
	}
	return dirs;
}

WINPR_ATTR_MALLOC(free, 1)
static char* aad_auth_helper_path_for_binary(const char* dir, const char* binaryName)
{
	const char extension[] = CMAKE_EXECUTABLE_SUFFIX;

	char* path = nullptr;
	size_t plen = 0;
	winpr_asprintf(&path, &plen, "%s/%s%s", dir, binaryName, extension);
	return path;
}

/* /azure:auth-helper:autodetect (or the option omitted entirely): probe the well-known binaries
 * in kHelperCandidates order and use whichever is actually present. */
WINPR_ATTR_MALLOC(free, 1)
static char* aad_auth_helper_auto_locate(void)
{
	size_t dirscount = 0;
	char** dirs = aad_auth_helper_binary_dirs(&dirscount);
	if (!dirs)
		return nullptr;

	char* path = nullptr;
	for (size_t i = 0; i < dirscount; i++)
	{
		char* dir = dirs[i];
		if (!dir)
			continue;
		for (size_t x = 0; x < ARRAYSIZE(kHelperCandidates); x++)
		{
			const char* binaryName = kHelperCandidates[x];
			char* cpath = aad_auth_helper_path_for_binary(dir, binaryName);
			if (winpr_PathFileExists(cpath))
			{
				path = cpath;
				break;
			}
			free(cpath);
		}
		if (path)
			break;
	}

	helper_binary_dirs_free(dirs, dirscount);
	return path;
}

/* @p helper is the caller's own per-connection storage slot (e.g. a member of its SdlContext) -
 * this file never stores anything itself, so it stays usable as one binary shared between the
 * SDL2 and SDL3 clients regardless of their (different) concrete SdlContext type. */
char* aad_auth_helper_detect_helper(void)
{
	char* path = aad_auth_helper_auto_locate();

	if (!path)
	{
		WLog_ERR(TAG, "[aad-auth] could not determine expected helper binary location");
		return nullptr;
	}

	if (!winpr_PathFileExists(path))
	{
		WLog_ERR(TAG, "[aad-auth] helper binary not found at '%s'", path);
		free(path);
		return nullptr;
	}

	WLog_DBG(TAG, "[aad-auth] auto-detected helper %s", path);
	return path;
}
