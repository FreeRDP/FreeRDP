/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Smartcard API test program
 *
 * This simple program can be used to trigger calls for (almost) the
 * entire SCARD API.
 * Compile on windows, connect with FreeRDP via RDP with smartcard
 * redirection enabled and run this test program on the windows
 * machine.
 *
 * Copyright 2020 Armin Novak <armin.novak@thincast.com>
 * Copyright 2020 Thincast Technologies GmbH
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

#include <iostream>
#include <string>
#include <sstream>
#include <locale>
#include <codecvt>

#include <comdef.h>
#include <winscard.h>

static const WCHAR* listW[] = { nullptr, L"SCard$AllReaders\000", L"SCard$DefaultReaders\000",
	                            L"SCard$LocalReaders\000", L"SCard$SystemReaders\000" };
static const char* listA[] = { nullptr, "SCard$AllReaders\000", "SCard$DefaultReaders\000",
	                           "SCard$LocalReaders\000", "SCard$SystemReaders\000" };

static std::string scope2str(DWORD scope)
{
	switch (scope)
	{
		case SCARD_SCOPE_USER:
			return "SCARD_SCOPE_USER";
		case SCARD_SCOPE_TERMINAL:
			return "SCARD_SCOPE_TERMINAL";
		case SCARD_SCOPE_SYSTEM:
			return "SCARD_SCOPE_SYSTEM";
		default:
			return "UNKNOWN";
	}
}

