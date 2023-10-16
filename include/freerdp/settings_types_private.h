/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Internal settings header for functions not exported
 *
 * Copyright 2018 Armin Novak <armin.novak@thincast.com>
 * Copyright 2018 Thincast Technologies GmbH
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

#ifndef FREERDP_SETTINGS_TYPES_PRIVATE_H
#define FREERDP_SETTINGS_TYPES_PRIVATE_H

#include <winpr/string.h>
#include <winpr/sspi.h>

#include <freerdp/config.h>

#include <freerdp/types.h>
#include <freerdp/api.h>

#include <string.h>

struct rdp_settings
{
	/**
	 * WARNING: this data structure is carefully padded for ABI stability!
	 * Keeping this area clean is particularly challenging, so unless you are
	 * a trusted developer you should NOT take the liberty of adding your own
	 * options straight into the ABI stable zone. Instead, append them to the
	 * very end of this data structure, in the zone marked as ABI unstable.
	 */

	ALIGN64 void* instance;    /* 0 */
	UINT64 padding001[16 - 1]; /* 1 */

	/* Core Parameters */
	ALIGN64 BOOL ServerMode;               /* 16 */
	ALIGN64 UINT32 ShareId;                /* 17 */
	ALIGN64 UINT32 PduSource;              /* 18 */
	ALIGN64 UINT32 ServerPort;             /* 19 */
	ALIGN64 char* ServerHostname;          /* 20 */
	ALIGN64 char* Username;                /* 21 */
	ALIGN64 char* Password;                /* 22 */
	ALIGN64 char* Domain;                  /* 23 */
	ALIGN64 char* PasswordHash;            /* 24 */
	ALIGN64 BOOL WaitForOutputBufferFlush; /* 25 */
	UINT64 padding26[27 - 26];             /* 26 */
	ALIGN64 char* AcceptedCert;            /* 27 */
	ALIGN64 UINT32 AcceptedCertLength;     /* 28 */
	ALIGN64 char* UserSpecifiedServerName; /* 29 */
	UINT64 padding0064[64 - 30];           /* 30 */
	/* resource management related options */
	ALIGN64 UINT32 ThreadingFlags; /* 64 */

	UINT64 padding0128[128 - 65]; /* 65 */

	/**
	 * GCC User Data Blocks
	 */

	/* Client/Server Core Data */
	ALIGN64 UINT32 RdpVersion;            /* 128 */
	ALIGN64 UINT32 DesktopWidth;          /* 129 */
	ALIGN64 UINT32 DesktopHeight;         /* 130 */
	ALIGN64 UINT32 ColorDepth;            /* 131 */
	ALIGN64 UINT32 ConnectionType;        /* 132 */
	ALIGN64 UINT32 ClientBuild;           /* 133 */
	ALIGN64 char* ClientHostname;         /* 134 */
	ALIGN64 char* ClientProductId;        /* 135 */
	ALIGN64 UINT32 EarlyCapabilityFlags;  /* 136 */
	ALIGN64 BOOL NetworkAutoDetect;       /* 137 */
	ALIGN64 BOOL SupportAsymetricKeys;    /* 138 */
	ALIGN64 BOOL SupportErrorInfoPdu;     /* 139 */
	ALIGN64 BOOL SupportStatusInfoPdu;    /* 140 */
	ALIGN64 BOOL SupportMonitorLayoutPdu; /* 141 */
	ALIGN64 BOOL SupportGraphicsPipeline; /* 142 */
	ALIGN64 BOOL SupportDynamicTimeZone;  /* 143 */
	ALIGN64 BOOL SupportHeartbeatPdu;     /* 144 */
	ALIGN64 UINT32 DesktopPhysicalWidth;  /* 145 */
	ALIGN64 UINT32 DesktopPhysicalHeight; /* 146 */
	ALIGN64 UINT16 DesktopOrientation;    /* 147 */
	ALIGN64 UINT32 DesktopScaleFactor;    /* 148 */
	ALIGN64 UINT32 DeviceScaleFactor;     /* 149 */
	ALIGN64 BOOL SupportEdgeActionV1;     /* 150 */
	ALIGN64 BOOL SupportEdgeActionV2;     /* 151 */
	ALIGN64 BOOL SupportSkipChannelJoin;  /* 152 */
	ALIGN64 UINT16 SupportedColorDepths;  /* 153 */
	UINT64 padding0192[192 - 154];        /* 154 */

	/* Client/Server Security Data */
	ALIGN64 BOOL UseRdpSecurityLayer;                /* 192 */
	ALIGN64 UINT32 EncryptionMethods;                /* 193 */
	ALIGN64 UINT32 ExtEncryptionMethods;             /* 194 */
	ALIGN64 UINT32 EncryptionLevel;                  /* 195 */
	ALIGN64 BYTE* ServerRandom;                      /* 196 */
	ALIGN64 UINT32 ServerRandomLength;               /* 197 */
	ALIGN64 BYTE* ServerCertificate;                 /* 198 */
	ALIGN64 UINT32 ServerCertificateLength;          /* 199 */
	ALIGN64 BYTE* ClientRandom;                      /* 200 */
	ALIGN64 UINT32 ClientRandomLength;               /* 201 */
	ALIGN64 BOOL ServerLicenseRequired;              /* 202 */
	ALIGN64 char* ServerLicenseCompanyName;          /* 203 */
	ALIGN64 UINT32 ServerLicenseProductVersion;      /* 204 */
	ALIGN64 char* ServerLicenseProductName;          /* 205 */
	ALIGN64 char** ServerLicenseProductIssuers;      /* 206 */
	ALIGN64 UINT32 ServerLicenseProductIssuersCount; /* 207 */
	UINT64 padding0256[256 - 208];                   /* 208 */

