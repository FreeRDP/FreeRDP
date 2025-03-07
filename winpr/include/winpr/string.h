/**
 * WinPR: Windows Portable Runtime
 * String Manipulation (CRT)
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2016 David PHAM-VAN <d.phamvan@inuvika.com>
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

#ifndef WINPR_CRT_STRING_H
#define WINPR_CRT_STRING_H

#include <wchar.h>
#include <stdio.h>
#include <string.h>
#include <winpr/config.h>
#include <winpr/winpr.h>
#include <winpr/wtypes.h>

WINPR_PRAGMA_DIAG_PUSH
WINPR_PRAGMA_DIAG_IGNORED_RESERVED_IDENTIFIER

#ifdef __cplusplus
extern "C"
{
#endif

	WINPR_API char* winpr_str_url_encode(const char* str, size_t len);
	WINPR_API char* winpr_str_url_decode(const char* str, size_t len);

	WINPR_API BOOL winpr_str_append(const char* what, char* buffer, size_t size,
	                                const char* separator);

	WINPR_API int winpr_asprintf(char** s, size_t* slen, const char* templ, ...);
	WINPR_API int winpr_vasprintf(char** s, size_t* slen, const char* templ, va_list ap);

#ifndef _WIN32

#define CSTR_LESS_THAN 1
#define CSTR_EQUAL 2
#define CSTR_GREATER_THAN 3

#define CP_ACP 0
#define CP_OEMCP 1
#define CP_MACCP 2
#define CP_THREAD_ACP 3
#define CP_SYMBOL 42
#define CP_UTF7 65000
#define CP_UTF8 65001

#define MB_PRECOMPOSED 0x00000001
#define MB_COMPOSITE 0x00000002
#define MB_USEGLYPHCHARS 0x00000004
#define MB_ERR_INVALID_CHARS 0x00000008

	WINPR_API char* _strdup(const char* strSource);
	WINPR_API WCHAR* _wcsdup(const WCHAR* strSource);

	WINPR_API int _stricmp(const char* string1, const char* string2);
	WINPR_API int _strnicmp(const char* string1, const char* string2, size_t count);

	WINPR_API int _wcscmp(const WCHAR* string1, const WCHAR* string2);
	WINPR_API int _wcsncmp(const WCHAR* string1, const WCHAR* string2, size_t count);

	WINPR_API size_t _wcslen(const WCHAR* str);
	WINPR_API size_t _wcsnlen(const WCHAR* str, size_t maxNumberOfElements);

	WINPR_API WCHAR* _wcsstr(const WCHAR* str, const WCHAR* strSearch);

	WINPR_API WCHAR* _wcschr(const WCHAR* str, WCHAR c);
	WINPR_API WCHAR* _wcsrchr(const WCHAR* str, WCHAR c);

	WINPR_API char* strtok_s(char* strToken, const char* strDelimit, char** context);
	WINPR_API WCHAR* wcstok_s(WCHAR* strToken, const WCHAR* strDelimit, WCHAR** context);

	WINPR_API WCHAR* _wcsncat(WCHAR* dst, const WCHAR* src, size_t sz);
#else

#define _wcscmp wcscmp
#define _wcsncmp wcsncmp
#define _wcslen wcslen
#define _wcsnlen wcsnlen
#define _wcsstr wcsstr
#define _wcschr wcschr
#define _wcsrchr wcsrchr
#define _wcsncat wcsncat

#endif /* _WIN32 */

#if !defined(_WIN32) || defined(_UWP)

	WINPR_API LPSTR CharUpperA(LPSTR lpsz);
	WINPR_API LPWSTR CharUpperW(LPWSTR lpsz);

#ifdef UNICODE
#define CharUpper CharUpperW
#else
#define CharUpper CharUpperA
#endif

	WINPR_API DWORD CharUpperBuffA(LPSTR lpsz, DWORD cchLength);
	WINPR_API DWORD CharUpperBuffW(LPWSTR lpsz, DWORD cchLength);

#ifdef UNICODE
#define CharUpperBuff CharUpperBuffW
#else
#define CharUpperBuff CharUpperBuffA
#endif

	WINPR_API LPSTR CharLowerA(LPSTR lpsz);
	WINPR_API LPWSTR CharLowerW(LPWSTR lpsz);

#ifdef UNICODE
#define CharLower CharLowerW
#else
#define CharLower CharLowerA
#endif

	WINPR_API DWORD CharLowerBuffA(LPSTR lpsz, DWORD cchLength);
	WINPR_API DWORD CharLowerBuffW(LPWSTR lpsz, DWORD cchLength);

