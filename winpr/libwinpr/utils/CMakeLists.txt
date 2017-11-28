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

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(${MODULE_PREFIX}_COLLECTIONS_SRCS
	collections/Queue.c
	collections/Stack.c
	collections/PubSub.c
	collections/BipBuffer.c
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
	
set(${MODULE_PREFIX}_LODEPNG_SRCS
	lodepng/lodepng.c
	lodepng/lodepng.h)

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

if (HAVE_SYSLOG_H)
	set(SYSLOG_SRCS
		wlog/SyslogAppender.c
		wlog/SyslogAppender.h
	)
endif()

if (LIBSYSTEMD_FOUND)
	set(JOURNALD_SRCS
		wlog/JournaldAppender.c
		wlog/JournaldAppender.h
	)

	winpr_include_directory_add(${LIBSYSTEMD_INCLUDE_DIR})
	winpr_library_add(${LIBSYSTEMD_LIBRARY})
endif()
	
set(${MODULE_PREFIX}_WLOG_SRCS
	wlog/wlog.c
	wlog/wlog.h
	wlog/Layout.c
	wlog/Layout.h
	wlog/Message.c
	wlog/Message.h
	wlog/TextMessage.c
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
	wlog/CallbackAppender.c
	wlog/CallbackAppender.h
	wlog/ConsoleAppender.c
	wlog/ConsoleAppender.h
	wlog/UdpAppender.c
	wlog/UdpAppender.h
	${SYSLOG_SRCS}
	${JOURNALD_SRCS}
	)


set(${MODULE_PREFIX}_SRCS
	ini.c
	sam.c
	ntlm.c
	image.c
	print.c
	stream.c
	debug.c
	winpr.c
	cmdline.c
	ssl.c)

if (ANDROID)
	include_directories(${CMAKE_CURRENT_SOURCE_DIR})
endif()

winpr_module_add(${${MODULE_PREFIX}_SRCS}
	${${MODULE_PREFIX}_COLLECTIONS_SRCS}
	${${MODULE_PREFIX}_LODEPNG_SRCS}
	${${MODULE_PREFIX}_TRIO_SRCS}
	${${MODULE_PREFIX}_WLOG_SRCS})

winpr_include_directory_add(
	"lodepng"
	"trio"
	".")
	
if(OPENSSL_FOUND)
	winpr_include_directory_add(${OPENSSL_INCLUDE_DIR})
	winpr_library_add(${OPENSSL_LIBRARIES})
endif()

if(MBEDTLS_FOUND)
	winpr_include_directory_add(${MBEDTLS_INCLUDE_DIR})
	winpr_library_add(${MBEDTLS_LIBRARIES})
endif()

if(UNIX)
	winpr_library_add(m)
endif()

if((FREEBSD) AND (NOT KFREEBSD))
	winpr_library_add(execinfo)
endif()

if(WIN32)
	winpr_library_add(Dbghelp)
endif()

if(BUILD_TESTING)
	add_subdirectory(test)
endif()