static std::string err2str(LONG code)
{
	switch (code)
	{
		case ERROR_BROKEN_PIPE:
			return "ERROR_BROKEN_PIPE";
		case SCARD_E_BAD_SEEK:
			return "SCARD_E_BAD_SEEK";
		case SCARD_E_CANCELLED:
			return "SCARD_E_CANCELLED";
		case SCARD_E_CANT_DISPOSE:
			return "SCARD_E_CANT_DISPOSE";
		case SCARD_E_CARD_UNSUPPORTED:
			return "SCARD_E_CARD_UNSUPPORTED";
		case SCARD_E_CERTIFICATE_UNAVAILABLE:
			return "SCARD_E_CERTIFICATE_UNAVAILABLE";
		case SCARD_E_COMM_DATA_LOST:
			return "SCARD_E_COMM_DATA_LOST";
		case SCARD_E_DIR_NOT_FOUND:
			return "SCARD_E_DIR_NOT_FOUND";
		case SCARD_E_DUPLICATE_READER:
			return "SCARD_E_DUPLICATE_READER";
		case SCARD_E_FILE_NOT_FOUND:
			return "SCARD_E_FILE_NOT_FOUND";
		case SCARD_E_ICC_CREATEORDER:
			return "SCARD_E_ICC_CREATEORDER";
		case SCARD_E_ICC_INSTALLATION:
			return "SCARD_E_ICC_INSTALLATION";
		case SCARD_E_INSUFFICIENT_BUFFER:
			return "SCARD_E_INSUFFICIENT_BUFFER";
		case SCARD_E_INVALID_ATR:
			return "SCARD_E_INVALID_ATR";
		case SCARD_E_INVALID_CHV:
			return "SCARD_E_INVALID_CHV";
		case SCARD_E_INVALID_HANDLE:
			return "SCARD_E_INVALID_HANDLE";
		case SCARD_E_INVALID_PARAMETER:
			return "SCARD_E_INVALID_PARAMETER";
		case SCARD_E_INVALID_TARGET:
			return "SCARD_E_INVALID_TARGET";
		case SCARD_E_INVALID_VALUE:
			return "SCARD_E_INVALID_VALUE";
		case SCARD_E_NO_ACCESS:
			return "SCARD_E_NO_ACCESS";
		case SCARD_E_NO_DIR:
			return "SCARD_E_NO_DIR";
		case SCARD_E_NO_FILE:
			return "SCARD_E_NO_FILE";
		case SCARD_E_NO_KEY_CONTAINER:
			return "SCARD_E_NO_KEY_CONTAINER";
		case SCARD_E_NO_MEMORY:
			return "SCARD_E_NO_MEMORY";
		case SCARD_E_NO_PIN_CACHE:
			return "SCARD_E_NO_PIN_CACHE";
		case SCARD_E_NO_READERS_AVAILABLE:
			return "SCARD_E_NO_READERS_AVAILABLE";
		case SCARD_E_NO_SERVICE:
			return "SCARD_E_NO_SERVICE";
		case SCARD_E_NO_SMARTCARD:
			return "SCARD_E_NO_SMARTCARD";
		case SCARD_E_NO_SUCH_CERTIFICATE:
			return "SCARD_E_NO_SUCH_CERTIFICATE";
		case SCARD_E_NOT_READY:
			return "SCARD_E_NOT_READY";
		case SCARD_E_NOT_TRANSACTED:
			return "SCARD_E_NOT_TRANSACTED";
		case SCARD_E_PCI_TOO_SMALL:
			return "SCARD_E_PCI_TOO_SMALL";
		case SCARD_E_PIN_CACHE_EXPIRED:
			return "SCARD_E_PIN_CACHE_EXPIRED";
		case SCARD_E_PROTO_MISMATCH:
			return "SCARD_E_PROTO_MISMATCH";
		case SCARD_E_READ_ONLY_CARD:
			return "SCARD_E_READ_ONLY_CARD";
		case SCARD_E_READER_UNAVAILABLE:
			return "SCARD_E_READER_UNAVAILABLE";
		case SCARD_E_READER_UNSUPPORTED:
			return "SCARD_E_READER_UNSUPPORTED";
		case SCARD_E_SERVER_TOO_BUSY:
			return "SCARD_E_SERVER_TOO_BUSY";
		case SCARD_E_SERVICE_STOPPED:
			return "SCARD_E_SERVICE_STOPPED";
		case SCARD_E_SHARING_VIOLATION:
			return "SCARD_E_SHARING_VIOLATION";
		case SCARD_E_SYSTEM_CANCELLED:
			return "SCARD_E_SYSTEM_CANCELLED";
		case SCARD_E_TIMEOUT:
			return "SCARD_E_TIMEOUT";
		case SCARD_E_UNEXPECTED:
			return "SCARD_E_UNEXPECTED";
		case SCARD_E_UNKNOWN_CARD:
			return "SCARD_E_UNKNOWN_CARD";
		case SCARD_E_UNKNOWN_READER:
			return "SCARD_E_UNKNOWN_READER";
		case SCARD_E_UNKNOWN_RES_MNG:
			return "SCARD_E_UNKNOWN_RES_MNG";
		case SCARD_E_UNSUPPORTED_FEATURE:
			return "SCARD_E_UNSUPPORTED_FEATURE";
		case SCARD_E_WRITE_TOO_MANY:
			return "SCARD_E_WRITE_TOO_MANY";
		case SCARD_F_COMM_ERROR:
			return "SCARD_F_COMM_ERROR";
		case SCARD_F_INTERNAL_ERROR:
			return "SCARD_F_INTERNAL_ERROR";
		case SCARD_F_UNKNOWN_ERROR:
			return "SCARD_F_UNKNOWN_ERROR";
		case SCARD_F_WAITED_TOO_LONG:
			return "SCARD_F_WAITED_TOO_LONG";
		case SCARD_P_SHUTDOWN:
			return "SCARD_P_SHUTDOWN";
		case SCARD_S_SUCCESS:
			return "SCARD_S_SUCCESS";
		case SCARD_W_CANCELLED_BY_USER:
			return "SCARD_W_CANCELLED_BY_USER";
		case SCARD_W_CACHE_ITEM_NOT_FOUND:
			return "SCARD_W_CACHE_ITEM_NOT_FOUND";
		case SCARD_W_CACHE_ITEM_STALE:
			return "SCARD_W_CACHE_ITEM_STALE";
		case SCARD_W_CACHE_ITEM_TOO_BIG:
			return "SCARD_W_CACHE_ITEM_TOO_BIG";
		case SCARD_W_CARD_NOT_AUTHENTICATED:
			return "SCARD_W_CARD_NOT_AUTHENTICATED";
		case SCARD_W_CHV_BLOCKED:
			return "SCARD_W_CHV_BLOCKED";
		case SCARD_W_EOF:
			return "SCARD_W_EOF";
		case SCARD_W_REMOVED_CARD:
			return "SCARD_W_REMOVED_CARD";
		case SCARD_W_RESET_CARD:
			return "SCARD_W_RESET_CARD";
		case SCARD_W_SECURITY_VIOLATION:
			return "SCARD_W_SECURITY_VIOLATION";
		case SCARD_W_UNPOWERED_CARD:
			return "SCARD_W_UNPOWERED_CARD";
		case SCARD_W_UNRESPONSIVE_CARD:
			return "SCARD_W_UNRESPONSIVE_CARD";
		case SCARD_W_UNSUPPORTED_CARD:
			return "SCARD_W_UNSUPPORTED_CARD";
		case SCARD_W_WRONG_CHV:
			return "SCARD_W_WRONG_CHV";
		default:
			return "UNKNOWN";
	}
}

