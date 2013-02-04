# FreeRDP: A Remote Desktop Protocol Implementation
# include headers cmake build script
#
# Copyright 2011 O.S. Systems Software Ltda.
# Copyright 2011 Otavio Salvador <otavio@ossystems.com.br>
# Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

file(GLOB FREERDP_HEADERS "freerdp/*.h")
install(FILES ${FREERDP_HEADERS} DESTINATION include/freerdp COMPONENT headers)

install(DIRECTORY freerdp/cache DESTINATION include/freerdp COMPONENT headers FILES_MATCHING PATTERN "*.h")
install(DIRECTORY freerdp/codec DESTINATION include/freerdp COMPONENT headers FILES_MATCHING PATTERN "*.h")
install(DIRECTORY freerdp/crypto DESTINATION include/freerdp COMPONENT headers FILES_MATCHING PATTERN "*.h")
install(DIRECTORY freerdp/gdi DESTINATION include/freerdp COMPONENT headers FILES_MATCHING PATTERN "*.h")
install(DIRECTORY freerdp/locale DESTINATION include/freerdp COMPONENT headers FILES_MATCHING PATTERN "*.h")
install(DIRECTORY freerdp/rail DESTINATION include/freerdp COMPONENT headers FILES_MATCHING PATTERN "*.h")
install(DIRECTORY freerdp/utils DESTINATION include/freerdp COMPONENT headers FILES_MATCHING PATTERN "*.h")
install(DIRECTORY freerdp/client DESTINATION include/freerdp COMPONENT headers FILES_MATCHING PATTERN "*.h")
install(DIRECTORY freerdp/server DESTINATION include/freerdp COMPONENT headers FILES_MATCHING PATTERN "*.h")
install(DIRECTORY freerdp/channels DESTINATION include/freerdp COMPONENT headers FILES_MATCHING PATTERN "*.h")
