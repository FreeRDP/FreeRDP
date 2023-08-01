include (ExternalProject)

set(CMAKE_ARGS "")
get_cmake_property(_variableNames VARIABLES)
list (SORT _variableNames)
foreach (_variableName ${_variableNames})
    if (NOT _variableName MATCHES "CMAKE_INSTALL_PREFIX")
        if (NOT _variableName MATCHES "CMAKE_POSITION_INDEPENDENT_CODE")
            if (_variableName MATCHES "^CMAKE_[^ \t]+")
                list(APPEND CMAKE_ARGS "-D${_variableName}=${${_variableName}}")
            endif()
        endif()
    endif()
endforeach()
string (REPLACE ";" " " CMAKE_ARGS "${CMAKE_ARGS}")

ExternalProject_Add(lodepngexternal
    DOWNLOAD_COMMAND
        GIT_REPOSITORY https://github.com/lvandeve/lodepng.git
        GIT_TAG master
        GIT_SHALLOW TRUE
        CMAKE_ARGS ${CMAKE_ARGS}i -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>)

ExternalProject_Add_Step(lodepngexternal
    pre-config
    COMMAND ${CMAKE_COMMAND} -E copy  lodepng.cpp lodepng.c
    COMMAND ${CMAKE_COMMAND} -E rm -f CMakeLists.txt
    COMMAND ${CMAKE_COMMAND} -E echo "cmake_minimum_required(VERSION 3.13)" >> CMakeLists.txt
    COMMAND ${CMAKE_COMMAND} -E echo "project(prjliblodepng)" >> CMakeLists.txt
    COMMAND ${CMAKE_COMMAND} -E echo "add_library(lodepng STATIC lodepng.h lodepng.c)" >> CMakeLists.txt
    COMMAND ${CMAKE_COMMAND} -E echo "install(TARGETS lodepng DESTINATION lib)" >> CMakeLists.txt
    COMMAND ${CMAKE_COMMAND} -E echo "install(FILES lodepng.h DESTINATION include)" >> CMakeLists.txt
    BYPRODUCTS lodepng.c CMakeLists.txt
    DEPENDEES patch 
    DEPENDERS configure
    WORKING_DIRECTORY <SOURCE_DIR>
)

ExternalProject_Get_Property(lodepngexternal install_dir)

add_library(lodepng STATIC IMPORTED GLOBAL)
set_property(TARGET lodepng PROPERTY IMPORTED_LOCATION ${install_dir}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}lodepng${CMAKE_STATIC_LIBRARY_SUFFIX})
add_dependencies(lodepng lodepngexternal)

set(LODEPNG_LIB lodepng)
set(LODEPNG_INC ${install_dir}/include)

set(lodepng_LIBRARY ${LODEPNG_LIB} CACHE PATH "lodepng strings" FORCE)
set(lodepng_LIBRARIES ${LODEPNG_LIB} CACHE PATH "lodepng strings" FORCE)
set(lodepng_INCLUDE_DIR ${LODEPNG_INC} CACHE PATH "lodepng strings" FORCE)
set(lodepng_INCLUDE_DIRS ${LODEPNG_INC} CACHE PATH "lodepng strings" FORCE)

