/**
 * WinPR: Windows Portable Runtime
 * Windows Terminal Services API
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef WINPR_WTSAPI_H
#define WINPR_WTSAPI_H

#include <winpr/winpr.h>
#include <winpr/wtypes.h>

#include <winpr/file.h>

#ifndef _WIN32

/**
 * Virtual Channel Protocol (Common)
 */

#define CHANNEL_CHUNK_LENGTH				1600

#define CHANNEL_PDU_LENGTH				(CHANNEL_CHUNK_LENGTH + sizeof(CHANNEL_PDU_HEADER))

#define CHANNEL_FLAG_FIRST				0x01
#define CHANNEL_FLAG_LAST				0x02
#define CHANNEL_FLAG_ONLY				(CHANNEL_FLAG_FIRST | CHANNEL_FLAG_LAST)
#define CHANNEL_FLAG_MIDDLE				0
#define CHANNEL_FLAG_FAIL				0x100

#define CHANNEL_FLAG_SHOW_PROTOCOL			0x10
#define CHANNEL_FLAG_SUSPEND				0x20
#define CHANNEL_FLAG_RESUME				0x40
#define CHANNEL_FLAG_SHADOW_PERSISTENT			0x80

#define CHANNEL_OPTION_INITIALIZED			0x80000000

#define CHANNEL_OPTION_ENCRYPT_RDP			0x40000000
#define CHANNEL_OPTION_ENCRYPT_SC			0x20000000
#define CHANNEL_OPTION_ENCRYPT_CS			0x10000000
#define CHANNEL_OPTION_PRI_HIGH				0x08000000
#define CHANNEL_OPTION_PRI_MED				0x04000000
#define CHANNEL_OPTION_PRI_LOW				0x02000000
#define CHANNEL_OPTION_COMPRESS_RDP			0x00800000
#define CHANNEL_OPTION_COMPRESS				0x00400000
#define CHANNEL_OPTION_SHOW_PROTOCOL			0x00200000
#define CHANNEL_OPTION_REMOTE_CONTROL_PERSISTENT	0x00100000

#define CHANNEL_MAX_COUNT				30
#define CHANNEL_NAME_LEN				7

typedef struct tagCHANNEL_DEF
{
	char name[CHANNEL_NAME_LEN + 1];
	ULONG options;
} CHANNEL_DEF;
typedef CHANNEL_DEF *PCHANNEL_DEF;
typedef PCHANNEL_DEF *PPCHANNEL_DEF;

typedef struct tagCHANNEL_PDU_HEADER
{
	UINT32 length;
	UINT32 flags;
} CHANNEL_PDU_HEADER, *PCHANNEL_PDU_HEADER;

/**
 * Virtual Channel Client API
 */

#ifdef _WIN32
#define VCAPITYPE __stdcall
#define VCEXPORT
#else
#define VCAPITYPE CALLBACK
#define VCEXPORT  __export
#endif

typedef VOID VCAPITYPE CHANNEL_INIT_EVENT_FN(LPVOID pInitHandle,
		UINT event, LPVOID pData, UINT dataLength);

typedef CHANNEL_INIT_EVENT_FN *PCHANNEL_INIT_EVENT_FN;

typedef VOID VCAPITYPE CHANNEL_INIT_EVENT_EX_FN(LPVOID lpUserParam,
		LPVOID pInitHandle, UINT event, LPVOID pData, UINT dataLength);

typedef CHANNEL_INIT_EVENT_EX_FN *PCHANNEL_INIT_EVENT_EX_FN;

#define CHANNEL_EVENT_INITIALIZED			0
#define CHANNEL_EVENT_CONNECTED				1
#define CHANNEL_EVENT_V1_CONNECTED			2
#define CHANNEL_EVENT_DISCONNECTED			3
#define CHANNEL_EVENT_TERMINATED			4
#define CHANNEL_EVENT_REMOTE_CONTROL_START		5
#define CHANNEL_EVENT_REMOTE_CONTROL_STOP		6
#define CHANNEL_EVENT_DATA_RECEIVED			10
#define CHANNEL_EVENT_WRITE_COMPLETE			11
#define CHANNEL_EVENT_WRITE_CANCELLED			12

typedef VOID VCAPITYPE CHANNEL_OPEN_EVENT_FN(DWORD openHandle, UINT event,
		LPVOID pData, UINT32 dataLength, UINT32 totalLength, UINT32 dataFlags);

typedef CHANNEL_OPEN_EVENT_FN *PCHANNEL_OPEN_EVENT_FN;

typedef VOID VCAPITYPE CHANNEL_OPEN_EVENT_EX_FN(LPVOID lpUserParam, DWORD openHandle, UINT event,
		LPVOID pData, UINT32 dataLength, UINT32 totalLength, UINT32 dataFlags);

typedef CHANNEL_OPEN_EVENT_EX_FN *PCHANNEL_OPEN_EVENT_EX_FN;

#define CHANNEL_RC_OK					0
#define CHANNEL_RC_ALREADY_INITIALIZED			1
#define CHANNEL_RC_NOT_INITIALIZED			2
#define CHANNEL_RC_ALREADY_CONNECTED			3
#define CHANNEL_RC_NOT_CONNECTED			4
#define CHANNEL_RC_TOO_MANY_CHANNELS			5
#define CHANNEL_RC_BAD_CHANNEL				6
#define CHANNEL_RC_BAD_CHANNEL_HANDLE			7
#define CHANNEL_RC_NO_BUFFER				8
#define CHANNEL_RC_BAD_INIT_HANDLE			9
#define CHANNEL_RC_NOT_OPEN				10
#define CHANNEL_RC_BAD_PROC				11
#define CHANNEL_RC_NO_MEMORY				12
#define CHANNEL_RC_UNKNOWN_CHANNEL_NAME			13
#define CHANNEL_RC_ALREADY_OPEN				14
#define CHANNEL_RC_NOT_IN_VIRTUALCHANNELENTRY		15
#define CHANNEL_RC_NULL_DATA				16
#define CHANNEL_RC_ZERO_LENGTH				17
#define CHANNEL_RC_INVALID_INSTANCE			18
#define CHANNEL_RC_UNSUPPORTED_VERSION			19
#define CHANNEL_RC_INITIALIZATION_ERROR			20

#define VIRTUAL_CHANNEL_VERSION_WIN2000			1

typedef UINT VCAPITYPE VIRTUALCHANNELINIT(LPVOID* ppInitHandle, PCHANNEL_DEF pChannel,
		INT channelCount, ULONG versionRequested, PCHANNEL_INIT_EVENT_FN pChannelInitEventProc);

typedef VIRTUALCHANNELINIT *PVIRTUALCHANNELINIT;

typedef UINT VCAPITYPE VIRTUALCHANNELINITEX(LPVOID lpUserParam, LPVOID pInitHandle, PCHANNEL_DEF pChannel,
		INT channelCount, ULONG versionRequested, PCHANNEL_INIT_EVENT_EX_FN pChannelInitEventProcEx);

typedef VIRTUALCHANNELINITEX *PVIRTUALCHANNELINITEX;

