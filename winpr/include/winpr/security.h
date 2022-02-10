/**
 * WinPR: Windows Portable Runtime
 * Security Definitions
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

#ifndef WINPR_SECURITY_H
#define WINPR_SECURITY_H

#include <winpr/winpr.h>
#include <winpr/wtypes.h>

/**
 * Windows Integrity Mechanism Design:
 * http://msdn.microsoft.com/en-us/library/bb625963.aspx
 */

#ifndef _WIN32

#include <winpr/nt.h>

#define ANYSIZE_ARRAY 1

typedef enum
{
	SecurityAnonymous,
	SecurityIdentification,
	SecurityImpersonation,
	SecurityDelegation
} SECURITY_IMPERSONATION_LEVEL,
    *PSECURITY_IMPERSONATION_LEVEL;

#define SECURITY_MAX_IMPERSONATION_LEVEL SecurityDelegation
#define SECURITY_MIN_IMPERSONATION_LEVEL SecurityAnonymous
#define DEFAULT_IMPERSONATION_LEVEL SecurityImpersonation
#define VALID_IMPERSONATION_LEVEL(L) \
	(((L) >= SECURITY_MIN_IMPERSONATION_LEVEL) && ((L) <= SECURITY_MAX_IMPERSONATION_LEVEL))

#define TOKEN_ASSIGN_PRIMARY (0x0001)
#define TOKEN_DUPLICATE (0x0002)
#define TOKEN_IMPERSONATE (0x0004)
#define TOKEN_QUERY (0x0008)
#define TOKEN_QUERY_SOURCE (0x0010)
#define TOKEN_ADJUST_PRIVILEGES (0x0020)
#define TOKEN_ADJUST_GROUPS (0x0040)
#define TOKEN_ADJUST_DEFAULT (0x0080)
#define TOKEN_ADJUST_SESSIONID (0x0100)

#define TOKEN_ALL_ACCESS_P                                                                   \
	(STANDARD_RIGHTS_REQUIRED | TOKEN_ASSIGN_PRIMARY | TOKEN_DUPLICATE | TOKEN_IMPERSONATE | \
	 TOKEN_QUERY | TOKEN_QUERY_SOURCE | TOKEN_ADJUST_PRIVILEGES | TOKEN_ADJUST_GROUPS |      \
	 TOKEN_ADJUST_DEFAULT)

#define TOKEN_ALL_ACCESS (TOKEN_ALL_ACCESS_P | TOKEN_ADJUST_SESSIONID)

#define TOKEN_READ (STANDARD_RIGHTS_READ | TOKEN_QUERY)

#define TOKEN_WRITE \
	(STANDARD_RIGHTS_WRITE | TOKEN_ADJUST_PRIVILEGES | TOKEN_ADJUST_GROUPS | TOKEN_ADJUST_DEFAULT)

#define TOKEN_EXECUTE (STANDARD_RIGHTS_EXECUTE)

#define TOKEN_MANDATORY_POLICY_OFF 0x0
#define TOKEN_MANDATORY_POLICY_NO_WRITE_UP 0x1
#define TOKEN_MANDATORY_POLICY_NEW_PROCESS_MIN 0x2

#define TOKEN_MANDATORY_POLICY_VALID_MASK \
	(TOKEN_MANDATORY_POLICY_NO_WRITE_UP | TOKEN_MANDATORY_POLICY_NEW_PROCESS_MIN)

#define POLICY_AUDIT_SUBCATEGORY_COUNT (56)

#define TOKEN_SOURCE_LENGTH 8

#define SID_REVISION (1)
#define SID_MAX_SUB_AUTHORITIES (15)
#define SID_RECOMMENDED_SUB_AUTHORITIES (1)

#define SID_HASH_SIZE 32

#define SECURITY_MANDATORY_UNTRUSTED_RID 0x0000
#define SECURITY_MANDATORY_LOW_RID 0x1000
#define SECURITY_MANDATORY_MEDIUM_RID 0x2000
#define SECURITY_MANDATORY_HIGH_RID 0x3000
#define SECURITY_MANDATORY_SYSTEM_RID 0x4000

#define SECURITY_NULL_SID_AUTHORITY \
	{                               \
		0, 0, 0, 0, 0, 0            \
	}
#define SECURITY_WORLD_SID_AUTHORITY \
	{                                \
		0, 0, 0, 0, 0, 1             \
	}
#define SECURITY_LOCAL_SID_AUTHORITY \
	{                                \
		0, 0, 0, 0, 0, 2             \
	}
