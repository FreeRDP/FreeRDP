
# types: DISABLED < RUNTIME < OPTIONAL < RECOMMENDED < REQUIRED

macro(find_feature _feature _type _purpose _description)

	string(TOUPPER ${_feature} _feature_upper)
	string(TOLOWER ${_type} _type_lower)

	if(${_type} STREQUAL "DISABLED")
		set(_feature_default "OFF")
		message(STATUS "Skipping ${_type_lower} feature ${_feature} for ${_purpose} (${_description})")
	else()
		if(${_type} STREQUAL "REQUIRED")
			set(_feature_default "ON")
			message(STATUS "Finding ${_type_lower} feature ${_feature} for ${_purpose} (${_description})")
			find_package(${_feature} REQUIRED)
		elseif(${_type} STREQUAL "RECOMMENDED")
			if(NOT ${WITH_${_feature_upper}})
				set(_feature_default "OFF")
				message(STATUS "Skipping ${_type_lower} feature ${_feature} for ${_purpose} (${_description})")
			else()
				set(_feature_default "ON")
				message(STATUS "Finding ${_type_lower} feature ${_feature} for ${_purpose} (${_description})")
				message(STATUS "    Disable feature ${_feature} using \"-DWITH_${_feature_upper}=OFF\"")
				find_package(${_feature})
			endif()
		elseif(${_type} STREQUAL "OPTIONAL")
			if(${WITH_${_feature_upper}})
				set(_feature_default "ON")
				message(STATUS "Finding ${_type_lower} feature ${_feature} for ${_purpose} (${_description})")
				find_package(${_feature} REQUIRED)
			else()
				set(_feature_default "OFF")
				message(STATUS "Skipping ${_type_lower} feature ${_feature} for ${_purpose} (${_description})")
				message(STATUS "    Enable feature ${_feature} using \"-DWITH_${_feature_upper}=ON\"")
			endif()
		else()
			set(_feature_default "ON")
			message(STATUS "Finding ${_type_lower} feature ${_feature} for ${_purpose} (${_description})")
			find_package(${_feature})
		endif()
		

		if(NOT ${${_feature_upper}_FOUND})
			if(${_feature_default})
				message(WARNING "    feature ${_feature} was requested but could not be found! ${_feature_default} / ${${_feature_upper}_FOUND}")
			endif()
			set(_feature_default "OFF")
		endif()

		option(WITH_${_feature_upper} "Enable feature ${_feature} for ${_purpose}" ${_feature_default})

		set_package_properties(${_feature} PROPERTIES
			TYPE ${_type}
			PURPOSE "${_purpose}"
			DESCRIPTION "${_description}")
	endif()
endmacro(find_feature)