typedef UINT VCAPITYPE VIRTUALCHANNELOPEN(LPVOID pInitHandle, LPDWORD pOpenHandle,
		PCHAR pChannelName, PCHANNEL_OPEN_EVENT_FN pChannelOpenEventProc);

typedef VIRTUALCHANNELOPEN *PVIRTUALCHANNELOPEN;

typedef UINT VCAPITYPE VIRTUALCHANNELOPENEX(LPVOID pInitHandle, LPDWORD pOpenHandle,
		PCHAR pChannelName, PCHANNEL_OPEN_EVENT_EX_FN pChannelOpenEventProcEx);

typedef VIRTUALCHANNELOPENEX *PVIRTUALCHANNELOPENEX;

typedef UINT VCAPITYPE VIRTUALCHANNELCLOSE(DWORD openHandle);

typedef VIRTUALCHANNELCLOSE *PVIRTUALCHANNELCLOSE;

typedef UINT VCAPITYPE VIRTUALCHANNELCLOSEEX(LPVOID pInitHandle, DWORD openHandle);

typedef VIRTUALCHANNELCLOSEEX *PVIRTUALCHANNELCLOSEEX;

typedef UINT VCAPITYPE VIRTUALCHANNELWRITE(DWORD openHandle,
		LPVOID pData, ULONG dataLength, LPVOID pUserData);

typedef VIRTUALCHANNELWRITE *PVIRTUALCHANNELWRITE;

typedef UINT VCAPITYPE VIRTUALCHANNELWRITEEX(LPVOID pInitHandle, DWORD openHandle,
		LPVOID pData, ULONG dataLength, LPVOID pUserData);

typedef VIRTUALCHANNELWRITEEX *PVIRTUALCHANNELWRITEEX;

typedef struct tagCHANNEL_ENTRY_POINTS
{
	DWORD cbSize;
	DWORD protocolVersion;
	PVIRTUALCHANNELINIT pVirtualChannelInit;
	PVIRTUALCHANNELOPEN pVirtualChannelOpen;
	PVIRTUALCHANNELCLOSE pVirtualChannelClose;
	PVIRTUALCHANNELWRITE pVirtualChannelWrite;
} CHANNEL_ENTRY_POINTS, *PCHANNEL_ENTRY_POINTS;

typedef struct tagCHANNEL_ENTRY_POINTS_EX
{
	DWORD cbSize;
	DWORD protocolVersion;
	PVIRTUALCHANNELINITEX pVirtualChannelInitEx;
	PVIRTUALCHANNELOPENEX pVirtualChannelOpenEx;
	PVIRTUALCHANNELCLOSEEX pVirtualChannelCloseEx;
	PVIRTUALCHANNELWRITEEX pVirtualChannelWriteEx;
} CHANNEL_ENTRY_POINTS_EX, *PCHANNEL_ENTRY_POINTS_EX;

typedef BOOL VCAPITYPE VIRTUALCHANNELENTRY(PCHANNEL_ENTRY_POINTS pEntryPoints);

typedef VIRTUALCHANNELENTRY *PVIRTUALCHANNELENTRY;

typedef BOOL VCAPITYPE VIRTUALCHANNELENTRYEX(PCHANNEL_ENTRY_POINTS_EX pEntryPointsEx, PVOID pInitHandle);

typedef VIRTUALCHANNELENTRYEX *PVIRTUALCHANNELENTRYEX;

typedef HRESULT (VCAPITYPE *PFNVCAPIGETINSTANCE)(REFIID refiid, PULONG pNumObjs, PVOID* ppObjArray);

/**
 * Windows Terminal Services API (WtsApi)
 */

#define WTS_CURRENT_SERVER			((HANDLE)NULL)
#define WTS_CURRENT_SERVER_HANDLE		((HANDLE)NULL)
#define WTS_CURRENT_SERVER_NAME			(NULL)

#define WTS_CURRENT_SESSION			((DWORD)-1)

#define WTS_ANY_SESSION				((DWORD)-2)

#define IDTIMEOUT				32000
#define IDASYNC					32001

#define USERNAME_LENGTH				20
#define CLIENTNAME_LENGTH			20
#define CLIENTADDRESS_LENGTH			30

#define WTS_WSD_LOGOFF				0x00000001
#define WTS_WSD_SHUTDOWN			0x00000002
#define WTS_WSD_REBOOT				0x00000004
#define WTS_WSD_POWEROFF			0x00000008
#define WTS_WSD_FASTREBOOT			0x00000010

#define MAX_ELAPSED_TIME_LENGTH			15
#define MAX_DATE_TIME_LENGTH			56
#define WINSTATIONNAME_LENGTH			32
#define DOMAIN_LENGTH				17

#define WTS_DRIVE_LENGTH			3
#define WTS_LISTENER_NAME_LENGTH		32
#define WTS_COMMENT_LENGTH			60

#define WTS_LISTENER_CREATE			0x00000001
#define WTS_LISTENER_UPDATE			0x00000010

#define WTS_SECURITY_QUERY_INFORMATION		0x00000001
#define WTS_SECURITY_SET_INFORMATION		0x00000002
#define WTS_SECURITY_RESET			0x00000004
#define WTS_SECURITY_VIRTUAL_CHANNELS		0x00000008
#define WTS_SECURITY_REMOTE_CONTROL		0x00000010
#define WTS_SECURITY_LOGON			0x00000020
#define WTS_SECURITY_LOGOFF			0x00000040
#define WTS_SECURITY_MESSAGE			0x00000080
#define WTS_SECURITY_CONNECT			0x00000100
#define WTS_SECURITY_DISCONNECT			0x00000200

#define WTS_SECURITY_GUEST_ACCESS		(WTS_SECURITY_LOGON)

#define WTS_SECURITY_CURRENT_GUEST_ACCESS	(WTS_SECURITY_VIRTUAL_CHANNELS | WTS_SECURITY_LOGOFF)

#define WTS_SECURITY_USER_ACCESS		(WTS_SECURITY_CURRENT_GUEST_ACCESS | WTS_SECURITY_QUERY_INFORMATION | WTS_SECURITY_CONNECT)

#define WTS_SECURITY_CURRENT_USER_ACCESS	(WTS_SECURITY_SET_INFORMATION | WTS_SECURITY_RESET \
						 WTS_SECURITY_VIRTUAL_CHANNELS | WTS_SECURITY_LOGOFF \
						 WTS_SECURITY_DISCONNECT)

#define WTS_SECURITY_ALL_ACCESS			(STANDARD_RIGHTS_REQUIRED | WTS_SECURITY_QUERY_INFORMATION | \
						 WTS_SECURITY_SET_INFORMATION | WTS_SECURITY_RESET | \
						 WTS_SECURITY_VIRTUAL_CHANNELS | WTS_SECURITY_REMOTE_CONTROL | \
						 WTS_SECURITY_LOGON | WTS_SECURITY_MESSAGE | \
						 WTS_SECURITY_CONNECT | WTS_SECURITY_DISCONNECT)

typedef enum _WTS_CONNECTSTATE_CLASS
{
	WTSActive,
	WTSConnected,
	WTSConnectQuery,
	WTSShadow,
	WTSDisconnected,
	WTSIdle,
	WTSListen,
	WTSReset,
	WTSDown,
	WTSInit,
} WTS_CONNECTSTATE_CLASS;