#define SECURITY_CREATOR_SID_AUTHORITY \
	{                                  \
		0, 0, 0, 0, 0, 3               \
	}
#define SECURITY_NON_UNIQUE_AUTHORITY \
	{                                 \
		0, 0, 0, 0, 0, 4              \
	}
#define SECURITY_RESOURCE_MANAGER_AUTHORITY \
	{                                       \
		0, 0, 0, 0, 0, 9                    \
	}

#define SECURITY_NULL_RID (0x00000000L)
#define SECURITY_WORLD_RID (0x00000000L)
#define SECURITY_LOCAL_RID (0x00000000L)
#define SECURITY_LOCAL_LOGON_RID (0x00000001L)

#define SECURITY_CREATOR_OWNER_RID (0x00000000L)
#define SECURITY_CREATOR_GROUP_RID (0x00000001L)
#define SECURITY_CREATOR_OWNER_SERVER_RID (0x00000002L)
#define SECURITY_CREATOR_GROUP_SERVER_RID (0x00000003L)
#define SECURITY_CREATOR_OWNER_RIGHTS_RID (0x00000004L)

typedef PVOID PACCESS_TOKEN;
typedef PVOID PCLAIMS_BLOB;

typedef struct
{
	LUID Luid;
	DWORD Attributes;
} LUID_AND_ATTRIBUTES, *PLUID_AND_ATTRIBUTES;
typedef LUID_AND_ATTRIBUTES LUID_AND_ATTRIBUTES_ARRAY[ANYSIZE_ARRAY];
typedef LUID_AND_ATTRIBUTES_ARRAY* PLUID_AND_ATTRIBUTES_ARRAY;

typedef struct
{
	BYTE Value[6];
} SID_IDENTIFIER_AUTHORITY, *PSID_IDENTIFIER_AUTHORITY;

typedef struct
{
	BYTE Revision;
	BYTE SubAuthorityCount;
	SID_IDENTIFIER_AUTHORITY IdentifierAuthority;
	DWORD SubAuthority[ANYSIZE_ARRAY];
} SID, *PISID;

typedef enum
{
	SidTypeUser = 1,
	SidTypeGroup,
	SidTypeDomain,
	SidTypeAlias,
	SidTypeWellKnownGroup,
	SidTypeDeletedAccount,
	SidTypeInvalid,
	SidTypeUnknown,
	SidTypeComputer,
	SidTypeLabel
} SID_NAME_USE,
    *PSID_NAME_USE;

typedef struct
{
	PSID Sid;
	DWORD Attributes;
} SID_AND_ATTRIBUTES, *PSID_AND_ATTRIBUTES;

typedef SID_AND_ATTRIBUTES SID_AND_ATTRIBUTES_ARRAY[ANYSIZE_ARRAY];
typedef SID_AND_ATTRIBUTES_ARRAY* PSID_AND_ATTRIBUTES_ARRAY;

typedef ULONG_PTR SID_HASH_ENTRY, *PSID_HASH_ENTRY;

typedef struct
{
	DWORD SidCount;
	PSID_AND_ATTRIBUTES SidAttr;
	SID_HASH_ENTRY Hash[SID_HASH_SIZE];
} SID_AND_ATTRIBUTES_HASH, *PSID_AND_ATTRIBUTES_HASH;

typedef enum
{
	TokenPrimary = 1,
	TokenImpersonation
} TOKEN_TYPE;
typedef TOKEN_TYPE* PTOKEN_TYPE;

typedef enum
{
	TokenElevationTypeDefault = 1,
	TokenElevationTypeFull,
	TokenElevationTypeLimited
} TOKEN_ELEVATION_TYPE,
    *PTOKEN_ELEVATION_TYPE;

typedef enum
{
	TokenUser = 1,
	TokenGroups,
	TokenPrivileges,
	TokenOwner,
	TokenPrimaryGroup,
	TokenDefaultDacl,
	TokenSource,
	TokenType,
	TokenImpersonationLevel,
	TokenStatistics,
	TokenRestrictedSids,
	TokenSessionId,
	TokenGroupsAndPrivileges,
	TokenSessionReference,
	TokenSandBoxInert,
	TokenAuditPolicy,
	TokenOrigin,
	TokenElevationType,
	TokenLinkedToken,
	TokenElevation,
	TokenHasRestrictions,
	TokenAccessInformation,
	TokenVirtualizationAllowed,
	TokenVirtualizationEnabled,
	TokenIntegrityLevel,
	TokenUIAccess,
	TokenMandatoryPolicy,
	TokenLogonSid,
	TokenIsAppContainer,
	TokenCapabilities,
	TokenAppContainerSid,
	TokenAppContainerNumber,
	TokenUserClaimAttributes,
	TokenDeviceClaimAttributes,
	TokenRestrictedUserClaimAttributes,
	TokenRestrictedDeviceClaimAttributes,
	TokenDeviceGroups,
	TokenRestrictedDeviceGroups,
	TokenSecurityAttributes,
	TokenIsRestricted,
	MaxTokenInfoClass
} TOKEN_INFORMATION_CLASS,
    *PTOKEN_INFORMATION_CLASS;

