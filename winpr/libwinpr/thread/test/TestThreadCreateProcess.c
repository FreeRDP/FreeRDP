
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/tchar.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/environment.h>

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

	FreeEnvironmentStrings(lpszEnvironmentBlock);

	WaitForSingleObject(ProcessInformation.hProcess, INFINITE);

	exitCode = 0;
	status = GetExitCodeProcess(ProcessInformation.hProcess, &exitCode);

	printf("GetExitCodeProcess status: %d\n", status);
	printf("Process exited with code: 0x%08X\n", exitCode);

	CloseHandle(ProcessInformation.hProcess);
	CloseHandle(ProcessInformation.hThread);

	return 0;
}

