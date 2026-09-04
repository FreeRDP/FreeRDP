
#include <stdio.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/windows.h>
#include <winpr/json.h>

/* Round-trips each message shape of the AAD auth helper's JSON-RPC protocol (see
 * client/common/aad_helper.c and client/common/webview-aad-helper/main.cpp) through
 * WINPR_JSON's serializer/parser, to catch protocol drift between the two implementations and
 * the spec they're both meant to follow. */

static WINPR_JSON* roundtrip(WINPR_JSON* obj)
{
	char* str = WINPR_JSON_PrintUnformatted(obj);
	WINPR_JSON_Delete(obj);
	if (!str)
		return NULL;

	WINPR_JSON* parsed = WINPR_JSON_Parse(str);
	free(str);
	return parsed;
}

static BOOL check_string(WINPR_JSON* obj, const char* name, const char* expected)
{
	WINPR_JSON* item = WINPR_JSON_GetObjectItemCaseSensitive(obj, name);
	if (!item || !WINPR_JSON_IsString(item))
	{
		printf("field '%s' missing or not a string\n", name);
		return FALSE;
	}
	const char* value = WINPR_JSON_GetStringValue(item);
	if (!value || strcmp(value, expected) != 0)
	{
		printf("field '%s': expected '%s', got '%s'\n", name, expected, value ? value : "(null)");
		return FALSE;
	}
	return TRUE;
}

static BOOL check_number(WINPR_JSON* obj, const char* name, double expected)
{
	WINPR_JSON* item = WINPR_JSON_GetObjectItemCaseSensitive(obj, name);
	if (!item || !WINPR_JSON_IsNumber(item))
	{
		printf("field '%s' missing or not a number\n", name);
		return FALSE;
	}
	if (WINPR_JSON_GetNumberValue(item) != expected)
	{
		printf("field '%s': expected %f, got %f\n", name, expected,
		       WINPR_JSON_GetNumberValue(item));
		return FALSE;
	}
	return TRUE;
}

static BOOL test_hello_request(void)
{
	WINPR_JSON* obj = WINPR_JSON_CreateObject();
	(void)WINPR_JSON_AddStringToObject(obj, "jsonrpc", "2.0");
	(void)WINPR_JSON_AddIntegerToObject(obj, "id", 1);
	(void)WINPR_JSON_AddStringToObject(obj, "method", "hello");
	WINPR_JSON* params = WINPR_JSON_AddObjectToObject(obj, "params");
	(void)WINPR_JSON_AddIntegerToObject(params, "protocol_version", 1);
	(void)WINPR_JSON_AddStringToObject(params, "client", "freerdp-test");

	WINPR_JSON* msg = roundtrip(obj);
	if (!msg)
		return FALSE;

	BOOL ok = check_string(msg, "jsonrpc", "2.0") && check_number(msg, "id", 1) &&
	          check_string(msg, "method", "hello");
	if (ok)
	{
		WINPR_JSON* p = WINPR_JSON_GetObjectItemCaseSensitive(msg, "params");
		ok = p && check_number(p, "protocol_version", 1) &&
		     check_string(p, "client", "freerdp-test");
	}

	WINPR_JSON_Delete(msg);
	return ok;
}

static BOOL test_navigate_request(void)
{
	WINPR_JSON* obj = WINPR_JSON_CreateObject();
	(void)WINPR_JSON_AddStringToObject(obj, "jsonrpc", "2.0");
	(void)WINPR_JSON_AddIntegerToObject(obj, "id", 2);
	(void)WINPR_JSON_AddStringToObject(obj, "method", "navigate");
	WINPR_JSON* params = WINPR_JSON_AddObjectToObject(obj, "params");
	(void)WINPR_JSON_AddStringToObject(params, "title", "Sign in");
	(void)WINPR_JSON_AddStringToObject(params, "url",
	                                   "https://login.microsoftonline.com/authorize");
	(void)WINPR_JSON_AddStringToObject(
	    params, "redirect_uri", "https://login.microsoftonline.com/common/oauth2/nativeclient");
	(void)WINPR_JSON_AddIntegerToObject(params, "timeout_ms", 180000);

	WINPR_JSON* msg = roundtrip(obj);
	if (!msg)
		return FALSE;

	BOOL ok = check_string(msg, "method", "navigate") && check_number(msg, "id", 2);
	if (ok)
	{
		WINPR_JSON* p = WINPR_JSON_GetObjectItemCaseSensitive(msg, "params");
		ok = p && check_string(p, "title", "Sign in") &&
		     check_string(p, "url", "https://login.microsoftonline.com/authorize") &&
		     check_string(p, "redirect_uri",
		                  "https://login.microsoftonline.com/common/oauth2/nativeclient") &&
		     check_number(p, "timeout_ms", 180000);
	}

	WINPR_JSON_Delete(msg);
	return ok;
}