	/* Client Network Data */
	ALIGN64 UINT32 ChannelCount;          /* 256 */
	ALIGN64 UINT32 ChannelDefArraySize;   /* 257 */
	ALIGN64 CHANNEL_DEF* ChannelDefArray; /* 258 */
	UINT64 padding0320[320 - 259];        /* 259 */

	/* Client Cluster Data */
	ALIGN64 UINT32 ClusterInfoFlags;    /* 320 */
	ALIGN64 UINT32 RedirectedSessionId; /* 321 */
	ALIGN64 BOOL ConsoleSession;        /* 322 */
	UINT64 padding0384[384 - 323];      /* 323 */

	/* Client Monitor Data */
	ALIGN64 UINT32 MonitorCount;          /*    384 */
	ALIGN64 UINT32 MonitorDefArraySize;   /*    385 */
	ALIGN64 rdpMonitor* MonitorDefArray;  /*    386 */
	ALIGN64 BOOL SpanMonitors;            /*    387 */
	ALIGN64 BOOL UseMultimon;             /*    388 */
	ALIGN64 BOOL ForceMultimon;           /*    389 */
	ALIGN64 UINT32 DesktopPosX;           /*    390 */
	ALIGN64 UINT32 DesktopPosY;           /*    391 */
	ALIGN64 BOOL ListMonitors;            /*    392 */
	ALIGN64 UINT32* MonitorIds;           /*    393 */
	ALIGN64 UINT32 NumMonitorIds;         /*    394 */
	ALIGN64 UINT32 MonitorLocalShiftX;    /*395 */
	ALIGN64 UINT32 MonitorLocalShiftY;    /*    396 */
	ALIGN64 BOOL HasMonitorAttributes;    /*    397 */
	ALIGN64 UINT32 MonitorFlags;          /* 398 */
	ALIGN64 UINT32 MonitorAttributeFlags; /* 399 */
	UINT64 padding0448[448 - 400];        /* 400 */

	/* Client Message Channel Data */
	UINT64 padding0512[512 - 448]; /* 448 */

	/* Client Multitransport Channel Data */
	ALIGN64 UINT32 MultitransportFlags; /* 512 */
	ALIGN64 BOOL SupportMultitransport; /* 513 */
	UINT64 padding0576[576 - 514];      /* 514 */
	UINT64 padding0640[640 - 576];      /* 576 */

	/*
	 * Client Info
	 */

	/* Client Info (Shell) */
	ALIGN64 char* AlternateShell;        /* 640 */
	ALIGN64 char* ShellWorkingDirectory; /* 641 */
	UINT64 padding0704[704 - 642];       /* 642 */

	/* Client Info Flags */
	ALIGN64 BOOL AutoLogonEnabled;       /* 704 */
	ALIGN64 BOOL CompressionEnabled;     /* 705 */
	ALIGN64 BOOL DisableCtrlAltDel;      /* 706 */
	ALIGN64 BOOL EnableWindowsKey;       /* 707 */
	ALIGN64 BOOL MaximizeShell;          /* 708 */
	ALIGN64 BOOL LogonNotify;            /* 709 */
	ALIGN64 BOOL LogonErrors;            /* 710 */
	ALIGN64 BOOL MouseAttached;          /* 711 */
	ALIGN64 BOOL MouseHasWheel;          /* 712 */
	ALIGN64 BOOL RemoteConsoleAudio;     /* 713 */
	ALIGN64 BOOL AudioPlayback;          /* 714 */
	ALIGN64 BOOL AudioCapture;           /* 715 */
	ALIGN64 BOOL VideoDisable;           /* 716 */
	ALIGN64 BOOL PasswordIsSmartcardPin; /* 717 */
	ALIGN64 BOOL UsingSavedCredentials;  /* 718 */
	ALIGN64 BOOL ForceEncryptedCsPdu;    /* 719 */
	ALIGN64 BOOL HiDefRemoteApp;         /* 720 */
	ALIGN64 UINT32 CompressionLevel;     /* 721 */
	UINT64 padding0768[768 - 722];       /* 722 */

	/* Client Info (Extra) */
	ALIGN64 BOOL IPv6Enabled;       /* 768 */
	ALIGN64 char* ClientAddress;    /* 769 */
	ALIGN64 char* ClientDir;        /* 770 */
	ALIGN64 UINT32 ClientSessionId; /* 771 */
	UINT64 padding0832[832 - 772];  /* 772 */

	/* Client Info (Auto Reconnection) */
	ALIGN64 BOOL AutoReconnectionEnabled;                     /* 832 */
	ALIGN64 UINT32 AutoReconnectMaxRetries;                   /* 833 */
	ALIGN64 ARC_CS_PRIVATE_PACKET* ClientAutoReconnectCookie; /* 834 */
	ALIGN64 ARC_SC_PRIVATE_PACKET* ServerAutoReconnectCookie; /* 835 */
	ALIGN64 BOOL PrintReconnectCookie;                        /* 836 */
	UINT64 padding0896[896 - 837];                            /* 837 */

	/* Client Info (Time Zone) */
	ALIGN64 TIME_ZONE_INFORMATION* ClientTimeZone; /* 896 */
	ALIGN64 char* DynamicDSTTimeZoneKeyName;       /* 897 */
	ALIGN64 BOOL DynamicDaylightTimeDisabled;      /* 898 */
	UINT64 padding0960[960 - 899];                 /* 899 */