typedef struct _WTS_SERVER_INFOW
{
	LPWSTR pServerName;
} WTS_SERVER_INFOW, *PWTS_SERVER_INFOW;

typedef struct _WTS_SERVER_INFOA
{
	LPSTR pServerName;
} WTS_SERVER_INFOA, *PWTS_SERVER_INFOA;

typedef struct _WTS_SESSION_INFOW
{
	DWORD SessionId;
	LPWSTR pWinStationName;
	WTS_CONNECTSTATE_CLASS State;
} WTS_SESSION_INFOW, *PWTS_SESSION_INFOW;

typedef struct _WTS_SESSION_INFOA
{
	DWORD SessionId;
	LPSTR pWinStationName;
	WTS_CONNECTSTATE_CLASS State;
} WTS_SESSION_INFOA, *PWTS_SESSION_INFOA;

typedef struct _WTS_SESSION_INFO_1W
{
	DWORD ExecEnvId;
	WTS_CONNECTSTATE_CLASS State;
	DWORD SessionId;
	LPWSTR pSessionName;
	LPWSTR pHostName;
	LPWSTR pUserName;
	LPWSTR pDomainName;
	LPWSTR pFarmName;
} WTS_SESSION_INFO_1W, *PWTS_SESSION_INFO_1W;

typedef struct _WTS_SESSION_INFO_1A
{
	DWORD ExecEnvId;
	WTS_CONNECTSTATE_CLASS State;
	DWORD SessionId;
	LPSTR pSessionName;
	LPSTR pHostName;
	LPSTR pUserName;
	LPSTR pDomainName;
	LPSTR pFarmName;
} WTS_SESSION_INFO_1A, *PWTS_SESSION_INFO_1A;

typedef struct _WTS_PROCESS_INFOW
{
	DWORD SessionId;
	DWORD ProcessId;
	LPWSTR pProcessName;
	PSID pUserSid;
} WTS_PROCESS_INFOW, *PWTS_PROCESS_INFOW;

typedef struct _WTS_PROCESS_INFOA
{
	DWORD SessionId;
	DWORD ProcessId;
	LPSTR pProcessName;
	PSID pUserSid;
} WTS_PROCESS_INFOA, *PWTS_PROCESS_INFOA;

#define WTS_PROTOCOL_TYPE_CONSOLE		0
#define WTS_PROTOCOL_TYPE_ICA			1
#define WTS_PROTOCOL_TYPE_RDP			2

typedef enum _WTS_INFO_CLASS
{
	WTSInitialProgram,
	WTSApplicationName,
	WTSWorkingDirectory,
	WTSOEMId,
	WTSSessionId,
	WTSUserName,
	WTSWinStationName,
	WTSDomainName,
	WTSConnectState,
	WTSClientBuildNumber,
	WTSClientName,
	WTSClientDirectory,
	WTSClientProductId,
	WTSClientHardwareId,
	WTSClientAddress,
	WTSClientDisplay,
	WTSClientProtocolType,
	WTSIdleTime,
	WTSLogonTime,
	WTSIncomingBytes,
	WTSOutgoingBytes,
	WTSIncomingFrames,
	WTSOutgoingFrames,
	WTSClientInfo,
	WTSSessionInfo,
	WTSSessionInfoEx,
	WTSConfigInfo,
	WTSValidationInfo,
	WTSSessionAddressV4,
	WTSIsRemoteSession
} WTS_INFO_CLASS;

typedef struct  _WTSCONFIGINFOW
{
	ULONG version;
	ULONG fConnectClientDrivesAtLogon;
	ULONG fConnectPrinterAtLogon;
	ULONG fDisablePrinterRedirection;
	ULONG fDisableDefaultMainClientPrinter;
	ULONG ShadowSettings;
	WCHAR LogonUserName[USERNAME_LENGTH + 1];
	WCHAR LogonDomain[DOMAIN_LENGTH + 1];
	WCHAR WorkDirectory[MAX_PATH + 1];
	WCHAR InitialProgram[MAX_PATH + 1];
	WCHAR ApplicationName[MAX_PATH + 1];
} WTSCONFIGINFOW, *PWTSCONFIGINFOW;

typedef struct  _WTSCONFIGINFOA
{
	ULONG version;
	ULONG fConnectClientDrivesAtLogon;
	ULONG fConnectPrinterAtLogon;
	ULONG fDisablePrinterRedirection;
	ULONG fDisableDefaultMainClientPrinter;
	ULONG ShadowSettings;
	CHAR LogonUserName[USERNAME_LENGTH + 1];
	CHAR LogonDomain[DOMAIN_LENGTH + 1];
	CHAR WorkDirectory[MAX_PATH + 1];
	CHAR InitialProgram[MAX_PATH + 1];
	CHAR ApplicationName[MAX_PATH + 1];
} WTSCONFIGINFOA, *PWTSCONFIGINFOA;

typedef struct _WTSINFOW
{
	WTS_CONNECTSTATE_CLASS State;
	DWORD SessionId;
	DWORD IncomingBytes;
	DWORD OutgoingBytes;
	DWORD IncomingFrames;
	DWORD OutgoingFrames;
	DWORD IncomingCompressedBytes;
	DWORD OutgoingCompressedBytes;
	WCHAR WinStationName[WINSTATIONNAME_LENGTH];
	WCHAR Domain[DOMAIN_LENGTH];
	WCHAR UserName[USERNAME_LENGTH+1];
	LARGE_INTEGER ConnectTime;
	LARGE_INTEGER DisconnectTime;
	LARGE_INTEGER LastInputTime;
	LARGE_INTEGER LogonTime;
	LARGE_INTEGER CurrentTime;
} WTSINFOW, *PWTSINFOW;

typedef struct _WTSINFOA
{
	WTS_CONNECTSTATE_CLASS State;
	DWORD SessionId;
	DWORD IncomingBytes;
	DWORD OutgoingBytes;
	DWORD IncomingFrames;
	DWORD OutgoingFrames;
	DWORD IncomingCompressedBytes;
	DWORD OutgoingCompressedBy;
	CHAR WinStationName[WINSTATIONNAME_LENGTH];
	CHAR Domain[DOMAIN_LENGTH];
	CHAR UserName[USERNAME_LENGTH+1];
	LARGE_INTEGER ConnectTime;
	LARGE_INTEGER DisconnectTime;
	LARGE_INTEGER LastInputTime;
	LARGE_INTEGER LogonTime;
	LARGE_INTEGER CurrentTime;
} WTSINFOA, *PWTSINFOA;

#define WTS_SESSIONSTATE_UNKNOWN		0xFFFFFFFF
#define WTS_SESSIONSTATE_LOCK			0x00000000
#define WTS_SESSIONSTATE_UNLOCK			0x00000001

