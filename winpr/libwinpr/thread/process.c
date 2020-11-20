/**
 * WinPR: Windows Portable Runtime
 * Process Thread Functions
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2014 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/handle.h>
#include "../handle/nonehandle.h"

#include <winpr/thread.h>

/**
 * CreateProcessA
 * CreateProcessW
 * CreateProcessAsUserA
 * CreateProcessAsUserW
 * ExitProcess
 * GetCurrentProcess
 * GetCurrentProcessId
 * GetExitCodeProcess
 * GetProcessHandleCount
 * GetProcessId
 * GetProcessIdOfThread
 * GetProcessMitigationPolicy
 * GetProcessTimes
 * GetProcessVersion
 * OpenProcess
 * OpenProcessToken
 * ProcessIdToSessionId
 * SetProcessAffinityUpdateMode
 * SetProcessMitigationPolicy
 * SetProcessShutdownParameters
 * TerminateProcess
 */

#ifndef _WIN32

#include <winpr/crt.h>
#include <winpr/path.h>
#include <winpr/environment.h>

#include <grp.h>

#include <signal.h>

#include "thread.h"

#include "../security/security.h"

#ifndef NSIG
#ifdef SIGMAX
#define NSIG SIGMAX
#else
#define NSIG 64
#endif
#endif

/**
 * If the file name does not contain a directory path, the system searches for the executable file
 * in the following sequence:
 *
 * 1) The directory from which the application loaded.
 * 2) The current directory for the parent process.
 * 3) The 32-bit Windows system directory. Use the GetSystemDirectory function to get the path of
 * this directory. 4) The 16-bit Windows system directory. There is no function that obtains the
 * path of this directory, but it is searched. The name of this directory is System. 5) The Windows
 * directory. Use the GetWindowsDirectory function to get the path of this directory. 6) The
 * directories that are listed in the PATH environment variable. Note that this function does not
 * search the per-application path specified by the App Paths registry key. To include this
 * per-application path in the search sequence, use the ShellExecute function.
 */

static char* FindApplicationPath(char* application)
{
	LPCSTR pathName = "PATH";
	char* path;
	char* save;
	DWORD nSize;
	LPSTR lpSystemPath;
	char* filename = NULL;

	if (!application)
		return NULL;

	if (application[0] == '/')
		return _strdup(application);

	nSize = GetEnvironmentVariableA(pathName, NULL, 0);

	if (!nSize)
		return _strdup(application);

	lpSystemPath = (LPSTR)malloc(nSize);

	if (!lpSystemPath)
		return NULL;

	if (GetEnvironmentVariableA(pathName, lpSystemPath, nSize) != nSize - 1)
	{
		free(lpSystemPath);
		return NULL;
	}

	save = NULL;
	path = strtok_s(lpSystemPath, ":", &save);

	while (path)
	{
		filename = GetCombinedPath(path, application);

		if (PathFileExistsA(filename))
		{
			break;
		}

		free(filename);
		filename = NULL;
		path = strtok_s(NULL, ":", &save);
	}

	free(lpSystemPath);
	return filename;
}

static HANDLE CreateProcessHandle(pid_t pid);
static BOOL ProcessHandleCloseHandle(HANDLE handle);

