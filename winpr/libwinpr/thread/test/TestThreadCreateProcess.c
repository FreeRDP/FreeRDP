
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/tchar.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/environment.h>
#include <winpr/pipe.h>

#define TESTENV "TEST_PROCESS=oyeah"

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
	char buf[255];
	DWORD read_bytes;
	int ret = 0;

	lpszEnvironmentBlock = GetEnvironmentStrings();

	lpApplicationName = NULL;
	//lpCommandLine = _T("ls -l /");

#ifdef _WIN32
	lpCommandLine = _T("env");
#else
	lpCommandLine = _T("printenv");
#endif

	lpProcessAttributes = NULL;
	lpThreadAttributes = NULL;
	bInheritHandles = FALSE;
	dwCreationFlags = 0;
	lpEnvironment = NULL;
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


	WaitForSingleObject(ProcessInformation.hProcess, INFINITE);

	exitCode = 0;
	status = GetExitCodeProcess(ProcessInformation.hProcess, &exitCode);

	printf("GetExitCodeProcess status: %d\n", status);
	printf("Process exited with code: 0x%08X\n", exitCode);

	CloseHandle(ProcessInformation.hProcess);
	CloseHandle(ProcessInformation.hThread);
	FreeEnvironmentStrings(lpszEnvironmentBlock);

	/* Test stdin,stdout,stderr redirection */
	ZeroMemory(&StartupInfo, sizeof(STARTUPINFO));
	StartupInfo.cb = sizeof(STARTUPINFO);
	ZeroMemory(&ProcessInformation, sizeof(PROCESS_INFORMATION));

	if (!CreatePipe(&pipe_read, &pipe_write, NULL, 0))
	{
		printf("Pipe creation failed. error=%d\n", GetLastError());
		return 1;
	}
	StartupInfo.hStdOutput = pipe_write;
	StartupInfo.hStdError = pipe_write;

	lpEnvironment = calloc(1, sizeof(TESTENV) + 1);
	strncpy(lpEnvironment, TESTENV, strlen(TESTENV));
	lpCommandLine = _T("printenv");

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

	if (WaitForSingleObject(pipe_read, 200) != WAIT_OBJECT_0)
	{
		printf("pipe wait failed.\n");
		ret = 1;
	}
	else
	{
		ReadFile(pipe_read, buf, 255, &read_bytes, NULL);
		if (read_bytes < strlen(TESTENV))
		{
			printf("pipe read problem?!\n");
			ret = 1;
		}
	}

	WaitForSingleObject(ProcessInformation.hProcess, INFINITE);

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

