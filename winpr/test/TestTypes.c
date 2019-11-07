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

BOOL TestSucceededFailedMacros(HRESULT hr, char* sym, BOOL isSuccess)
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

	if (S_OK != (HRESULT)0L)
	{
		printf("Error: S_OK should be 0\n");
		goto err;
	}
	if (S_FALSE != (HRESULT)1L)
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