typedef struct
{
	SID_AND_ATTRIBUTES User;
} TOKEN_USER, *PTOKEN_USER;

typedef struct
{
	DWORD GroupCount;
	SID_AND_ATTRIBUTES Groups[ANYSIZE_ARRAY];
} TOKEN_GROUPS, *PTOKEN_GROUPS;

typedef struct
{
	DWORD PrivilegeCount;
	LUID_AND_ATTRIBUTES Privileges[ANYSIZE_ARRAY];
} TOKEN_PRIVILEGES, *PTOKEN_PRIVILEGES;

typedef struct
{
	PSID Owner;
} TOKEN_OWNER, *PTOKEN_OWNER;

typedef struct
{
	PSID PrimaryGroup;
} TOKEN_PRIMARY_GROUP, *PTOKEN_PRIMARY_GROUP;

typedef struct
{
	PACL DefaultDacl;
} TOKEN_DEFAULT_DACL, *PTOKEN_DEFAULT_DACL;

typedef struct
{
	PCLAIMS_BLOB UserClaims;
} TOKEN_USER_CLAIMS, *PTOKEN_USER_CLAIMS;

typedef struct
{
	PCLAIMS_BLOB DeviceClaims;
} TOKEN_DEVICE_CLAIMS, *PTOKEN_DEVICE_CLAIMS;

typedef struct
{
	DWORD SidCount;
	DWORD SidLength;
	PSID_AND_ATTRIBUTES Sids;
	DWORD RestrictedSidCount;
	DWORD RestrictedSidLength;
	PSID_AND_ATTRIBUTES RestrictedSids;
	DWORD PrivilegeCount;
	DWORD PrivilegeLength;
	PLUID_AND_ATTRIBUTES Privileges;
	LUID AuthenticationId;
} TOKEN_GROUPS_AND_PRIVILEGES, *PTOKEN_GROUPS_AND_PRIVILEGES;

typedef struct
{
	HANDLE LinkedToken;
} TOKEN_LINKED_TOKEN, *PTOKEN_LINKED_TOKEN;

typedef struct
{
	DWORD TokenIsElevated;
} TOKEN_ELEVATION, *PTOKEN_ELEVATION;

typedef struct
{
	SID_AND_ATTRIBUTES Label;
} TOKEN_MANDATORY_LABEL, *PTOKEN_MANDATORY_LABEL;

typedef struct
{
	DWORD Policy;
} TOKEN_MANDATORY_POLICY, *PTOKEN_MANDATORY_POLICY;

typedef struct
{
	PSID_AND_ATTRIBUTES_HASH SidHash;
	PSID_AND_ATTRIBUTES_HASH RestrictedSidHash;
	PTOKEN_PRIVILEGES Privileges;
	LUID AuthenticationId;
	TOKEN_TYPE TokenType;
	SECURITY_IMPERSONATION_LEVEL ImpersonationLevel;
	TOKEN_MANDATORY_POLICY MandatoryPolicy;
	DWORD Flags;
	DWORD AppContainerNumber;
	PSID PackageSid;
	PSID_AND_ATTRIBUTES_HASH CapabilitiesHash;
} TOKEN_ACCESS_INFORMATION, *PTOKEN_ACCESS_INFORMATION;

typedef struct
{
	BYTE PerUserPolicy[((POLICY_AUDIT_SUBCATEGORY_COUNT) >> 1) + 1];
} TOKEN_AUDIT_POLICY, *PTOKEN_AUDIT_POLICY;

typedef struct
{
	CHAR SourceName[TOKEN_SOURCE_LENGTH];
	LUID SourceIdentifier;
} TOKEN_SOURCE, *PTOKEN_SOURCE;

typedef struct
{
	LUID TokenId;
	LUID AuthenticationId;
	LARGE_INTEGER ExpirationTime;
	TOKEN_TYPE TokenType;
	SECURITY_IMPERSONATION_LEVEL ImpersonationLevel;
	DWORD DynamicCharged;
	DWORD DynamicAvailable;
	DWORD GroupCount;
	DWORD PrivilegeCount;
	LUID ModifiedId;
} TOKEN_STATISTICS, *PTOKEN_STATISTICS;