	/* Client Info (Performance Flags) */
	ALIGN64 UINT32 PerformanceFlags;      /* 960 */
	ALIGN64 BOOL AllowFontSmoothing;      /* 961 */
	ALIGN64 BOOL DisableWallpaper;        /* 962 */
	ALIGN64 BOOL DisableFullWindowDrag;   /* 963 */
	ALIGN64 BOOL DisableMenuAnims;        /* 964 */
	ALIGN64 BOOL DisableThemes;           /* 965 */
	ALIGN64 BOOL DisableCursorShadow;     /* 966 */
	ALIGN64 BOOL DisableCursorBlinking;   /* 967 */
	ALIGN64 BOOL AllowDesktopComposition; /* 968 */
	UINT64 padding1024[1024 - 969];       /* 969 */

	/* Remote Assistance */
	ALIGN64 BOOL RemoteAssistanceMode;           /* 1024 */
	ALIGN64 char* RemoteAssistanceSessionId;     /* 1025 */
	ALIGN64 char* RemoteAssistancePassStub;      /* 1026 */
	ALIGN64 char* RemoteAssistancePassword;      /* 1027 */
	ALIGN64 char* RemoteAssistanceRCTicket;      /* 1028 */
	ALIGN64 BOOL EncomspVirtualChannel;          /* 1029 */
	ALIGN64 BOOL RemdeskVirtualChannel;          /* 1030 */
	ALIGN64 BOOL LyncRdpMode;                    /* 1031 */
	ALIGN64 BOOL RemoteAssistanceRequestControl; /* 1032 */
	UINT64 padding1088[1088 - 1033];             /* 1033 */

	/**
	 * X.224 Connection Request/Confirm
	 */

	/* Protocol Security */
	ALIGN64 BOOL TlsSecurity;                  /* 1088 */
	ALIGN64 BOOL NlaSecurity;                  /* 1089 */
	ALIGN64 BOOL RdpSecurity;                  /* 1090 */
	ALIGN64 BOOL ExtSecurity;                  /* 1091 */
	ALIGN64 BOOL Authentication;               /* 1092 */
	ALIGN64 UINT32 RequestedProtocols;         /* 1093 */
	ALIGN64 UINT32 SelectedProtocol;           /* 1094 */
	ALIGN64 UINT32 NegotiationFlags;           /* 1095 */
	ALIGN64 BOOL NegotiateSecurityLayer;       /* 1096 */
	ALIGN64 BOOL RestrictedAdminModeRequired;  /* 1097 */
	ALIGN64 char* AuthenticationServiceClass;  /* 1098 */
	ALIGN64 BOOL DisableCredentialsDelegation; /* 1099 */
	ALIGN64 UINT32 AuthenticationLevel;        /* 1100 */
	ALIGN64 char* AllowedTlsCiphers;           /* 1101 */
	ALIGN64 BOOL VmConnectMode;                /* 1102 */
	ALIGN64 char* NtlmSamFile;                 /* 1103 */
	ALIGN64 BOOL FIPSMode;                     /* 1104 */
	ALIGN64 UINT32 TlsSecLevel;                /* 1105 */
	ALIGN64 char* SspiModule;                  /* 1106 */
	ALIGN64 UINT16 TLSMinVersion;              /* 1107 */
	ALIGN64 UINT16 TLSMaxVersion;              /* 1108 */
	ALIGN64 char* TlsSecretsFile;              /* 1109 */
	ALIGN64 char* AuthenticationPackageList;   /* 1110 */
	ALIGN64 BOOL RdstlsSecurity;               /* 1111 */
	ALIGN64 BOOL AadSecurity;                  /* 1112 */
	ALIGN64 char* WinSCardModule;              /* 1113 */
	UINT64 padding1152[1152 - 1114];           /* 1114 */

	/* Connection Cookie */
	ALIGN64 BOOL MstscCookieMode;      /* 1152 */
	ALIGN64 UINT32 CookieMaxLength;    /* 1153 */
	ALIGN64 UINT32 PreconnectionId;    /* 1154 */
	ALIGN64 char* PreconnectionBlob;   /* 1155 */
	ALIGN64 BOOL SendPreconnectionPdu; /* 1156 */
	UINT64 padding1216[1216 - 1157];   /* 1157 */

	/* Server Redirection */
	ALIGN64 UINT32 RedirectionFlags;                      /* 1216 */
	ALIGN64 char* TargetNetAddress;                       /* 1217 */
	ALIGN64 BYTE* LoadBalanceInfo;                        /* 1218 */
	ALIGN64 UINT32 LoadBalanceInfoLength;                 /* 1219 */
	ALIGN64 char* RedirectionUsername;                    /* 1220 */
	ALIGN64 char* RedirectionDomain;                      /* 1221 */
	ALIGN64 BYTE* RedirectionPassword;                    /* 1222 */
	ALIGN64 UINT32 RedirectionPasswordLength;             /* 1223 */
	ALIGN64 char* RedirectionTargetFQDN;                  /* 1224 */
	ALIGN64 char* RedirectionTargetNetBiosName;           /* 1225 */
	ALIGN64 BYTE* RedirectionTsvUrl;                      /* 1226 */
	ALIGN64 UINT32 RedirectionTsvUrlLength;               /* 1227 */
	ALIGN64 UINT32 TargetNetAddressCount;                 /* 1228 */
	ALIGN64 char** TargetNetAddresses;                    /* 1229 */
	ALIGN64 UINT32* TargetNetPorts;                       /* 1230 */
	ALIGN64 char* RedirectionAcceptedCert;                /* 1231 */
	ALIGN64 UINT32 RedirectionAcceptedCertLength;         /* 1232 */
	ALIGN64 UINT32 RedirectionPreferType;                 /* 1233 */
	ALIGN64 BYTE* RedirectionGuid;                        /* 1234 */
	ALIGN64 UINT32 RedirectionGuidLength;                 /* 1235 */
	ALIGN64 rdpCertificate* RedirectionTargetCertificate; /* 1236 */
	UINT64 padding1280[1280 - 1237];                      /* 1237 */