typedef struct _WTSINFOEX_LEVEL1_W
{
	ULONG SessionId;
	WTS_CONNECTSTATE_CLASS SessionState;
	LONG SessionFlags;
	WCHAR WinStationName[WINSTATIONNAME_LENGTH + 1];
	WCHAR UserName[USERNAME_LENGTH + 1];
	WCHAR DomainName[DOMAIN_LENGTH + 1];
	LARGE_INTEGER LogonTime;
	LARGE_INTEGER ConnectTime;
	LARGE_INTEGER DisconnectTime;
	LARGE_INTEGER LastInputTime;
	LARGE_INTEGER CurrentTime;
	DWORD IncomingBytes;
	DWORD OutgoingBytes;
	DWORD IncomingFrames;
	DWORD OutgoingFrames;
	DWORD IncomingCompressedBytes;
	DWORD OutgoingCompressedBytes;
} WTSINFOEX_LEVEL1_W, *PWTSINFOEX_LEVEL1_W;

typedef struct _WTSINFOEX_LEVEL1_A
{
	ULONG SessionId;
	WTS_CONNECTSTATE_CLASS SessionState;
	LONG SessionFlags;
	CHAR WinStationName[WINSTATIONNAME_LENGTH + 1];
	CHAR UserName[USERNAME_LENGTH + 1];
	CHAR DomainName[DOMAIN_LENGTH + 1];
	LARGE_INTEGER LogonTime;
	LARGE_INTEGER ConnectTime;
	LARGE_INTEGER DisconnectTime;
	LARGE_INTEGER LastInputTime;
	LARGE_INTEGER CurrentTime;
	DWORD IncomingBytes;
	DWORD OutgoingBytes;
	DWORD IncomingFrames;
	DWORD OutgoingFrames;
	DWORD IncomingCompressedBytes;
	DWORD OutgoingCompressedBytes;
} WTSINFOEX_LEVEL1_A, *PWTSINFOEX_LEVEL1_A;

typedef union _WTSINFOEX_LEVEL_W
{
	WTSINFOEX_LEVEL1_W WTSInfoExLevel1;
} WTSINFOEX_LEVEL_W, *PWTSINFOEX_LEVEL_W;

typedef union _WTSINFOEX_LEVEL_A
{
	WTSINFOEX_LEVEL1_A WTSInfoExLevel1;
} WTSINFOEX_LEVEL_A, *PWTSINFOEX_LEVEL_A;

typedef struct _WTSINFOEXW
{
	DWORD Level;
	WTSINFOEX_LEVEL_W Data;
} WTSINFOEXW, *PWTSINFOEXW;

typedef struct _WTSINFOEXA
{
	DWORD Level;
	WTSINFOEX_LEVEL_A Data;
} WTSINFOEXA, *PWTSINFOEXA;

typedef struct _WTSCLIENTW
{
	WCHAR ClientName[CLIENTNAME_LENGTH + 1];
	WCHAR Domain[DOMAIN_LENGTH + 1];
	WCHAR UserName[USERNAME_LENGTH + 1];
	WCHAR WorkDirectory[MAX_PATH + 1];
	WCHAR InitialProgram[MAX_PATH + 1];
	BYTE EncryptionLevel;
	ULONG ClientAddressFamily;
	USHORT ClientAddress[CLIENTADDRESS_LENGTH + 1];
	USHORT HRes;
	USHORT VRes;
	USHORT ColorDepth;
	WCHAR ClientDirectory[MAX_PATH + 1];
	ULONG ClientBuildNumber;
	ULONG ClientHardwareId;
	USHORT ClientProductId;
	USHORT OutBufCountHost;
	USHORT OutBufCountClient;
	USHORT OutBufLength;
	WCHAR DeviceId[MAX_PATH + 1];
} WTSCLIENTW, *PWTSCLIENTW;

typedef struct _WTSCLIENTA
{
	CHAR ClientName[CLIENTNAME_LENGTH + 1];
	CHAR Domain[DOMAIN_LENGTH + 1];
	CHAR UserName[USERNAME_LENGTH + 1];
	CHAR WorkDirectory[MAX_PATH + 1];
	CHAR InitialProgram[MAX_PATH + 1];
	BYTE EncryptionLevel;
	ULONG ClientAddressFamily;
	USHORT ClientAddress[CLIENTADDRESS_LENGTH + 1];
	USHORT HRes;
	USHORT VRes;
	USHORT ColorDepth;
	CHAR ClientDirectory[MAX_PATH + 1];
	ULONG ClientBuildNumber;
	ULONG ClientHardwareId;
	USHORT ClientProductId;
	USHORT OutBufCountHost;
	USHORT OutBufCountClient;
	USHORT OutBufLength;
	CHAR DeviceId[MAX_PATH + 1];
} WTSCLIENTA, *PWTSCLIENTA;

#define PRODUCTINFO_COMPANYNAME_LENGTH			256
#define PRODUCTINFO_PRODUCTID_LENGTH			4

typedef struct _WTS_PRODUCT_INFOA
{
	CHAR CompanyName[PRODUCTINFO_COMPANYNAME_LENGTH];
	CHAR ProductID[PRODUCTINFO_PRODUCTID_LENGTH];
} PRODUCT_INFOA;

typedef struct _WTS_PRODUCT_INFOW
{
	WCHAR CompanyName[PRODUCTINFO_COMPANYNAME_LENGTH];
	WCHAR ProductID[PRODUCTINFO_PRODUCTID_LENGTH];
} PRODUCT_INFOW;

#define VALIDATIONINFORMATION_LICENSE_LENGTH		16384
#define VALIDATIONINFORMATION_HARDWAREID_LENGTH		20

typedef struct _WTS_VALIDATION_INFORMATIONA
{
	PRODUCT_INFOA ProductInfo;
	BYTE License[VALIDATIONINFORMATION_LICENSE_LENGTH];
	DWORD LicenseLength;
	BYTE HardwareID[VALIDATIONINFORMATION_HARDWAREID_LENGTH];
	DWORD HardwareIDLength;
} WTS_VALIDATION_INFORMATIONA, *PWTS_VALIDATION_INFORMATIONA;

typedef struct _WTS_VALIDATION_INFORMATIONW
{
	PRODUCT_INFOW ProductInfo;
	BYTE License[VALIDATIONINFORMATION_LICENSE_LENGTH];
	DWORD LicenseLength;
	BYTE HardwareID[VALIDATIONINFORMATION_HARDWAREID_LENGTH];
	DWORD HardwareIDLength;
} WTS_VALIDATION_INFORMATIONW, *PWTS_VALIDATION_INFORMATIONW;

typedef struct _WTS_CLIENT_ADDRESS
{
	DWORD AddressFamily;
	BYTE Address[20];
} WTS_CLIENT_ADDRESS, *PWTS_CLIENT_ADDRESS;

typedef struct _WTS_CLIENT_DISPLAY
{
	DWORD HorizontalResolution;
	DWORD VerticalResolution;
	DWORD ColorDepth;
} WTS_CLIENT_DISPLAY, *PWTS_CLIENT_DISPLAY;