#ifdef UNICODE
#define CharLowerBuff CharLowerBuffW
#else
#define CharLowerBuff CharLowerBuffA
#endif

	WINPR_API BOOL IsCharAlphaA(CHAR ch);
	WINPR_API BOOL IsCharAlphaW(WCHAR ch);

#ifdef UNICODE
#define IsCharAlpha IsCharAlphaW
#else
#define IsCharAlpha IsCharAlphaA
#endif

	WINPR_API BOOL IsCharAlphaNumericA(CHAR ch);
	WINPR_API BOOL IsCharAlphaNumericW(WCHAR ch);

#ifdef UNICODE
#define IsCharAlphaNumeric IsCharAlphaNumericW
#else
#define IsCharAlphaNumeric IsCharAlphaNumericA
#endif

	WINPR_API BOOL IsCharUpperA(CHAR ch);
	WINPR_API BOOL IsCharUpperW(WCHAR ch);

#ifdef UNICODE
#define IsCharUpper IsCharUpperW
#else
#define IsCharUpper IsCharUpperA
#endif

	WINPR_API BOOL IsCharLowerA(CHAR ch);
	WINPR_API BOOL IsCharLowerW(WCHAR ch);

#ifdef UNICODE
#define IsCharLower IsCharLowerW
#else
#define IsCharLower IsCharLowerA
#endif

#endif

#ifndef _WIN32

#define sprintf_s snprintf
#define _snprintf snprintf
#define _scprintf(...) snprintf(NULL, 0, __VA_ARGS__)

#define _scprintf(...) snprintf(NULL, 0, __VA_ARGS__)

	/* Unicode Conversion */

#if defined(WITH_WINPR_DEPRECATED)
	WINPR_DEPRECATED_VAR("Use ConvertUtf8ToWChar instead",
	                     WINPR_API int MultiByteToWideChar(UINT CodePage, DWORD dwFlags,
	                                                       LPCSTR lpMultiByteStr, int cbMultiByte,
	                                                       LPWSTR lpWideCharStr, int cchWideChar));

	WINPR_DEPRECATED_VAR("Use ConvertWCharToUtf8 instead",
	                     WINPR_API int WideCharToMultiByte(UINT CodePage, DWORD dwFlags,
	                                                       LPCWSTR lpWideCharStr, int cchWideChar,
	                                                       LPSTR lpMultiByteStr, int cbMultiByte,
	                                                       LPCSTR lpDefaultChar,
	                                                       LPBOOL lpUsedDefaultChar));
#endif