	/**
	 * Security
	 */

	/* Credentials Cache */
	ALIGN64 BYTE* Password51;          /* 1280 */
	ALIGN64 UINT32 Password51Length;   /* 1281 */
	ALIGN64 BOOL SmartcardLogon;       /* 1282 */
	ALIGN64 BOOL PromptForCredentials; /* 1283 */
	UINT64 padding1284[1285 - 1284];   /* 1284 */

	/* Settings used for smartcard emulation */
	ALIGN64 char* SmartcardCertificate; /* 1285 */
	ALIGN64 char* SmartcardPrivateKey;  /* 1286 */
	UINT64 padding1287[1288 - 1287];    /* 1287 */
	ALIGN64 BOOL SmartcardEmulation;    /* 1288 */
	ALIGN64 char* Pkcs11Module;         /* 1289 */
	ALIGN64 char* PkinitAnchors;        /* 1290 */
	ALIGN64 UINT32 KeySpec;             /* 1291 */
	ALIGN64 char* CardName;             /* 1292 */
	ALIGN64 char* ReaderName;           /* 1293 */
	ALIGN64 char* ContainerName;        /* 1294 */
	ALIGN64 char* CspName;              /* 1295 */
	UINT64 padding1344[1344 - 1296];    /* 1296 */

	/* Kerberos Authentication */
	ALIGN64 char* KerberosKdcUrl;            /* 1344 */
	ALIGN64 char* KerberosRealm;             /* 1345 */
	ALIGN64 char* KerberosStartTime;         /* 1346 */
	ALIGN64 char* KerberosLifeTime;          /* 1347 */
	ALIGN64 char* KerberosRenewableLifeTime; /* 1348 */
	ALIGN64 char* KerberosCache;             /* 1349 */
	ALIGN64 char* KerberosArmor;             /* 1350 */
	ALIGN64 char* KerberosKeytab;            /* 1351 */
	ALIGN64 BOOL KerberosRdgIsProxy;         /* 1352 */
	UINT64 padding1408[1408 - 1353];         /* 1353 */

	/* Server Certificate */
	ALIGN64 BOOL IgnoreCertificate;                /* 1408 */
	ALIGN64 char* CertificateName;                 /* 1409 */
	UINT64 padding1410[1413 - 1410];               /* 1410 */
	ALIGN64 rdpPrivateKey* RdpServerRsaKey;        /* 1413 */
	ALIGN64 rdpCertificate* RdpServerCertificate;  /* 1414 */
	ALIGN64 BOOL ExternalCertificateManagement;    /* 1415 */
	UINT64 padding1416[1419 - 1416];               /* 1416 */
	ALIGN64 BOOL AutoAcceptCertificate;            /* 1419 */
	ALIGN64 BOOL AutoDenyCertificate;              /* 1420 */
	ALIGN64 char* CertificateAcceptedFingerprints; /* 1421 */
	UINT64 padding1422[1423 - 1422];               /* 1422 */
	ALIGN64 BOOL CertificateCallbackPreferPEM;     /* 1423 */
	UINT64 padding1472[1472 - 1424];               /* 1424 */
	UINT64 padding1536[1536 - 1472];               /* 1472 */

	/**
	 * User Interface
	 */

	/* Window Settings */
	ALIGN64 BOOL Workarea;                /* 1536 */
	ALIGN64 BOOL Fullscreen;              /* 1537 */
	ALIGN64 UINT32 PercentScreen;         /* 1538 */
	ALIGN64 BOOL GrabKeyboard;            /* 1539 */
	ALIGN64 BOOL Decorations;             /* 1540 */
	ALIGN64 BOOL MouseMotion;             /* 1541 */
	ALIGN64 char* WindowTitle;            /* 1542 */
	ALIGN64 UINT64 ParentWindowId;        /* 1543 */
	UINT64 padding1544[1545 - 1544];      /* 1544 */
	ALIGN64 BOOL AsyncUpdate;             /* 1545 */
	ALIGN64 BOOL AsyncChannels;           /* 1546 */
	UINT64 padding1548[1548 - 1547];      /* 1547 */
	ALIGN64 BOOL ToggleFullscreen;        /* 1548 */
	ALIGN64 char* WmClass;                /* 1549 */
	ALIGN64 BOOL EmbeddedWindow;          /* 1550 */
	ALIGN64 BOOL SmartSizing;             /* 1551 */
	ALIGN64 INT32 XPan;                   /* 1552 */
	ALIGN64 INT32 YPan;                   /* 1553 */
	ALIGN64 UINT32 SmartSizingWidth;      /* 1554 */
	ALIGN64 UINT32 SmartSizingHeight;     /* 1555 */
	ALIGN64 BOOL PercentScreenUseWidth;   /* 1556 */
	ALIGN64 BOOL PercentScreenUseHeight;  /* 1557 */
	ALIGN64 BOOL DynamicResolutionUpdate; /* 1558 */
	ALIGN64 BOOL GrabMouse;               /* 1559 */
	UINT64 padding1601[1601 - 1560];      /* 1560 */

	/* Miscellaneous */
	ALIGN64 BOOL SoftwareGdi;             /* 1601 */
	ALIGN64 BOOL LocalConnection;         /* 1602 */
	ALIGN64 BOOL AuthenticationOnly;      /* 1603 */
	ALIGN64 BOOL CredentialsFromStdin;    /* 1604 */
	ALIGN64 BOOL UnmapButtons;            /* 1605 */
	ALIGN64 BOOL OldLicenseBehaviour;     /* 1606 */
	ALIGN64 BOOL MouseUseRelativeMove;    /* 1607 */
	ALIGN64 BOOL UseCommonStdioCallbacks; /* 1608 */
	ALIGN64 BOOL ConnectChildSession;     /* 1609 */
	UINT64 padding1664[1664 - 1610];      /* 1610 */