typedef enum _WTS_CONFIG_CLASS
{
	WTSUserConfigInitialProgram,
	WTSUserConfigWorkingDirectory,
	WTSUserConfigfInheritInitialProgram,
	WTSUserConfigfAllowLogonTerminalServer,
	WTSUserConfigTimeoutSettingsConnections,
	WTSUserConfigTimeoutSettingsDisconnections,
	WTSUserConfigTimeoutSettingsIdle,
	WTSUserConfigfDeviceClientDrives,
	WTSUserConfigfDeviceClientPrinters,
	WTSUserConfigfDeviceClientDefaultPrinter,
	WTSUserConfigBrokenTimeoutSettings,
	WTSUserConfigReconnectSettings,
	WTSUserConfigModemCallbackSettings,
	WTSUserConfigModemCallbackPhoneNumber,
	WTSUserConfigShadowingSettings,
	WTSUserConfigTerminalServerProfilePath,
	WTSUserConfigTerminalServerHomeDir,
	WTSUserConfigTerminalServerHomeDirDrive,
	WTSUserConfigfTerminalServerRemoteHomeDir,
	WTSUserConfigUser,
} WTS_CONFIG_CLASS;

typedef enum _WTS_CONFIG_SOURCE
{
	WTSUserConfigSourceSAM
} WTS_CONFIG_SOURCE;

typedef struct _WTSUSERCONFIGA
{
	DWORD Source;
	DWORD InheritInitialProgram;
	DWORD AllowLogonTerminalServer;
	DWORD TimeoutSettingsConnections;
	DWORD TimeoutSettingsDisconnections;
	DWORD TimeoutSettingsIdle;
	DWORD DeviceClientDrives;
	DWORD DeviceClientPrinters;
	DWORD ClientDefaultPrinter;
	DWORD BrokenTimeoutSettings;
	DWORD ReconnectSettings;
	DWORD ShadowingSettings;
	DWORD TerminalServerRemoteHomeDir;
	CHAR InitialProgram[MAX_PATH + 1];
	CHAR WorkDirectory[MAX_PATH + 1];
	CHAR TerminalServerProfilePath[MAX_PATH + 1];
	CHAR TerminalServerHomeDir[MAX_PATH + 1];
	CHAR TerminalServerHomeDirDrive[WTS_DRIVE_LENGTH + 1];
} WTSUSERCONFIGA, *PWTSUSERCONFIGA;

typedef struct _WTSUSERCONFIGW
{
	DWORD Source;
	DWORD InheritInitialProgram;
	DWORD AllowLogonTerminalServer;
	DWORD TimeoutSettingsConnections;
	DWORD TimeoutSettingsDisconnections;
	DWORD TimeoutSettingsIdle;
	DWORD DeviceClientDrives;
	DWORD DeviceClientPrinters;
	DWORD ClientDefaultPrinter;
	DWORD BrokenTimeoutSettings;
	DWORD ReconnectSettings;
	DWORD ShadowingSettings;
	DWORD TerminalServerRemoteHomeDir;
	WCHAR InitialProgram[MAX_PATH + 1];
	WCHAR WorkDirectory[MAX_PATH + 1];
	WCHAR TerminalServerProfilePath[MAX_PATH + 1];
	WCHAR TerminalServerHomeDir[MAX_PATH + 1];
	WCHAR TerminalServerHomeDirDrive[WTS_DRIVE_LENGTH + 1];
} WTSUSERCONFIGW, *PWTSUSERCONFIGW;

#define WTS_EVENT_NONE					0x00000000
#define WTS_EVENT_CREATE				0x00000001
#define WTS_EVENT_DELETE				0x00000002
#define WTS_EVENT_RENAME				0x00000004
#define WTS_EVENT_CONNECT				0x00000008
#define WTS_EVENT_DISCONNECT				0x00000010
#define WTS_EVENT_LOGON					0x00000020
#define WTS_EVENT_LOGOFF				0x00000040
#define WTS_EVENT_STATECHANGE				0x00000080
#define WTS_EVENT_LICENSE				0x00000100
#define WTS_EVENT_ALL					0x7fffffff
#define WTS_EVENT_FLUSH					0x80000000

#define REMOTECONTROL_KBDSHIFT_HOTKEY			0x1
#define REMOTECONTROL_KBDCTRL_HOTKEY			0x2
#define REMOTECONTROL_KBDALT_HOTKEY			0x4

typedef enum _WTS_VIRTUAL_CLASS
{
	WTSVirtualClientData,
	WTSVirtualFileHandle,
	WTSVirtualEventHandle, /* Extended */
	WTSVirtualChannelReady /* Extended */
} WTS_VIRTUAL_CLASS;

typedef struct _WTS_SESSION_ADDRESS
{
	DWORD AddressFamily;
	BYTE Address[20];
} WTS_SESSION_ADDRESS, *PWTS_SESSION_ADDRESS;

#define WTS_CHANNEL_OPTION_DYNAMIC			0x00000001
#define WTS_CHANNEL_OPTION_DYNAMIC_PRI_LOW		0x00000000
#define WTS_CHANNEL_OPTION_DYNAMIC_PRI_MED		0x00000002
#define WTS_CHANNEL_OPTION_DYNAMIC_PRI_HIGH		0x00000004
#define WTS_CHANNEL_OPTION_DYNAMIC_PRI_REAL		0x00000006
#define WTS_CHANNEL_OPTION_DYNAMIC_NO_COMPRESS		0x00000008

#define NOTIFY_FOR_ALL_SESSIONS				1
#define NOTIFY_FOR_THIS_SESSION				0

#define WTS_PROCESS_INFO_LEVEL_0			0
#define WTS_PROCESS_INFO_LEVEL_1			1

typedef struct _WTS_PROCESS_INFO_EXW
{
	DWORD SessionId;
	DWORD ProcessId;
	LPWSTR pProcessName;
	PSID pUserSid;
	DWORD NumberOfThreads;
	DWORD HandleCount;
	DWORD PagefileUsage;
	DWORD PeakPagefileUsage;
	DWORD WorkingSetSize;
	DWORD PeakWorkingSetSize;
	LARGE_INTEGER UserTime;
	LARGE_INTEGER KernelTime;
} WTS_PROCESS_INFO_EXW, *PWTS_PROCESS_INFO_EXW;

typedef struct _WTS_PROCESS_INFO_EXA
{
	DWORD SessionId;
	DWORD ProcessId;
	LPSTR pProcessName;
	PSID pUserSid;
	DWORD NumberOfThreads;
	DWORD HandleCount;
	DWORD PagefileUsage;
	DWORD PeakPagefileUsage;
	DWORD WorkingSetSize;
	DWORD PeakWorkingSetSize;
	LARGE_INTEGER UserTime;
	LARGE_INTEGER KernelTime;
} WTS_PROCESS_INFO_EXA, *PWTS_PROCESS_INFO_EXA;

typedef enum _WTS_TYPE_CLASS
{
	WTSTypeProcessInfoLevel0,
	WTSTypeProcessInfoLevel1,
	WTSTypeSessionInfoLevel1,
} WTS_TYPE_CLASS;

typedef WCHAR WTSLISTENERNAMEW[WTS_LISTENER_NAME_LENGTH + 1];
typedef WTSLISTENERNAMEW *PWTSLISTENERNAMEW;
typedef CHAR WTSLISTENERNAMEA[WTS_LISTENER_NAME_LENGTH + 1];
typedef WTSLISTENERNAMEA *PWTSLISTENERNAMEA;