#endif

	/* Extended API */
	/** \brief Converts form UTF-16 to UTF-8
	 *
	 * The function does string conversions of any '\0' terminated input string
	 *
	 * Supplying len = 0 will return the required size of the buffer in characters.
	 *
	 * \warning Supplying a buffer length smaller than required will result in
	 * platform dependent (=undefined) behaviour!
	 *
	 *  \param wstr A '\0' terminated WCHAR string, may be NULL
	 *  \param str A pointer to the result string
	 *  \param len The length in characters of the result buffer
	 *
	 *  \return the size of the converted string in char (strlen), or -1 for failure
	 */
	WINPR_API SSIZE_T ConvertWCharToUtf8(const WCHAR* wstr, char* str, size_t len);

	/** \brief Converts form UTF-16 to UTF-8
	 *
	 * The function does string conversions of any input string of wlen (or less)
	 * characters until it reaches the first '\0'.
	 *
	 * Supplying len = 0 will return the required size of the buffer in characters.
	 *
	 * \warning Supplying a buffer length smaller than required will result in
	 * platform dependent (=undefined) behaviour!
	 *
	 *  \param wstr A WCHAR string of \b wlen length
	 *  \param wlen The (buffer) length in characters of \b wstr
	 *  \param str A pointer to the result string
	 *  \param len The length in characters of the result buffer
	 *
	 *  \return the size of the converted string in char (strlen), or -1 for failure
	 */
	WINPR_API SSIZE_T ConvertWCharNToUtf8(const WCHAR* wstr, size_t wlen, char* str, size_t len);

	/** \brief Converts multistrings form UTF-16 to UTF-8
	 *
	 * The function does string conversions of any input string of wlen characters.
	 * Any character in the buffer (including any '\0') is converted.
	 *
	 * Supplying len = 0 will return the required size of the buffer in characters.
	 *
	 * \warning Supplying a buffer length smaller than required will result in
	 * platform dependent (=undefined) behaviour!
	 *
	 *  \param wstr A WCHAR string of \b wlen length
	 *  \param wlen The (buffer) length in characters of \b wstr
	 *  \param str A pointer to the result string
	 *  \param len The length in characters of the result buffer
	 *
	 *  \return the size of the converted string in CHAR characters (including any '\0'), or -1 for
	 * failure
	 */
	WINPR_API SSIZE_T ConvertMszWCharNToUtf8(const WCHAR* wstr, size_t wlen, char* str, size_t len);

	/** \brief Converts form UTF-8 to UTF-16
	 *
	 * The function does string conversions of any '\0' terminated input string
	 *
	 * Supplying wlen = 0 will return the required size of the buffer in characters.
	 *
	 * \warning Supplying a buffer length smaller than required will result in
	 * platform dependent (=undefined) behaviour!
	 *
	 *  \param str A '\0' terminated CHAR string, may be NULL
	 *  \param wstr A pointer to the result WCHAR string
	 *  \param wlen The length in WCHAR characters of the result buffer
	 *
	 *  \return the size of the converted string in WCHAR characters (wcslen), or -1 for failure
	 */
	WINPR_API SSIZE_T ConvertUtf8ToWChar(const char* str, WCHAR* wstr, size_t wlen);

	/** \brief Converts form UTF-8 to UTF-16
	 *
	 * The function does string conversions of any input string of len (or less)
	 * characters until it reaches the first '\0'.
	 *
	 * Supplying wlen = 0 will return the required size of the buffer in characters.
	 *
	 * \warning Supplying a buffer length smaller than required will result in
	 * platform dependent (=undefined) behaviour!
	 *
	 *  \param str A CHAR string of \b len length
	 *  \param len The (buffer) length in characters of \b str
	 *  \param wstr A pointer to the result WCHAR string
	 *  \param wlen The length in WCHAR characters of the result buffer
	 *
	 *  \return the size of the converted string in WCHAR characters (wcslen), or -1 for failure
	 */
	WINPR_API SSIZE_T ConvertUtf8NToWChar(const char* str, size_t len, WCHAR* wstr, size_t wlen);

	/** \brief Converts multistrings form UTF-8 to UTF-16
	 *
	 * The function does string conversions of any input string of len characters.
	 * Any character in the buffer (including any '\0') is converted.
	 *
	 * Supplying wlen = 0 will return the required size of the buffer in characters.
	 *
	 * \warning Supplying a buffer length smaller than required will result in
	 * platform dependent (=undefined) behaviour!
	 *
	 *  \param str A CHAR string of \b len length
	 *  \param len The (buffer) length in characters of \b str
	 *  \param wstr A pointer to the result WCHAR string
	 *  \param wlen The length in WCHAR characters of the result buffer
	 *
	 *  \return the size of the converted string in WCHAR characters (including any '\0'), or -1 for
	 * failure
	 */
	WINPR_API SSIZE_T ConvertMszUtf8NToWChar(const char* str, size_t len, WCHAR* wstr, size_t wlen);

	/** \brief Converts form UTF-16 to UTF-8, returns an allocated string
	 *
	 * The function does string conversions of any '\0' terminated input string
	 *
	 *  \param wstr A '\0' terminated WCHAR string, may be NULL
	 *  \param pUtfCharLength Ignored if NULL, otherwise receives the length of the result string in
	 * characters (strlen)
	 *
	 *  \return An allocated zero terminated UTF-8 string or NULL in case of failure.
	 */
	WINPR_API char* ConvertWCharToUtf8Alloc(const WCHAR* wstr, size_t* pUtfCharLength);

	/** \brief Converts form UTF-16 to UTF-8, returns an allocated string
	 *
	 * The function does string conversions of any input string of wlen (or less)
	 * characters until it reaches the first '\0'.
	 *
	 *  \param wstr A WCHAR string of \b wlen length
	 *  \param wlen The (buffer) length in characters of \b wstr
	 *  \param pUtfCharLength Ignored if NULL, otherwise receives the length of the result string in
	 * characters (strlen)
	 *
	 *  \return An allocated zero terminated UTF-8 string or NULL in case of failure.
	 */
	WINPR_API char* ConvertWCharNToUtf8Alloc(const WCHAR* wstr, size_t wlen,
	                                         size_t* pUtfCharLength);

	/** \brief Converts multistring form UTF-16 to UTF-8, returns an allocated string
	 *
	 * The function does string conversions of any input string of len characters.
	 * Any character in the buffer (including any '\0') is converted.
	 *
	 *  \param wstr A WCHAR string of \b len character length
	 *  \param wlen The (buffer) length in characters of \b str
	 *  \param pUtfCharLength Ignored if NULL, otherwise receives the length of the result string in
	 * characters (including any '\0' character)
	 *
	 *  \return An allocated double zero terminated UTF-8 string or NULL in case of failure.
	 */
	WINPR_API char* ConvertMszWCharNToUtf8Alloc(const WCHAR* wstr, size_t wlen,
	                                            size_t* pUtfCharLength);

	/** \brief Converts form UTF-8 to UTF-16, returns an allocated string
	 *
	 * The function does string conversions of any '\0' terminated input string
	 *
	 *  \param str A '\0' terminated CHAR string, may be NULL
	 *  \param pSize Ignored if NULL, otherwise receives the length of the result string in
	 * characters (wcslen)
	 *
	 *  \return An allocated zero terminated UTF-16 string or NULL in case of failure.
	 */
	WINPR_API WCHAR* ConvertUtf8ToWCharAlloc(const char* str, size_t* pSize);

	/** \brief Converts form UTF-8 to UTF-16, returns an allocated string
	 *
	 * The function does string conversions of any input string of len (or less)
	 * characters until it reaches the first '\0'.
	 *
	 *  \param str A CHAR string of \b len length
	 *  \param len The (buffer) length in characters of \b str
	 *  \param pSize Ignored if NULL, otherwise receives the length of the result string in
	 * characters (wcslen)
	 *
	 *  \return An allocated zero terminated UTF-16 string or NULL in case of failure.
	 */
	WINPR_API WCHAR* ConvertUtf8NToWCharAlloc(const char* str, size_t len, size_t* pSize);

	/** \brief Converts multistring form UTF-8 to UTF-16, returns an allocated string
	 *
	 * The function does string conversions of any input string of len characters.
	 * Any character in the buffer (including any '\0') is converted.
	 *
	 *  \param str A CHAR string of \b len byte length
	 *  \param len The (buffer) length in characters of \b str
	 *  \param pSize Ignored if NULL, otherwise receives the length of the result string in
	 * characters (including any '\0' character)
	 *
	 *  \return An allocated double zero terminated UTF-16 string or NULL in case of failure.
	 */
	WINPR_API WCHAR* ConvertMszUtf8NToWCharAlloc(const char* str, size_t len, size_t* pSize);

	/** \brief Helper function to initialize const WCHAR pointer from a Utf8 string
	 *
	 *  \param str The Utf8 string to use for initialization
	 *  \param buffer The WCHAR buffer used to store the converted data
	 *  \param len The size of the buffer in number of WCHAR
	 *
	 *  \return The WCHAR string (a pointer to buffer)
	 */
	WINPR_API const WCHAR* InitializeConstWCharFromUtf8(const char* str, WCHAR* buffer, size_t len);