	/* Names */
	ALIGN64 char* ComputerName;      /* 1664 */
	UINT64 padding1728[1728 - 1665]; /* 1665 */

	/* Files */
	ALIGN64 char* ConnectionFile;    /* 1728 */
	ALIGN64 char* AssistanceFile;    /* 1729 */
	UINT64 padding1792[1792 - 1730]; /* 1730 */

	/* Paths */
	ALIGN64 char* HomePath;          /* 1792 */
	ALIGN64 char* ConfigPath;        /* 1793 */
	ALIGN64 char* CurrentPath;       /* 1794 */
	UINT64 padding1856[1856 - 1795]; /* 1795 */

	/* Recording */
	ALIGN64 BOOL DumpRemoteFx;             /* 1856 */
	ALIGN64 BOOL PlayRemoteFx;             /* 1857 */
	ALIGN64 char* DumpRemoteFxFile;        /* 1858 */
	ALIGN64 char* PlayRemoteFxFile;        /* 1859 */
	ALIGN64 BOOL TransportDump;            /* 1860 */
	ALIGN64 char* TransportDumpFile;       /* 1861 */
	ALIGN64 BOOL TransportDumpReplay;      /* 1862 */
	ALIGN64 BOOL DeactivateClientDecoding; /* 1863 */
	UINT64 padding1920[1920 - 1864];       /* 1864 */
	UINT64 padding1984[1984 - 1920];       /* 1920 */

	/**
	 * Gateway
	 */

	/* Gateway */
	ALIGN64 UINT32 GatewayUsageMethod;            /* 1984 */
	ALIGN64 UINT32 GatewayPort;                   /* 1985 */
	ALIGN64 char* GatewayHostname;                /* 1986 */
	ALIGN64 char* GatewayUsername;                /* 1987 */
	ALIGN64 char* GatewayPassword;                /* 1988 */
	ALIGN64 char* GatewayDomain;                  /* 1989 */
	ALIGN64 UINT32 GatewayCredentialsSource;      /* 1990 */
	ALIGN64 BOOL GatewayUseSameCredentials;       /* 1991 */
	ALIGN64 BOOL GatewayEnabled;                  /* 1992 */
	ALIGN64 BOOL GatewayBypassLocal;              /* 1993 */
	ALIGN64 BOOL GatewayRpcTransport;             /* 1994 */
	ALIGN64 BOOL GatewayHttpTransport;            /* 1995 */
	ALIGN64 BOOL GatewayUdpTransport;             /* 1996 */
	ALIGN64 char* GatewayAccessToken;             /* 1997 */
	ALIGN64 char* GatewayAcceptedCert;            /* 1998 */
	ALIGN64 UINT32 GatewayAcceptedCertLength;     /* 1999 */
	ALIGN64 BOOL GatewayHttpUseWebsockets;        /* 2000 */
	ALIGN64 BOOL GatewayHttpExtAuthSspiNtlm;      /* 2001 */
	ALIGN64 char* GatewayHttpExtAuthBearer;       /* 2002 */
	ALIGN64 char* GatewayUrl;                     /* 2003 */
	ALIGN64 BOOL GatewayArmTransport;             /* 2004 */
	ALIGN64 char* GatewayAvdWvdEndpointPool;      /* 2005 */
	ALIGN64 char* GatewayAvdGeo;                  /* 2006 */
	ALIGN64 char* GatewayAvdArmpath;              /* 2007 */
	ALIGN64 char* GatewayAvdAadtenantid;          /* 2008 */
	ALIGN64 char* GatewayAvdDiagnosticserviceurl; /* 2009 */
	ALIGN64 char* GatewayAvdHubdiscoverygeourl;   /* 2010 */
	ALIGN64 char* GatewayAvdActivityhint;         /* 2011 */
	UINT64 padding2015[2015 - 2012];              /* 2012 */

	/* Proxy */
	ALIGN64 UINT32 ProxyType;        /* 2015 */
	ALIGN64 char* ProxyHostname;     /* 2016 */
	ALIGN64 UINT16 ProxyPort;        /* 2017 */
	ALIGN64 char* ProxyUsername;     /* 2018 */
	ALIGN64 char* ProxyPassword;     /* 2019 */
	UINT64 padding2112[2112 - 2020]; /* 2020 */

	/**
	 * RemoteApp
	 */

	/* RemoteApp */
	ALIGN64 BOOL RemoteApplicationMode;               /* 2112 */
	ALIGN64 char* RemoteApplicationName;              /* 2113 */
	ALIGN64 char* RemoteApplicationIcon;              /* 2114 */
	ALIGN64 char* RemoteApplicationProgram;           /* 2115 */
	ALIGN64 char* RemoteApplicationFile;              /* 2116 */
	ALIGN64 char* RemoteApplicationGuid;              /* 2117 */
	ALIGN64 char* RemoteApplicationCmdLine;           /* 2118 */
	ALIGN64 UINT32 RemoteApplicationExpandCmdLine;    /* 2119 */
	ALIGN64 UINT32 RemoteApplicationExpandWorkingDir; /* 2120 */
	ALIGN64 BOOL DisableRemoteAppCapsCheck;           /* 2121 */
	ALIGN64 UINT32 RemoteAppNumIconCaches;            /* 2122 */
	ALIGN64 UINT32 RemoteAppNumIconCacheEntries;      /* 2123 */
	ALIGN64 BOOL RemoteAppLanguageBarSupported;       /* 2124 */
	ALIGN64 UINT32 RemoteWndSupportLevel;             /* 2125 */
	ALIGN64 UINT32 RemoteApplicationSupportLevel;     /* 2126 */
	ALIGN64 UINT32 RemoteApplicationSupportMask;      /* 2127 */
	ALIGN64 char* RemoteApplicationWorkingDir;        /* 2128 */
	UINT64 padding2176[2176 - 2129];                  /* 2129 */
	UINT64 padding2240[2240 - 2176];                  /* 2176 */