typedef struct _WTSLISTENERCONFIGW
{
	ULONG version;
	ULONG fEnableListener;
	ULONG MaxConnectionCount;
	ULONG fPromptForPassword;
	ULONG fInheritColorDepth;
	ULONG ColorDepth;
	ULONG fInheritBrokenTimeoutSettings;
	ULONG BrokenTimeoutSettings;
	ULONG fDisablePrinterRedirection;
	ULONG fDisableDriveRedirection;
	ULONG fDisableComPortRedirection;
	ULONG fDisableLPTPortRedirection;
	ULONG fDisableClipboardRedirection;
	ULONG fDisableAudioRedirection;
	ULONG fDisablePNPRedirection;
	ULONG fDisableDefaultMainClientPrinter;
	ULONG LanAdapter;
	ULONG PortNumber;
	ULONG fInheritShadowSettings;
	ULONG ShadowSettings;
	ULONG TimeoutSettingsConnection;
	ULONG TimeoutSettingsDisconnection;
	ULONG TimeoutSettingsIdle;
	ULONG SecurityLayer;
	ULONG MinEncryptionLevel;
	ULONG UserAuthentication;
	WCHAR Comment[WTS_COMMENT_LENGTH + 1];
	WCHAR LogonUserName[USERNAME_LENGTH + 1];
	WCHAR LogonDomain[DOMAIN_LENGTH + 1];
	WCHAR WorkDirectory[MAX_PATH + 1];
	WCHAR InitialProgram[MAX_PATH + 1];
} WTSLISTENERCONFIGW, *PWTSLISTENERCONFIGW;

typedef struct _WTSLISTENERCONFIGA
{
	ULONG version;
	ULONG fEnableListener;
	ULONG MaxConnectionCount;
	ULONG fPromptForPassword;
	ULONG fInheritColorDepth;
	ULONG ColorDepth;
	ULONG fInheritBrokenTimeoutSettings;
	ULONG BrokenTimeoutSettings;
	ULONG fDisablePrinterRedirection;
	ULONG fDisableDriveRedirection;
	ULONG fDisableComPortRedirection;
	ULONG fDisableLPTPortRedirection;
	ULONG fDisableClipboardRedirection;
	ULONG fDisableAudioRedirection;
	ULONG fDisablePNPRedirection;
	ULONG fDisableDefaultMainClientPrinter;
	ULONG LanAdapter;
	ULONG PortNumber;
	ULONG fInheritShadowSettings;
	ULONG ShadowSettings;
	ULONG TimeoutSettingsConnection;
	ULONG TimeoutSettingsDisconnection;
	ULONG TimeoutSettingsIdle;
	ULONG SecurityLayer;
	ULONG MinEncryptionLevel;
	ULONG UserAuthentication;
	CHAR Comment[WTS_COMMENT_LENGTH + 1];
	CHAR LogonUserName[USERNAME_LENGTH + 1];
	CHAR LogonDomain[DOMAIN_LENGTH + 1];
	CHAR WorkDirectory[MAX_PATH + 1];
	CHAR InitialProgram[MAX_PATH + 1];
} WTSLISTENERCONFIGA, *PWTSLISTENERCONFIGA;

#ifdef UNICODE
#define WTS_SERVER_INFO			WTS_SERVER_INFOW
#define PWTS_SERVER_INFO		PWTS_SERVER_INFOW
#define WTS_SESSION_INFO		WTS_SESSION_INFOW
#define PWTS_SESSION_INFO		PWTS_SESSION_INFOW
#define WTS_SESSION_INFO_1		WTS_SESSION_INFO_1W
#define PWTS_SESSION_INFO_1		PWTS_SESSION_INFO_1W
#define WTS_PROCESS_INFO		WTS_PROCESS_INFOW
#define PWTS_PROCESS_INFO		PWTS_PROCESS_INFOW
#define WTSCONFIGINFO			WTSCONFIGINFOW
#define PWTSCONFIGINFO			PWTSCONFIGINFOW
#define WTSINFO				WTSINFOW
#define PWTSINFO			PWTSINFOW
#define WTSINFOEX			WTSINFOEXW
#define PWTSINFOEX			PWTSINFOEXW
#define WTSINFOEX_LEVEL			WTSINFOEX_LEVEL_W
#define PWTSINFOEX_LEVEL		PWTSINFOEX_LEVEL_W
#define WTSINFOEX_LEVEL1		WTSINFOEX_LEVEL1_W
#define PWTSINFOEX_LEVEL1		PWTSINFOEX_LEVEL1_W
#define WTSCLIENT			WTSCLIENTW
#define PWTSCLIENT			PWTSCLIENTW
#define PRODUCT_INFO			PRODUCT_INFOW
#define WTS_VALIDATION_INFORMATION	WTS_VALIDATION_INFORMATIONW
#define PWTS_VALIDATION_INFORMATION	PWTS_VALIDATION_INFORMATIONW
#define WTSUSERCONFIG			WTSUSERCONFIGW
#define PWTSUSERCONFIG			PWTSUSERCONFIGW
#define WTS_PROCESS_INFO_EX		WTS_PROCESS_INFO_EXW
#define PWTS_PROCESS_INFO_EX		PWTS_PROCESS_INFO_EXW
#define WTSLISTENERNAME			WTSLISTENERNAMEW
#define PWTSLISTENERNAME		PWTSLISTENERNAMEW
#define WTSLISTENERCONFIG		WTSLISTENERCONFIGW
#define PWTSLISTENERCONFIG		PWTSLISTENERCONFIGW
#else
#define WTS_SERVER_INFO			WTS_SERVER_INFOA
#define PWTS_SERVER_INFO		PWTS_SERVER_INFOA
#define WTS_SESSION_INFO		WTS_SESSION_INFOA
#define PWTS_SESSION_INFO		PWTS_SESSION_INFOA
#define WTS_SESSION_INFO_1		WTS_SESSION_INFO_1A
#define PWTS_SESSION_INFO_1		PWTS_SESSION_INFO_1A
#define WTS_PROCESS_INFO		WTS_PROCESS_INFOA
#define PWTS_PROCESS_INFO		PWTS_PROCESS_INFOA
#define WTSCONFIGINFO			WTSCONFIGINFOA
#define PWTSCONFIGINFO			PWTSCONFIGINFOA
#define WTSINFO				WTSINFOA
#define PWTSINFO			PWTSINFOA
#define WTSINFOEX			WTSINFOEXA
#define PWTSINFOEX			PWTSINFOEXA
#define WTSINFOEX_LEVEL			WTSINFOEX_LEVEL_A
#define PWTSINFOEX_LEVEL		PWTSINFOEX_LEVEL_A
#define WTSINFOEX_LEVEL1		WTSINFOEX_LEVEL1_A
#define PWTSINFOEX_LEVEL1		PWTSINFOEX_LEVEL1_A
#define WTSCLIENT			WTSCLIENTA
#define PWTSCLIENT			PWTSCLIENTA
#define PRODUCT_INFO			PRODUCT_INFOA
#define WTS_VALIDATION_INFORMATION	WTS_VALIDATION_INFORMATIONA
#define PWTS_VALIDATION_INFORMATION	PWTS_VALIDATION_INFORMATIONA
#define WTSUSERCONFIG			WTSUSERCONFIGA
#define PWTSUSERCONFIG			PWTSUSERCONFIGA
#define WTS_PROCESS_INFO_EX		WTS_PROCESS_INFO_EXA
#define PWTS_PROCESS_INFO_EX		PWTS_PROCESS_INFO_EXA
#define WTSLISTENERNAME			WTSLISTENERNAMEA
#define PWTSLISTENERNAME		PWTSLISTENERNAMEA
#define WTSLISTENERCONFIG		WTSLISTENERCONFIGA
#define PWTSLISTENERCONFIG		PWTSLISTENERCONFIGA
#endif