static std::wstring err2wstr(LONG code)
{
	auto str = err2str(code);
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
	return converter.from_bytes(str);
}

#if 0
static bool test_listreadergroups(SCARDCONTEXT hContext) {
    auto rc = SCardListReaderGroupsA(hContext, &groups, &foobar);
    rc = SCardListReaderGroupsW(hContext, &groups, &foobar);
}
#endif

static bool test_valid(SCARDCONTEXT context)
{
	auto rc = SCardIsValidContext(context);
	if (rc)
		std::cerr << "SCardIsValidContext failed with " << err2str(rc) << std::endl;
	return true;
}

static bool test_list_readers_a(SCARDCONTEXT context)
{
	for (auto cur : listA)
	{
		LPSTR mszReaders = nullptr;
		DWORD chReaders = SCARD_AUTOALLOCATE;
		auto rc = SCardListReadersA(context, cur, reinterpret_cast<LPSTR>(&mszReaders), &chReaders);
		if (!cur)
		{
			cur = "NULL";
		}
		if (rc != SCARD_S_SUCCESS)
		{
			std::cerr << "SCardListReadersA [" << cur << "] failed with " << err2str(rc)
			          << std::endl;
		}
		else
		{
			auto start = mszReaders;
			auto end = &mszReaders[chReaders];

			std::cout << "SCardListReadersA [" << cur << "] " << chReaders << " [";
			while (start < end)
			{
				std::cout << start << ", ";
				start += strnlen(start, chReaders) + 2;
			}
			std::cout << "]" << std::endl;
		}
		SCardFreeMemory(context, mszReaders);
	}

	return true;
}

static bool test_list_readers_w(SCARDCONTEXT context)
{
	for (auto cur : listW)
	{
		LPWSTR mszReaders = nullptr;
		DWORD chReaders = SCARD_AUTOALLOCATE;
		auto rc =
		    SCardListReadersW(context, cur, reinterpret_cast<LPWSTR>(&mszReaders), &chReaders);
		if (!cur)
		{
			cur = L"NULL";
		}
		if (rc != SCARD_S_SUCCESS)
		{
			std::wcerr << L"SCardListReadersW [" << cur << L"] failed with " << err2wstr(rc)
			           << std::endl;
		}
		else
		{
			auto start = mszReaders;
			auto end = &mszReaders[chReaders];

			std::wcout << L"SCardListReadersW [" << cur << L"] " << chReaders << L" [";
			while (start < end)
			{
				std::wcout << start << L", ";
				start += wcsnlen(start, chReaders) + 2;
			}
			std::wcout << L"]" << std::endl;
		}
		SCardFreeMemory(context, mszReaders);
	}

	return true;
}