static BOOL test_navigate_result_ok(void)
{
	WINPR_JSON* obj = WINPR_JSON_CreateObject();
	(void)WINPR_JSON_AddStringToObject(obj, "jsonrpc", "2.0");
	(void)WINPR_JSON_AddIntegerToObject(obj, "id", 2);
	WINPR_JSON* result = WINPR_JSON_AddObjectToObject(obj, "result");
	(void)WINPR_JSON_AddStringToObject(result, "status", "ok");
	(void)WINPR_JSON_AddStringToObject(
	    result, "redirect_url",
	    "https://login.microsoftonline.com/common/oauth2/nativeclient"
	    "?code=AAAB&session_state=xyz");

	WINPR_JSON* msg = roundtrip(obj);
	if (!msg)
		return FALSE;

	WINPR_JSON* result_item = WINPR_JSON_GetObjectItemCaseSensitive(msg, "result");
	BOOL ok = result_item && check_string(result_item, "status", "ok") &&
	          check_string(result_item, "redirect_url",
	                       "https://login.microsoftonline.com/common/oauth2/nativeclient"
	                       "?code=AAAB&session_state=xyz");

	/* a well-formed success response must not also carry an "error" member */
	ok = ok && !WINPR_JSON_HasObjectItem(msg, "error");

	WINPR_JSON_Delete(msg);
	return ok;
}

static BOOL test_navigate_result_error(void)
{
	WINPR_JSON* obj = WINPR_JSON_CreateObject();
	(void)WINPR_JSON_AddStringToObject(obj, "jsonrpc", "2.0");
	(void)WINPR_JSON_AddIntegerToObject(obj, "id", 2);
	WINPR_JSON* error = WINPR_JSON_AddObjectToObject(obj, "error");
	(void)WINPR_JSON_AddIntegerToObject(error, "code", 1);
	(void)WINPR_JSON_AddStringToObject(error, "message", "user_cancelled");

	WINPR_JSON* msg = roundtrip(obj);
	if (!msg)
		return FALSE;

	WINPR_JSON* error_item = WINPR_JSON_GetObjectItemCaseSensitive(msg, "error");
	BOOL ok = error_item && check_number(error_item, "code", 1) &&
	          check_string(error_item, "message", "user_cancelled");

	ok = ok && !WINPR_JSON_HasObjectItem(msg, "result");

	WINPR_JSON_Delete(msg);
	return ok;
}

static BOOL test_cancel_notification(void)
{
	WINPR_JSON* obj = WINPR_JSON_CreateObject();
	(void)WINPR_JSON_AddStringToObject(obj, "jsonrpc", "2.0");
	(void)WINPR_JSON_AddStringToObject(obj, "method", "$/cancel");
	WINPR_JSON* params = WINPR_JSON_AddObjectToObject(obj, "params");
	(void)WINPR_JSON_AddIntegerToObject(params, "id", 2);

	WINPR_JSON* msg = roundtrip(obj);
	if (!msg)
		return FALSE;

	/* notifications carry no "id" of their own */
	BOOL ok = !WINPR_JSON_HasObjectItem(msg, "id") && check_string(msg, "method", "$/cancel");
	if (ok)
	{
		WINPR_JSON* p = WINPR_JSON_GetObjectItemCaseSensitive(msg, "params");
		ok = p && check_number(p, "id", 2);
	}

	WINPR_JSON_Delete(msg);
	return ok;
}