#ifdef __cplusplus
extern "C" {
#endif

WINPR_API BOOL WTSStopRemoteControlSession(ULONG LogonId);

WINPR_API BOOL WTSStartRemoteControlSessionW(LPWSTR pTargetServerName, ULONG TargetLogonId, BYTE HotkeyVk, USHORT HotkeyModifiers);
WINPR_API BOOL WTSStartRemoteControlSessionA(LPSTR pTargetServerName, ULONG TargetLogonId, BYTE HotkeyVk, USHORT HotkeyModifiers);

WINPR_API BOOL WTSConnectSessionW(ULONG LogonId, ULONG TargetLogonId, PWSTR pPassword, BOOL bWait);
WINPR_API BOOL WTSConnectSessionA(ULONG LogonId, ULONG TargetLogonId, PSTR pPassword, BOOL bWait);

WINPR_API BOOL WTSEnumerateServersW(LPWSTR pDomainName, DWORD Reserved, DWORD Version, PWTS_SERVER_INFOW* ppServerInfo, DWORD* pCount);
WINPR_API BOOL WTSEnumerateServersA(LPSTR pDomainName, DWORD Reserved, DWORD Version, PWTS_SERVER_INFOA* ppServerInfo, DWORD* pCount);

WINPR_API HANDLE WTSOpenServerW(LPWSTR pServerName);
WINPR_API HANDLE WTSOpenServerA(LPSTR pServerName);

WINPR_API HANDLE WTSOpenServerExW(LPWSTR pServerName);
WINPR_API HANDLE WTSOpenServerExA(LPSTR pServerName);

WINPR_API VOID WTSCloseServer(HANDLE hServer);

WINPR_API BOOL WTSEnumerateSessionsW(HANDLE hServer, DWORD Reserved, DWORD Version, PWTS_SESSION_INFOW* ppSessionInfo, DWORD* pCount);
WINPR_API BOOL WTSEnumerateSessionsA(HANDLE hServer, DWORD Reserved, DWORD Version, PWTS_SESSION_INFOA* ppSessionInfo, DWORD* pCount);

WINPR_API BOOL WTSEnumerateSessionsExW(HANDLE hServer, DWORD* pLevel, DWORD Filter, PWTS_SESSION_INFO_1W* ppSessionInfo, DWORD* pCount);
WINPR_API BOOL WTSEnumerateSessionsExA(HANDLE hServer, DWORD* pLevel, DWORD Filter, PWTS_SESSION_INFO_1A* ppSessionInfo, DWORD* pCount);

WINPR_API BOOL WTSEnumerateProcessesW(HANDLE hServer, DWORD Reserved, DWORD Version, PWTS_PROCESS_INFOW* ppProcessInfo, DWORD* pCount);
WINPR_API BOOL WTSEnumerateProcessesA(HANDLE hServer, DWORD Reserved, DWORD Version, PWTS_PROCESS_INFOA* ppProcessInfo, DWORD* pCount);

WINPR_API BOOL WTSTerminateProcess(HANDLE hServer, DWORD ProcessId, DWORD ExitCode);

WINPR_API BOOL WTSQuerySessionInformationW(HANDLE hServer, DWORD SessionId, WTS_INFO_CLASS WTSInfoClass, LPWSTR* ppBuffer, DWORD* pBytesReturned);
WINPR_API BOOL WTSQuerySessionInformationA(HANDLE hServer, DWORD SessionId, WTS_INFO_CLASS WTSInfoClass, LPSTR* ppBuffer, DWORD* pBytesReturned);

WINPR_API BOOL WTSQueryUserConfigW(LPWSTR pServerName, LPWSTR pUserName, WTS_CONFIG_CLASS WTSConfigClass, LPWSTR* ppBuffer, DWORD* pBytesReturned);
WINPR_API BOOL WTSQueryUserConfigA(LPSTR pServerName, LPSTR pUserName, WTS_CONFIG_CLASS WTSConfigClass, LPSTR* ppBuffer, DWORD* pBytesReturned);

WINPR_API BOOL WTSSetUserConfigW(LPWSTR pServerName, LPWSTR pUserName, WTS_CONFIG_CLASS WTSConfigClass, LPWSTR pBuffer, DWORD DataLength);
WINPR_API BOOL WTSSetUserConfigA(LPSTR pServerName, LPSTR pUserName, WTS_CONFIG_CLASS WTSConfigClass, LPSTR pBuffer, DWORD DataLength);

WINPR_API BOOL WTSSendMessageW(HANDLE hServer, DWORD SessionId, LPWSTR pTitle, DWORD TitleLength,
		LPWSTR pMessage, DWORD MessageLength, DWORD Style, DWORD Timeout, DWORD* pResponse, BOOL bWait);
WINPR_API BOOL WTSSendMessageA(HANDLE hServer, DWORD SessionId, LPSTR pTitle, DWORD TitleLength,
		LPSTR pMessage, DWORD MessageLength, DWORD Style, DWORD Timeout, DWORD* pResponse, BOOL bWait);

WINPR_API BOOL WTSDisconnectSession(HANDLE hServer, DWORD SessionId, BOOL bWait);

WINPR_API BOOL WTSLogoffSession(HANDLE hServer, DWORD SessionId, BOOL bWait);

WINPR_API BOOL WTSShutdownSystem(HANDLE hServer, DWORD ShutdownFlag);

WINPR_API BOOL WTSWaitSystemEvent(HANDLE hServer, DWORD EventMask, DWORD* pEventFlags);

WINPR_API HANDLE WTSVirtualChannelOpen(HANDLE hServer, DWORD SessionId, LPSTR pVirtualName);

WINPR_API HANDLE WTSVirtualChannelOpenEx(DWORD SessionId, LPSTR pVirtualName, DWORD flags);

WINPR_API BOOL WTSVirtualChannelClose(HANDLE hChannelHandle);

WINPR_API BOOL WTSVirtualChannelRead(HANDLE hChannelHandle, ULONG TimeOut, PCHAR Buffer, ULONG BufferSize, PULONG pBytesRead);

WINPR_API BOOL WTSVirtualChannelWrite(HANDLE hChannelHandle, PCHAR Buffer, ULONG Length, PULONG pBytesWritten);

WINPR_API BOOL WTSVirtualChannelPurgeInput(HANDLE hChannelHandle);

WINPR_API BOOL WTSVirtualChannelPurgeOutput(HANDLE hChannelHandle);

WINPR_API BOOL WTSVirtualChannelQuery(HANDLE hChannelHandle, WTS_VIRTUAL_CLASS WtsVirtualClass, PVOID* ppBuffer, DWORD* pBytesReturned);

WINPR_API VOID WTSFreeMemory(PVOID pMemory);

WINPR_API BOOL WTSRegisterSessionNotification(HWND hWnd, DWORD dwFlags);

WINPR_API BOOL WTSUnRegisterSessionNotification(HWND hWnd);

WINPR_API BOOL WTSRegisterSessionNotificationEx(HANDLE hServer, HWND hWnd, DWORD dwFlags);

WINPR_API BOOL WTSUnRegisterSessionNotificationEx(HANDLE hServer, HWND hWnd);

WINPR_API BOOL WTSQueryUserToken(ULONG SessionId, PHANDLE phToken);

WINPR_API BOOL WTSFreeMemoryExW(WTS_TYPE_CLASS WTSTypeClass, PVOID pMemory, ULONG NumberOfEntries);
WINPR_API BOOL WTSFreeMemoryExA(WTS_TYPE_CLASS WTSTypeClass, PVOID pMemory, ULONG NumberOfEntries);

WINPR_API BOOL WTSEnumerateProcessesExW(HANDLE hServer, DWORD* pLevel, DWORD SessionId, LPWSTR* ppProcessInfo, DWORD* pCount);
WINPR_API BOOL WTSEnumerateProcessesExA(HANDLE hServer, DWORD* pLevel, DWORD SessionId, LPSTR* ppProcessInfo, DWORD* pCount);

WINPR_API BOOL WTSEnumerateListenersW(HANDLE hServer, PVOID pReserved, DWORD Reserved, PWTSLISTENERNAMEW pListeners, DWORD* pCount);
WINPR_API BOOL WTSEnumerateListenersA(HANDLE hServer, PVOID pReserved, DWORD Reserved, PWTSLISTENERNAMEA pListeners, DWORD* pCount);

WINPR_API BOOL WTSQueryListenerConfigW(HANDLE hServer, PVOID pReserved, DWORD Reserved, LPWSTR pListenerName, PWTSLISTENERCONFIGW pBuffer);
WINPR_API BOOL WTSQueryListenerConfigA(HANDLE hServer, PVOID pReserved, DWORD Reserved, LPSTR pListenerName, PWTSLISTENERCONFIGA pBuffer);

WINPR_API BOOL WTSCreateListenerW(HANDLE hServer, PVOID pReserved, DWORD Reserved,
		LPWSTR pListenerName, PWTSLISTENERCONFIGW pBuffer, DWORD flag);
WINPR_API BOOL WTSCreateListenerA(HANDLE hServer, PVOID pReserved, DWORD Reserved,
		LPSTR pListenerName, PWTSLISTENERCONFIGA pBuffer, DWORD flag);

WINPR_API BOOL WTSSetListenerSecurityW(HANDLE hServer, PVOID pReserved, DWORD Reserved,
		LPWSTR pListenerName, SECURITY_INFORMATION SecurityInformation,
		PSECURITY_DESCRIPTOR pSecurityDescriptor);
WINPR_API BOOL WTSSetListenerSecurityA(HANDLE hServer, PVOID pReserved, DWORD Reserved,
		LPSTR pListenerName, SECURITY_INFORMATION SecurityInformation,
		PSECURITY_DESCRIPTOR pSecurityDescriptor);

WINPR_API BOOL WTSGetListenerSecurityW(HANDLE hServer, PVOID pReserved, DWORD Reserved,
		LPWSTR pListenerName, SECURITY_INFORMATION SecurityInformation,
		PSECURITY_DESCRIPTOR pSecurityDescriptor, DWORD nLength, LPDWORD lpnLengthNeeded);
WINPR_API BOOL WTSGetListenerSecurityA(HANDLE hServer, PVOID pReserved, DWORD Reserved,
		LPSTR pListenerName, SECURITY_INFORMATION SecurityInformation,
		PSECURITY_DESCRIPTOR pSecurityDescriptor, DWORD nLength, LPDWORD lpnLengthNeeded);

WINPR_API BOOL WTSEnableChildSessions(BOOL bEnable);

WINPR_API BOOL WTSIsChildSessionsEnabled(PBOOL pbEnabled);

WINPR_API BOOL WTSGetChildSessionId(PULONG pSessionId);

#ifdef __cplusplus
}
#endif

