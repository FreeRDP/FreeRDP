# WinPR: Windows Portable Runtime
# libwinpr-utils cmake build script
#
# Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

set(MODULE_NAME "winpr-utils")
set(MODULE_PREFIX "WINPR_UTILS")

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(${MODULE_PREFIX}_COLLECTIONS_SRCS
	collections/Queue.c
	collections/Stack.c
	collections/PubSub.c
	collections/BitStream.c
	collections/Reference.c
	collections/ArrayList.c
	collections/Dictionary.c
	collections/LinkedList.c
	collections/HashTable.c
	collections/ListDictionary.c
	collections/KeyValuePair.c
	collections/CountdownEvent.c
	collections/BufferPool.c
	collections/ObjectPool.c
	collections/StreamPool.c
	collections/MessageQueue.c
	collections/MessagePipe.c)

set(${MODULE_PREFIX}_TRIO_SRCS
	trio/strio.h
	trio/trio.c
	trio/trio.h
	trio/triodef.h
	trio/trionan.c
	trio/trionan.h
	trio/triop.h
	trio/triostr.c
	trio/triostr.h)
	
set(${MODULE_PREFIX}_WLOG_SRCS
	wlog/wlog.c
	wlog/wlog.h
	wlog/Layout.c
	wlog/Layout.h
	wlog/Message.c
	wlog/Message.h
	wlog/TextMessage.c
	wlog/TextMessage.h
	wlog/DataMessage.c
	wlog/DataMessage.h
	wlog/ImageMessage.c
	wlog/ImageMessage.h
	wlog/PacketMessage.c
	wlog/PacketMessage.h
	wlog/Appender.c
	wlog/Appender.h
	wlog/FileAppender.c
	wlog/FileAppender.h
	wlog/BinaryAppender.c
	wlog/BinaryAppender.h
	wlog/ConsoleAppender.c
	wlog/ConsoleAppender.h)

set(${MODULE_PREFIX}_SRCS
	ini.c
	sam.c
	ntlm.c
	print.c
	stream.c
	cmdline.c)

set(${MODULE_PREFIX}_SRCS ${${MODULE_PREFIX}_SRCS}
	${${MODULE_PREFIX}_COLLECTIONS_SRCS}
	${${MODULE_PREFIX}_TRIO_SRCS}
	${${MODULE_PREFIX}_WLOG_SRCS})

include_directories("trio")
include_directories(${ZLIB_INCLUDE_DIRS})
include_directories(${OPENSSL_INCLUDE_DIR})

add_complex_library(MODULE ${MODULE_NAME} TYPE "OBJECT"
	MONOLITHIC ${MONOLITHIC_BUILD}
	SOURCES ${${MODULE_PREFIX}_SRCS}
	EXPORT)

set_target_properties(${MODULE_NAME} PROPERTIES VERSION ${WINPR_VERSION_FULL} SOVERSION ${WINPR_VERSION} PREFIX "lib")

set(${MODULE_PREFIX}_LIBS
	${ZLIB_LIBRARIES}
	${OPENSSL_LIBRARIES})

if(UNIX)
	set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} m)
endif()

set_complex_link_libraries(VARIABLE ${MODULE_PREFIX}_LIBS
	MONOLITHIC ${MONOLITHIC_BUILD} INTERNAL
	MODULE winpr
	MODULES winpr-crt winpr-file winpr-path winpr-synch winpr-sysinfo)

if(MONOLITHIC_BUILD)
	set(WINPR_LIBS ${WINPR_LIBS} ${${MODULE_PREFIX}_LIBS} PARENT_SCOPE)
else()	
	target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS})
	install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR} EXPORT WinPRTargets)
endif()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "WinPR")

if(BUILD_TESTING)
	add_subdirectory(test)
endif()