static BOOL _CreateProcessExA(HANDLE hToken, DWORD dwLogonFlags, LPCSTR lpApplicationName,
                              LPSTR lpCommandLine, LPSECURITY_ATTRIBUTES lpProcessAttributes,
                              LPSECURITY_ATTRIBUTES lpThreadAttributes, BOOL bInheritHandles,
                              DWORD dwCreationFlags, LPVOID lpEnvironment,
                              LPCSTR lpCurrentDirectory, LPSTARTUPINFOA lpStartupInfo,
                              LPPROCESS_INFORMATION lpProcessInformation)
{
	pid_t pid;
	int numArgs;
	LPSTR* pArgs = NULL;
	char** envp = NULL;
	char* filename = NULL;
	HANDLE thread;
	HANDLE process;
	WINPR_ACCESS_TOKEN* token;
	LPTCH lpszEnvironmentBlock;
	BOOL ret = FALSE;
	sigset_t oldSigMask;
	sigset_t newSigMask;
	BOOL restoreSigMask = FALSE;
	numArgs = 0;
	lpszEnvironmentBlock = NULL;
	/* https://docs.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessa
	 */
	if (lpCommandLine)
		pArgs = CommandLineToArgvA(lpCommandLine, &numArgs);
	else
		pArgs = CommandLineToArgvA(lpApplicationName, &numArgs);

	if (!pArgs)
		return FALSE;

	token = (WINPR_ACCESS_TOKEN*)hToken;

	if (lpEnvironment)
	{
		envp = EnvironmentBlockToEnvpA(lpEnvironment);
	}
	else
	{
		lpszEnvironmentBlock = GetEnvironmentStrings();

		if (!lpszEnvironmentBlock)
			goto finish;

		envp = EnvironmentBlockToEnvpA(lpszEnvironmentBlock);
	}

	if (!envp)
		goto finish;

	filename = FindApplicationPath(pArgs[0]);

	if (NULL == filename)
		goto finish;

	/* block all signals so that the child can safely reset the caller's handlers */
	sigfillset(&newSigMask);
	restoreSigMask = !pthread_sigmask(SIG_SETMASK, &newSigMask, &oldSigMask);
	/* fork and exec */
	pid = fork();

	if (pid < 0)
	{
		/* fork failure */
		goto finish;
	}

	if (pid == 0)
	{
		/* child process */
#ifndef __sun
		int maxfd;
#endif
		int fd;
		int sig;
		sigset_t set;
		struct sigaction act;
		/* set default signal handlers */
		memset(&act, 0, sizeof(act));
		act.sa_handler = SIG_DFL;
		act.sa_flags = 0;
		sigemptyset(&act.sa_mask);

		for (sig = 1; sig < NSIG; sig++)
			sigaction(sig, &act, NULL);

		/* unblock all signals */
		sigfillset(&set);
		pthread_sigmask(SIG_UNBLOCK, &set, NULL);

		if (lpStartupInfo)
		{
			int handle_fd;
			handle_fd = winpr_Handle_getFd(lpStartupInfo->hStdOutput);

			if (handle_fd != -1)
				dup2(handle_fd, STDOUT_FILENO);

			handle_fd = winpr_Handle_getFd(lpStartupInfo->hStdError);

			if (handle_fd != -1)
				dup2(handle_fd, STDERR_FILENO);

			handle_fd = winpr_Handle_getFd(lpStartupInfo->hStdInput);

			if (handle_fd != -1)
				dup2(handle_fd, STDIN_FILENO);
		}

#ifdef __sun
		closefrom(3);
#else
#ifdef F_MAXFD // on some BSD derivates
		maxfd = fcntl(0, F_MAXFD);
#else
		maxfd = sysconf(_SC_OPEN_MAX);
#endif

		for (fd = 3; fd < maxfd; fd++)
			close(fd);

#endif // __sun

		if (token)
		{
			if (token->GroupId)
			{
				int rc = setgid((gid_t)token->GroupId);

				if (rc < 0)
				{
				}
				else
				{
					initgroups(token->Username, (gid_t)token->GroupId);
				}
			}

			if (token->UserId)
				setuid((uid_t)token->UserId);
		}

		/* TODO: add better cwd handling and error checking */
		if (lpCurrentDirectory && strlen(lpCurrentDirectory) > 0)
			chdir(lpCurrentDirectory);

		if (execve(filename, pArgs, envp) < 0)
		{
			/* execve failed - end the process */
			_exit(1);
		}
	}
	else
	{
		/* parent process */
	}

	process = CreateProcessHandle(pid);

	if (!process)
	{
		goto finish;
	}

	thread = CreateNoneHandle();

	if (!thread)
	{
		ProcessHandleCloseHandle(process);
		goto finish;
	}

	lpProcessInformation->hProcess = process;
	lpProcessInformation->hThread = thread;
	lpProcessInformation->dwProcessId = (DWORD)pid;
	lpProcessInformation->dwThreadId = (DWORD)pid;
	ret = TRUE;
finish:

	/* restore caller's original signal mask */
	if (restoreSigMask)
		pthread_sigmask(SIG_SETMASK, &oldSigMask, NULL);

	free(filename);

	if (pArgs)
	{
		HeapFree(GetProcessHeap(), 0, pArgs);
	}

	if (lpszEnvironmentBlock)
		FreeEnvironmentStrings(lpszEnvironmentBlock);

	if (envp)
	{
		int i = 0;

		while (envp[i])
		{
			free(envp[i]);
			i++;
		}

		free(envp);
	}

	return ret;
}

