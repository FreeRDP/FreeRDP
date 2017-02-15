# DbusGlib library detection
#
# Copyright 2013 Thincast Technologies GmbH
# Copyright 2013 Armin Novak <armin.novak@thincast.com>
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

find_package(PkgConfig)
pkg_check_modules(PC_DBUS_GLIB QUIET dbus-glib-1)
set(DBUS_GLIB_DEFINITIONS ${PC_DBUS_GLIB_CFLAGS_OTHER})

find_path(DBUS_GLIB_INCLUDE_DIR dbus/dbus-glib.h
          HINTS ${PC_DBUS_GLIB_INCLUDEDIR} ${PC_DBUS_GLIB_INCLUDE_DIRS}
          PATH_SUFFIXES dbus-glib-1 )

find_library(DBUS_GLIB_LIBRARY NAMES dbus-glib-1 dbus-glib
             HINTS ${PC_DBUS_GLIB_LIBDIR} ${PC_DBUS_GLIB_LIBRARY_DIRS} )

set(DBUS_GLIB_LIBRARIES ${DBUS_GLIB_LIBRARY} )
set(DBUS_GLIB_INCLUDE_DIRS ${DBUS_GLIB_INCLUDE_DIR} )

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set DBUS_GLIB_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(dbus-glib  DEFAULT_MSG
                                  DBUS_GLIB_LIBRARY DBUS_GLIB_INCLUDE_DIR)

if(DBUS_GLIB_LIBRARIES AND DBUS_GLIB_INCLUDE_DIRS)
          set(DBUS_GLIB_FOUND TRUE)
endif()
                                  
mark_as_advanced(DBUS_GLIB_INCLUDE_DIR DBUS_GLIB_LIBRARY)
