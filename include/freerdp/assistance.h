/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Remote Assistance
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_REMOTE_ASSISTANCE_H
#define FREERDP_REMOTE_ASSISTANCE_H

#include <freerdp/api.h>
#include <freerdp/freerdp.h>

typedef struct rdp_assistance_file rdpAssistanceFile;

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_API BYTE* freerdp_assistance_hex_string_to_bin(const void* str, size_t* size);
	FREERDP_API char* freerdp_assistance_bin_to_hex_string(const void* data, size_t size);

	FREERDP_API char* freerdp_assistance_generate_pass_stub(DWORD flags);
	FREERDP_API char* freerdp_assistance_construct_expert_blob(const char* name, const char* pass);
	FREERDP_API BYTE* freerdp_assistance_encrypt_pass_stub(const char* password,
	                                                       const char* passStub,
	                                                       size_t* pEncryptedSize);

	FREERDP_API int freerdp_assistance_set_connection_string2(rdpAssistanceFile* file,
	                                                          const char* string,
	                                                          const char* password);

	FREERDP_API int freerdp_assistance_parse_file_buffer(rdpAssistanceFile* file,
	                                                     const char* buffer, size_t size,
	                                                     const char* password);
	FREERDP_API int freerdp_assistance_parse_file(rdpAssistanceFile* file, const char* name,
	                                              const char* password);

	FREERDP_API BOOL freerdp_assistance_populate_settings_from_assistance_file(
	    rdpAssistanceFile* file, rdpSettings* settings);
	FREERDP_API BOOL freerdp_assistance_get_encrypted_pass_stub(rdpAssistanceFile* file,
	                                                            const char** pwd, size_t* size);

	FREERDP_API rdpAssistanceFile* freerdp_assistance_file_new(void);
	FREERDP_API void freerdp_assistance_file_free(rdpAssistanceFile* file);

	FREERDP_API void freerdp_assistance_print_file(rdpAssistanceFile* file, wLog* log, DWORD level);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_REMOTE_ASSISTANCE_H */
