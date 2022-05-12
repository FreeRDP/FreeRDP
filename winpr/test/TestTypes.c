/**
 * CTest for winpr types and macros
 *
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 Norbert Federa <norbert.federa@thincast.com>
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

#include <winpr/crt.h>
#include <winpr/error.h>

static BOOL test_co_errors(void)
{
	size_t x;
	const LONG should[] = {
		(LONG)0x80004006l, (LONG)0x80004007l, (LONG)0x80004008l, (LONG)0x80004009l,
		(LONG)0x8000400Al, (LONG)0x8000400Bl, (LONG)0x8000400Cl, (LONG)0x8000400Dl,
		(LONG)0x8000400El, (LONG)0x8000400Fl, (LONG)0x80004010l, (LONG)0x80004011l,
		(LONG)0x80004012l, (LONG)0x80004013l, (LONG)0x80004014l, (LONG)0x80004015l,
		(LONG)0x80004016l, (LONG)0x80004017l, (LONG)0x80004018l, (LONG)0x80004019l,
		(LONG)0x8000401Al, (LONG)0x8000401Bl, (LONG)0x8000401Cl, (LONG)0x8000401Dl,
		(LONG)0x8000401El, (LONG)0x8000401Fl, (LONG)0x80004020l, (LONG)0x80004021l,
		(LONG)0x80004022l, (LONG)0x80004023l, (LONG)0x80004024l, (LONG)0x80004025l,
		(LONG)0x80004026l, (LONG)0x80004027l, (LONG)0x80004028l, (LONG)0x80004029l,
		(LONG)0x8000402Al, (LONG)0x8000402Bl, (LONG)0x80004030l, (LONG)0x80004031l,
		(LONG)0x80004032l, (LONG)0x80004033l, (LONG)0x8000FFFFL, (LONG)0x80070005L,
		(LONG)0x80070006L, (LONG)0x8007000EL, (LONG)0x80070057L, (LONG)0x80004001L,
		(LONG)0x80004002L, (LONG)0x80004003L, (LONG)0x80004004L, (LONG)0x80004005L
	};
	const LONG are[] = { CO_E_INIT_TLS,
		                 CO_E_INIT_SHARED_ALLOCATOR,
		                 CO_E_INIT_MEMORY_ALLOCATOR,
		                 CO_E_INIT_CLASS_CACHE,
		                 CO_E_INIT_RPC_CHANNEL,
		                 CO_E_INIT_TLS_SET_CHANNEL_CONTROL,
		                 CO_E_INIT_TLS_CHANNEL_CONTROL,
		                 CO_E_INIT_UNACCEPTED_USER_ALLOCATOR,
		                 CO_E_INIT_SCM_MUTEX_EXISTS,
		                 CO_E_INIT_SCM_FILE_MAPPING_EXISTS,
		                 CO_E_INIT_SCM_MAP_VIEW_OF_FILE,
		                 CO_E_INIT_SCM_EXEC_FAILURE,
		                 CO_E_INIT_ONLY_SINGLE_THREADED,
		                 CO_E_CANT_REMOTE,
		                 CO_E_BAD_SERVER_NAME,
		                 CO_E_WRONG_SERVER_IDENTITY,
		                 CO_E_OLE1DDE_DISABLED,
		                 CO_E_RUNAS_SYNTAX,
		                 CO_E_CREATEPROCESS_FAILURE,
		                 CO_E_RUNAS_CREATEPROCESS_FAILURE,
		                 CO_E_RUNAS_LOGON_FAILURE,
		                 CO_E_LAUNCH_PERMSSION_DENIED,
		                 CO_E_START_SERVICE_FAILURE,
		                 CO_E_REMOTE_COMMUNICATION_FAILURE,
		                 CO_E_SERVER_START_TIMEOUT,
		                 CO_E_CLSREG_INCONSISTENT,
		                 CO_E_IIDREG_INCONSISTENT,
		                 CO_E_NOT_SUPPORTED,
		                 CO_E_RELOAD_DLL,
		                 CO_E_MSI_ERROR,
		                 CO_E_ATTEMPT_TO_CREATE_OUTSIDE_CLIENT_CONTEXT,
		                 CO_E_SERVER_PAUSED,
		                 CO_E_SERVER_NOT_PAUSED,
		                 CO_E_CLASS_DISABLED,
		                 CO_E_CLRNOTAVAILABLE,
		                 CO_E_ASYNC_WORK_REJECTED,
		                 CO_E_SERVER_INIT_TIMEOUT,
		                 CO_E_NO_SECCTX_IN_ACTIVATE,
		                 CO_E_TRACKER_CONFIG,
		                 CO_E_THREADPOOL_CONFIG,
		                 CO_E_SXS_CONFIG,
		                 CO_E_MALFORMED_SPN,
		                 E_UNEXPECTED,
		                 E_ACCESSDENIED,
		                 E_HANDLE,
		                 E_OUTOFMEMORY,
		                 E_INVALIDARG,
		                 E_NOTIMPL,
		                 E_NOINTERFACE,
		                 E_POINTER,
		                 E_ABORT,
		                 E_FAIL };

	if (ARRAYSIZE(should) != ARRAYSIZE(are))
	{
		const size_t a = ARRAYSIZE(should);
		const size_t b = ARRAYSIZE(are);
		printf("mismatch: %" PRIuz " vs %" PRIuz "\n", a, b);
		return FALSE;
	}
	for (x = 0; x < ARRAYSIZE(are); x++)
	{
		const LONG a = are[x];
		const LONG b = should[x];
		if (a != b)
		{
			printf("mismatch[%" PRIuz "]: %0x" PRIx32 " vs %08" PRIx32 "\n", x, a, b);
			return FALSE;
		}
	}
	return TRUE;
}

static BOOL TestSucceededFailedMacros(HRESULT hr, char* sym, BOOL isSuccess)
{
	BOOL rv = TRUE;

	if (SUCCEEDED(hr) && !isSuccess)
	{
		printf("Error: SUCCEEDED with \"%s\" must be false\n", sym);
		rv = FALSE;
	}
	if (!SUCCEEDED(hr) && isSuccess)
	{
		printf("Error: SUCCEEDED with \"%s\" must be true\n", sym);
		rv = FALSE;
	}
	if (!FAILED(hr) && !isSuccess)
	{
		printf("Error: FAILED with \"%s\" must be true\n", sym);
		rv = FALSE;
	}
	if (FAILED(hr) && isSuccess)
	{
		printf("Error: FAILED with \"%s\" must be false\n", sym);
		rv = FALSE;
	}

	return rv;
}

int TestTypes(int argc, char* argv[])
{
	BOOL ok = TRUE;
	HRESULT hr;

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	if (!test_co_errors())
		goto err;

	if (S_OK != 0L)
	{
		printf("Error: S_OK should be 0\n");
		goto err;
	}
	if (S_FALSE != 1L)
	{
		printf("Error: S_FALSE should be 1\n");
		goto err;
	}

	/* Test HRESULT success codes */
	ok &= TestSucceededFailedMacros(S_OK, "S_OK", TRUE);
	ok &= TestSucceededFailedMacros(S_FALSE, "S_FALSE", TRUE);

	/* Test some HRESULT error codes */
	ok &= TestSucceededFailedMacros(E_NOTIMPL, "E_NOTIMPL", FALSE);
	ok &= TestSucceededFailedMacros(E_OUTOFMEMORY, "E_OUTOFMEMORY", FALSE);
	ok &= TestSucceededFailedMacros(E_INVALIDARG, "E_INVALIDARG", FALSE);
	ok &= TestSucceededFailedMacros(E_FAIL, "E_FAIL", FALSE);
	ok &= TestSucceededFailedMacros(E_ABORT, "E_ABORT", FALSE);

	/* Test some WIN32 error codes converted to HRESULT*/
	hr = HRESULT_FROM_WIN32(ERROR_SUCCESS);
	ok &= TestSucceededFailedMacros(hr, "HRESULT_FROM_WIN32(ERROR_SUCCESS)", TRUE);

	hr = HRESULT_FROM_WIN32(ERROR_INVALID_FUNCTION);
	ok &= TestSucceededFailedMacros(hr, "HRESULT_FROM_WIN32(ERROR_INVALID_FUNCTION)", FALSE);

	hr = HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
	ok &= TestSucceededFailedMacros(hr, "HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED)", FALSE);

	hr = HRESULT_FROM_WIN32(ERROR_NOACCESS);
	ok &= TestSucceededFailedMacros(hr, "HRESULT_FROM_WIN32(ERROR_NOACCESS)", FALSE);

	hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
	ok &= TestSucceededFailedMacros(hr, "HRESULT_FROM_WIN32(ERROR_NOT_FOUND)", FALSE);

	hr = HRESULT_FROM_WIN32(ERROR_TIMEOUT);
	ok &= TestSucceededFailedMacros(hr, "HRESULT_FROM_WIN32(ERROR_TIMEOUT)", FALSE);

	hr = HRESULT_FROM_WIN32(RPC_S_ZERO_DIVIDE);
	ok &= TestSucceededFailedMacros(hr, "HRESULT_FROM_WIN32(RPC_S_ZERO_DIVIDE)", FALSE);

	hr = HRESULT_FROM_WIN32(ERROR_STATIC_INIT);
	ok &= TestSucceededFailedMacros(hr, "HRESULT_FROM_WIN32(ERROR_STATIC_INIT)", FALSE);

	hr = HRESULT_FROM_WIN32(ERROR_ENCRYPTION_FAILED);
	ok &= TestSucceededFailedMacros(hr, "HRESULT_FROM_WIN32(ERROR_ENCRYPTION_FAILED)", FALSE);

	hr = HRESULT_FROM_WIN32(WSAECANCELLED);
	ok &= TestSucceededFailedMacros(hr, "HRESULT_FROM_WIN32(WSAECANCELLED)", FALSE);

	if (ok)
	{
		printf("Test completed successfully\n");
		return 0;
	}

err:
	printf("Error: Test failed\n");
	return -1;
}