#if defined(WITH_WINPR_DEPRECATED)
	WINPR_DEPRECATED_VAR("Use ConvertUtf8ToWChar functions instead",
	                     WINPR_API int ConvertToUnicode(UINT CodePage, DWORD dwFlags,
	                                                    LPCSTR lpMultiByteStr, int cbMultiByte,
	                                                    LPWSTR* lpWideCharStr, int cchWideChar));

	WINPR_DEPRECATED_VAR("Use ConvertWCharToUtf8 functions instead",
	                     WINPR_API int ConvertFromUnicode(UINT CodePage, DWORD dwFlags,
	                                                      LPCWSTR lpWideCharStr, int cchWideChar,
	                                                      LPSTR* lpMultiByteStr, int cbMultiByte,
	                                                      LPCSTR lpDefaultChar,
	                                                      LPBOOL lpUsedDefaultChar));
#endif

	WINPR_API const WCHAR* ByteSwapUnicode(WCHAR* wstr, size_t length);

	WINPR_API size_t ConvertLineEndingToLF(char* str, size_t size);
	WINPR_API char* ConvertLineEndingToCRLF(const char* str, size_t* size);

	WINPR_API char* StrSep(char** stringp, const char* delim);

	WINPR_API INT64 GetLine(char** lineptr, size_t* size, FILE* stream);

#if !defined(WINPR_HAVE_STRNDUP)
	WINPR_ATTR_MALLOC(free, 1)
	WINPR_API char* strndup(const char* s, size_t n);
#endif

	/** @brief WCHAR version of \b strndup
	 *  creates a copy of a \b WCHAR string of \b n characters length. The copy will always be \b \0
	 * terminated
	 *
	 *  @param s The \b WCHAR string to copy
	 *  @param n The number of WCHAR to copy
	 *
	 *  @return An allocated copy of \b s, always \b \0 terminated
	 *  @since version 3.10.0
	 */
	WINPR_ATTR_MALLOC(free, 1)
	WINPR_API WCHAR* wcsndup(const WCHAR* s, size_t n);

#ifdef __cplusplus
}
#endif

WINPR_PRAGMA_DIAG_POP

#endif /* WINPR_CRT_STRING_H */