static bool test_list_reader_groups_a(SCARDCONTEXT context)
{
	LPSTR mszReaders = nullptr;
	DWORD chReaders = SCARD_AUTOALLOCATE;
	auto rc = SCardListReaderGroupsA(context, reinterpret_cast<LPSTR>(&mszReaders), &chReaders);
	if (rc != SCARD_S_SUCCESS)
	{
		std::cerr << "SCardListReaderGroupsA failed with " << err2str(rc) << std::endl;
	}
	else
	{
		auto start = mszReaders;
		auto end = &mszReaders[chReaders];

		std::cout << "SCardListReaderGroupsA " << chReaders << " [";
		while (start < end)
		{
			std::cout << start << ", ";
			start += strnlen(start, chReaders) + 2;
		}
		std::cout << "]" << std::endl;
	}
	SCardFreeMemory(context, mszReaders);

	return true;
}

static bool test_list_reader_groups_w(SCARDCONTEXT context)
{
	LPWSTR mszReaders = nullptr;
	DWORD chReaders = SCARD_AUTOALLOCATE;
	auto rc = SCardListReaderGroupsW(context, reinterpret_cast<LPWSTR>(&mszReaders), &chReaders);
	if (rc != SCARD_S_SUCCESS)
	{
		std::wcerr << L"SCardListReaderGroupsW failed with " << err2wstr(rc) << std::endl;
	}
	else
	{
		auto start = mszReaders;
		auto end = &mszReaders[chReaders];

		std::wcout << L"SCardListReaderGroupsW " << chReaders << L" [";
		while (start < end)
		{
			std::wcout << start << L", ";
			start += wcsnlen(start, chReaders) + 2;
		}
		std::wcout << L"]" << std::endl;
	}
	SCardFreeMemory(context, mszReaders);

	return true;
}

static bool test_introduce_forget_reader_groups_a(SCARDCONTEXT context)
{
	LPSTR group = "somefancygroup";

	auto rc = SCardIntroduceReaderGroupA(context, group);
	if (rc != SCARD_S_SUCCESS)
	{
		std::cerr << "SCardIntroduceReaderGroupA failed with " << err2str(rc) << std::endl;
		return false;
	}
	else
	{
		rc = SCardForgetReaderGroupA(context, group);
		if (rc != SCARD_S_SUCCESS)
		{
			std::cerr << "SCardForgetReaderGroupA failed with " << err2str(rc) << std::endl;
			return false;
		}
		return true;
	}
}

static bool test_introduce_forget_reader_groups_w(SCARDCONTEXT context)
{
	LPWSTR group = L"somefancygroup";

	auto rc = SCardIntroduceReaderGroupW(context, group);
	if (rc != SCARD_S_SUCCESS)
	{
		std::cerr << "SCardIntroduceReaderGroupW failed with " << err2str(rc) << std::endl;
		return false;
	}
	else
	{
		rc = SCardForgetReaderGroupW(context, group);
		if (rc != SCARD_S_SUCCESS)
		{
			std::cerr << "SCardForgetReaderGroupW failed with " << err2str(rc) << std::endl;
			return false;
		}
		return true;
	}
}

static bool test_introduce_forget_reader_a(SCARDCONTEXT context)
{
	LPSTR reader = "somefancygroup";
	LPSTR device = "otherfancy";

	auto rc = SCardIntroduceReaderA(context, reader, device);
	if (rc != SCARD_S_SUCCESS)
	{
		std::cerr << "SCardIntroduceReaderA failed with " << err2str(rc) << std::endl;
		return false;
	}
	else
	{
		rc = SCardForgetReaderA(context, reader);
		if (rc != SCARD_S_SUCCESS)
		{
			std::cerr << "SCardForgetReaderA failed with " << err2str(rc) << std::endl;
			return false;
		}
		return true;
	}
}

static bool test_introduce_forget_reader_w(SCARDCONTEXT context)
{
	LPWSTR reader = L"somefancygroup";
	LPWSTR device = L"otherfancy";

	auto rc = SCardIntroduceReaderW(context, reader, device);
	if (rc != SCARD_S_SUCCESS)
	{
		std::cerr << "SCardIntroduceReaderW failed with " << err2str(rc) << std::endl;
		return false;
	}
	else
	{
		rc = SCardForgetReaderW(context, reader);
		if (rc != SCARD_S_SUCCESS)
		{
			std::cerr << "SCardForgetReaderW failed with " << err2str(rc) << std::endl;
			return false;
		}
		return true;
	}
}

