/**
 * WinPR: Windows Portable Runtime
 * Windows Registry
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef WINPR_REGISTRY_H
#define WINPR_REGISTRY_H

#include <winpr/windows.h>

#if defined(_WIN32) && !defined(_UWP)

#include <winreg.h>

#else

#ifdef __cplusplus
extern "C" {
#endif

#include <winpr/winpr.h>
#include <winpr/wtypes.h>

#include <winpr/nt.h>
#include <winpr/io.h>
#include <winpr/error.h>

#ifndef _WIN32

#define OWNER_SECURITY_INFORMATION	0x00000001
#define GROUP_SECURITY_INFORMATION	0x00000002
#define DACL_SECURITY_INFORMATION	0x00000004
#define SACL_SECURITY_INFORMATION	0x00000008

#define REG_OPTION_RESERVED		0x00000000
#define REG_OPTION_NON_VOLATILE		0x00000000
#define REG_OPTION_VOLATILE		0x00000001
#define REG_OPTION_CREATE_LINK		0x00000002
#define REG_OPTION_BACKUP_RESTORE	0x00000004
#define REG_OPTION_OPEN_LINK		0x00000008

#define REG_CREATED_NEW_KEY		0x00000001
#define REG_OPENED_EXISTING_KEY		0x00000002

#define REG_NOTIFY_CHANGE_NAME		0x01
#define REG_NOTIFY_CHANGE_ATTRIBUTES	0x02
#define REG_NOTIFY_CHANGE_LAST_SET	0x04
#define REG_NOTIFY_CHANGE_SECURITY	0x08

#define KEY_QUERY_VALUE			0x00000001
#define KEY_SET_VALUE			0x00000002
#define KEY_CREATE_SUB_KEY		0x00000004
#define KEY_ENUMERATE_SUB_KEYS		0x00000008
#define KEY_NOTIFY			0x00000010
#define KEY_CREATE_LINK			0x00000020
#define KEY_WOW64_64KEY			0x00000100
#define KEY_WOW64_32KEY			0x00000200
#define KEY_WOW64_RES			0x00000300

#define REG_WHOLE_HIVE_VOLATILE		0x00000001
#define REG_REFRESH_HIVE		0x00000002
#define REG_NO_LAZY_FLUSH		0x00000004
#define REG_FORCE_RESTORE		0x00000008

#define KEY_READ			((STANDARD_RIGHTS_READ | KEY_QUERY_VALUE | \
						KEY_ENUMERATE_SUB_KEYS | KEY_NOTIFY) & (~SYNCHRONIZE))

#define KEY_WRITE			((STANDARD_RIGHTS_WRITE | KEY_SET_VALUE | \
						KEY_CREATE_SUB_KEY) & (~SYNCHRONIZE))

#define KEY_EXECUTE			((KEY_READ) & (~SYNCHRONIZE))

#define KEY_ALL_ACCESS			((STANDARD_RIGHTS_ALL | KEY_QUERY_VALUE | \
						KEY_SET_VALUE | KEY_CREATE_SUB_KEY | \
						KEY_ENUMERATE_SUB_KEYS | KEY_NOTIFY | \
						KEY_CREATE_LINK) & (~SYNCHRONIZE))

#define REG_NONE			0
#define REG_SZ				1
#define REG_EXPAND_SZ			2
#define REG_BINARY			3
#define REG_DWORD			4
#define REG_DWORD_LITTLE_ENDIAN		4
#define REG_DWORD_BIG_ENDIAN		5
#define REG_LINK			6
#define REG_MULTI_SZ			7
#define REG_RESOURCE_LIST		8
#define REG_FULL_RESOURCE_DESCRIPTOR	9
#define REG_RESOURCE_REQUIREMENTS_LIST	10
#define REG_QWORD			11
#define REG_QWORD_LITTLE_ENDIAN		11

typedef HANDLE HKEY;
typedef HANDLE* PHKEY;

#endif

typedef ACCESS_MASK REGSAM;

#define HKEY_CLASSES_ROOT				((HKEY) (LONG_PTR) (LONG) 0x80000000)
#define HKEY_CURRENT_USER				((HKEY) (LONG_PTR) (LONG) 0x80000001)
#define HKEY_LOCAL_MACHINE				((HKEY) (LONG_PTR) (LONG) 0x80000002)
#define HKEY_USERS					((HKEY) (LONG_PTR) (LONG) 0x80000003)
#define HKEY_PERFORMANCE_DATA				((HKEY) (LONG_PTR) (LONG) 0x80000004)
#define HKEY_PERFORMANCE_TEXT				((HKEY) (LONG_PTR) (LONG) 0x80000050)
#define HKEY_PERFORMANCE_NLSTEXT			((HKEY) (LONG_PTR) (LONG) 0x80000060)
#define HKEY_CURRENT_CONFIG				((HKEY) (LONG_PTR) (LONG) 0x80000005)
#define HKEY_DYN_DATA					((HKEY) (LONG_PTR) (LONG) 0x80000006)
#define HKEY_CURRENT_USER_LOCAL_SETTINGS		((HKEY) (LONG_PTR) (LONG) 0x80000007)

#define RRF_RT_REG_NONE					0x00000001
#define RRF_RT_REG_SZ					0x00000002
#define RRF_RT_REG_EXPAND_SZ				0x00000004
#define RRF_RT_REG_BINARY				0x00000008
#define RRF_RT_REG_DWORD				0x00000010
#define RRF_RT_REG_MULTI_SZ				0x00000020
#define RRF_RT_REG_QWORD				0x00000040

#define RRF_RT_DWORD					(RRF_RT_REG_BINARY | RRF_RT_REG_DWORD)
#define RRF_RT_QWORD					(RRF_RT_REG_BINARY | RRF_RT_REG_QWORD)
#define RRF_RT_ANY					0x0000FFFF

#define RRF_NOEXPAND					0x10000000
#define RRF_ZEROONFAILURE				0x20000000

struct val_context
{
	int valuelen;
	LPVOID value_context;
	LPVOID val_buff_ptr;
};

typedef struct val_context *PVALCONTEXT;

typedef struct pvalueA
{
	LPSTR pv_valuename;
	int pv_valuelen;
	LPVOID pv_value_context;
	DWORD pv_type;
} PVALUEA, *PPVALUEA;

typedef struct pvalueW
{
	LPWSTR pv_valuename;
	int pv_valuelen;
	LPVOID pv_value_context;
	DWORD pv_type;
} PVALUEW, *PPVALUEW;

#ifdef UNICODE
typedef PVALUEW PVALUE;
typedef PPVALUEW PPVALUE;
#else
typedef PVALUEA PVALUE;
typedef PPVALUEA PPVALUE;
#endif

typedef struct value_entA
{
	LPSTR ve_valuename;
	DWORD ve_valuelen;
	DWORD_PTR ve_valueptr;
	DWORD ve_type;
} VALENTA, *PVALENTA;

typedef struct value_entW
{
	LPWSTR ve_valuename;
	DWORD ve_valuelen;
	DWORD_PTR ve_valueptr;
	DWORD ve_type;
} VALENTW, *PVALENTW;

#ifdef UNICODE
typedef VALENTW VALENT;
typedef PVALENTW PVALENT;
#else
typedef VALENTA VALENT;
typedef PVALENTA PVALENT;
#endif

WINPR_API LONG RegCloseKey(HKEY hKey);

WINPR_API LONG RegCopyTreeW(HKEY hKeySrc, LPCWSTR lpSubKey, HKEY hKeyDest);
WINPR_API LONG RegCopyTreeA(HKEY hKeySrc, LPCSTR lpSubKey, HKEY hKeyDest);

#ifdef UNICODE
#define RegCopyTree RegCopyTreeW
#else
#define RegCopyTree RegCopyTreeA
#endif

WINPR_API LONG RegCreateKeyExW(HKEY hKey, LPCWSTR lpSubKey, DWORD Reserved, LPWSTR lpClass, DWORD dwOptions,
		REGSAM samDesired, LPSECURITY_ATTRIBUTES lpSecurityAttributes, PHKEY phkResult, LPDWORD lpdwDisposition);
WINPR_API LONG RegCreateKeyExA(HKEY hKey, LPCSTR lpSubKey, DWORD Reserved, LPSTR lpClass, DWORD dwOptions,
		REGSAM samDesired, LPSECURITY_ATTRIBUTES lpSecurityAttributes, PHKEY phkResult, LPDWORD lpdwDisposition);

#ifdef UNICODE
#define RegCreateKeyEx RegCreateKeyExW
#else
#define RegCreateKeyEx RegCreateKeyExA
#endif

WINPR_API LONG RegDeleteKeyExW(HKEY hKey, LPCWSTR lpSubKey, REGSAM samDesired, DWORD Reserved);
WINPR_API LONG RegDeleteKeyExA(HKEY hKey, LPCSTR lpSubKey, REGSAM samDesired, DWORD Reserved);

#ifdef UNICODE
#define RegDeleteKeyEx RegDeleteKeyExW
#else
#define RegDeleteKeyEx RegDeleteKeyExA
#endif

WINPR_API LONG RegDeleteTreeW(HKEY hKey, LPCWSTR lpSubKey);
WINPR_API LONG RegDeleteTreeA(HKEY hKey, LPCSTR lpSubKey);

#ifdef UNICODE
#define RegDeleteTree RegDeleteTreeW
#else
#define RegDeleteTree RegDeleteTreeA
#endif

WINPR_API LONG RegDeleteValueW(HKEY hKey, LPCWSTR lpValueName);
WINPR_API LONG RegDeleteValueA(HKEY hKey, LPCSTR lpValueName);

#ifdef UNICODE
#define RegDeleteValue RegDeleteValueW
#else
#define RegDeleteValue RegDeleteValueA
#endif

WINPR_API LONG RegDisablePredefinedCacheEx(void);

WINPR_API LONG RegEnumKeyExW(HKEY hKey, DWORD dwIndex, LPWSTR lpName, LPDWORD lpcName,
		LPDWORD lpReserved, LPWSTR lpClass, LPDWORD lpcClass, PFILETIME lpftLastWriteTime);
WINPR_API LONG RegEnumKeyExA(HKEY hKey, DWORD dwIndex, LPSTR lpName, LPDWORD lpcName,
		LPDWORD lpReserved, LPSTR lpClass, LPDWORD lpcClass, PFILETIME lpftLastWriteTime);

#ifdef UNICODE
#define RegEnumKeyEx RegEnumKeyExW
#else
#define RegEnumKeyEx RegEnumKeyExA
#endif

WINPR_API LONG RegEnumValueW(HKEY hKey, DWORD dwIndex, LPWSTR lpValueName,
		LPDWORD lpcchValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData);
WINPR_API LONG RegEnumValueA(HKEY hKey, DWORD dwIndex, LPSTR lpValueName,
		LPDWORD lpcchValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData);

#ifdef UNICODE
#define RegEnumValue RegEnumValueW
#else
#define RegEnumValue RegEnumValueA
#endif

WINPR_API LONG RegFlushKey(HKEY hKey);

WINPR_API LONG RegGetKeySecurity(HKEY hKey, SECURITY_INFORMATION SecurityInformation,
		PSECURITY_DESCRIPTOR pSecurityDescriptor, LPDWORD lpcbSecurityDescriptor);

WINPR_API LONG RegGetValueW(HKEY hkey, LPCWSTR lpSubKey, LPCWSTR lpValue,
		DWORD dwFlags, LPDWORD pdwType, PVOID pvData, LPDWORD pcbData);
WINPR_API LONG RegGetValueA(HKEY hkey, LPCSTR lpSubKey, LPCSTR lpValue,
		DWORD dwFlags, LPDWORD pdwType, PVOID pvData, LPDWORD pcbData);

#ifdef UNICODE
#define RegGetValue RegGetValueW
#else
#define RegGetValue RegGetValueA
#endif

WINPR_API LONG RegLoadAppKeyW(LPCWSTR lpFile, PHKEY phkResult,
		REGSAM samDesired, DWORD dwOptions, DWORD Reserved);
WINPR_API LONG RegLoadAppKeyA(LPCSTR lpFile, PHKEY phkResult,
		REGSAM samDesired, DWORD dwOptions, DWORD Reserved);

#ifdef UNICODE
#define RegLoadAppKey RegLoadAppKeyW
#else
#define RegLoadAppKey RegLoadAppKeyA
#endif

WINPR_API LONG RegLoadKeyW(HKEY hKey, LPCWSTR lpSubKey, LPCWSTR lpFile);
WINPR_API LONG RegLoadKeyA(HKEY hKey, LPCSTR lpSubKey, LPCSTR lpFile);

#ifdef UNICODE
#define RegLoadKey RegLoadKeyW
#else
#define RegLoadKey RegLoadKeyA
#endif

WINPR_API LONG RegLoadMUIStringW(HKEY hKey, LPCWSTR pszValue, LPWSTR pszOutBuf,
		DWORD cbOutBuf, LPDWORD pcbData, DWORD Flags, LPCWSTR pszDirectory);
WINPR_API LONG RegLoadMUIStringA(HKEY hKey, LPCSTR pszValue, LPSTR pszOutBuf,
		DWORD cbOutBuf, LPDWORD pcbData, DWORD Flags, LPCSTR pszDirectory);

#ifdef UNICODE
#define RegLoadMUIString RegLoadMUIStringW
#else
#define RegLoadMUIString RegLoadMUIStringA
#endif

WINPR_API LONG RegNotifyChangeKeyValue(HKEY hKey, BOOL bWatchSubtree, DWORD dwNotifyFilter, HANDLE hEvent, BOOL fAsynchronous);

WINPR_API LONG RegOpenCurrentUser(REGSAM samDesired, PHKEY phkResult);

WINPR_API LONG RegOpenKeyExW(HKEY hKey, LPCWSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult);
WINPR_API LONG RegOpenKeyExA(HKEY hKey, LPCSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult);

#ifdef UNICODE
#define RegOpenKeyEx RegOpenKeyExW
#else
#define RegOpenKeyEx RegOpenKeyExA
#endif

WINPR_API LONG RegOpenUserClassesRoot(HANDLE hToken, DWORD dwOptions, REGSAM samDesired, PHKEY phkResult);

WINPR_API LONG RegQueryInfoKeyW(HKEY hKey, LPWSTR lpClass, LPDWORD lpcClass, LPDWORD lpReserved,
		LPDWORD lpcSubKeys, LPDWORD lpcMaxSubKeyLen, LPDWORD lpcMaxClassLen,
		LPDWORD lpcValues, LPDWORD lpcMaxValueNameLen, LPDWORD lpcMaxValueLen,
		LPDWORD lpcbSecurityDescriptor, PFILETIME lpftLastWriteTime);
WINPR_API LONG RegQueryInfoKeyA(HKEY hKey, LPSTR lpClass, LPDWORD lpcClass, LPDWORD lpReserved,
		LPDWORD lpcSubKeys, LPDWORD lpcMaxSubKeyLen, LPDWORD lpcMaxClassLen,
		LPDWORD lpcValues, LPDWORD lpcMaxValueNameLen, LPDWORD lpcMaxValueLen,
		LPDWORD lpcbSecurityDescriptor, PFILETIME lpftLastWriteTime);

#ifdef UNICODE
#define RegQueryInfoKey RegQueryInfoKeyW
#else
#define RegQueryInfoKey RegQueryInfoKeyA
#endif

WINPR_API LONG RegQueryValueExW(HKEY hKey, LPCWSTR lpValueName,
		LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData);
WINPR_API LONG RegQueryValueExA(HKEY hKey, LPCSTR lpValueName,
		LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData);

#ifdef UNICODE
#define RegQueryValueEx RegQueryValueExW
#else
#define RegQueryValueEx RegQueryValueExA
#endif

WINPR_API LONG RegRestoreKeyW(HKEY hKey, LPCWSTR lpFile, DWORD dwFlags);
WINPR_API LONG RegRestoreKeyA(HKEY hKey, LPCSTR lpFile, DWORD dwFlags);

#ifdef UNICODE
#define RegRestoreKey RegRestoreKeyW
#else
#define RegRestoreKey RegRestoreKeyA
#endif

WINPR_API LONG RegSaveKeyExW(HKEY hKey, LPCWSTR lpFile, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD Flags);
WINPR_API LONG RegSaveKeyExA(HKEY hKey, LPCSTR lpFile, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD Flags);

#ifdef UNICODE
#define RegSaveKeyEx RegSaveKeyExW
#else
#define RegSaveKeyEx RegSaveKeyExA
#endif

WINPR_API LONG RegSetKeySecurity(HKEY hKey, SECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR pSecurityDescriptor);

WINPR_API LONG RegSetValueExW(HKEY hKey, LPCWSTR lpValueName, DWORD Reserved, DWORD dwType, const BYTE* lpData, DWORD cbData);
WINPR_API LONG RegSetValueExA(HKEY hKey, LPCSTR lpValueName, DWORD Reserved, DWORD dwType, const BYTE* lpData, DWORD cbData);

#ifdef UNICODE
#define RegSetValueEx RegSetValueExW
#else
#define RegSetValueEx RegSetValueExA
#endif

WINPR_API LONG RegUnLoadKeyW(HKEY hKey, LPCWSTR lpSubKey);
WINPR_API LONG RegUnLoadKeyA(HKEY hKey, LPCSTR lpSubKey);

#ifdef UNICODE
#define RegUnLoadKey RegUnLoadKeyW
#else
#define RegUnLoadKey RegUnLoadKeyA
#endif

#ifdef __cplusplus
}
#endif

#endif

#endif /* WINPR_REGISTRY_H */