BOOL CreateProcessA(LPCSTR lpApplicationName, LPSTR lpCommandLine,
                    LPSECURITY_ATTRIBUTES lpProcessAttributes,
                    LPSECURITY_ATTRIBUTES lpThreadAttributes, BOOL bInheritHandles,
                    DWORD dwCreationFlags, LPVOID lpEnvironment, LPCSTR lpCurrentDirectory,
                    LPSTARTUPINFOA lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation)
{
	return _CreateProcessExA(NULL, 0, lpApplicationName, lpCommandLine, lpProcessAttributes,
	                         lpThreadAttributes, bInheritHandles, dwCreationFlags, lpEnvironment,
	                         lpCurrentDirectory, lpStartupInfo, lpProcessInformation);
}

BOOL CreateProcessW(LPCWSTR lpApplicationName, LPWSTR lpCommandLine,
                    LPSECURITY_ATTRIBUTES lpProcessAttributes,
                    LPSECURITY_ATTRIBUTES lpThreadAttributes, BOOL bInheritHandles,
                    DWORD dwCreationFlags, LPVOID lpEnvironment, LPCWSTR lpCurrentDirectory,
                    LPSTARTUPINFOW lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation)
{
	return TRUE;
}

BOOL CreateProcessAsUserA(HANDLE hToken, LPCSTR lpApplicationName, LPSTR lpCommandLine,
                          LPSECURITY_ATTRIBUTES lpProcessAttributes,
                          LPSECURITY_ATTRIBUTES lpThreadAttributes, BOOL bInheritHandles,
                          DWORD dwCreationFlags, LPVOID lpEnvironment, LPCSTR lpCurrentDirectory,
                          LPSTARTUPINFOA lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation)
{
	return _CreateProcessExA(hToken, 0, lpApplicationName, lpCommandLine, lpProcessAttributes,
	                         lpThreadAttributes, bInheritHandles, dwCreationFlags, lpEnvironment,
	                         lpCurrentDirectory, lpStartupInfo, lpProcessInformation);
}

BOOL CreateProcessAsUserW(HANDLE hToken, LPCWSTR lpApplicationName, LPWSTR lpCommandLine,
                          LPSECURITY_ATTRIBUTES lpProcessAttributes,
                          LPSECURITY_ATTRIBUTES lpThreadAttributes, BOOL bInheritHandles,
                          DWORD dwCreationFlags, LPVOID lpEnvironment, LPCWSTR lpCurrentDirectory,
                          LPSTARTUPINFOW lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation)
{
	return TRUE;
}

BOOL CreateProcessWithLogonA(LPCSTR lpUsername, LPCSTR lpDomain, LPCSTR lpPassword,
                             DWORD dwLogonFlags, LPCSTR lpApplicationName, LPSTR lpCommandLine,
                             DWORD dwCreationFlags, LPVOID lpEnvironment, LPCSTR lpCurrentDirectory,
                             LPSTARTUPINFOA lpStartupInfo,
                             LPPROCESS_INFORMATION lpProcessInformation)
{
	return TRUE;
}