static bool test_list_cards_a(SCARDCONTEXT context)
{
	DWORD chCards = SCARD_AUTOALLOCATE;
	LPSTR mszCards = nullptr;
	auto rc =
	    SCardListCardsA(context, nullptr, nullptr, 0, reinterpret_cast<LPSTR>(&mszCards), &chCards);
	if (rc != SCARD_S_SUCCESS)
	{
		std::cerr << "SCardListCardsA failed with " << err2str(rc) << std::endl;
	}
	else
	{
		auto start = mszCards;
		auto end = &mszCards[chCards];
		std::cout << "SCardListCardsA " << chCards << " [";
		while (start < end)
		{
			std::cout << start << ", ";
			start += strnlen(start, chCards) + 2;
		}
		std::cout << "]" << std::endl;
	}
	return true;
}

static bool test_list_cards_w(SCARDCONTEXT context)
{
	DWORD chCards = SCARD_AUTOALLOCATE;
	LPWSTR mszCards = nullptr;
	auto rc = SCardListCardsW(context, nullptr, nullptr, 0, reinterpret_cast<LPWSTR>(&mszCards),
	                          &chCards);
	if (rc != SCARD_S_SUCCESS)
	{
		std::cerr << "SCardListCardsW failed with " << err2str(rc) << std::endl;
	}
	else
	{
		auto start = mszCards;
		auto end = &mszCards[chCards];
		std::cout << "SCardListCardsW " << chCards << " [";
		while (start < end)
		{
			std::wcout << start << L", ";
			start += wcsnlen(start, chCards) + 2;
		}
		std::cout << "]" << std::endl;
	}
	return true;
}

