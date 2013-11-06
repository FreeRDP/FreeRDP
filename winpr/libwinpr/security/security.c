/**
 * WinPR: Windows Portable Runtime
 * Base Security Functions
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <winpr/security.h>

/**
 * api-ms-win-security-base-l1-2-0.dll:
 *
 * AccessCheck
 * AccessCheckAndAuditAlarmW
 * AccessCheckByType
 * AccessCheckByTypeAndAuditAlarmW
 * AccessCheckByTypeResultList
 * AccessCheckByTypeResultListAndAuditAlarmByHandleW
 * AccessCheckByTypeResultListAndAuditAlarmW
 * AddAccessAllowedAce
 * AddAccessAllowedAceEx
 * AddAccessAllowedObjectAce
 * AddAccessDeniedAce
 * AddAccessDeniedAceEx
 * AddAccessDeniedObjectAce
 * AddAce
 * AddAuditAccessAce
 * AddAuditAccessAceEx
 * AddAuditAccessObjectAce
 * AddMandatoryAce
 * AddResourceAttributeAce
 * AddScopedPolicyIDAce
 * AdjustTokenGroups
 * AdjustTokenPrivileges
 * AllocateAndInitializeSid
 * AllocateLocallyUniqueId
 * AreAllAccessesGranted
 * AreAnyAccessesGranted
 * CheckTokenCapability
 * CheckTokenMembership
 * CheckTokenMembershipEx
 * ConvertToAutoInheritPrivateObjectSecurity
 * CopySid
 * CreatePrivateObjectSecurity
 * CreatePrivateObjectSecurityEx
 * CreatePrivateObjectSecurityWithMultipleInheritance
 * CreateRestrictedToken
 * CreateWellKnownSid
 * DeleteAce
 * DestroyPrivateObjectSecurity
 * DuplicateToken
 * DuplicateTokenEx
 * EqualDomainSid
 * EqualPrefixSid
 * EqualSid
 * FindFirstFreeAce
 * FreeSid
 * GetAce
 * GetAclInformation
 * GetAppContainerAce
 * GetCachedSigningLevel
 * GetFileSecurityW
 * GetKernelObjectSecurity
 * GetLengthSid
 * GetPrivateObjectSecurity
 * GetSecurityDescriptorControl
 * GetSecurityDescriptorDacl
 * GetSecurityDescriptorGroup
 * GetSecurityDescriptorLength
 * GetSecurityDescriptorOwner
 * GetSecurityDescriptorRMControl
 * GetSecurityDescriptorSacl
 * GetSidIdentifierAuthority
 * GetSidLengthRequired
 * GetSidSubAuthority
 * GetSidSubAuthorityCount
 * GetTokenInformation
 * GetWindowsAccountDomainSid
 * ImpersonateAnonymousToken
 * ImpersonateLoggedOnUser
 * ImpersonateSelf
 * InitializeAcl
 * InitializeSecurityDescriptor
 * InitializeSid
 * IsTokenRestricted
 * IsValidAcl
 * IsValidSecurityDescriptor
 * IsValidSid
 * IsWellKnownSid
 * MakeAbsoluteSD
 * MakeSelfRelativeSD
 * MapGenericMask
 * ObjectCloseAuditAlarmW
 * ObjectDeleteAuditAlarmW
 * ObjectOpenAuditAlarmW
 * ObjectPrivilegeAuditAlarmW
 * PrivilegeCheck
 * PrivilegedServiceAuditAlarmW
 * QuerySecurityAccessMask
 * RevertToSelf
 * SetAclInformation
 * SetCachedSigningLevel
 * SetFileSecurityW
 * SetKernelObjectSecurity
 * SetPrivateObjectSecurity
 * SetPrivateObjectSecurityEx
 * SetSecurityAccessMask
 * SetSecurityDescriptorControl
 * SetSecurityDescriptorDacl
 * SetSecurityDescriptorGroup
 * SetSecurityDescriptorOwner
 * SetSecurityDescriptorRMControl
 * SetSecurityDescriptorSacl
 * SetTokenInformation
 */

#ifndef _WIN32

#include "security.h"

#endif