BOOL CreateProcessWithLogonW(LPCWSTR lpUsername, LPCWSTR lpDomain, LPCWSTR lpPassword,
                             DWORD dwLogonFlags, LPCWSTR lpApplicationName, LPWSTR lpCommandLine,
                             DWORD dwCreationFlags, LPVOID lpEnvironment,
                             LPCWSTR lpCurrentDirectory, LPSTARTUPINFOW lpStartupInfo,
                             LPPROCESS_INFORMATION lpProcessInformation)
{
	return TRUE;
}

BOOL CreateProcessWithTokenA(HANDLE hToken, DWORD dwLogonFlags, LPCSTR lpApplicationName,
                             LPSTR lpCommandLine, DWORD dwCreationFlags, LPVOID lpEnvironment,
                             LPCSTR lpCurrentDirectory, LPSTARTUPINFOA lpStartupInfo,
                             LPPROCESS_INFORMATION lpProcessInformation)
{
	return _CreateProcessExA(NULL, 0, lpApplicationName, lpCommandLine, NULL, NULL, FALSE,
	                         dwCreationFlags, lpEnvironment, lpCurrentDirectory, lpStartupInfo,
	                         lpProcessInformation);
}

BOOL CreateProcessWithTokenW(HANDLE hToken, DWORD dwLogonFlags, LPCWSTR lpApplicationName,
                             LPWSTR lpCommandLine, DWORD dwCreationFlags, LPVOID lpEnvironment,
                             LPCWSTR lpCurrentDirectory, LPSTARTUPINFOW lpStartupInfo,
                             LPPROCESS_INFORMATION lpProcessInformation)
{
	return TRUE;
}

VOID ExitProcess(UINT uExitCode)
{
	exit((int)uExitCode);
}

BOOL GetExitCodeProcess(HANDLE hProcess, LPDWORD lpExitCode)
{
	WINPR_PROCESS* process;

	if (!hProcess)
		return FALSE;

	if (!lpExitCode)
		return FALSE;

	process = (WINPR_PROCESS*)hProcess;
	*lpExitCode = process->dwExitCode;
	return TRUE;
}

HANDLE _GetCurrentProcess(VOID)
{
	return NULL;
}

DWORD GetCurrentProcessId(VOID)
{
	return ((DWORD)getpid());
}

BOOL TerminateProcess(HANDLE hProcess, UINT uExitCode)
{
	WINPR_PROCESS* process;
	process = (WINPR_PROCESS*)hProcess;

	if (!process || (process->pid <= 0))
		return FALSE;

	if (kill(process->pid, SIGTERM))
		return FALSE;

	return TRUE;
}

static BOOL ProcessHandleCloseHandle(HANDLE handle)
{
	WINPR_PROCESS* process = (WINPR_PROCESS*)handle;
	free(process);
	return TRUE;
}

static BOOL ProcessHandleIsHandle(HANDLE handle)
{
	WINPR_PROCESS* process = (WINPR_PROCESS*)handle;

	if (!process || process->Type != HANDLE_TYPE_PROCESS)
	{
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}

	return TRUE;
}

static int ProcessGetFd(HANDLE handle)
{
	WINPR_PROCESS* process = (WINPR_PROCESS*)handle;

	if (!ProcessHandleIsHandle(handle))
		return -1;

	/* TODO: Process does not support fd... */
	(void)process;
	return -1;
}

static HANDLE_OPS ops = { ProcessHandleIsHandle,
	                      ProcessHandleCloseHandle,
	                      ProcessGetFd,
	                      NULL, /* CleanupHandle */
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL };

HANDLE CreateProcessHandle(pid_t pid)
{
	WINPR_PROCESS* process;
	process = (WINPR_PROCESS*)calloc(1, sizeof(WINPR_PROCESS));

	if (!process)
		return NULL;

	process->pid = pid;
	process->Type = HANDLE_TYPE_PROCESS;
	process->ops = &ops;
	return (HANDLE)process;
}

#endif
