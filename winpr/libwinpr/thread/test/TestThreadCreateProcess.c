
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/tchar.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/environment.h>
#include <winpr/pipe.h>

#define TESTENV_A "HELLO=WORLD"
#define TESTENV_T _T(TESTENV_A)

int TestThreadCreateProcess(int argc, char* argv[])
{
	BOOL status;
	DWORD exitCode;
	LPCTSTR lpApplicationName;
	LPTSTR lpCommandLine;
	LPSECURITY_ATTRIBUTES lpProcessAttributes;
	LPSECURITY_ATTRIBUTES lpThreadAttributes;
	BOOL bInheritHandles;
	DWORD dwCreationFlags;
	LPVOID lpEnvironment;
	LPCTSTR lpCurrentDirectory;
	STARTUPINFO StartupInfo;
	PROCESS_INFORMATION ProcessInformation;
	LPTCH lpszEnvironmentBlock;
	HANDLE pipe_read = NULL;
	HANDLE pipe_write = NULL;
	char buf[1024];
	DWORD read_bytes;
	int ret = 0;
	SECURITY_ATTRIBUTES saAttr;

	lpszEnvironmentBlock = GetEnvironmentStrings();

	lpApplicationName = NULL;

#ifdef _WIN32
	lpCommandLine = _T("cmd /C set");
#else
	lpCommandLine = _T("printenv");
#endif

	lpProcessAttributes = NULL;
	lpThreadAttributes = NULL;
	bInheritHandles = FALSE;
	dwCreationFlags = 0;
#ifdef _UNICODE
	dwCreationFlags |= CREATE_UNICODE_ENVIRONMENT;
#endif
	lpEnvironment = lpszEnvironmentBlock;
	lpCurrentDirectory = NULL;
	ZeroMemory(&StartupInfo, sizeof(STARTUPINFO));
	StartupInfo.cb = sizeof(STARTUPINFO);
	ZeroMemory(&ProcessInformation, sizeof(PROCESS_INFORMATION));

	status = CreateProcess(lpApplicationName,
			lpCommandLine,
			lpProcessAttributes,
			lpThreadAttributes,
			bInheritHandles,
			dwCreationFlags,
			lpEnvironment,
			lpCurrentDirectory,
			&StartupInfo,
			&ProcessInformation);

	if (!status)
	{
		printf("CreateProcess failed. error=%d\n", GetLastError());
		return 1;
	}

	if (WaitForSingleObject(ProcessInformation.hProcess, 5000) != WAIT_OBJECT_0)
	{
		printf("Failed to wait for first process. error=%d\n", GetLastError());
		return 1;
	}

	exitCode = 0;
	status = GetExitCodeProcess(ProcessInformation.hProcess, &exitCode);

	printf("GetExitCodeProcess status: %d\n", status);
	printf("Process exited with code: 0x%08X\n", exitCode);

	CloseHandle(ProcessInformation.hProcess);
	CloseHandle(ProcessInformation.hThread);
	FreeEnvironmentStrings(lpszEnvironmentBlock);


	/* Test stdin,stdout,stderr redirection */

	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = TRUE;
	saAttr.lpSecurityDescriptor = NULL;

	if (!CreatePipe(&pipe_read, &pipe_write, &saAttr, 0))
	{
		printf("Pipe creation failed. error=%d\n", GetLastError());
		return 1;
	}

	bInheritHandles = TRUE;

	ZeroMemory(&StartupInfo, sizeof(STARTUPINFO));
	StartupInfo.cb = sizeof(STARTUPINFO);
	StartupInfo.hStdOutput = pipe_write;
	StartupInfo.hStdError = pipe_write;
	StartupInfo.dwFlags = STARTF_USESTDHANDLES;

	ZeroMemory(&ProcessInformation, sizeof(PROCESS_INFORMATION));

	if (!(lpEnvironment = calloc(1, sizeof(TESTENV_T) + sizeof(TCHAR))))
	{
		printf("Failed to allocate environment buffer. error=%d\n", GetLastError());
		return 1;
	}
	memcpy(lpEnvironment, (void*)TESTENV_T, sizeof(TESTENV_T));


	status = CreateProcess(lpApplicationName,
						   lpCommandLine,
						   lpProcessAttributes,
						   lpThreadAttributes,
						   bInheritHandles,
						   dwCreationFlags,
						   lpEnvironment,
						   lpCurrentDirectory,
						   &StartupInfo,
						   &ProcessInformation);

	free(lpEnvironment);

	if (!status)
	{
		CloseHandle(pipe_read);
		CloseHandle(pipe_write);
		printf("CreateProcess failed. error=%d\n", GetLastError());
		return 1;
	}

	if (WaitForSingleObject(ProcessInformation.hProcess, 5000) != WAIT_OBJECT_0)
	{
		printf("Failed to wait for second process. error=%d\n", GetLastError());
		return 1;
	}

	ZeroMemory(buf, sizeof(buf));
	ReadFile(pipe_read, buf, sizeof(buf)-1, &read_bytes, NULL);
	if (!strstr((const char*)buf, TESTENV_A))
	{
		printf("No or unexpected data read from pipe\n");
		ret = 1;
	}

	CloseHandle(pipe_read);
	CloseHandle(pipe_write);

	exitCode = 0;
	status = GetExitCodeProcess(ProcessInformation.hProcess, &exitCode);

	printf("GetExitCodeProcess status: %d\n", status);
	printf("Process exited with code: 0x%08X\n", exitCode);

	CloseHandle(ProcessInformation.hProcess);
	CloseHandle(ProcessInformation.hThread);

	return ret;
}