#ifdef UNICODE
#define WTSStartRemoteControlSession	WTSStartRemoteControlSessionW
#define WTSConnectSession		WTSConnectSessionW
#define WTSEnumerateServers		WTSEnumerateServersW
#define WTSOpenServer			WTSOpenServerW
#define WTSOpenServerEx			WTSOpenServerExW
#define WTSEnumerateSessions		WTSEnumerateSessionsW
#define WTSEnumerateSessionsEx		WTSEnumerateSessionsExW
#define WTSEnumerateProcesses		WTSEnumerateProcessesW
#define WTSQuerySessionInformation	WTSQuerySessionInformationW
#define WTSQueryUserConfig		WTSQueryUserConfigW
#define WTSSetUserConfig		WTSSetUserConfigW
#define WTSSendMessage			WTSSendMessageW
#define WTSFreeMemoryEx			WTSFreeMemoryExW
#define WTSEnumerateProcessesEx		WTSEnumerateProcessesExW
#define WTSEnumerateListeners		WTSEnumerateListenersW
#define WTSQueryListenerConfig		WTSQueryListenerConfigW
#define WTSCreateListener		WTSCreateListenerW
#define WTSSetListenerSecurity		WTSSetListenerSecurityW
#define WTSGetListenerSecurity		WTSGetListenerSecurityW
#else
#define WTSStartRemoteControlSession	WTSStartRemoteControlSessionA
#define WTSConnectSession		WTSConnectSessionA
#define WTSEnumerateServers		WTSEnumerateServersA
#define WTSOpenServer			WTSOpenServerA
#define WTSOpenServerEx			WTSOpenServerExA
#define WTSEnumerateSessions		WTSEnumerateSessionsA
#define WTSEnumerateSessionsEx		WTSEnumerateSessionsExA
#define WTSEnumerateProcesses		WTSEnumerateProcessesA
#define WTSQuerySessionInformation	WTSQuerySessionInformationA
#define WTSQueryUserConfig		WTSQueryUserConfigA
#define WTSSetUserConfig		WTSSetUserConfigA
#define WTSSendMessage			WTSSendMessageA
#define WTSFreeMemoryEx			WTSFreeMemoryExA
#define WTSEnumerateProcessesEx		WTSEnumerateProcessesExA
#define WTSEnumerateListeners		WTSEnumerateListenersA
#define WTSQueryListenerConfig		WTSQueryListenerConfigA
#define WTSCreateListener		WTSCreateListenerA
#define WTSSetListenerSecurity		WTSSetListenerSecurityA
#define WTSGetListenerSecurity		WTSGetListenerSecurityA
#endif

#endif

#endif /* WINPR_WTSAPI_H */

