
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/tchar.h>
#include <winpr/synch.h>
#include <winpr/thread.h>

int TestThreadCreateProcess(int argc, char* argv[])
{
	BOOL status;
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

	lpApplicationName = NULL;
	lpCommandLine = _T("ls -l /");
	lpProcessAttributes = NULL;
	lpThreadAttributes = NULL;
	bInheritHandles = FALSE;
	dwCreationFlags = 0;
	lpEnvironment = NULL;
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

	WaitForSingleObject(ProcessInformation.hProcess, INFINITE);

	CloseHandle(ProcessInformation.hProcess);
	CloseHandle(ProcessInformation.hThread);

	return 0;
}

