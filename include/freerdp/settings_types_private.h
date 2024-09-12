/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Internal settings header for functions not exported
 *
 * Copyright 2018 Armin Novak <armin.novak@thincast.com>
 * Copyright 2018 Thincast Technologies GmbH
 *
 * Licensed under the Apache License, Version 2.0 (the "License")
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
#include <freerdp/settings_types.h>

#include <string.h>

/** \addtogroup rdpSettings
 *  @{
 */

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef FREERDP_SETTINGS_INTERNAL_USE
#define SETTINGS_DEPRECATED(x) WINPR_DEPRECATED(x)
#else
#define SETTINGS_DEPRECATED(x) x
#endif

struct rdp_settings
{
	/**
	 * WARNING: this data structure is carefully padded for ABI stability!
	 * Keeping this area clean is particularly challenging, so unless you are
	 * a trusted developer you should NOT take the liberty of adding your own
	 * options straight into the ABI stable zone. Instead, append them to the
	 * very end of this data structure, in the zone marked as ABI unstable.
	 */

	SETTINGS_DEPRECATED(ALIGN64 void* instance); /* 0 */
	UINT64 padding001[16 - 1];                   /* 1 */

	/* Core Parameters */
	SETTINGS_DEPRECATED(ALIGN64 BOOL ServerMode);               /* 16 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 ShareId);                /* 17 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 PduSource);              /* 18 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 ServerPort);             /* 19 */
	SETTINGS_DEPRECATED(ALIGN64 char* ServerHostname);          /* 20 */
	SETTINGS_DEPRECATED(ALIGN64 char* Username);                /* 21 */
	SETTINGS_DEPRECATED(ALIGN64 char* Password);                /* 22 */
	SETTINGS_DEPRECATED(ALIGN64 char* Domain);                  /* 23 */
	SETTINGS_DEPRECATED(ALIGN64 char* PasswordHash);            /* 24 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL WaitForOutputBufferFlush); /* 25 */
	UINT64 padding26[27 - 26];                                  /* 26 */
	SETTINGS_DEPRECATED(ALIGN64 char* AcceptedCert);            /* 27 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 AcceptedCertLength);     /* 28 */
	SETTINGS_DEPRECATED(ALIGN64 char* UserSpecifiedServerName); /* 29 */
	SETTINGS_DEPRECATED(ALIGN64 char* AadServerHostname);       /** 30
		                                                         * @since version 3.1.0
		                                                         */
	UINT64 padding0064[64 - 31];                                /* 31 */
	/* resource management related options */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 ThreadingFlags); /* 64 */

	UINT64 padding0128[128 - 65]; /* 65 */

	/**
	 * GCC User Data Blocks
	 */

	/* Client/Server Core Data */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 RdpVersion);            /* 128 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 DesktopWidth);          /* 129 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 DesktopHeight);         /* 130 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 ColorDepth);            /* 131 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 ConnectionType);        /* 132 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 ClientBuild);           /* 133 */
	SETTINGS_DEPRECATED(ALIGN64 char* ClientHostname);         /* 134 */
	SETTINGS_DEPRECATED(ALIGN64 char* ClientProductId);        /* 135 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 EarlyCapabilityFlags);  /* 136 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL NetworkAutoDetect);       /* 137 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL SupportAsymetricKeys);    /* 138 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL SupportErrorInfoPdu);     /* 139 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL SupportStatusInfoPdu);    /* 140 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL SupportMonitorLayoutPdu); /* 141 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL SupportGraphicsPipeline); /* 142 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL SupportDynamicTimeZone);  /* 143 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL SupportHeartbeatPdu);     /* 144 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 DesktopPhysicalWidth);  /* 145 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 DesktopPhysicalHeight); /* 146 */
	SETTINGS_DEPRECATED(ALIGN64 UINT16 DesktopOrientation);    /* 147 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 DesktopScaleFactor);    /* 148 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 DeviceScaleFactor);     /* 149 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL SupportEdgeActionV1);     /* 150 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL SupportEdgeActionV2);     /* 151 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL SupportSkipChannelJoin);  /* 152 */
	SETTINGS_DEPRECATED(ALIGN64 UINT16 SupportedColorDepths);  /* 153 */
	UINT64 padding0192[192 - 154];                             /* 154 */

	/* Client/Server Security Data */
	SETTINGS_DEPRECATED(ALIGN64 BOOL UseRdpSecurityLayer);                /* 192 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 EncryptionMethods);                /* 193 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 ExtEncryptionMethods);             /* 194 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 EncryptionLevel);                  /* 195 */
	SETTINGS_DEPRECATED(ALIGN64 BYTE* ServerRandom);                      /* 196 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 ServerRandomLength);               /* 197 */
	SETTINGS_DEPRECATED(ALIGN64 BYTE* ServerCertificate);                 /* 198 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 ServerCertificateLength);          /* 199 */
	SETTINGS_DEPRECATED(ALIGN64 BYTE* ClientRandom);                      /* 200 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 ClientRandomLength);               /* 201 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL ServerLicenseRequired);              /* 202 */
	SETTINGS_DEPRECATED(ALIGN64 char* ServerLicenseCompanyName);          /* 203 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 ServerLicenseProductVersion);      /* 204 */
	SETTINGS_DEPRECATED(ALIGN64 char* ServerLicenseProductName);          /* 205 */
	SETTINGS_DEPRECATED(ALIGN64 char** ServerLicenseProductIssuers);      /* 206 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 ServerLicenseProductIssuersCount); /* 207 */
	UINT64 padding0256[256 - 208];                                        /* 208 */

	/* Client Network Data */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 ChannelCount);          /* 256 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 ChannelDefArraySize);   /* 257 */
	SETTINGS_DEPRECATED(ALIGN64 CHANNEL_DEF* ChannelDefArray); /* 258 */
	UINT64 padding0320[320 - 259];                             /* 259 */

	/* Client Cluster Data */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 ClusterInfoFlags);    /* 320 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 RedirectedSessionId); /* 321 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL ConsoleSession);        /* 322 */
	UINT64 padding0384[384 - 323];                           /* 323 */

	/* Client Monitor Data */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 MonitorCount);          /*    384 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 MonitorDefArraySize);   /*    385 */
	SETTINGS_DEPRECATED(ALIGN64 rdpMonitor* MonitorDefArray);  /*    386 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL SpanMonitors);            /*    387 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL UseMultimon);             /*    388 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL ForceMultimon);           /*    389 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 DesktopPosX);           /*    390 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 DesktopPosY);           /*    391 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL ListMonitors);            /*    392 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32* MonitorIds);           /*    393 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 NumMonitorIds);         /*    394 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 MonitorLocalShiftX);    /*395 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 MonitorLocalShiftY);    /*    396 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL HasMonitorAttributes);    /*    397 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 MonitorFlags);          /* 398 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 MonitorAttributeFlags); /* 399 */
	UINT64 padding0448[448 - 400];                             /* 400 */

	/* Client Message Channel Data */
	UINT64 padding0512[512 - 448]; /* 448 */

	/* Client Multitransport Channel Data */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 MultitransportFlags); /* 512 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL SupportMultitransport); /* 513 */
	UINT64 padding0576[576 - 514];                           /* 514 */
	UINT64 padding0640[640 - 576];                           /* 576 */

	/*
	 * Client Info
	 */

	/* Client Info (Shell) */
	SETTINGS_DEPRECATED(ALIGN64 char* AlternateShell);        /* 640 */
	SETTINGS_DEPRECATED(ALIGN64 char* ShellWorkingDirectory); /* 641 */
	UINT64 padding0704[704 - 642];                            /* 642 */

	/* Client Info Flags */
	SETTINGS_DEPRECATED(ALIGN64 BOOL AutoLogonEnabled);       /* 704 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL CompressionEnabled);     /* 705 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL DisableCtrlAltDel);      /* 706 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL EnableWindowsKey);       /* 707 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL MaximizeShell);          /* 708 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL LogonNotify);            /* 709 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL LogonErrors);            /* 710 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL MouseAttached);          /* 711 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL MouseHasWheel);          /* 712 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL RemoteConsoleAudio);     /* 713 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL AudioPlayback);          /* 714 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL AudioCapture);           /* 715 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL VideoDisable);           /* 716 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL PasswordIsSmartcardPin); /* 717 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL UsingSavedCredentials);  /* 718 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL ForceEncryptedCsPdu);    /* 719 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL HiDefRemoteApp);         /* 720 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 CompressionLevel);     /* 721 */
	UINT64 padding0768[768 - 722];                            /* 722 */

	/* Client Info (Extra) */
	SETTINGS_DEPRECATED(ALIGN64 BOOL IPv6Enabled);       /* 768 */
	SETTINGS_DEPRECATED(ALIGN64 char* ClientAddress);    /* 769 */
	SETTINGS_DEPRECATED(ALIGN64 char* ClientDir);        /* 770 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 ClientSessionId); /* 771 */
	UINT64 padding0832[832 - 772];                       /* 772 */

	/* Client Info (Auto Reconnection) */
	SETTINGS_DEPRECATED(ALIGN64 BOOL AutoReconnectionEnabled);                     /* 832 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 AutoReconnectMaxRetries);                   /* 833 */
	SETTINGS_DEPRECATED(ALIGN64 ARC_CS_PRIVATE_PACKET* ClientAutoReconnectCookie); /* 834 */
	SETTINGS_DEPRECATED(ALIGN64 ARC_SC_PRIVATE_PACKET* ServerAutoReconnectCookie); /* 835 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL PrintReconnectCookie);                        /* 836 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL AutoReconnectionPacketSupported);             /** 837
		                                                                             @since version 3.5.0
*/
	UINT64 padding0896[896 - 838];                                                 /* 838 */

	/* Client Info (Time Zone) */
	SETTINGS_DEPRECATED(ALIGN64 TIME_ZONE_INFORMATION* ClientTimeZone); /* 896 */
	SETTINGS_DEPRECATED(ALIGN64 char* DynamicDSTTimeZoneKeyName);       /* 897 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL DynamicDaylightTimeDisabled);      /* 898 */
	UINT64 padding0960[960 - 899];                                      /* 899 */

	/* Client Info (Performance Flags) */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 PerformanceFlags);      /* 960 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL AllowFontSmoothing);      /* 961 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL DisableWallpaper);        /* 962 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL DisableFullWindowDrag);   /* 963 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL DisableMenuAnims);        /* 964 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL DisableThemes);           /* 965 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL DisableCursorShadow);     /* 966 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL DisableCursorBlinking);   /* 967 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL AllowDesktopComposition); /* 968 */
	UINT64 padding1024[1024 - 969];                            /* 969 */

	/* Remote Assistance */
	SETTINGS_DEPRECATED(ALIGN64 BOOL RemoteAssistanceMode);           /* 1024 */
	SETTINGS_DEPRECATED(ALIGN64 char* RemoteAssistanceSessionId);     /* 1025 */
	SETTINGS_DEPRECATED(ALIGN64 char* RemoteAssistancePassStub);      /* 1026 */
	SETTINGS_DEPRECATED(ALIGN64 char* RemoteAssistancePassword);      /* 1027 */
	SETTINGS_DEPRECATED(ALIGN64 char* RemoteAssistanceRCTicket);      /* 1028 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL EncomspVirtualChannel);          /* 1029 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL RemdeskVirtualChannel);          /* 1030 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL LyncRdpMode);                    /* 1031 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL RemoteAssistanceRequestControl); /* 1032 */
	UINT64 padding1088[1088 - 1033];                                  /* 1033 */

	/**
	 * X.224 Connection Request/Confirm
	 */

	/* Protocol Security */
	SETTINGS_DEPRECATED(ALIGN64 BOOL TlsSecurity);                  /* 1088 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL NlaSecurity);                  /* 1089 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL RdpSecurity);                  /* 1090 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL ExtSecurity);                  /* 1091 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL Authentication);               /* 1092 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 RequestedProtocols);         /* 1093 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 SelectedProtocol);           /* 1094 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 NegotiationFlags);           /* 1095 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL NegotiateSecurityLayer);       /* 1096 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL RestrictedAdminModeRequired);  /* 1097 */
	SETTINGS_DEPRECATED(ALIGN64 char* AuthenticationServiceClass);  /* 1098 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL DisableCredentialsDelegation); /* 1099 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 AuthenticationLevel);        /* 1100 */
	SETTINGS_DEPRECATED(ALIGN64 char* AllowedTlsCiphers);           /* 1101 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL VmConnectMode);                /* 1102 */
	SETTINGS_DEPRECATED(ALIGN64 char* NtlmSamFile);                 /* 1103 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL FIPSMode);                     /* 1104 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 TlsSecLevel);                /* 1105 */
	SETTINGS_DEPRECATED(ALIGN64 char* SspiModule);                  /* 1106 */
	SETTINGS_DEPRECATED(ALIGN64 UINT16 TLSMinVersion);              /* 1107 */
	SETTINGS_DEPRECATED(ALIGN64 UINT16 TLSMaxVersion);              /* 1108 */
	SETTINGS_DEPRECATED(ALIGN64 char* TlsSecretsFile);              /* 1109 */
	SETTINGS_DEPRECATED(ALIGN64 char* AuthenticationPackageList);   /* 1110 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL RdstlsSecurity);               /* 1111 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL AadSecurity);                  /* 1112 */
	SETTINGS_DEPRECATED(ALIGN64 char* WinSCardModule);              /* 1113 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL RemoteCredentialGuard);        /* 1114 */
	UINT64 padding1152[1152 - 1115];                                /* 1115 */

	/* Connection Cookie */
	SETTINGS_DEPRECATED(ALIGN64 BOOL MstscCookieMode);      /* 1152 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 CookieMaxLength);    /* 1153 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 PreconnectionId);    /* 1154 */
	SETTINGS_DEPRECATED(ALIGN64 char* PreconnectionBlob);   /* 1155 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL SendPreconnectionPdu); /* 1156 */
	UINT64 padding1216[1216 - 1157];                        /* 1157 */

	/* Server Redirection */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 RedirectionFlags);                      /* 1216 */
	SETTINGS_DEPRECATED(ALIGN64 char* TargetNetAddress);                       /* 1217 */
	SETTINGS_DEPRECATED(ALIGN64 BYTE* LoadBalanceInfo);                        /* 1218 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 LoadBalanceInfoLength);                 /* 1219 */
	SETTINGS_DEPRECATED(ALIGN64 char* RedirectionUsername);                    /* 1220 */
	SETTINGS_DEPRECATED(ALIGN64 char* RedirectionDomain);                      /* 1221 */
	SETTINGS_DEPRECATED(ALIGN64 BYTE* RedirectionPassword);                    /* 1222 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 RedirectionPasswordLength);             /* 1223 */
	SETTINGS_DEPRECATED(ALIGN64 char* RedirectionTargetFQDN);                  /* 1224 */
	SETTINGS_DEPRECATED(ALIGN64 char* RedirectionTargetNetBiosName);           /* 1225 */
	SETTINGS_DEPRECATED(ALIGN64 BYTE* RedirectionTsvUrl);                      /* 1226 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 RedirectionTsvUrlLength);               /* 1227 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 TargetNetAddressCount);                 /* 1228 */
	SETTINGS_DEPRECATED(ALIGN64 char** TargetNetAddresses);                    /* 1229 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32* TargetNetPorts);                       /* 1230 */
	SETTINGS_DEPRECATED(ALIGN64 char* RedirectionAcceptedCert);                /* 1231 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 RedirectionAcceptedCertLength);         /* 1232 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 RedirectionPreferType);                 /* 1233 */
	SETTINGS_DEPRECATED(ALIGN64 BYTE* RedirectionGuid);                        /* 1234 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 RedirectionGuidLength);                 /* 1235 */
	SETTINGS_DEPRECATED(ALIGN64 rdpCertificate* RedirectionTargetCertificate); /* 1236 */
	UINT64 padding1280[1280 - 1237];                                           /* 1237 */

	/**
	 * Security
	 */

	/* Credentials Cache */
	SETTINGS_DEPRECATED(ALIGN64 BYTE* Password51);          /* 1280 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 Password51Length);   /* 1281 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL SmartcardLogon);       /* 1282 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL PromptForCredentials); /* 1283 */
	UINT64 padding1284[1285 - 1284];                        /* 1284 */

	/* Settings used for smartcard emulation */
	SETTINGS_DEPRECATED(ALIGN64 char* SmartcardCertificate); /* 1285 */
	SETTINGS_DEPRECATED(ALIGN64 char* SmartcardPrivateKey);  /* 1286 */
	UINT64 padding1287[1288 - 1287];                         /* 1287 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL SmartcardEmulation);    /* 1288 */
	SETTINGS_DEPRECATED(ALIGN64 char* Pkcs11Module);         /* 1289 */
	SETTINGS_DEPRECATED(ALIGN64 char* PkinitAnchors);        /* 1290 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 KeySpec);             /* 1291 */
	SETTINGS_DEPRECATED(ALIGN64 char* CardName);             /* 1292 */
	SETTINGS_DEPRECATED(ALIGN64 char* ReaderName);           /* 1293 */
	SETTINGS_DEPRECATED(ALIGN64 char* ContainerName);        /* 1294 */
	SETTINGS_DEPRECATED(ALIGN64 char* CspName);              /* 1295 */
	UINT64 padding1344[1344 - 1296];                         /* 1296 */

	/* Kerberos Authentication */
	SETTINGS_DEPRECATED(ALIGN64 char* KerberosKdcUrl);            /* 1344 */
	SETTINGS_DEPRECATED(ALIGN64 char* KerberosRealm);             /* 1345 */
	SETTINGS_DEPRECATED(ALIGN64 char* KerberosStartTime);         /* 1346 */
	SETTINGS_DEPRECATED(ALIGN64 char* KerberosLifeTime);          /* 1347 */
	SETTINGS_DEPRECATED(ALIGN64 char* KerberosRenewableLifeTime); /* 1348 */
	SETTINGS_DEPRECATED(ALIGN64 char* KerberosCache);             /* 1349 */
	SETTINGS_DEPRECATED(ALIGN64 char* KerberosArmor);             /* 1350 */
	SETTINGS_DEPRECATED(ALIGN64 char* KerberosKeytab);            /* 1351 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL KerberosRdgIsProxy);         /* 1352 */
	UINT64 padding1408[1408 - 1353];                              /* 1353 */

	/* Server Certificate */
	SETTINGS_DEPRECATED(ALIGN64 BOOL IgnoreCertificate);                /* 1408 */
	SETTINGS_DEPRECATED(ALIGN64 char* CertificateName);                 /* 1409 */
	UINT64 padding1410[1413 - 1410];                                    /* 1410 */
	SETTINGS_DEPRECATED(ALIGN64 rdpPrivateKey* RdpServerRsaKey);        /* 1413 */
	SETTINGS_DEPRECATED(ALIGN64 rdpCertificate* RdpServerCertificate);  /* 1414 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL ExternalCertificateManagement);    /* 1415 */
	UINT64 padding1416[1419 - 1416];                                    /* 1416 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL AutoAcceptCertificate);            /* 1419 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL AutoDenyCertificate);              /* 1420 */
	SETTINGS_DEPRECATED(ALIGN64 char* CertificateAcceptedFingerprints); /* 1421 */
	UINT64 padding1422[1423 - 1422];                                    /* 1422 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL CertificateCallbackPreferPEM);     /* 1423 */
	UINT64 padding1472[1472 - 1424];                                    /* 1424 */
	UINT64 padding1536[1536 - 1472];                                    /* 1472 */

	/**
	 * User Interface
	 */

	/* Window Settings */
	SETTINGS_DEPRECATED(ALIGN64 BOOL Workarea);                /* 1536 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL Fullscreen);              /* 1537 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 PercentScreen);         /* 1538 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL GrabKeyboard);            /* 1539 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL Decorations);             /* 1540 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL MouseMotion);             /* 1541 */
	SETTINGS_DEPRECATED(ALIGN64 char* WindowTitle);            /* 1542 */
	SETTINGS_DEPRECATED(ALIGN64 UINT64 ParentWindowId);        /* 1543 */
	UINT64 padding1544[1545 - 1544];                           /* 1544 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL AsyncUpdate);             /* 1545 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL AsyncChannels);           /* 1546 */
	UINT64 padding1548[1548 - 1547];                           /* 1547 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL ToggleFullscreen);        /* 1548 */
	SETTINGS_DEPRECATED(ALIGN64 char* WmClass);                /* 1549 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL EmbeddedWindow);          /* 1550 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL SmartSizing);             /* 1551 */
	SETTINGS_DEPRECATED(ALIGN64 INT32 XPan);                   /* 1552 */
	SETTINGS_DEPRECATED(ALIGN64 INT32 YPan);                   /* 1553 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 SmartSizingWidth);      /* 1554 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 SmartSizingHeight);     /* 1555 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL PercentScreenUseWidth);   /* 1556 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL PercentScreenUseHeight);  /* 1557 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL DynamicResolutionUpdate); /* 1558 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL GrabMouse);               /* 1559 */
	UINT64 padding1601[1601 - 1560];                           /* 1560 */

	/* Miscellaneous */
	SETTINGS_DEPRECATED(ALIGN64 BOOL SoftwareGdi);             /* 1601 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL LocalConnection);         /* 1602 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL AuthenticationOnly);      /* 1603 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL CredentialsFromStdin);    /* 1604 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL UnmapButtons);            /* 1605 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL OldLicenseBehaviour);     /* 1606 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL MouseUseRelativeMove);    /* 1607 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL UseCommonStdioCallbacks); /* 1608 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL ConnectChildSession);     /* 1609 */
	UINT64 padding1664[1664 - 1610];                           /* 1610 */

	/* Names */
	SETTINGS_DEPRECATED(ALIGN64 char* ComputerName); /* 1664 */
	UINT64 padding1728[1728 - 1665];                 /* 1665 */

	/* Files */
	SETTINGS_DEPRECATED(ALIGN64 char* ConnectionFile); /* 1728 */
	SETTINGS_DEPRECATED(ALIGN64 char* AssistanceFile); /* 1729 */
	UINT64 padding1792[1792 - 1730];                   /* 1730 */

	/* Paths */
	SETTINGS_DEPRECATED(ALIGN64 char* HomePath);    /* 1792 */
	SETTINGS_DEPRECATED(ALIGN64 char* ConfigPath);  /* 1793 */
	SETTINGS_DEPRECATED(ALIGN64 char* CurrentPath); /* 1794 */
	UINT64 padding1856[1856 - 1795];                /* 1795 */

	/* Recording */
	SETTINGS_DEPRECATED(ALIGN64 BOOL DumpRemoteFx);             /* 1856 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL PlayRemoteFx);             /* 1857 */
	SETTINGS_DEPRECATED(ALIGN64 char* DumpRemoteFxFile);        /* 1858 */
	SETTINGS_DEPRECATED(ALIGN64 char* PlayRemoteFxFile);        /* 1859 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL TransportDump);            /* 1860 */
	SETTINGS_DEPRECATED(ALIGN64 char* TransportDumpFile);       /* 1861 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL TransportDumpReplay);      /* 1862 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL DeactivateClientDecoding); /* 1863 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL TransportDumpReplayNodelay); /** 1864
		                                                           * @since version 3.6.0
		                                                           */
	UINT64 padding1920[1920 - 1865];                              /* 1865 */
	UINT64 padding1984[1984 - 1920];                            /* 1920 */

	/**
	 * Gateway
	 */

	/* Gateway */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 GatewayUsageMethod);            /* 1984 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 GatewayPort);                   /* 1985 */
	SETTINGS_DEPRECATED(ALIGN64 char* GatewayHostname);                /* 1986 */
	SETTINGS_DEPRECATED(ALIGN64 char* GatewayUsername);                /* 1987 */
	SETTINGS_DEPRECATED(ALIGN64 char* GatewayPassword);                /* 1988 */
	SETTINGS_DEPRECATED(ALIGN64 char* GatewayDomain);                  /* 1989 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 GatewayCredentialsSource);      /* 1990 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL GatewayUseSameCredentials);       /* 1991 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL GatewayEnabled);                  /* 1992 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL GatewayBypassLocal);              /* 1993 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL GatewayRpcTransport);             /* 1994 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL GatewayHttpTransport);            /* 1995 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL GatewayUdpTransport);             /* 1996 */
	SETTINGS_DEPRECATED(ALIGN64 char* GatewayAccessToken);             /* 1997 */
	SETTINGS_DEPRECATED(ALIGN64 char* GatewayAcceptedCert);            /* 1998 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 GatewayAcceptedCertLength);     /* 1999 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL GatewayHttpUseWebsockets);        /* 2000 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL GatewayHttpExtAuthSspiNtlm);      /* 2001 */
	SETTINGS_DEPRECATED(ALIGN64 char* GatewayHttpExtAuthBearer);       /* 2002 */
	SETTINGS_DEPRECATED(ALIGN64 char* GatewayUrl);                     /* 2003 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL GatewayArmTransport);             /* 2004 */
	SETTINGS_DEPRECATED(ALIGN64 char* GatewayAvdWvdEndpointPool);      /* 2005 */
	SETTINGS_DEPRECATED(ALIGN64 char* GatewayAvdGeo);                  /* 2006 */
	SETTINGS_DEPRECATED(ALIGN64 char* GatewayAvdArmpath);              /* 2007 */
	SETTINGS_DEPRECATED(ALIGN64 char* GatewayAvdAadtenantid);          /* 2008 */
	SETTINGS_DEPRECATED(ALIGN64 char* GatewayAvdDiagnosticserviceurl); /* 2009 */
	SETTINGS_DEPRECATED(ALIGN64 char* GatewayAvdHubdiscoverygeourl);   /* 2010 */
	SETTINGS_DEPRECATED(ALIGN64 char* GatewayAvdActivityhint);         /* 2011 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL GatewayIgnoreRedirectionPolicy);  /** 2012
		                                                                * @since version 3.4.0
		                                                                */
	UINT64 padding2015[2015 - 2013];                                   /* 2013 */

	/* Proxy */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 ProxyType);    /* 2015 */
	SETTINGS_DEPRECATED(ALIGN64 char* ProxyHostname); /* 2016 */
	SETTINGS_DEPRECATED(ALIGN64 UINT16 ProxyPort);    /* 2017 */
	SETTINGS_DEPRECATED(ALIGN64 char* ProxyUsername); /* 2018 */
	SETTINGS_DEPRECATED(ALIGN64 char* ProxyPassword); /* 2019 */
	UINT64 padding2112[2112 - 2020];                  /* 2020 */

	/**
	 * RemoteApp
	 */

	/* RemoteApp */
	SETTINGS_DEPRECATED(ALIGN64 BOOL RemoteApplicationMode);               /* 2112 */
	SETTINGS_DEPRECATED(ALIGN64 char* RemoteApplicationName);              /* 2113 */
	SETTINGS_DEPRECATED(ALIGN64 char* RemoteApplicationIcon);              /* 2114 */
	SETTINGS_DEPRECATED(ALIGN64 char* RemoteApplicationProgram);           /* 2115 */
	SETTINGS_DEPRECATED(ALIGN64 char* RemoteApplicationFile);              /* 2116 */
	SETTINGS_DEPRECATED(ALIGN64 char* RemoteApplicationGuid);              /* 2117 */
	SETTINGS_DEPRECATED(ALIGN64 char* RemoteApplicationCmdLine);           /* 2118 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 RemoteApplicationExpandCmdLine);    /* 2119 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 RemoteApplicationExpandWorkingDir); /* 2120 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL DisableRemoteAppCapsCheck);           /* 2121 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 RemoteAppNumIconCaches);            /* 2122 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 RemoteAppNumIconCacheEntries);      /* 2123 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL RemoteAppLanguageBarSupported);       /* 2124 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 RemoteWndSupportLevel);             /* 2125 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 RemoteApplicationSupportLevel);     /* 2126 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 RemoteApplicationSupportMask);      /* 2127 */
	SETTINGS_DEPRECATED(ALIGN64 char* RemoteApplicationWorkingDir);        /* 2128 */
	UINT64 padding2176[2176 - 2129];                                       /* 2129 */
	UINT64 padding2240[2240 - 2176];                                       /* 2176 */

	/**
	 * Mandatory Capabilities
	 */

	/* Capabilities */
	SETTINGS_DEPRECATED(ALIGN64 BYTE* ReceivedCapabilities);          /* 2240 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 ReceivedCapabilitiesSize);     /* 2241 */
	SETTINGS_DEPRECATED(ALIGN64 BYTE** ReceivedCapabilityData);       /* 2242 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32* ReceivedCapabilityDataSizes); /* 2243 */
	UINT64 padding2304[2304 - 2244];                                  /* 2244 */

	/* General Capabilities */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 OsMajorType);                 /* 2304 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 OsMinorType);                 /* 2305 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL RefreshRect);                   /* 2306 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL SuppressOutput);                /* 2307 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL FastPathOutput);                /* 2308 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL SaltedChecksum);                /* 2309 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL LongCredentialsSupported);      /* 2310 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL NoBitmapCompressionHeader);     /* 2311 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL BitmapCompressionDisabled);     /* 2312 */
	SETTINGS_DEPRECATED(ALIGN64 UINT16 CapsProtocolVersion);         /* 2313 */
	SETTINGS_DEPRECATED(ALIGN64 UINT16 CapsGeneralCompressionTypes); /* 2314 */
	SETTINGS_DEPRECATED(ALIGN64 UINT16 CapsUpdateCapabilityFlag);    /* 2315 */
	SETTINGS_DEPRECATED(ALIGN64 UINT16 CapsRemoteUnshareFlag);       /* 2316 */
	SETTINGS_DEPRECATED(ALIGN64 UINT16 CapsGeneralCompressionLevel); /* 2317 */
	UINT64 padding2368[2368 - 2318];                                 /* 2318 */

	/* Bitmap Capabilities */
	SETTINGS_DEPRECATED(ALIGN64 BOOL DesktopResize);                 /* 2368 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL DrawAllowDynamicColorFidelity); /* 2369 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL DrawAllowColorSubsampling);     /* 2370 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL DrawAllowSkipAlpha);            /* 2371 */
	UINT64 padding2432[2432 - 2372];                                 /* 2372 */

	/* Order Capabilities */
	SETTINGS_DEPRECATED(ALIGN64 BYTE* OrderSupport);                   /* 2432 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL BitmapCacheV3Enabled);            /* 2433 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL AltSecFrameMarkerSupport);        /* 2434 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL AllowUnanouncedOrdersFromServer); /* 2435 */
	SETTINGS_DEPRECATED(ALIGN64 UINT16 OrderSupportFlags);             /* 2436 */
	SETTINGS_DEPRECATED(ALIGN64 UINT16 OrderSupportFlagsEx);           /* 2437 */
	SETTINGS_DEPRECATED(ALIGN64 char* TerminalDescriptor);             /* 2438 */
	SETTINGS_DEPRECATED(ALIGN64 UINT16 TextANSICodePage);              /* 2439 */
	UINT64 padding2497[2497 - 2440];                                   /* 2440 */

	/* Bitmap Cache Capabilities */
	SETTINGS_DEPRECATED(ALIGN64 BOOL BitmapCacheEnabled);                          /* 2497 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 BitmapCacheVersion);                        /* 2498 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL AllowCacheWaitingList);                       /* 2499 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL BitmapCachePersistEnabled);                   /* 2500 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 BitmapCacheV2NumCells);                     /* 2501 */
	SETTINGS_DEPRECATED(ALIGN64 BITMAP_CACHE_V2_CELL_INFO* BitmapCacheV2CellInfo); /* 2502 */
	SETTINGS_DEPRECATED(ALIGN64 char* BitmapCachePersistFile);                     /* 2503 */
	UINT64 padding2560[2560 - 2504];                                               /* 2504 */

	/* Pointer Capabilities */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 ColorPointerCacheSize); /* 2560 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 PointerCacheSize);      /* 2561 */
	UINT64 padding2624[2622 - 2562];                           /* 2562 */

	/* Input Capabilities */
	SETTINGS_DEPRECATED(ALIGN64 char* KeyboardRemappingList); /* 2622 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 KeyboardCodePage);     /* 2623 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 KeyboardLayout);       /* 2624 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 KeyboardType);         /* 2625 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 KeyboardSubType);      /* 2626 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 KeyboardFunctionKey);  /* 2627 */
	SETTINGS_DEPRECATED(ALIGN64 char* ImeFileName);           /* 2628 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL UnicodeInput);           /* 2629 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL FastPathInput);          /* 2630 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL MultiTouchInput);        /* 2631 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL MultiTouchGestures);     /* 2632 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 KeyboardHook);         /* 2633 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL HasHorizontalWheel);     /* 2634 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL HasExtendedMouseEvent);  /* 2635 */

	/** SuspendInput disables processing of keyboard/mouse/multitouch input.
	 * If used by an implementation ensure proper state resync after reenabling
	 * input
	 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL SuspendInput);          /* 2636 */
	SETTINGS_DEPRECATED(ALIGN64 char* KeyboardPipeName);     /* 2637 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL HasRelativeMouseEvent); /* 2638 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL HasQoeEvent);           /* 2639 */
	UINT64 padding2688[2688 - 2640];                         /* 2640 */

	/* Brush Capabilities */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 BrushSupportLevel); /* 2688 */
	UINT64 padding2752[2752 - 2689];                       /* 2689 */

	/* Glyph Cache Capabilities */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 GlyphSupportLevel);           /* 2752 */
	SETTINGS_DEPRECATED(ALIGN64 GLYPH_CACHE_DEFINITION* GlyphCache); /* 2753 */
	SETTINGS_DEPRECATED(ALIGN64 GLYPH_CACHE_DEFINITION* FragCache);  /* 2754 */
	UINT64 padding2816[2816 - 2755];                                 /* 2755 */

	/* Offscreen Bitmap Cache */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 OffscreenSupportLevel); /* 2816 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 OffscreenCacheSize);    /* 2817 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 OffscreenCacheEntries); /* 2818 */
	UINT64 padding2880[2880 - 2819];                           /* 2819 */

	/* Virtual Channel Capabilities */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 VCFlags);     /* 2880 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 VCChunkSize); /* 2881 */
	UINT64 padding2944[2944 - 2882];                 /* 2882 */

	/* Sound Capabilities */
	SETTINGS_DEPRECATED(ALIGN64 BOOL SoundBeepsEnabled); /* 2944 */
	UINT64 padding3008[3008 - 2945];                     /* 2945 */
	UINT64 padding3072[3072 - 3008];                     /* 3008 */

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
	SETTINGS_DEPRECATED(ALIGN64 UINT32 MultifragMaxRequestSize); /* 3328 */
	UINT64 padding3392[3392 - 3329];                             /* 3329 */

	/* Large Pointer Update Capabilities */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 LargePointerFlag); /* 3392 */
	UINT64 padding3456[3456 - 3393];                      /* 3393 */

	/* Desktop Composition Capabilities */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 CompDeskSupportLevel); /* 3456 */
	UINT64 padding3520[3520 - 3457];                          /* 3457 */

	/* Surface Commands Capabilities */
	SETTINGS_DEPRECATED(ALIGN64 BOOL SurfaceCommandsEnabled);    /* 3520 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL FrameMarkerCommandEnabled); /* 3521 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL SurfaceFrameMarkerEnabled); /* 3522 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 SurfaceCommandsSupported); /** 3523
		                                                           * @since version 3.7.0
		                                                           */
	UINT64 padding3584[3584 - 3524];                              /* 3524 */
	UINT64 padding3648[3648 - 3584];                             /* 3584 */

	/*
	 * Bitmap Codecs Capabilities
	 */

	/* RemoteFX */
	SETTINGS_DEPRECATED(ALIGN64 BOOL RemoteFxOnly);           /* 3648 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL RemoteFxCodec);          /* 3649 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 RemoteFxCodecId);      /* 3650 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 RemoteFxCodecMode);    /* 3651 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL RemoteFxImageCodec);     /* 3652 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 RemoteFxCaptureFlags); /* 3653 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 RemoteFxRlgrMode);     /** 3654
		                                                       * @since version 3.7.0
		                                                       */
	UINT64 padding3712[3712 - 3655];                          /* 3655 */

	/* NSCodec */
	SETTINGS_DEPRECATED(ALIGN64 BOOL NSCodec);                          /* 3712 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 NSCodecId);                      /* 3713 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 FrameAcknowledge);               /* 3714 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 NSCodecColorLossLevel);          /* 3715 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL NSCodecAllowSubsampling);          /* 3716 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL NSCodecAllowDynamicColorFidelity); /* 3717 */
	UINT64 padding3776[3776 - 3718];                                    /* 3718 */

	/* JPEG */
	SETTINGS_DEPRECATED(ALIGN64 BOOL JpegCodec);     /* 3776 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 JpegCodecId); /* 3777 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 JpegQuality); /* 3778 */
	UINT64 padding3840[3840 - 3779];                 /* 3779 */

	SETTINGS_DEPRECATED(ALIGN64 BOOL GfxThinClient);    /* 3840 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL GfxSmallCache);    /* 3841 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL GfxProgressive);   /* 3842 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL GfxProgressiveV2); /* 3843 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL GfxH264);          /* 3844 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL GfxAVC444);        /* 3845 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL GfxSendQoeAck);    /* 3846 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL GfxAVC444v2);      /* 3847 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 GfxCapsFilter);  /* 3848 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL GfxPlanar);        /* 3849 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL GfxSuspendFrameAck); /** 3850
		                                                   * @since version 3.6.0
		                                                   */
	UINT64 padding3904[3904 - 3851];                      /* 3851 */

	/**
	 * Caches
	 */

	/* Bitmap Cache V3 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 BitmapCacheV3CodecId); /* 3904 */
	UINT64 padding3968[3968 - 3905];                          /* 3905 */

	/* Draw Nine Grid */
	SETTINGS_DEPRECATED(ALIGN64 BOOL DrawNineGridEnabled);        /* 3968 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 DrawNineGridCacheSize);    /* 3969 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 DrawNineGridCacheEntries); /* 3970 */
	UINT64 padding4032[4032 - 3971];                              /* 3971 */

	/* Draw GDI+ */
	SETTINGS_DEPRECATED(ALIGN64 BOOL DrawGdiPlusEnabled);      /* 4032 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL DrawGdiPlusCacheEnabled); /* 4033 */
	UINT64 padding4096[4096 - 4034];                           /* 4034 */
	UINT64 padding4160[4160 - 4096];                           /* 4096 */

	/**
	 * Device Redirection
	 */

	/* Device Redirection */
	SETTINGS_DEPRECATED(ALIGN64 BOOL DeviceRedirection);     /* 4160 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 DeviceCount);         /* 4161 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 DeviceArraySize);     /* 4162 */
	SETTINGS_DEPRECATED(ALIGN64 RDPDR_DEVICE** DeviceArray); /* 4163 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL IgnoreInvalidDevices);  /* 4164 */
	UINT64 padding4288[4288 - 4165];                         /* 4165 */

	/* Drive Redirection */
	SETTINGS_DEPRECATED(ALIGN64 BOOL RedirectDrives);    /* 4288 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL RedirectHomeDrive); /* 4289 */
	SETTINGS_DEPRECATED(ALIGN64 char* DrivesToRedirect); /* 4290 */
	UINT64 padding4416[4416 - 4291];                     /* 4291 */

	/* Smartcard Redirection */
	SETTINGS_DEPRECATED(ALIGN64 BOOL RedirectSmartCards); /* 4416 */
	/* WebAuthN Redirection */
	SETTINGS_DEPRECATED(ALIGN64 BOOL RedirectWebAuthN); /* 4417 */
	UINT64 padding4544[4544 - 4418];                    /* 4418 */

	/* Printer Redirection */
	SETTINGS_DEPRECATED(ALIGN64 BOOL RedirectPrinters); /* 4544 */
	UINT64 padding4672[4672 - 4545];                    /* 4545 */

	/* Serial and Parallel Port Redirection */
	SETTINGS_DEPRECATED(ALIGN64 BOOL RedirectSerialPorts);   /* 4672 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL RedirectParallelPorts); /* 4673 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL PreferIPv6OverIPv4);    /* 4674 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 ForceIPvX);           /** 4675
		                                                      * @since version 3.6.0
		                                                      */
	UINT64 padding4800[4800 - 4676];                         /* 4676 */

	/**
	 * Other Redirection
	 */

	SETTINGS_DEPRECATED(ALIGN64 BOOL RedirectClipboard);      /* 4800 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 ClipboardFeatureMask); /* 4801 */
	SETTINGS_DEPRECATED(ALIGN64 char* ClipboardUseSelection); /* 4802 */
	SETTINGS_DEPRECATED(UINT64 padding4928[4928 - 4803]);     /* 4803 */

	/**
	 * Static Virtual Channels
	 */

	SETTINGS_DEPRECATED(ALIGN64 UINT32 StaticChannelCount);       /* 4928 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 StaticChannelArraySize);   /* 4929 */
	SETTINGS_DEPRECATED(ALIGN64 ADDIN_ARGV** StaticChannelArray); /* 4930 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL SynchronousStaticChannels);  /** 4931
		                                                           * @since version 3.3.0 */
	UINT64 padding5056[5056 - 4932];                              /* 4932 */

	/**
	 * Dynamic Virtual Channels
	 */

	SETTINGS_DEPRECATED(ALIGN64 UINT32 DynamicChannelCount);       /* 5056 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 DynamicChannelArraySize);   /* 5057 */
	SETTINGS_DEPRECATED(ALIGN64 ADDIN_ARGV** DynamicChannelArray); /* 5058 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL SupportDynamicChannels);      /* 5059 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL SynchronousDynamicChannels);  /** 5060
		                                                            * @since version 3.2.0
		                                                            */
	UINT64 padding5184[5184 - 5061];                               /* 5061 */

	SETTINGS_DEPRECATED(ALIGN64 BOOL SupportEchoChannel);        /* 5184 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL SupportDisplayControl);     /* 5185 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL SupportGeometryTracking);   /* 5186 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL SupportSSHAgentChannel);    /* 5187 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL SupportVideoOptimized);     /* 5188 */
	SETTINGS_DEPRECATED(ALIGN64 char* RDP2TCPArgs);              /* 5189 */
	SETTINGS_DEPRECATED(ALIGN64 BOOL TcpKeepAlive);              /* 5190 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 TcpKeepAliveRetries);     /* 5191 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 TcpKeepAliveDelay);       /* 5192 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 TcpKeepAliveInterval);    /* 5193 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 TcpAckTimeout);           /* 5194 */
	SETTINGS_DEPRECATED(ALIGN64 char* ActionScript);             /* 5195 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 Floatbar);                /* 5196 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 TcpConnectTimeout);       /* 5197 */
	SETTINGS_DEPRECATED(ALIGN64 UINT32 FakeMouseMotionInterval); /* 5198 */
	UINT64 padding5312[5312 - 5199];                             /* 5199 */

	/**
	 * WARNING: End of ABI stable zone!
	 *
	 * The zone below this point is ABI unstable, and
	 * is therefore potentially subject to ABI breakage.
	 */
};

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* FREERDP_SETTINGS_TYPES_PRIVATE_H */
