cmake_minimum_required(VERSION 3.13)

if(POLICY CMP0091)
  cmake_policy(SET CMP0091 NEW)
endif()

project(tzextract VERSION 1.0.0 LANGUAGES CSharp)

set(CMAKE_CSharp_FLAGS "/langversion:10")
set(CMAKE_DOTNET_TARGET_FRAMEWORK "net6.0")
set(CMAKE_DOTNET_SDK "Microsoft.NET.Sdk")

add_executable(${PROJECT_NAME} tzextract.cs)

set_property(TARGET ${PROJECT_NAME} PROPERTY WIN32_EXECUTABLE FALSE)

set_property(
  TARGET ${PROJECT_NAME}
  PROPERTY VS_DOTNET_REFERENCES
           "System"
           "System.Collections.Generic"
           "System.IO"
           "System.Net.Http"
           "System.Linq"
           "System.Threading"
           "System.Threading.Tasks"
)
install(TARGETS ${PROJECT_NAME} ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBRARY_DIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBRARY_DIR} RUNTIME DESTINATION ${CMAKE_INSTALL_BINARY_DIR} INCLUDES
        DESTINATION ${CMAKE_INSTALL_INCLUDE_DIR}
)