	/**
	 * Mandatory Capabilities
	 */

	/* Capabilities */
	ALIGN64 BYTE* ReceivedCapabilities;          /* 2240 */
	ALIGN64 UINT32 ReceivedCapabilitiesSize;     /* 2241 */
	ALIGN64 BYTE** ReceivedCapabilityData;       /* 2242 */
	ALIGN64 UINT32* ReceivedCapabilityDataSizes; /* 2243 */
	UINT64 padding2304[2304 - 2244];             /* 2244 */

	/* General Capabilities */
	ALIGN64 UINT32 OsMajorType;                 /* 2304 */
	ALIGN64 UINT32 OsMinorType;                 /* 2305 */
	ALIGN64 BOOL RefreshRect;                   /* 2306 */
	ALIGN64 BOOL SuppressOutput;                /* 2307 */
	ALIGN64 BOOL FastPathOutput;                /* 2308 */
	ALIGN64 BOOL SaltedChecksum;                /* 2309 */
	ALIGN64 BOOL LongCredentialsSupported;      /* 2310 */
	ALIGN64 BOOL NoBitmapCompressionHeader;     /* 2311 */
	ALIGN64 BOOL BitmapCompressionDisabled;     /* 2312 */
	ALIGN64 UINT16 CapsProtocolVersion;         /* 2313 */
	ALIGN64 UINT16 CapsGeneralCompressionTypes; /* 2314 */
	ALIGN64 UINT16 CapsUpdateCapabilityFlag;    /* 2315 */
	ALIGN64 UINT16 CapsRemoteUnshareFlag;       /* 2316 */
	ALIGN64 UINT16 CapsGeneralCompressionLevel; /* 2317 */
	UINT64 padding2368[2368 - 2318];            /* 2318 */

	/* Bitmap Capabilities */
	ALIGN64 BOOL DesktopResize;                 /* 2368 */
	ALIGN64 BOOL DrawAllowDynamicColorFidelity; /* 2369 */
	ALIGN64 BOOL DrawAllowColorSubsampling;     /* 2370 */
	ALIGN64 BOOL DrawAllowSkipAlpha;            /* 2371 */
	UINT64 padding2432[2432 - 2372];            /* 2372 */

	/* Order Capabilities */
	ALIGN64 BYTE* OrderSupport;                   /* 2432 */
	ALIGN64 BOOL BitmapCacheV3Enabled;            /* 2433 */
	ALIGN64 BOOL AltSecFrameMarkerSupport;        /* 2434 */
	ALIGN64 BOOL AllowUnanouncedOrdersFromServer; /* 2435 */
	ALIGN64 UINT16 OrderSupportFlags;             /* 2436 */
	ALIGN64 UINT16 OrderSupportFlagsEx;           /* 2437 */
	ALIGN64 char* TerminalDescriptor;             /* 2438 */
	ALIGN64 UINT16 TextANSICodePage;              /* 2439 */
	UINT64 padding2497[2497 - 2440];              /* 2440 */

	/* Bitmap Cache Capabilities */
	ALIGN64 BOOL BitmapCacheEnabled;                          /* 2497 */
	ALIGN64 UINT32 BitmapCacheVersion;                        /* 2498 */
	ALIGN64 BOOL AllowCacheWaitingList;                       /* 2499 */
	ALIGN64 BOOL BitmapCachePersistEnabled;                   /* 2500 */
	ALIGN64 UINT32 BitmapCacheV2NumCells;                     /* 2501 */
	ALIGN64 BITMAP_CACHE_V2_CELL_INFO* BitmapCacheV2CellInfo; /* 2502 */
	ALIGN64 char* BitmapCachePersistFile;                     /* 2503 */
	UINT64 padding2560[2560 - 2504];                          /* 2504 */

	/* Pointer Capabilities */
	ALIGN64 UINT32 ColorPointerCacheSize; /* 2560 */
	ALIGN64 UINT32 PointerCacheSize;      /* 2561 */
	UINT64 padding2624[2622 - 2562];      /* 2562 */

	/* Input Capabilities */
	ALIGN64 char* KeyboardRemappingList; /* 2622 */
	ALIGN64 UINT32 KeyboardCodePage;     /* 2623 */
	ALIGN64 UINT32 KeyboardLayout;       /* 2624 */
	ALIGN64 UINT32 KeyboardType;         /* 2625 */
	ALIGN64 UINT32 KeyboardSubType;      /* 2626 */
	ALIGN64 UINT32 KeyboardFunctionKey;  /* 2627 */
	ALIGN64 char* ImeFileName;           /* 2628 */
	ALIGN64 BOOL UnicodeInput;           /* 2629 */
	ALIGN64 BOOL FastPathInput;          /* 2630 */
	ALIGN64 BOOL MultiTouchInput;        /* 2631 */
	ALIGN64 BOOL MultiTouchGestures;     /* 2632 */
	ALIGN64 UINT32 KeyboardHook;         /* 2633 */
	ALIGN64 BOOL HasHorizontalWheel;     /* 2634 */
	ALIGN64 BOOL HasExtendedMouseEvent;  /* 2635 */