typedef struct
{
	LUID TokenId;
	LUID AuthenticationId;
	LUID ModifiedId;
	TOKEN_SOURCE TokenSource;
} TOKEN_CONTROL, *PTOKEN_CONTROL;

typedef struct
{
	LUID OriginatingLogonSession;
} TOKEN_ORIGIN, *PTOKEN_ORIGIN;

typedef enum
{
	MandatoryLevelUntrusted = 0,
	MandatoryLevelLow,
	MandatoryLevelMedium,
	MandatoryLevelHigh,
	MandatoryLevelSystem,
	MandatoryLevelSecureProcess,
	MandatoryLevelCount
} MANDATORY_LEVEL,
    *PMANDATORY_LEVEL;

typedef struct
{
	PSID TokenAppContainer;
} TOKEN_APPCONTAINER_INFORMATION, *PTOKEN_APPCONTAINER_INFORMATION;

#ifdef __cplusplus
extern "C"
{
#endif

	WINPR_API BOOL InitializeSecurityDescriptor(PSECURITY_DESCRIPTOR pSecurityDescriptor,
	                                            DWORD dwRevision);
	WINPR_API DWORD GetSecurityDescriptorLength(PSECURITY_DESCRIPTOR pSecurityDescriptor);
	WINPR_API BOOL IsValidSecurityDescriptor(PSECURITY_DESCRIPTOR pSecurityDescriptor);

	WINPR_API BOOL GetSecurityDescriptorControl(PSECURITY_DESCRIPTOR pSecurityDescriptor,
	                                            PSECURITY_DESCRIPTOR_CONTROL pControl,
	                                            LPDWORD lpdwRevision);
	WINPR_API BOOL SetSecurityDescriptorControl(PSECURITY_DESCRIPTOR pSecurityDescriptor,
	                                            SECURITY_DESCRIPTOR_CONTROL ControlBitsOfInterest,
	                                            SECURITY_DESCRIPTOR_CONTROL ControlBitsToSet);

	WINPR_API BOOL GetSecurityDescriptorDacl(PSECURITY_DESCRIPTOR pSecurityDescriptor,
	                                         LPBOOL lpbDaclPresent, PACL* pDacl,
	                                         LPBOOL lpbDaclDefaulted);
	WINPR_API BOOL SetSecurityDescriptorDacl(PSECURITY_DESCRIPTOR pSecurityDescriptor,
	                                         BOOL bDaclPresent, PACL pDacl, BOOL bDaclDefaulted);

	WINPR_API BOOL GetSecurityDescriptorGroup(PSECURITY_DESCRIPTOR pSecurityDescriptor,
	                                          PSID* pGroup, LPBOOL lpbGroupDefaulted);
	WINPR_API BOOL SetSecurityDescriptorGroup(PSECURITY_DESCRIPTOR pSecurityDescriptor, PSID pGroup,
	                                          BOOL bGroupDefaulted);

	WINPR_API BOOL GetSecurityDescriptorOwner(PSECURITY_DESCRIPTOR pSecurityDescriptor,
	                                          PSID* pOwner, LPBOOL lpbOwnerDefaulted);
	WINPR_API BOOL SetSecurityDescriptorOwner(PSECURITY_DESCRIPTOR pSecurityDescriptor, PSID pOwner,
	                                          BOOL bOwnerDefaulted);

	WINPR_API DWORD GetSecurityDescriptorRMControl(PSECURITY_DESCRIPTOR SecurityDescriptor,
	                                               PUCHAR RMControl);
	WINPR_API DWORD SetSecurityDescriptorRMControl(PSECURITY_DESCRIPTOR SecurityDescriptor,
	                                               PUCHAR RMControl);

	WINPR_API BOOL GetSecurityDescriptorSacl(PSECURITY_DESCRIPTOR pSecurityDescriptor,
	                                         LPBOOL lpbSaclPresent, PACL* pSacl,
	                                         LPBOOL lpbSaclDefaulted);
	WINPR_API BOOL SetSecurityDescriptorSacl(PSECURITY_DESCRIPTOR pSecurityDescriptor,
	                                         BOOL bSaclPresent, PACL pSacl, BOOL bSaclDefaulted);

#ifdef __cplusplus
}
#endif

#endif

#endif /* WINPR_SECURITY_H */
