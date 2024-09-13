/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * ASN1 routines for RDPEAR
 *
 * Copyright 2024 David Fort <contact@hardening-consulting.com>
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

#ifndef RPDEAR_RDPEAR_ASN1_H__
#define RPDEAR_RDPEAR_ASN1_H__

#include <krb5.h>

#include <winpr/stream.h>

wStream* rdpear_enc_Checksum(UINT32 cksumtype, krb5_checksum* csum);

wStream* rdpear_enc_EncryptedData(UINT32 encType, krb5_data* payload);

#endif /* RPDEAR_RDPEAR_ASN1_H__ */