	/** SuspendInput disables processing of keyboard/mouse/multitouch input.
	 * If used by an implementation ensure proper state resync after reenabling
	 * input
	 */
	ALIGN64 BOOL SuspendInput;          /* 2636 */
	ALIGN64 char* KeyboardPipeName;     /* 2637 */
	ALIGN64 BOOL HasRelativeMouseEvent; /* 2638 */
	ALIGN64 BOOL HasQoeEvent;           /* 2639 */
	UINT64 padding2688[2688 - 2640];    /* 2640 */

	/* Brush Capabilities */
	ALIGN64 UINT32 BrushSupportLevel; /* 2688 */
	UINT64 padding2752[2752 - 2689];  /* 2689 */

	/* Glyph Cache Capabilities */
	ALIGN64 UINT32 GlyphSupportLevel;           /* 2752 */
	ALIGN64 GLYPH_CACHE_DEFINITION* GlyphCache; /* 2753 */
	ALIGN64 GLYPH_CACHE_DEFINITION* FragCache;  /* 2754 */
	UINT64 padding2816[2816 - 2755];            /* 2755 */

	/* Offscreen Bitmap Cache */
	ALIGN64 UINT32 OffscreenSupportLevel; /* 2816 */
	ALIGN64 UINT32 OffscreenCacheSize;    /* 2817 */
	ALIGN64 UINT32 OffscreenCacheEntries; /* 2818 */
	UINT64 padding2880[2880 - 2819];      /* 2819 */

	/* Virtual Channel Capabilities */
	ALIGN64 UINT32 VirtualChannelCompressionFlags; /* 2880 */
	ALIGN64 UINT32 VirtualChannelChunkSize;        /* 2881 */
	UINT64 padding2944[2944 - 2882];               /* 2882 */

	/* Sound Capabilities */
	ALIGN64 BOOL SoundBeepsEnabled;  /* 2944 */
	UINT64 padding3008[3008 - 2945]; /* 2945 */
	UINT64 padding3072[3072 - 3008]; /* 3008 */

	/**
	 * Optional Capabilities
	 */

	/* Bitmap Cache Host Capabilities */
	UINT64 padding3136[3136 - 3072]; /* 3072 */

	/* Control Capabilities */
	UINT64 padding3200[3200 - 3136]; /* 3136 */

	/* Window Activation Capabilities */
	UINT64 padding3264[3264 - 3200]; /* 3200 */

	/* Font Capabilities */
	UINT64 padding3328[3328 - 3264]; /* 3264 */

	/* Multifragment Update Capabilities */
	ALIGN64 UINT32 MultifragMaxRequestSize; /* 3328 */
	UINT64 padding3392[3392 - 3329];        /* 3329 */

	/* Large Pointer Update Capabilities */
	ALIGN64 UINT32 LargePointerFlag; /* 3392 */
	UINT64 padding3456[3456 - 3393]; /* 3393 */

	/* Desktop Composition Capabilities */
	ALIGN64 UINT32 CompDeskSupportLevel; /* 3456 */
	UINT64 padding3520[3520 - 3457];     /* 3457 */

	/* Surface Commands Capabilities */
	ALIGN64 BOOL SurfaceCommandsEnabled;    /* 3520 */
	ALIGN64 BOOL FrameMarkerCommandEnabled; /* 3521 */
	ALIGN64 BOOL SurfaceFrameMarkerEnabled; /* 3522 */
	UINT64 padding3584[3584 - 3523];        /* 3523 */
	UINT64 padding3648[3648 - 3584];        /* 3584 */

	/*
	 * Bitmap Codecs Capabilities
	 */

	/* RemoteFX */
	ALIGN64 BOOL RemoteFxOnly;           /* 3648 */
	ALIGN64 BOOL RemoteFxCodec;          /* 3649 */
	ALIGN64 UINT32 RemoteFxCodecId;      /* 3650 */
	ALIGN64 UINT32 RemoteFxCodecMode;    /* 3651 */
	ALIGN64 BOOL RemoteFxImageCodec;     /* 3652 */
	ALIGN64 UINT32 RemoteFxCaptureFlags; /* 3653 */
	UINT64 padding3712[3712 - 3654];     /* 3654 */

	/* NSCodec */
	ALIGN64 BOOL NSCodec;                          /* 3712 */
	ALIGN64 UINT32 NSCodecId;                      /* 3713 */
	ALIGN64 UINT32 FrameAcknowledge;               /* 3714 */
	ALIGN64 UINT32 NSCodecColorLossLevel;          /* 3715 */
	ALIGN64 BOOL NSCodecAllowSubsampling;          /* 3716 */
	ALIGN64 BOOL NSCodecAllowDynamicColorFidelity; /* 3717 */
	UINT64 padding3776[3776 - 3718];               /* 3718 */

	/* JPEG */
	ALIGN64 BOOL JpegCodec;          /* 3776 */
	ALIGN64 UINT32 JpegCodecId;      /* 3777 */
	ALIGN64 UINT32 JpegQuality;      /* 3778 */
	UINT64 padding3840[3840 - 3779]; /* 3779 */

	ALIGN64 BOOL GfxThinClient;      /* 3840 */
	ALIGN64 BOOL GfxSmallCache;      /* 3841 */
	ALIGN64 BOOL GfxProgressive;     /* 3842 */
	ALIGN64 BOOL GfxProgressiveV2;   /* 3843 */
	ALIGN64 BOOL GfxH264;            /* 3844 */
	ALIGN64 BOOL GfxAVC444;          /* 3845 */
	ALIGN64 BOOL GfxSendQoeAck;      /* 3846 */
	ALIGN64 BOOL GfxAVC444v2;        /* 3847 */
	ALIGN64 UINT32 GfxCapsFilter;    /* 3848 */
	ALIGN64 BOOL GfxPlanar;          /* 3849 */
	UINT64 padding3904[3904 - 3850]; /* 3850 */

	/**
	 * Caches
	 */