static BOOL test_log_notification(void)
{
	WINPR_JSON* obj = WINPR_JSON_CreateObject();
	(void)WINPR_JSON_AddStringToObject(obj, "jsonrpc", "2.0");
	(void)WINPR_JSON_AddStringToObject(obj, "method", "log");
	WINPR_JSON* params = WINPR_JSON_AddObjectToObject(obj, "params");
	(void)WINPR_JSON_AddStringToObject(params, "level", "warn");
	(void)WINPR_JSON_AddStringToObject(params, "message", "webkit_web_view_load_failed");

	WINPR_JSON* msg = roundtrip(obj);
	if (!msg)
		return FALSE;

	BOOL ok = !WINPR_JSON_HasObjectItem(msg, "id") && check_string(msg, "method", "log");
	if (ok)
	{
		WINPR_JSON* p = WINPR_JSON_GetObjectItemCaseSensitive(msg, "params");
		ok = p && check_string(p, "level", "warn") &&
		     check_string(p, "message", "webkit_web_view_load_failed");
	}

	WINPR_JSON_Delete(msg);
	return ok;
}

static BOOL test_shutdown_sequence(void)
{
	WINPR_JSON* req = WINPR_JSON_CreateObject();
	(void)WINPR_JSON_AddStringToObject(req, "jsonrpc", "2.0");
	(void)WINPR_JSON_AddIntegerToObject(req, "id", 3);
	(void)WINPR_JSON_AddStringToObject(req, "method", "shutdown");

	WINPR_JSON* reqMsg = roundtrip(req);
	BOOL ok = reqMsg && check_string(reqMsg, "method", "shutdown") && check_number(reqMsg, "id", 3);
	if (reqMsg)
		WINPR_JSON_Delete(reqMsg);
	if (!ok)
		return FALSE;

	WINPR_JSON* resp = WINPR_JSON_CreateObject();
	(void)WINPR_JSON_AddStringToObject(resp, "jsonrpc", "2.0");
	(void)WINPR_JSON_AddIntegerToObject(resp, "id", 3);
	(void)WINPR_JSON_AddNullToObject(resp, "result");

	WINPR_JSON* respMsg = roundtrip(resp);
	if (!respMsg)
		return FALSE;
	/* some backends (json-c) represent a JSON null value as a NULL pointer, indistinguishable
	 * from the key being absent - check presence via HasObjectItem() rather than the pointer */
	WINPR_JSON* result = WINPR_JSON_GetObjectItemCaseSensitive(respMsg, "result");
	ok = WINPR_JSON_HasObjectItem(respMsg, "result") && WINPR_JSON_IsNull(result);
	WINPR_JSON_Delete(respMsg);
	if (!ok)
		return FALSE;

	WINPR_JSON* exit = WINPR_JSON_CreateObject();
	(void)WINPR_JSON_AddStringToObject(exit, "jsonrpc", "2.0");
	(void)WINPR_JSON_AddStringToObject(exit, "method", "exit");

	WINPR_JSON* exitMsg = roundtrip(exit);
	if (!exitMsg)
		return FALSE;
	ok = !WINPR_JSON_HasObjectItem(exitMsg, "id") && check_string(exitMsg, "method", "exit");
	WINPR_JSON_Delete(exitMsg);
	return ok;
}

int TestClientAadAuthHelperProtocol(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	struct
	{
		const char* name;
		BOOL (*fn)(void);
	} tests[] = { { "hello request", test_hello_request },
		          { "navigate request", test_navigate_request },
		          { "navigate result (ok)", test_navigate_result_ok },
		          { "navigate result (error)", test_navigate_result_error },
		          { "$/cancel notification", test_cancel_notification },
		          { "log notification", test_log_notification },
		          { "shutdown/exit sequence", test_shutdown_sequence } };

	int rc = 0;
	for (size_t i = 0; i < ARRAYSIZE(tests); i++)
	{
		if (!tests[i].fn())
		{
			printf("FAILED: %s\n", tests[i].name);
			rc = -1;
		}
	}

	return rc;
}
