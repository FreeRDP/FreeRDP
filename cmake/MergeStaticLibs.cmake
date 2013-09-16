 
#    Copyright (C) 2012 Modelon AB

#    This program is free software: you can redistribute it and/or modify
#    it under the terms of the BSD style license.

#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    FMILIB_License.txt file for more details.

#    You should have received a copy of the FMILIB_License.txt file
#    along with this program. If not, contact Modelon AB <http://www.modelon.com>.

# Merge_static_libs(output_library lib1 lib2 ... libn) merges a number of static
# libs into a single static library
function(merge_static_libs output_library)
	set(output_target "${output_library}")
	string(REGEX REPLACE "-" "_" output_library ${output_library})
	set(libs ${ARGV})
	list(REMOVE_AT libs 0)
	
	# Create a dummy file that the target will depend on
	set(dummyfile ${CMAKE_CURRENT_BINARY_DIR}/${output_library}_dummy.c)
	file(WRITE ${dummyfile} "const char * dummy = \"${dummyfile}\";")
	
	add_library(${output_target} STATIC ${dummyfile})

	if("${CMAKE_CFG_INTDIR}" STREQUAL ".")
		set(multiconfig FALSE)
	else()
		set(multiconfig TRUE)
	endif()
	
	# First get the file names of the libraries to be merged	
	foreach(lib ${libs})
		get_target_property(libtype ${lib} TYPE)
		if(NOT libtype STREQUAL "STATIC_LIBRARY")
			message(FATAL_ERROR "Merge_static_libs can only process static libraries")
		endif()
		if(multiconfig)
			foreach(CONFIG_TYPE ${CMAKE_CONFIGURATION_TYPES})
				get_target_property("libfile_${CONFIG_TYPE}" ${lib} "LOCATION_${CONFIG_TYPE}")
				list(APPEND libfiles_${CONFIG_TYPE} ${libfile_${CONFIG_TYPE}})
			endforeach()
		else()
			get_target_property(libfile ${lib} LOCATION)
			list(APPEND libfiles "${libfile}")
		endif(multiconfig)
	endforeach()

	# Just to be sure: cleanup from duplicates
	if(multiconfig)	
		foreach(CONFIG_TYPE ${CMAKE_CONFIGURATION_TYPES})
			list(REMOVE_DUPLICATES libfiles_${CONFIG_TYPE})
			set(libfiles ${libfiles} ${libfiles_${CONFIG_TYPE}})
		endforeach()
	endif()
	list(REMOVE_DUPLICATES libfiles)

	# Now the easy part for MSVC and for MAC
  if(MSVC)
    # lib.exe does the merging of libraries just need to conver the list into string
	foreach(CONFIG_TYPE ${CMAKE_CONFIGURATION_TYPES})
		set(flags "")
		foreach(lib ${libfiles_${CONFIG_TYPE}})
			set(flags "${flags} ${lib}")
		endforeach()
		string(TOUPPER "STATIC_LIBRARY_FLAGS_${CONFIG_TYPE}" PROPNAME)
		set_target_properties(${output_target} PROPERTIES ${PROPNAME} "${flags}")
	endforeach()
	
  elseif(APPLE)
    # Use OSX's libtool to merge archives
	if(multiconfig)
		message(FATAL_ERROR "Multiple configurations are not supported")
	endif()
	get_target_property(outfile ${output_target} LOCATION)  
	add_custom_command(TARGET ${output_target} POST_BUILD
		COMMAND rm ${outfile}
		COMMAND /usr/bin/libtool -static -o ${outfile} 
		${libfiles}
	)
  else() 
  # general UNIX - need to "ar -x" and then "ar -ru"
	if(multiconfig)
		message(FATAL_ERROR "Multiple configurations are not supported")
	endif()
	get_target_property(outfile ${output_target} LOCATION)
	message(STATUS "output file location is ${outfile}")
	foreach(lib ${libfiles})
		# objlistfile will contain the list of object files for the library
		set(objlistfile ${lib}.objlist)
		set(objdir ${lib}.objdir)
		set(objlistcmake  ${objlistfile}.cmake)
		get_filename_component(libname ${lib} NAME_WE)

		if(${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/cmake.check_cache IS_NEWER_THAN ${objlistcmake})

			file(WRITE ${objlistcmake} "
				# delete previous object files
				message(STATUS \"Removing previous object files from ${lib}\")
				EXECUTE_PROCESS(COMMAND ls .
					WORKING_DIRECTORY ${objdir}
					COMMAND xargs -I {} rm {}
					WORKING_DIRECTORY ${objdir})
				# Extract object files from the library
				message(STATUS \"Extracting object files from ${lib}\")
				EXECUTE_PROCESS(COMMAND ${CMAKE_AR} -x ${lib}
					WORKING_DIRECTORY ${objdir})
				# Prefixing object files to avoid conflicts
				message(STATUS \"Prefixing object files to avoid conflicts\")
				EXECUTE_PROCESS(COMMAND ls .
					WORKING_DIRECTORY ${objdir}
					COMMAND xargs -I {} mv {} ${libname}_{}
					WORKING_DIRECTORY ${objdir})
				# save the list of object files
				EXECUTE_PROCESS(COMMAND ls . 
					OUTPUT_FILE ${objlistfile}
					WORKING_DIRECTORY ${objdir})
			")
				
			file(MAKE_DIRECTORY ${objdir})
				
			add_custom_command(
				OUTPUT ${objlistfile}
				COMMAND ${CMAKE_COMMAND} -P ${objlistcmake}
				DEPENDS ${lib})
			
		endif()
		
		list(APPEND extrafiles "${objlistfile}")
		# relative path is needed by ar under MSYS
		file(RELATIVE_PATH objlistfilerpath ${objdir} ${objlistfile})
		add_custom_command(TARGET ${output_target} POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E echo "Running: ${CMAKE_AR} ru ${outfile} @${objlistfilerpath}"
			COMMAND ${CMAKE_AR} ru "${outfile}" @"${objlistfilerpath}"
			#COMMAND ld -r -static -o "${outfile}" --whole-archive @"${objlistfilerpath}"
			WORKING_DIRECTORY ${objdir})
	endforeach()
	add_custom_command(TARGET ${output_target} POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E echo "Running: ${CMAKE_RANLIB} ${outfile}"
			COMMAND ${CMAKE_RANLIB} ${outfile})
  endif()
  file(WRITE ${dummyfile}.base "const char* ${output_library}_sublibs=\"${libs}\";")
  add_custom_command( 
		OUTPUT ${dummyfile}
		COMMAND ${CMAKE_COMMAND} -E copy ${dummyfile}.base ${dummyfile}
		DEPENDS ${libs} ${extrafiles})

endfunction()