static bool test_cache_a(SCARDCONTEXT context)
{
	BYTE wdata[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
	const DWORD wdatalen = sizeof(wdata);
	BYTE data[32] = {};
	DWORD datalen = sizeof(data);
	LPSTR name = "testdata";
	UUID id = {};

	auto rc = SCardWriteCacheA(context, &id, 0, name, wdata, wdatalen);
	if (rc != SCARD_S_SUCCESS)
	{
		std::cerr << "SCardWriteCacheA failed with " << err2str(rc) << std::endl;
		return false;
	}

	rc = SCardReadCacheA(context, &id, 0, name, data, &datalen);
	if (rc != SCARD_S_SUCCESS)
	{
		std::cerr << "SCardReadCacheA failed with " << err2str(rc) << std::endl;
		return false;
	}

	if (wdatalen != datalen)
	{
		std::cerr << "SCardWriteCacheA wrote " << wdatalen << "bytes, SCardReadCacheA read "
		          << datalen << "bytes" << std::endl;
		return false;
	}

	if (memcmp(wdata, data, wdatalen) != 0)
	{
		std::cerr << "SCardWriteCacheA / SCardReadCacheA data corruption detected" << std::endl;
		return false;
	}

	return true;
}

static bool test_cache_w(SCARDCONTEXT context)
{
	BYTE wdata[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
	const DWORD wdatalen = sizeof(wdata);
	BYTE data[32] = {};
	DWORD datalen = sizeof(data);
	LPWSTR name = L"testdata";
	UUID id = {};

	auto rc = SCardWriteCacheW(context, &id, 0, name, wdata, wdatalen);
	if (rc != SCARD_S_SUCCESS)
	{
		std::cerr << "SCardWriteCacheW failed with " << err2str(rc) << std::endl;
		return false;
	}

	rc = SCardReadCacheW(context, &id, 0, name, data, &datalen);
	if (rc != SCARD_S_SUCCESS)
	{
		std::cerr << "SCardReadCacheW failed with " << err2str(rc) << std::endl;
		return false;
	}

	if (wdatalen != datalen)
	{
		std::cerr << "SCardReadCacheW wrote " << wdatalen << "bytes, SCardReadCacheW read "
		          << datalen << "bytes" << std::endl;
		return false;
	}

	if (memcmp(wdata, data, wdatalen) != 0)
	{
		std::cerr << "SCardReadCacheW / SCardReadCacheW data corruption detected" << std::endl;
		return false;
	}

	return true;
}

static bool test_reader_icon_a(SCARDCONTEXT context)
{
	LPSTR name = "Gemalto PC Twin Reader 00 00\0\0";
	LPBYTE pbIcon = nullptr;
	DWORD cbIcon = SCARD_AUTOALLOCATE;

	auto rc = SCardGetReaderIconA(context, name, reinterpret_cast<LPBYTE>(&pbIcon), &cbIcon);
	SCardFreeMemory(context, pbIcon);
	if (rc != SCARD_S_SUCCESS)
	{
		std::cerr << "SCardGetReaderIconA failed with " << err2str(rc) << std::endl;
		return false;
	}

	return true;
}

static bool test_reader_icon_w(SCARDCONTEXT context)
{
	LPWSTR name = L"Gemalto PC Twin Reader 00 00\0\0";
	LPBYTE pbIcon = nullptr;
	DWORD cbIcon = SCARD_AUTOALLOCATE;

	auto rc = SCardGetReaderIconW(context, name, reinterpret_cast<LPBYTE>(&pbIcon), &cbIcon);
	SCardFreeMemory(context, pbIcon);
	if (rc != SCARD_S_SUCCESS)
	{
		std::cerr << "SCardGetReaderIconW failed with " << err2str(rc) << std::endl;
		return false;
	}

	return true;
}

static bool test_locate_cards_a(SCARDCONTEXT context)
{
	LPSTR name = "Gemalto PC Twin Reader 00 00\0\0";
	SCARD_READERSTATEA rgReaderStates[16] = {};

	auto rc = SCardLocateCardsA(context, name, rgReaderStates, ARRAYSIZE(rgReaderStates));
	if (rc != SCARD_S_SUCCESS)
	{
		std::cerr << "SCardLocateCardsA failed with " << err2str(rc) << std::endl;
		return false;
	}

	return true;
}

static bool test_locate_cards_w(SCARDCONTEXT context)
{
	LPWSTR name = L"Gemalto PC Twin Reader 00 00\0\0";
	SCARD_READERSTATEW rgReaderStates[16] = {};

	auto rc = SCardLocateCardsW(context, name, rgReaderStates, ARRAYSIZE(rgReaderStates));
	if (rc != SCARD_S_SUCCESS)
	{
		std::cerr << "SCardLocateCardsW failed with " << err2str(rc) << std::endl;
		return false;
	}

	return true;
}

static bool test_locate_cards_by_atr_a(SCARDCONTEXT context)
{
	SCARD_READERSTATEA rgReaderStates[16] = {};
	SCARD_ATRMASK rgAtrMasks[16] = {};

	auto rc = SCardLocateCardsByATRA(context, rgAtrMasks, ARRAYSIZE(rgAtrMasks), rgReaderStates,
	                                 ARRAYSIZE(rgReaderStates));
	if (rc != SCARD_S_SUCCESS)
	{
		std::cerr << "SCardLocateCardsByATRA failed with " << err2str(rc) << std::endl;
		return false;
	}

	return true;
}

static bool test_locate_cards_by_atr_w(SCARDCONTEXT context)
{
	SCARD_READERSTATEW rgReaderStates[16] = {};
	SCARD_ATRMASK rgAtrMasks[16] = {};

	auto rc = SCardLocateCardsByATRW(context, rgAtrMasks, ARRAYSIZE(rgAtrMasks), rgReaderStates,
	                                 ARRAYSIZE(rgReaderStates));
	if (rc != SCARD_S_SUCCESS)
	{
		std::cerr << "SCardLocateCardsByATRW failed with " << err2str(rc) << std::endl;
		return false;
	}

	return true;
}

static bool test_devicetype_id_a(SCARDCONTEXT context)
{
	BYTE data[32] = {};
	LPSTR name = "testdata";
	DWORD type;

	auto rc = SCardGetDeviceTypeIdA(context, name, &type);
	if (rc != SCARD_S_SUCCESS)
	{
		std::cerr << "SCardGetDeviceTypeIdA failed with " << err2str(rc) << std::endl;
		return false;
	}

	return true;
}

static bool test_devicetype_id_w(SCARDCONTEXT context)
{
	BYTE data[32] = {};
	LPWSTR name = L"testdata";
	DWORD type;

	auto rc = SCardGetDeviceTypeIdW(context, name, &type);
	if (rc != SCARD_S_SUCCESS)
	{
		std::cerr << "SCardGetDeviceTypeIdW failed with " << err2str(rc) << std::endl;
		return false;
	}

	return true;
}

static bool test_transmitcount(SCARDHANDLE handle)
{
	BYTE data[32] = {};
	LPWSTR name = L"testdata";
	DWORD count;

	auto rc = SCardGetTransmitCount(handle, &count);
	if (rc != SCARD_S_SUCCESS)
	{
		std::cerr << "SCardGetTransmitCount failed with " << err2str(rc) << std::endl;
		return false;
	}
	std::cout << "SCardGetTransmitCount() " << count << std::endl;
	return true;
}

static bool test_status_a(SCARDHANDLE handle)
{
	BYTE data[32] = {};
	LPWSTR name = L"testdata";
	DWORD count;
	/*
	    auto rc = SCardStatusA(handle, names, len, &state, &protocol, attr, &attrlen);
	    if (rc != SCARD_S_SUCCESS) {
	        std::cerr << "SCardGetDeviceTypeIdW failed with " << err2str(rc) << std::endl;
	        return false;
	    }
	*/
	return true;
}

static bool test_status_w(SCARDHANDLE handle)
{
	BYTE data[32] = {};
	LPWSTR name = L"testdata";
	DWORD count;
	/*
	auto rc = SCardStatusA(handle, names, len, &state, &protocol, attr, &attrlen);
	if (rc != SCARD_S_SUCCESS) {
	    std::cerr << "SCardGetDeviceTypeIdW failed with " << err2str(rc) << std::endl;
	    return false;
	}
*/
	return true;
}

static bool test_get_attrib(SCARDCONTEXT context, SCARDHANDLE handle)
{
	DWORD attrlen = SCARD_AUTOALLOCATE;
	LPBYTE attr = nullptr;

	auto rc =
	    SCardGetAttrib(handle, SCARD_ATTR_ATR_STRING, reinterpret_cast<LPBYTE>(&attr), &attrlen);
	if (rc != SCARD_S_SUCCESS)
	{
		std::cerr << "SCardGetAttrib failed with " << err2str(rc) << std::endl;
		return false;
	}
	std::cout << "SCardGetAttrib [" << attrlen << "]: " << (char*)attr << std::endl;
	SCardFreeMemory(context, attr);

	return true;
}

static bool test_set_attrib(SCARDCONTEXT context, SCARDHANDLE handle)
{
	DWORD attrlen = SCARD_AUTOALLOCATE;
	BYTE attr[] = "0123456789";

	auto rc = SCardSetAttrib(handle, SCARD_ATTR_SUPRESS_T1_IFS_REQUEST, attr, ARRAYSIZE(attr));
	if (rc != SCARD_S_SUCCESS)
	{
		std::cerr << "SCardSetAttrib failed with " << err2str(rc) << std::endl;
		return false;
	}
	std::cout << "SCardSetAttrib [" << attrlen << "]: " << (char*)attr << std::endl;
	SCardFreeMemory(context, attr);

	return true;
}

int main()
{
	std::cout << "Hello World!" << std::endl;
	try
	{
		auto scopes = { SCARD_SCOPE_USER, SCARD_SCOPE_SYSTEM };
		for (auto scope : scopes)
		{
			SCARDCONTEXT context;
			auto rc = SCardEstablishContext(scope, nullptr, nullptr, &context);
			if (rc != SCARD_S_SUCCESS)
			{
				std::cerr << "SCardEstablishContext [" << scope2str(scope) << "] failed with "
				          << err2str(rc) << std::endl;
			}
			else
			{
				std::cerr << "SCardEstablishContext [" << scope2str(scope) << "] success"
				          << std::endl;

				test_valid(context);

				test_list_reader_groups_a(context);
				test_list_reader_groups_w(context);

				test_list_readers_a(context);
				test_list_readers_w(context);

				test_list_cards_a(context);
				test_list_cards_w(context);

				test_introduce_forget_reader_groups_a(context);
				test_introduce_forget_reader_groups_w(context);

				test_introduce_forget_reader_a(context);
				test_introduce_forget_reader_w(context);

				// TODO: Introduce/Remove reader to group
				test_locate_cards_a(context);
				test_locate_cards_w(context);

				test_locate_cards_by_atr_a(context);
				test_locate_cards_by_atr_w(context);

				test_cache_a(context);
				test_cache_w(context);

				test_reader_icon_a(context);
				test_reader_icon_w(context);

				test_devicetype_id_a(context);
				test_devicetype_id_w(context);

				// TODO: statuschange
				// TODO: begin/end transaction
				// TODO: state
				// TODO: transmit
				// TODO: control

				{
					DWORD protocol;
					SCARDHANDLE handle = 0;
					LPSTR mszReaders;
					DWORD chReaders = SCARD_AUTOALLOCATE;

					LONG status = SCardListReadersA(
					    context, nullptr, reinterpret_cast<LPSTR>(&mszReaders), &chReaders);
					if (status == SCARD_S_SUCCESS)
						status = SCardConnectA(context, mszReaders, SCARD_SHARE_SHARED,
						                       SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1 |
						                           SCARD_PROTOCOL_Tx | SCARD_PROTOCOL_RAW,
						                       &handle, &protocol);
					SCardFreeMemory(context, mszReaders);
					if (status != SCARD_S_SUCCESS)
					{
						std::cerr << "SCardConnectA ["
						          << "] failed with " << err2str(status) << std::endl;
					}
					else
					{
						test_status_a(handle);
						test_status_w(handle);
						test_get_attrib(context, handle);
						test_set_attrib(context, handle);
						test_transmitcount(handle);

						status = SCardDisconnect(handle, 0);
						if (status)
						{
							std::cerr << "SCardDisconnect ["
							          << "] failed with " << err2str(status) << std::endl;
						}
					}
				}
				{
					DWORD protocol;
					SCARDHANDLE handle = 0;
					LPWSTR mszReaders;
					DWORD chReaders = SCARD_AUTOALLOCATE;

					LONG status = SCardListReadersW(
					    context, nullptr, reinterpret_cast<LPWSTR>(&mszReaders), &chReaders);
					if (status == SCARD_S_SUCCESS)
						status = SCardConnectW(context, mszReaders, SCARD_SHARE_SHARED,
						                       SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1 |
						                           SCARD_PROTOCOL_Tx | SCARD_PROTOCOL_RAW,
						                       &handle, &protocol);
					SCardFreeMemory(context, mszReaders);

					if (status != SCARD_S_SUCCESS)
					{
						std::cerr << "SCardConnectW ["
						          << "] failed with " << err2str(status) << std::endl;
					}
					else
					{
						test_status_a(handle);
						test_status_w(handle);
						test_get_attrib(context, handle);
						test_set_attrib(context, handle);
						test_transmitcount(handle);

						status = SCardDisconnect(handle, 0);
						if (status)
						{
							std::cerr << "SCardDisconnect ["
							          << "] failed with " << err2str(status) << std::endl;
						}
					}
				}

				rc = SCardReleaseContext(context);
				if (rc != SCARD_S_SUCCESS)
				{
					std::cerr << "SCardReleaseContext [" << scope2str(scope) << "] failed with "
					          << err2str(rc) << std::endl;
				}
			}
		}
	}
	catch (...)
	{
		std::cerr << "exception!!!!" << std::endl;
	}

	return 0;
}