	/* Bitmap Cache V3 */
	ALIGN64 UINT32 BitmapCacheV3CodecId; /* 3904 */
	UINT64 padding3968[3968 - 3905];     /* 3905 */

	/* Draw Nine Grid */
	ALIGN64 BOOL DrawNineGridEnabled;        /* 3968 */
	ALIGN64 UINT32 DrawNineGridCacheSize;    /* 3969 */
	ALIGN64 UINT32 DrawNineGridCacheEntries; /* 3970 */
	UINT64 padding4032[4032 - 3971];         /* 3971 */

	/* Draw GDI+ */
	ALIGN64 BOOL DrawGdiPlusEnabled;      /* 4032 */
	ALIGN64 BOOL DrawGdiPlusCacheEnabled; /* 4033 */
	UINT64 padding4096[4096 - 4034];      /* 4034 */
	UINT64 padding4160[4160 - 4096];      /* 4096 */

	/**
	 * Device Redirection
	 */

	/* Device Redirection */
	ALIGN64 BOOL DeviceRedirection;     /* 4160 */
	ALIGN64 UINT32 DeviceCount;         /* 4161 */
	ALIGN64 UINT32 DeviceArraySize;     /* 4162 */
	ALIGN64 RDPDR_DEVICE** DeviceArray; /* 4163 */
	ALIGN64 BOOL IgnoreInvalidDevices;  /* 4164 */
	UINT64 padding4288[4288 - 4165];    /* 4165 */

	/* Drive Redirection */
	ALIGN64 BOOL RedirectDrives;     /* 4288 */
	ALIGN64 BOOL RedirectHomeDrive;  /* 4289 */
	ALIGN64 char* DrivesToRedirect;  /* 4290 */
	UINT64 padding4416[4416 - 4291]; /* 4291 */

	/* Smartcard Redirection */
	ALIGN64 BOOL RedirectSmartCards; /* 4416 */
	/* WebAuthN Redirection */
	ALIGN64 BOOL RedirectWebAuthN;   /* 4417 */
	UINT64 padding4544[4544 - 4418]; /* 4418 */

	/* Printer Redirection */
	ALIGN64 BOOL RedirectPrinters;   /* 4544 */
	UINT64 padding4672[4672 - 4545]; /* 4545 */

	/* Serial and Parallel Port Redirection */
	ALIGN64 BOOL RedirectSerialPorts;   /* 4672 */
	ALIGN64 BOOL RedirectParallelPorts; /* 4673 */
	ALIGN64 BOOL PreferIPv6OverIPv4;    /* 4674 */
	UINT64 padding4800[4800 - 4675];    /* 4675 */

	/**
	 * Other Redirection
	 */

	ALIGN64 BOOL RedirectClipboard;      /* 4800 */
	ALIGN64 UINT32 ClipboardFeatureMask; /* 4801 */
	ALIGN64 char* ClipboardUseSelection; /* 4802 */
	UINT64 padding4928[4928 - 4803];     /* 4803 */

	/**
	 * Static Virtual Channels
	 */

	ALIGN64 UINT32 StaticChannelCount;       /* 4928 */
	ALIGN64 UINT32 StaticChannelArraySize;   /* 4929 */
	ALIGN64 ADDIN_ARGV** StaticChannelArray; /* 4930 */
	UINT64 padding5056[5056 - 4931];         /* 4931 */

	/**
	 * Dynamic Virtual Channels
	 */

	ALIGN64 UINT32 DynamicChannelCount;       /* 5056 */
	ALIGN64 UINT32 DynamicChannelArraySize;   /* 5057 */
	ALIGN64 ADDIN_ARGV** DynamicChannelArray; /* 5058 */
	ALIGN64 BOOL SupportDynamicChannels;      /* 5059 */
	UINT64 padding5184[5184 - 5060];          /* 5060 */

	ALIGN64 BOOL SupportEchoChannel;        /* 5184 */
	ALIGN64 BOOL SupportDisplayControl;     /* 5185 */
	ALIGN64 BOOL SupportGeometryTracking;   /* 5186 */
	ALIGN64 BOOL SupportSSHAgentChannel;    /* 5187 */
	ALIGN64 BOOL SupportVideoOptimized;     /* 5188 */
	ALIGN64 char* RDP2TCPArgs;              /* 5189 */
	ALIGN64 BOOL TcpKeepAlive;              /* 5190 */
	ALIGN64 UINT32 TcpKeepAliveRetries;     /* 5191 */
	ALIGN64 UINT32 TcpKeepAliveDelay;       /* 5192 */
	ALIGN64 UINT32 TcpKeepAliveInterval;    /* 5193 */
	ALIGN64 UINT32 TcpAckTimeout;           /* 5194 */
	ALIGN64 char* ActionScript;             /* 5195 */
	ALIGN64 UINT32 Floatbar;                /* 5196 */
	ALIGN64 UINT32 TcpConnectTimeout;       /* 5197 */
	ALIGN64 UINT32 FakeMouseMotionInterval; /* 5198 */
	UINT64 padding5312[5312 - 5199];        /* 5199 */

	/**
	 * WARNING: End of ABI stable zone!
	 *
	 * The zone below this point is ABI unstable, and
	 * is therefore potentially subject to ABI breakage.
	 */

	/*
	 * Extensions
	 */

	/* Extensions */
	ALIGN64 INT32 num_extensions;              /*  */
	ALIGN64 struct rdp_ext_set extensions[16]; /*  */

	ALIGN64 BYTE* SettingsModified; /* byte array marking fields that have been modified from
	                                   their default value - currently UNUSED! */
	ALIGN64 char* XSelectionAtom;
};

#endif /* FREERDP_SETTINGS_TYPES_PRIVATE_H */
